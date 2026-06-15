#include "test_harness.h"
#include "test_main.h"

AnimaTestState g_anima_test_state;

void AnimaTest_RunOne(const char *name, void (*fn)(void))
{
    g_anima_test_state.current_name = name;
    g_anima_test_state.current_failed = 0;
    g_anima_test_state.total++;
    fn();
    if (g_anima_test_state.current_failed == 0) {
        g_anima_test_state.passed++;
        printf("  [PASS] %s\n", name);
    } else {
        g_anima_test_state.failed++;
        printf("  [FAIL] %s\n", name);
    }
}

int main(void)
{
    memset(&g_anima_test_state, 0, sizeof(g_anima_test_state));

    printf("Running AnimaEngine unit tests...\n");

    anima_test_lz_register();
    anima_test_pokemon_catalog_register();

    printf("\nResults: %d passed, %d failed, %d total\n",
           g_anima_test_state.passed,
           g_anima_test_state.failed,
           g_anima_test_state.total);

    return g_anima_test_state.failed == 0 ? 0 : 1;
}
