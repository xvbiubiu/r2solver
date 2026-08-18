/*
 * search.cpp —— R2Solver 搜索核心（C++17 版，对照 legacy/c/search.c）
 *
 * 逐行翻译 C 版，逻辑语义完全一致，包含 v1.3 fill_segment 漏解修复
 * （失败后 top-- 继续枚举本块其他填充）。
 * 仅变化：State 引用传递、std::vector 替代手写栈/数组。
 */
#include "r2solver.hpp"
#include "r2log.hpp"
#include <climits>
#include <cmath>
#include <vector>

/* ---------------- 尝试扩展一步 ---------------- */

/* k 层前瞻剪枝：计算"填充末项必须 ≤ V_max"的上界（MIN_SUM 必要条件）
 *   V(k) = (2^k·Tn - L_{n+k}(L_{n+k}-1) - 2·L_{n+k}·(D_k+1)) / L_{n+k}
 *   L_{n+k} = a[n+k+1]-a[n+k]，D_k = a[n+k]-a[n+1]
 * 若填充末项 v_L > V(k)，则第 n+k 层新增段最小和必超过 2^k·Tn（必死）。
 * 返回所有可用 k 的最小上界；无可用时返回 LLONG_MAX（不剪）。
 * 注意：该上界与当前块长 L 无关（仅与后续层块长有关）。
 */
static ll compute_vcap(const State& s) {
    ll best = LLONG_MAX;
    ll Tn2 = s.Tn;
    for (int k = 1; k <= 5; k++) {
        int idx = (int)s.n + k + 1;          /* 需要 a[idx] 与 a[idx-1] 已填 */
        if (idx > s.m) break;
        ll Lk = s.a[idx] - s.a[idx - 1];     /* 第 n+k 层块长 */
        if (Lk < 2) break;
        Tn2 *= 2;
        ll D = s.a[(int)s.n + k] - s.a[(int)s.n + 1];   /* 第 n+1..n+k-1 层块长之和 */
        ll num = Tn2 - Lk * (Lk - 1) - 2 * Lk * (D + 1);
        ll V = num / Lk;
        if (V < best) best = V;
    }
    return best;
}

bool try_extend(State& s) {
    log_track_node(s);   /* 节点跟踪钩子 */

    if (s.Tn > LIMIT ||
        s.n > PROMISING_N_THRESHOLD ||
        s.a[s.m] > PROMISING_ITEM_THRESHOLD) {
        log_attempt(FailureReason::FAIL_LIMIT_REACHED, s, "try_extend: 满足 promising 条件");
        save_promising_sequence(s);
        return true;
    }

    ll lb = global_lower_bound(s.a[1], s.n);
    ll ub_orig = global_upper_bound_original(s.a[1], s.Tn);
    if (s.m < lb || s.m > ub_orig) {
        log_attempt(FailureReason::FAIL_STEP_ABOVE_MAX, s, "try_extend: 全局上界/下界检查失败");
        return false;
    }

    ll old_m = s.m;
    ll old_n = s.n;
    ll old_Tn = s.Tn;
    ll L;

    if (old_n + 1 <= old_m) {
        L = s.a[old_n + 1] - s.a[old_n];

        if (L < 2) {
            log_attempt(FailureReason::FAIL_STEP_BELOW_MIN, s, "try_extend: 已知步长 < 2");
            return false;
        }
        if (L > old_m - old_n) {
            log_attempt(FailureReason::FAIL_STEP_ABOVE_MAX, s, "try_extend: 已知步长超过上界");
            return false;
        }

        FailureReason why = FailureReason::OK;
        if (!prune_run_all(s, L, why)) {
            log_attempt(why, s, "try_extend: 剪枝检查失败");
            return false;
        }

        /* ---- 相邻步长严格递增长度剪枝（v1.5，用户推导）----
         * 序列严格递增 ⟹ 相邻两段平均值严格递增：
         *   2T_k/L_{k+1} > T_k/L_k  ⟺  L_{k+1} < 2·L_k
         * 违反即矛盾（第二段不可能每项都大于第一段）。严格必要条件。
         * 检查所有已确定的层（k=1..m-2）：浅层违反在深层也必须成立，
         * 提前发现未来层的 L 违反（如 L_10=52 >= 2*L_9=22），在枚举前整段剪。 */
        for (ll k = 1; k + 2 <= s.m; k++) {
            ll Lk = s.a[k + 1] - s.a[k];
            if (Lk < 2) break;
            ll Lk1 = s.a[k + 2] - s.a[k + 1];
            if (Lk1 >= 2 * Lk) {
                log_attempt(FailureReason::FAIL_STEP_ABOVE_MAX, s, "try_extend: L_{k+1}<2L_k 剪枝");
                return false;
            }
        }

        /* ---- 相邻步长二次上界剪枝（v1.5，用户推导）----
         * 第 n+1 层块长 L_{n+1} 必须满足（X>=Tn/Ln 为当前段末项下界）：
         *   L_{n+1}^2 + L_{n+1}*(Tn/Ln + 1) <= 2*Tn
         * 正根: L_{n+1} <= (-(Tn/Ln+1) + sqrt((Tn/Ln+1)^2 + 8Tn))/2
         * 严格必要条件；O(1)（一次 sqrtl），L 过大的分支整段剪（不进 fill_segment）。
         * 链式：可继续检查 L_{n+2}（用 T_{n+1}=2Tn, L_{n+1}）。 */
        {
            ll Tn_tmp = s.Tn, Ln_tmp = L;
            for (int k = 1; k <= 3; k++) {
                if ((ll)s.n + k + 1 > s.m) break;           /* 需 a[n+k+1] 已填 */
                ll L_next = s.a[s.n + k + 1] - s.a[s.n + k];
                if (L_next < 2) break;
                long double ratio = (long double)Tn_tmp / (long double)Ln_tmp;
                long double disc = (ratio + 1.0L) * (ratio + 1.0L) + 8.0L * (long double)Tn_tmp;
                long double bound = (-(ratio + 1.0L) + sqrtl(disc)) / 2.0L;
                ll bound_floor = (ll)floorl(bound);
                if (L_next > bound_floor) {                  /* 向下取整，保守：少剪不误剪 */
                    log_attempt(FailureReason::FAIL_STEP_ABOVE_MAX, s, "try_extend: 相邻步长二次上界剪枝");
                    return false;
                }
                Tn_tmp *= 2;
                Ln_tmp = L_next;
            }
        }

        /* ---- k 层前瞻剪枝（v1.4）----
         * 整段剪（O(1)，minLast>v_cap 提前返回）+ 末项下界剪枝（fill_segment 内，
         * 每层 val 上限压缩到 v_cap-2*剩余项数，把大块枚举树从 10^13 压到 10^6）。 */
        ll v_cap = compute_vcap(s);
        if (v_cap != LLONG_MAX) {
            ll minLast = s.a[old_m] + 2 * L;      /* 任何合法填充的末项下界 */
            if (minLast > v_cap) {
                /* 整段剪：所有填充的末项都超过上界，后续层 MIN_SUM 必死 */
                log_attempt(FailureReason::FAIL_MIN_SUM_EXCEED, s, "try_extend: k层前瞻整段剪");
                return false;
            }
        }

        ll start_min = s.a[old_m] + 2;
        return fill_segment(s, old_m, L, 0, old_Tn, start_min, v_cap);

    } else {
        ll upper = old_m - old_n;
        ll v_cap = compute_vcap(s);   /* 与 L 无关，算一次 */
        for (L = 2; L <= upper; L++) {
            if (old_m + L > MAX_VAL) break;
            FailureReason why = FailureReason::OK;
            if (!prune_run_all(s, L, why)) {
                log_attempt(why, s, "try_extend: 枚举L剪枝失败");
                break;
            }
            if (v_cap != LLONG_MAX) {
                ll minLast = s.a[old_m] + 2 * L;
                if (minLast > v_cap) continue;    /* 该 L 整段剪 */
            }
            ll start_min = s.a[old_m] + 2;
            if (fill_segment(s, old_m, L, 0, old_Tn, start_min, v_cap)) {
                return true;
            }
        }
        log_attempt(FailureReason::FAIL_FILL_IMPOSSIBLE, s, "try_extend: 枚举L全部失败");
        return false;
    }
}

/* ---------------- 填充新增段（迭代回溯版） ---------------- */
struct FillFrame {
    ll pos;
    ll min_val;
    ll max_val;
    ll curr_val;
    ll need_sum;
    ll idx;
};

bool fill_segment(State& s, ll old_m, ll L, ll pos, ll need_sum, ll min_val, ll v_cap) {
    if (L == 0) return (need_sum == 0);

    std::vector<FillFrame> stack;
    stack.reserve(64);   /* 预分配，减少扩容（性能） */

    ll top = 0;
    stack.push_back({ pos, min_val, 0, 0, need_sum, old_m + pos + 1 });

    ll prev_idx = stack[top].idx - 1;
    ll prev_val = (pos == 0) ? s.a[old_m] : s.a[prev_idx];
    ll max_allowed = 2 * prev_val - prev_idx;
    if (max_allowed > MAX_VAL) max_allowed = MAX_VAL;

    ll rem = L - pos - 1;
    ll val_max_sum;
    if (rem == 0) {
        val_max_sum = need_sum;
    } else {
        ll tmp;
        if (__builtin_mul_overflow(rem, rem + 1, &tmp)) {
            return false;
        }
        if (tmp > need_sum) {
            return false;
        }
        val_max_sum = (need_sum - tmp) / (rem + 1);
    }
    stack[top].max_val = (val_max_sum < max_allowed) ? val_max_sum : max_allowed;
    if (pos == L - 1 && v_cap < stack[top].max_val) stack[top].max_val = v_cap;  /* 末项帧上限 */
    if (stack[top].max_val < min_val) {
        return false;
    }
    stack[top].curr_val = min_val - 1;

    long long iter = 0;   /* 死循环保护：单次 fill_segment 迭代超限即打印卡点并放弃 */
    while (top >= 0) {
        if (++iter > 1000000000LL) {
            fprintf(stderr, "[卡死保护] L=%lld m=%lld n=%lld v_cap=%lld top=%lld\n",
                    (long long)L, (long long)s.m, (long long)s.n, (long long)v_cap, (long long)top);
            for (ll t = 0; t <= top && t < 64; t++) {
                fprintf(stderr, "  帧[%lld]: pos=%lld min=%lld max=%lld curr=%lld need=%lld idx=%lld\n",
                        (long long)t, (long long)stack[t].pos, (long long)stack[t].min_val,
                        (long long)stack[t].max_val, (long long)stack[t].curr_val,
                        (long long)stack[t].need_sum, (long long)stack[t].idx);
            }
            fprintf(stderr, "  前缀: ");
            for (ll i = 1; i <= s.m && i <= 30; i++) fprintf(stderr, "%lld%s", (long long)s.a[i], i < s.m && i < 30 ? "," : "");
            fprintf(stderr, "\n");
            return false;
        }
        FillFrame& f = stack[top];

        /* push 辅助：写入 top+1 槽位（覆盖弹栈后的残留，与 C 版手写栈语义一致）。
         * 不能用 push_back 追加——弹栈后旧帧残留在中间槽位，追加会把新帧放到
         * 错误位置，导致读到残留帧（C++ 迁移引入的经典 bug，已修复）。 */
        auto push_new = [&](FillFrame fr) {
            if ((size_t)top + 1 >= stack.size()) stack.resize(stack.size() * 2 + 1);
            stack[top + 1] = fr;
            top++;
        };

        if (f.pos == L) {
            if (f.need_sum == 0) {
                ll old_m_saved = s.m;
                ll old_n_saved = s.n;
                ll old_Tn_saved = s.Tn;

                s.m = old_m + L;
                s.n = s.n + 1;
                s.Tn = old_Tn_saved + old_Tn_saved;

                bool ok = try_extend(s);

                s.m = old_m_saved;
                s.n = old_n_saved;
                s.Tn = old_Tn_saved;

                if (ok) {
                    return true;
                }
                /* 修复(2026-08-17): 深层失败时继续枚举本块的其他合法填充 */
                top--;
                continue;
            } else {
                top--;
                continue;
            }
        }

        f.curr_val++;
        if (f.curr_val > f.max_val) {
            top--;
            continue;
        }

        ll val = f.curr_val;
        s.ensure(f.idx);
        s.a[f.idx] = val;

        ll next_pos = f.pos + 1;
        ll next_need_sum = f.need_sum - val;
        ll next_min_val = val + 2;
        ll next_idx = f.idx + 1;

        /* 末项下界剪枝（v1.4 修复）：
         * 填第 next_pos 项（值 val）后，剩余 L-next_pos 项按最小递增（每项+2），
         * 末项 >= val + 2*(L-next_pos)。若该下界已超过 v_cap（末项必要上界），
         * 本分支所有填充的末项必超限（后续层 MIN_SUM 必死）→ 整个子树剪掉。
         * 否则 v_cap 只限制末帧，前几层组合会全部展开并失败在末项（枚举爆炸）。 */
        if (v_cap != LLONG_MAX && val + 2 * (L - next_pos) > v_cap) {
            continue;
        }

        if (next_pos == L) {
            push_new({ L, next_min_val, 0, 0, next_need_sum, next_idx });
        } else {
            prev_idx = next_idx - 1;
            prev_val = val;
            max_allowed = 2 * prev_val - prev_idx;
            if (max_allowed > MAX_VAL) max_allowed = MAX_VAL;

            ll next_rem = L - next_pos - 1;
            ll next_val_max_sum;
            if (next_rem == 0) {
                next_val_max_sum = next_need_sum;
            } else {
                ll tmp;
                if (__builtin_mul_overflow(next_rem, next_rem + 1, &tmp)) {
                    continue;
                }
                if (tmp > next_need_sum) {
                    continue;
                }
                next_val_max_sum = (next_need_sum - tmp) / (next_rem + 1);
            }
            ll next_max_allowed_final = (next_val_max_sum < max_allowed) ? next_val_max_sum : max_allowed;
            if (next_pos == L - 1 && v_cap < next_max_allowed_final) next_max_allowed_final = v_cap;  /* 末项帧上限 */
            if (next_max_allowed_final < next_min_val) {
                continue;
            }

            push_new({ next_pos, next_min_val, next_max_allowed_final, next_min_val - 1, next_need_sum, next_idx });
        }
    }

    return false;
}

/* ---------------- 枚举初始段 ---------------- */
bool enumerate_initial(State& s, ll A, ll cur_idx, ll need_terms, ll min_val, ll sum_so_far) {
    if (need_terms == 0) {
        s.m = A;
        s.n = 1;
        s.Tn = sum_so_far;

        long double a1_minus1_sq = (long double)(A - 1) * (A - 1);
        long double term = (a1_minus1_sq - 1.0L) * (a1_minus1_sq - 1.0L) /
                           (16.0L * (long double)sum_so_far);
        if (term <= 0.0L) {
            s.N_threshold = 1;
        } else {
            s.N_threshold = (ll)ceill(1.0L + log2l(term));
        }

        return try_extend(s);
    }

    ll prev = s.a[cur_idx - 1];
    ll prev_idx = cur_idx - 1;
    ll max_allowed = 2 * prev - prev_idx;
    if (max_allowed > MAX_VAL) max_allowed = MAX_VAL;
    if (max_allowed < min_val) {
        log_attempt(FailureReason::FAIL_INITIAL_RANGE_EMPTY, s, "enumerate_initial: 枚举范围为空");
        return false;
    }

    for (ll val = min_val; val <= max_allowed; val++) {
        s.ensure(cur_idx);
        s.a[cur_idx] = val;
        if (enumerate_initial(s, A, cur_idx + 1, need_terms - 1, val + 2, sum_so_far + val)) {
            return true;
        }
    }

    log_attempt(FailureReason::FAIL_INITIAL_RANGE_EMPTY, s, "enumerate_initial: 所有val失败");
    return false;
}
