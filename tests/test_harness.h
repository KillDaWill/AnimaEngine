#ifndef ANIMA_TEST_HARNESS_H
#define ANIMA_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AnimaTestState {
    int total;
    int passed;
    int failed;
    int current_failed;
    const char *current_name;
    const char *file;
    int line;
} AnimaTestState;

extern AnimaTestState g_anima_test_state;

void AnimaTest_RunOne(const char *name, void (*fn)(void));

#define ANIMA_RUN(fn) AnimaTest_RunOne(#fn, fn)

#define ANIMA_ASSERT(cond)                                                   \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("    [%s] assertion failed: %s\n      at %s:%d\n",        \
                   g_anima_test_state.current_name,                          \
                   #cond, __FILE__, __LINE__);                               \
            g_anima_test_state.current_failed = 1;                           \
        }                                                                    \
    } while (0)

#define ANIMA_ASSERT_EQ_U(a, b)                                              \
    do {                                                                     \
        unsigned long long _a = (unsigned long long)(a);                     \
        unsigned long long _b = (unsigned long long)(b);                     \
        if (_a != _b) {                                                      \
            printf("    [%s] expected %llu, got %llu\n      at %s:%d\n",      \
                   g_anima_test_state.current_name, _b, _a,                  \
                   __FILE__, __LINE__);                                      \
            g_anima_test_state.current_failed = 1;                           \
        }                                                                    \
    } while (0)

#define ANIMA_ASSERT_STR_EQ(a, b)                                            \
    do {                                                                     \
        const char *_a = (a);                                                \
        const char *_b = (b);                                                \
        if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) {               \
            printf("    [%s] expected \"%s\", got \"%s\"\n      at %s:%d\n",  \
                   g_anima_test_state.current_name,                          \
                   _b ? _b : "(null)",                                       \
                   _a ? _a : "(null)",                                       \
                   __FILE__, __LINE__);                                      \
            g_anima_test_state.current_failed = 1;                           \
        }                                                                    \
    } while (0)

#define ANIMA_ASSERT_MEM_EQ(a, b, n)                                         \
    do {                                                                     \
        if (memcmp((a), (b), (n)) != 0) {                                    \
            printf("    [%s] memory mismatch over %zu bytes\n      at %s:%d\n", \
                   g_anima_test_state.current_name, (size_t)(n),             \
                   __FILE__, __LINE__);                                      \
            g_anima_test_state.current_failed = 1;                           \
        }                                                                    \
    } while (0)

#endif
