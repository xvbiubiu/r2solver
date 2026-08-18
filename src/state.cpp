/*
 * state.cpp —— R2Solver 状态与通用工具（C++17 版，对照 legacy/c/state.c）
 */
#include "r2solver.hpp"
#include <cstdlib>
#include <cstring>

/* promising 快照 */
std::vector<ll> promising_a;
ll promising_m = 0;

const char* failure_reason_str(FailureReason r) {
    switch (r) {
        case FailureReason::OK: return "OK";
        case FailureReason::FAIL_STEP_BELOW_MIN: return "步长小于2";
        case FailureReason::FAIL_STEP_ABOVE_MAX: return "步长超过上界";
        case FailureReason::FAIL_REFINED_INEQ: return "精细不等式不成立";
        case FailureReason::FAIL_MIN_SUM_EXCEED: return "新增段最小和超过Tn";
        case FailureReason::FAIL_FILL_IMPOSSIBLE: return "填充范围为空";
        case FailureReason::FAIL_L_EXTRA: return "L取上界额外限制失败";
        case FailureReason::FAIL_NEED_SUM_NOT_ZERO: return "填充后need_sum非零";
        case FailureReason::FAIL_INITIAL_RANGE_EMPTY: return "初始段枚举范围为空";
        case FailureReason::FAIL_LIMIT_REACHED: return "达到LIMIT(promising)";
        default: return "未知";
    }
}

void print_sequence_values(const State& s, FILE* out, ll max_items) {
    ll limit = (s.m < max_items) ? s.m : max_items;
    for (ll i = 1; i <= limit; i++) {
        fprintf(out, "%lld", s.a[i]);
        if (i < limit) fprintf(out, " ");
    }
    if (s.m > max_items) fprintf(out, " ... (共 %lld 项)", s.m);
    fprintf(out, "\n");
}

void save_promising_sequence(const State& s) {
    promising_a.assign(s.a.begin(), s.a.begin() + s.m + 1);  /* 拷贝 a[0..m] */
    promising_m = s.m;
}
