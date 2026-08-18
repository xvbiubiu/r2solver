/*
 * log_stats.cpp —— R2Solver 统计日志模块（C++17 版，对照 legacy/c/log_stats.c）
 */
#include "r2log.hpp"
#include <cstdio>

struct FailureStats {
    long long fail_step_below_min = 0;
    long long fail_step_above_max = 0;
    long long fail_refined_ineq = 0;
    long long fail_min_sum_exceed = 0;
    long long fail_fill_impossible = 0;
    long long fail_L_extra = 0;
    long long fail_need_sum_not_zero = 0;
    long long fail_initial_range_empty = 0;
    long long limit_reached = 0;
};

static FailureStats stats;

void StatsLog::write(FailureReason reason, const State&, const char*) {
    switch (reason) {
        case FailureReason::FAIL_STEP_BELOW_MIN: stats.fail_step_below_min++; break;
        case FailureReason::FAIL_STEP_ABOVE_MAX: stats.fail_step_above_max++; break;
        case FailureReason::FAIL_REFINED_INEQ: stats.fail_refined_ineq++; break;
        case FailureReason::FAIL_MIN_SUM_EXCEED: stats.fail_min_sum_exceed++; break;
        case FailureReason::FAIL_FILL_IMPOSSIBLE: stats.fail_fill_impossible++; break;
        case FailureReason::FAIL_L_EXTRA: stats.fail_L_extra++; break;
        case FailureReason::FAIL_NEED_SUM_NOT_ZERO: stats.fail_need_sum_not_zero++; break;
        case FailureReason::FAIL_INITIAL_RANGE_EMPTY: stats.fail_initial_range_empty++; break;
        case FailureReason::FAIL_LIMIT_REACHED: stats.limit_reached++; break;
        default: break;
    }
}

void StatsLog::close() {
    fprintf(stderr, "\n===== 失败统计 =====\n");
    fprintf(stderr, "步长小于2: %lld\n", stats.fail_step_below_min);
    fprintf(stderr, "步长超过上界: %lld\n", stats.fail_step_above_max);
    fprintf(stderr, "精细不等式不成立: %lld\n", stats.fail_refined_ineq);
    fprintf(stderr, "新增段最小和超过Tn: %lld\n", stats.fail_min_sum_exceed);
    fprintf(stderr, "填充范围为空: %lld\n", stats.fail_fill_impossible);
    fprintf(stderr, "L取上界额外限制失败: %lld\n", stats.fail_L_extra);
    fprintf(stderr, "填充后need_sum非零: %lld\n", stats.fail_need_sum_not_zero);
    fprintf(stderr, "初始段枚举范围为空: %lld\n", stats.fail_initial_range_empty);
    fprintf(stderr, "达到LIMIT(promising): %lld\n", stats.limit_reached);
}

StatsLog g_stats_sink;
