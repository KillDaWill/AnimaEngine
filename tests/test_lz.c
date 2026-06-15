#include "test_harness.h"
#include "test_main.h"

#include "lz.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * LZ10 compression: inverse of Lz_Decompress10 in source/lz.c.
 *
 *   - Header: 0x10, then 3 little-endian bytes for the uncompressed size.
 *   - Then groups of 1 flag byte + up to 8 literal/back-reference tokens.
 *   - Flag bit 0 = literal byte, flag bit 1 = (length, disp) back-reference.
 *   - Back-reference: b1 = (len-3)<<4 | (disp>>8), b2 = disp & 0xFF.
 *
 * The DS hardware decompressor copies byte-by-byte from out[out_pos - disp - 1]
 * onwards, advancing both pointers. For tight disp values the source range
 * overlaps the destination range, so the i-th destination byte is the i-th
 * source byte *as it stands at the time of the copy*, not the original input.
 *
 * This compressor therefore verifies every candidate back-reference by
 * simulating that byte-by-byte copy against the current output buffer, and
 * falls back to a literal if the back-reference would not reproduce the
 * source. That guarantees the produced stream is roundtrip-safe.
 */
typedef struct LzWriter {
    uint8_t *buf;
    size_t cap;
    size_t len;
    int error;
} LzWriter;

static void LzW_WriteByte(LzWriter *w, uint8_t b)
{
    if (w->len + 1 > w->cap) {
        size_t new_cap = w->cap == 0 ? 64 : w->cap * 2;
        uint8_t *nb = realloc(w->buf, new_cap);
        if (nb == NULL) {
            w->error = 1;
            return;
        }
        w->buf = nb;
        w->cap = new_cap;
    }
    w->buf[w->len++] = b;
}

static void LzW_WriteU24LE(LzWriter *w, uint32_t v)
{
    LzW_WriteByte(w, (uint8_t)(v & 0xFF));
    LzW_WriteByte(w, (uint8_t)((v >> 8) & 0xFF));
    LzW_WriteByte(w, (uint8_t)((v >> 16) & 0xFF));
}

typedef struct LzMatch {
    int length;
    int disp;
} LzMatch;

static LzMatch Lz_FindBestMatch(
    const uint8_t *src,
    int src_len,
    int pos,
    const uint8_t *produced,
    int produced_len)
{
    LzMatch best = { 0, 0 };
    int max_disp = pos > 0 ? pos - 1 : 0;
    int max_len = src_len - pos;
    if (max_len > 18) {
        max_len = 18;
    }
    if (max_disp > 4095) {
        max_disp = 4095;
    }

    /*
     * For a back-reference (length, disp) at output position `pos` to be
     * valid, the byte-by-byte copy from out[pos-disp-1+i] to out[pos+i]
     * must reproduce src[pos+i] for every i. We check that here using the
     * already-produced output buffer.
     */
    for (int disp = 1; disp <= max_disp; disp++) {
        int len = 0;
        int src_pos = pos - disp - 1;
        while (len < max_len) {
            uint8_t copied;
            if (src_pos + len < produced_len) {
                copied = produced[src_pos + len];
            } else {
                break;
            }
            if (copied != src[pos + len]) {
                break;
            }
            len++;
        }
        if (len >= 3 && len > best.length) {
            best.length = len;
            best.disp = disp;
        }
    }
    return best;
}

static size_t Lz10_Compress(const uint8_t *src, size_t src_len, uint8_t **out)
{
    LzWriter w = { 0 };
    uint8_t *produced = NULL;
    int produced_len = 0;
    int produced_cap = 0;

    LzW_WriteByte(&w, 0x10);
    LzW_WriteU24LE(&w, (uint32_t)src_len);

    int pos = 0;
    while (pos < (int)src_len) {
        uint8_t flags = 0;
        LzWriter group = { 0 };

        for (int bit = 7; bit >= 0 && pos < (int)src_len; bit--) {
            LzMatch m = Lz_FindBestMatch(src, (int)src_len, pos,
                                         produced, produced_len);
            if (m.length >= 3) {
                flags |= (uint8_t)(1 << bit);
                uint8_t b1 = (uint8_t)(((m.length - 3) & 0x0F) << 4
                              | ((m.disp >> 8) & 0x0F));
                uint8_t b2 = (uint8_t)(m.disp & 0xFF);
                LzW_WriteByte(&group, b1);
                LzW_WriteByte(&group, b2);
                for (int i = 0; i < m.length; i++) {
                    if (produced_len + 1 > produced_cap) {
                        int new_cap = produced_cap == 0 ? 64 : produced_cap * 2;
                        uint8_t *np = realloc(produced, (size_t)new_cap);
                        if (np == NULL) {
                            free(produced);
                            free(group.buf);
                            free(w.buf);
                            *out = NULL;
                            return 0;
                        }
                        produced = np;
                        produced_cap = new_cap;
                    }
                    produced[produced_len++] = src[pos + i];
                }
                pos += m.length;
            } else {
                LzW_WriteByte(&group, src[pos]);
                if (produced_len + 1 > produced_cap) {
                    int new_cap = produced_cap == 0 ? 64 : produced_cap * 2;
                    uint8_t *np = realloc(produced, (size_t)new_cap);
                    if (np == NULL) {
                        free(produced);
                        free(group.buf);
                        free(w.buf);
                        *out = NULL;
                        return 0;
                    }
                    produced = np;
                    produced_cap = new_cap;
                }
                produced[produced_len++] = src[pos++];
            }
        }
        LzW_WriteByte(&w, flags);
        for (size_t i = 0; i < group.len; i++) {
            LzW_WriteByte(&w, group.buf[i]);
        }
        free(group.buf);
    }

    free(produced);

    if (w.error) {
        free(w.buf);
        *out = NULL;
        return 0;
    }
    *out = w.buf;
    return w.len;
}

static void test_detect_lz10_stream(void)
{
    const uint8_t stream[] = { 0x10, 0x04, 0x00, 0x00, 0x00 };
    ANIMA_ASSERT_EQ_U(Lz_Detect(stream, sizeof(stream)), COMPRESSION_LZ10);
}

static void test_detect_lz11_stream(void)
{
    const uint8_t stream[] = { 0x11, 0x04, 0x00, 0x00, 0x00 };
    ANIMA_ASSERT_EQ_U(Lz_Detect(stream, sizeof(stream)), COMPRESSION_LZ11);
}

static void test_detect_uncompressed_short_buffer(void)
{
    const uint8_t stream[] = { 0x10, 0x00 };
    ANIMA_ASSERT_EQ_U(Lz_Detect(stream, sizeof(stream)), COMPRESSION_NONE);
}

static void test_detect_unknown_first_byte(void)
{
    const uint8_t stream[] = { 0x99, 0x00, 0x00, 0x00 };
    ANIMA_ASSERT_EQ_U(Lz_Detect(stream, sizeof(stream)), COMPRESSION_NONE);
}

static void test_decompress_passthrough_uncompressed(void)
{
    const uint8_t input[] = { 'h', 'e', 'l', 'l', 'o' };
    uint8_t *out = NULL;
    size_t out_size = 0;
    CompressionType type = COMPRESSION_UNKNOWN;

    int rc = Lz_Decompress(input, sizeof(input), &out, &out_size, &type);
    ANIMA_ASSERT_EQ_U(rc, 0);
    ANIMA_ASSERT_EQ_U(type, COMPRESSION_NONE);
    ANIMA_ASSERT_EQ_U(out_size, sizeof(input));
    ANIMA_ASSERT_MEM_EQ(out, input, sizeof(input));
    free(out);
}

static void test_decompress_null_inputs_rejected(void)
{
    uint8_t *out = NULL;
    size_t out_size = 0;
    CompressionType type = COMPRESSION_UNKNOWN;
    int rc = Lz_Decompress(NULL, 0, &out, &out_size, &type);
    ANIMA_ASSERT(rc != 0);
}

/*
 * Manually construct a small LZ10 stream with one back-reference and
 * verify the decompressor produces the expected output. This exercises
 * the back-reference code path independently of the compressor above.
 */
static void test_decompress_backref_simple(void)
{
    /*
     * Bytes: A A B B C C C C C C C C
     *         ^- pos 2 emits 8-byte back-ref (length=8, disp=1) starting
     *         from the C position. Source = out[2-1-1..2-1-1+7] = out[0..7]
     *         which after the header and initial literals is whatever was
     *         in the buffer. Easier to encode something self-consistent:
     *         use 0x10 0x04 0x00 0x00 0x00 0x41 0x42, then a back-ref
     *         that copies A and B (out[0]=A, out[1]=B) twice.
     *
     * Layout: size=4, "ABAB"
     *   Header: 0x10, 0x04, 0x00, 0x00
     *   Group flags: bit 7=0 literal A, bit 6=0 literal B, bit 5=1 back-ref
     *                (len=3, disp=1: b1=(3-3)<<4|0=0x00, b2=0x01),
     *                bit 4..0 unused
     *   Tokens: A B 0x00 0x01
     *
     * With size=4: emit A, B, then a 3-byte back-ref (len=3, disp=1) which
     * copies from out[1-1-1+0..+2] = out[-1..1]. That's invalid (out_pos=3,
     * disp+1=2 <= 3, OK; but src_start=-1, invalid). So use a larger disp.
     *
     * Simpler: use size=5, "ABCAB". Then at pos=3 (after literal A, B, C),
     * a back-ref (len=2... no, min len=3) -- doesn't fit. Use a stream
     * where the back-ref has a non-overlapping source.
     */
    /*
     * size = 6, "AABBAB"
     *   pos 0: literal A
     *   pos 1: literal A
     *   pos 2: literal B
     *   pos 3: literal B
     *   pos 4: back-ref (len=2, disp=1) -- no, min len=3
     *   pos 4: literal A
     *   pos 5: literal B
     *   That's all literals. Let me try with a 6-byte output that includes
     *   one back-ref.
     *
     * size = 9, "AABBCCAAB"
     *   pos 0: A
     *   pos 1: A
     *   pos 2: B
     *   pos 3: B
     *   pos 4: C
     *   pos 5: C
     *   pos 6: back-ref (len=2, disp=5) -- no, min len=3
     *
     * Let me just make a longer stream.
     * size = 10, "AABBCCAABB"
     *   pos 6: A
     *   pos 7: A
     *   pos 8: B
     *   pos 9: B
     *   For pos=6: window has out[0..5] = A,A,B,B,C,C. src[6]=A, found
     *   at out[0], out[1]. Best match: disp=6, len=2 (A,A). Not enough.
     *   So literal A.
     *
     * For pos=7: window has out[0..6] = A,A,B,B,C,C,A. src[7]=A, found at
     *   out[0,1,6]. disp=6 len=1, disp=1 len=1. Not enough.
     *
     * Hmm. Just use a run of same character.
     *
     * size = 6, "AAAAAA"
     *   pos 0: literal A
     *   pos 1: literal A
     *   pos 2: back-ref (len=4, disp=1). copy_pos = 2-1-1 = 0. Copy
     *     out[0]=A, out[1]=A, out[2]=A (just set), out[3]=A (just set).
     *     out[2..5] = A,A,A,A. ✓
     *
     * Header: 0x10, 0x06, 0x00, 0x00
     * Group 1:
     *   bit 7: literal A
     *   bit 6: literal A
     *   bit 5: back-ref (len=4, disp=1): b1=(4-3)<<4|0=0x10, b2=0x01
     *   bit 4..0: unused (we've reached pos=6 = size)
     *   flags = 0b00100000 = 0x20
     *   tokens: A A 0x10 0x01
     * Total: 0x10 0x06 0x00 0x00 0x20 0x41 0x41 0x10 0x01
     */
    const uint8_t stream[] = {
        0x10, 0x06, 0x00, 0x00,
        0x20, 0x41, 0x41, 0x10, 0x01
    };
    uint8_t *out = NULL;
    size_t out_size = 0;
    CompressionType type = COMPRESSION_UNKNOWN;
    int rc = Lz_Decompress(stream, sizeof(stream), &out, &out_size, &type);
    ANIMA_ASSERT_EQ_U(rc, 0);
    ANIMA_ASSERT_EQ_U(type, COMPRESSION_LZ10);
    ANIMA_ASSERT_EQ_U(out_size, 6);
    if (rc == 0 && out != NULL) {
        const uint8_t expected[] = { 'A', 'A', 'A', 'A', 'A', 'A' };
        ANIMA_ASSERT_MEM_EQ(out, expected, sizeof(expected));
    }
    free(out);
}

static void roundtrip(const uint8_t *src, size_t src_len, const char *label)
{
    uint8_t *compressed = NULL;
    size_t compressed_len = Lz10_Compress(src, src_len, &compressed);
    if (compressed == NULL) {
        printf("    [%s] Lz10_Compress failed for %s\n",
               g_anima_test_state.current_name, label);
        g_anima_test_state.current_failed = 1;
        return;
    }

    uint8_t *out = NULL;
    size_t out_size = 0;
    CompressionType type = COMPRESSION_UNKNOWN;
    int rc = Lz_Decompress(compressed, compressed_len, &out, &out_size, &type);
    if (rc != 0) {
        printf("    [%s] Lz_Decompress failed for %s (rc=%d)\n",
               g_anima_test_state.current_name, label, rc);
        g_anima_test_state.current_failed = 1;
        free(compressed);
        return;
    }
    if (out_size != src_len || (src_len > 0 && memcmp(out, src, src_len) != 0)) {
        printf("    [%s] roundtrip mismatch for %s (in=%zu, out=%zu)\n",
               g_anima_test_state.current_name, label, src_len, out_size);
        g_anima_test_state.current_failed = 1;
    }
    free(compressed);
    free(out);
}

static void test_roundtrip_repeating_pattern(void)
{
    const uint8_t src[] = "AAAAAABBBBBBCCCCCCDDDDDDEEEEEEAAAAAABBBBBB";
    roundtrip(src, sizeof(src) - 1, "repeating_pattern");
}

static void test_roundtrip_pseudorandom(void)
{
    uint8_t src[256];
    uint32_t seed = 0x12345678u;
    for (size_t i = 0; i < sizeof(src); i++) {
        seed = seed * 1103515245u + 12345u;
        src[i] = (uint8_t)(seed >> 16);
    }
    roundtrip(src, sizeof(src), "pseudorandom_256");
}

static void test_roundtrip_long_repeating_run(void)
{
    uint8_t src[512];
    for (size_t i = 0; i < sizeof(src); i++) {
        src[i] = (uint8_t)('A' + (i % 26));
    }
    roundtrip(src, sizeof(src), "long_repeating_run");
}

static void test_roundtrip_all_zeros(void)
{
    uint8_t src[300];
    memset(src, 0, sizeof(src));
    roundtrip(src, sizeof(src), "all_zeros");
}

void anima_test_lz_register(void)
{
    ANIMA_RUN(test_detect_lz10_stream);
    ANIMA_RUN(test_detect_lz11_stream);
    ANIMA_RUN(test_detect_uncompressed_short_buffer);
    ANIMA_RUN(test_detect_unknown_first_byte);
    ANIMA_RUN(test_decompress_passthrough_uncompressed);
    ANIMA_RUN(test_decompress_backref_simple);
    ANIMA_RUN(test_roundtrip_repeating_pattern);
    ANIMA_RUN(test_roundtrip_pseudorandom);
    ANIMA_RUN(test_roundtrip_long_repeating_run);
    ANIMA_RUN(test_roundtrip_all_zeros);
    ANIMA_RUN(test_decompress_null_inputs_rejected);
}
