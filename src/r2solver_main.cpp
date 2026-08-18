/*
 * r2solver_main.cpp —— R2Solver 组装入口（C++17 版，对照 legacy/c/r2solver_main.c）
 */
#include "r2solver.hpp"
#include "r2log.hpp"
#include <cstdio>
#include <cstdlib>

int main(void) {
    setvbuf(stdout, nullptr, _IONBF, 0);   /* 禁用 stdout 缓冲，保证输出顺序与执行顺序一致 */

    /* ===== 挂载日志模块 ===== */
    init_json_log("C:/Users/21797/Desktop/Helloworld/R2Solver/data/r2_stats.json");
    /* 示例：同时挂 detailed 与 stats
     * init_detailed_log("C:/Users/21797/Desktop/Helloworld/r2.log");
     * log_register(&g_stats_sink);
     */

    /* ===== 注册剪枝 ===== */
    prune_register_builtin();

    /* ===== 搜索范围 ===== */
    ll max_a1 = 5;    /* 临时：小范围验证；改回 50 需谨慎（搜索空间指数爆炸） */

    for (ll A = 2; A <= max_a1; A++) {
        json_reset((int)A);
        printf("正在测试 a1 = %lld ...\n", A);

        State st;
        st.ensure(A);
        st.a[1] = A;

        ll a2_min = A + 2;
        ll a2_max = 2 * A - 2;
        if (a2_max > MAX_VAL) a2_max = MAX_VAL;

        bool found = false;
        for (ll a2 = a2_min; a2 <= a2_max; a2++) {
            st.a[2] = a2;
            ll sum = A + a2;

            if (enumerate_initial(st, A, 3, A - 2, a2 + 2, sum)) {
                found = true;
                break;
            }
        }

        if (found) {
            printf("发现一个promising序列: a1=%lld\n", A);
            if (!promising_a.empty() && promising_m > 0) {
                printf("PROMISING: a_1..a_%lld =", promising_m);
                ll limit = promising_m < 200 ? promising_m : 200;
                for (ll i = 1; i <= limit; i++) {
                    printf(" %lld", promising_a[i]);
                }
                if (promising_m > 200) printf(" ... (共 %lld 项)", promising_m);
                printf("\n");
            }
        }

        json_finish_run();

        if (found) {
            break;
        }
    }

    log_flush_all();
    log_close_all();

    return 0;
}
