/*
 * r2solver.hpp —— R2Solver 核心公共接口（C++17 版）
 *
 * 由 C 版 r2solver.h 逐模块翻译而来（R2Solver/legacy/c/r2solver.h）。
 * 逻辑语义与 C 版完全一致；差异仅为语言特性：
 *   - 手写动态数组 -> std::vector（索引 1 起，a[0] 保留不用）
 *   - 宏阈值 -> constexpr
 *   - 失败原因枚举 -> enum class
 * 搜索核心（search.cpp）逐行对照 C 版，含 v1.3 漏解修复。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

using ll = long long;

/* ---------------- 可配置阈值 ---------------- */
constexpr ll LIMIT                  = 1LL << 60;                    /* T_n 超过此值视为严格 promising */
constexpr ll MAX_VAL                = 1000000000LL;                 /* 单项值上限，防溢出 */
constexpr ll PROMISING_N_THRESHOLD  = 20;                           /* 扩展步数启发式阈值 */
constexpr ll PROMISING_ITEM_THRESHOLD = 100000000LL;                /* 末项启发式阈值 */

/* ---------------- 失败原因枚举 ---------------- */
enum class FailureReason : int {
    OK = 0,
    FAIL_STEP_BELOW_MIN,
    FAIL_STEP_ABOVE_MAX,
    FAIL_REFINED_INEQ,
    FAIL_MIN_SUM_EXCEED,
    FAIL_FILL_IMPOSSIBLE,
    FAIL_L_EXTRA,
    FAIL_NEED_SUM_NOT_ZERO,
    FAIL_INITIAL_RANGE_EMPTY,
    FAIL_LIMIT_REACHED
};

const char* failure_reason_str(FailureReason r);

/* ---------------- 搜索状态 ---------------- */
struct State {
    std::vector<ll> a;   /* 序列值，索引 1 起（a[0] 保留不用） */
    ll m = 0;            /* 已填充最大索引（= a_n） */
    ll n = 0;            /* 当前步数 */
    ll Tn = 0;           /* 当前和 T_n */
    ll N_threshold = 0;

    /* 保证 a 至少可索引 0..need（即 size >= need+1）；等价于 C 版 ensure_capacity */
    void ensure(ll need) {
        if ((ll)a.size() > need) return;
        a.resize(need + 1, 0);
    }
};

/* promising 快照（索引 1 起） */
extern std::vector<ll> promising_a;
extern ll promising_m;

/* ---------------- state.cpp ---------------- */
void print_sequence_values(const State& s, FILE* out, ll max_items);
void save_promising_sequence(const State& s);

/* ---------------- prune.cpp：剪枝（可挂载） ---------------- */
using PruneCheck = bool (*)(const State& s, ll L);
struct Prune {
    const char*   name;      /* 剪枝名 */
    FailureReason reason;    /* 失败原因 */
    PruneCheck    check;
};

void prune_register(const Prune& p);
void prune_register_builtin(void);
bool prune_run_all(const State& s, ll L, FailureReason& out_reason);

ll global_lower_bound(ll a1, ll n);
ll global_upper_bound_original(ll a1, ll Tn);

/* ---------------- search.cpp：搜索核心 ---------------- */
bool try_extend(State& s);
bool fill_segment(State& s, ll old_m, ll L, ll pos, ll need_sum, ll min_val, ll v_cap);
bool enumerate_initial(State& s, ll A, ll cur_idx, ll need_terms, ll min_val, ll sum_so_far);
