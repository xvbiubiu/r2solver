/*
 * prune.cpp —— R2Solver 剪枝模块（C++17 版，对照 legacy/c/prune.c）
 * 逻辑与 C 版逐条一致；仅函数签名改为 const State&。
 */
#include "r2solver.hpp"
#include <cmath>
#include <cstdio>
#include <vector>

/* ---------------- 注册表 ---------------- */
static std::vector<const Prune*> g_prunes;

void prune_register(const Prune& p) {
    for (const Prune* q : g_prunes) if (q == &p) return;   /* 防重复 */
    g_prunes.push_back(&p);
}

bool prune_run_all(const State& s, ll L, FailureReason& out_reason) {
    for (const Prune* p : g_prunes) {
        if (!p->check(s, L)) {
            out_reason = p->reason;
            return false;
        }
    }
    return true;
}

/* ---------------- 全局上下界（与 L 无关，search.cpp 直接调用） ---------------- */
ll global_lower_bound(ll a1, ll n) {
    return a1 + 2 * (n - 1);
}

ll global_upper_bound_original(ll a1, ll Tn) {
    long double a1_minus1 = (long double)(a1 - 1);
    long double disc = a1_minus1 * a1_minus1 + 4.0L * (long double)Tn;
    long double sqrt_disc = sqrtl(disc);
    long double result = (-a1_minus1 + sqrt_disc) / 2.0L;

    ll ub = (ll)floorl(result);
    while ((long double)(ub + 1) * (long double)(ub + 1) +
               a1_minus1 * (long double)(ub + 1) - (long double)Tn <= 0) {
        ub++;
    }
    while ((long double)ub * (long double)ub +
               a1_minus1 * (long double)ub - (long double)Tn > 0) {
        ub--;
    }
    return ub;
}

/* ---------------- 内置剪枝 ---------------- */

/* 精细必要条件 */
static bool prune_refined_ineq(const State& s, ll L) {
    ll A = s.m;
    ll n = s.n;
    ll B = s.a[A];
    ll a1 = s.a[1];

    long double left = (long double)a1 +
                       (long double)(n - 1) * (long double)(A - n + 2) +
                       (long double)(A - n) * (long double)(B - A + n + 1);
    long double right = (long double)L * ((long double)B + (long double)L + 1.0L);

    return left >= right;
}

/* L 取上界额外限制 */
static bool prune_l_extra(const State& s, ll L) {
    if (L != s.m - s.n) return true;
    long double diff = (long double)(s.m - s.n);
    long double threshold = 1.0L + sqrtl((long double)(1 + s.a[1]));
    return diff <= threshold;
}

/* 新增段最小和 */
static bool prune_min_sum(const State& s, ll L) {
    ll B = s.a[s.m];
    ll part1 = L * (B + 2);
    ll part2 = L * (L - 1);
    return (part1 + part2) <= s.Tn;
}

static const Prune PRUNE_REFINED = { "REFINED_INEQ", FailureReason::FAIL_REFINED_INEQ, prune_refined_ineq };
static const Prune PRUNE_L_EXTRA = { "L_EXTRA", FailureReason::FAIL_L_EXTRA, prune_l_extra };
static const Prune PRUNE_MIN_SUM = { "MIN_SUM_EXCEED", FailureReason::FAIL_MIN_SUM_EXCEED, prune_min_sum };

void prune_register_builtin(void) {
    /* 注册顺序 = 检查顺序。与单文件版 solverer.c 一致：L_EXTRA → REFINED → MIN_SUM，
     * 保证失败原因统计标签与基准逐字节一致。 */
    prune_register(PRUNE_L_EXTRA);
    prune_register(PRUNE_REFINED);
    prune_register(PRUNE_MIN_SUM);
}
