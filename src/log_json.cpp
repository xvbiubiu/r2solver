/*
 * log_json.cpp —— R2Solver JSON 统计日志模块（C++17 版，对照 legacy/c/log_json.c）
 * 功能与输出格式与 C 版完全一致：r2_stats.json 字段、口径逐字节相同。
 */
#include "r2log.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

struct JsonRun {
    int a1 = 0;
    long long elapsed_ms = 0;
    long long start_ms = 0;
    long long nodes[64] = {0};
    long long fail[64][16] = {{0}};
    long long reason_total[16] = {0};
    long long max_n = 0;
    int found = 0;
    ll d_m = 0, d_Tn = 0;
    ll d_prefix[40] = {0};
    int d_len = 0;
};

static JsonRun json_runs[64];
static int json_run_count = 0;
static const char* JSON_PATH = "C:/Users/21797/Desktop/Helloworld/R2Solver/data/r2_stats.json";

static long long json_total_nodes = 0;
static long long json_last_report = 0;

static const char* reason_names[16] = {
    "OK", "STEP_BELOW_MIN", "STEP_ABOVE_MAX", "REFINED_INEQ", "MIN_SUM_EXCEED",
    "FILL_IMPOSSIBLE", "L_EXTRA", "NEED_SUM_NOT_ZERO", "INITIAL_RANGE_EMPTY", "LIMIT_REACHED",
    "R10", "R11", "R12", "R13", "R14", "R15"
};

static long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

static JsonRun* json_cur() { return &json_runs[json_run_count]; }
static void json_write_file();

void json_reset(int a1) {
    JsonRun* r = json_cur();
    memset(r, 0, sizeof(*r));
    r->a1 = a1;
    r->start_ms = now_ms();
}

void json_finish_run(void) {
    JsonRun* r = json_cur();
    r->elapsed_ms = now_ms() - r->start_ms;
    json_run_count++;
    json_write_file();   /* 每完成一个 a1 立即写盘 */
}

static void json_track_node(const State& s) {
    JsonRun* r = json_cur();
    int n = (int)s.n;
    if (n < 1 || n >= 64) return;
    r->nodes[n]++;
    json_total_nodes++;
    if (json_total_nodes - json_last_report >= 100000000LL) {   /* 每 1 亿节点报一次进度 */
        json_last_report = json_total_nodes;
        fprintf(stderr, "[进度] a1=%d max_n=%lld 节点=%lld 耗时=%.1fs\n",
                r->a1, r->max_n, json_total_nodes,
                (double)(now_ms() - r->start_ms) / 1000.0);
    }
    if (n > r->max_n) {
        r->max_n = n;
        r->d_m = s.m; r->d_Tn = s.Tn;
        r->d_len = (s.m < 40) ? (int)s.m : 40;
        for (int i = 1; i <= r->d_len; i++) r->d_prefix[i - 1] = s.a[i];
    }
}

void JsonLog::write(FailureReason reason, const State& s, const char*) {
    JsonRun* r = json_cur();
    if ((int)reason > 0 && (int)reason < 16) {
        int k = (int)reason;
        r->reason_total[k]++;
        int n = (int)s.n;
        if (n >= 1 && n < 64) r->fail[n][k]++;
        if (reason == FailureReason::FAIL_LIMIT_REACHED) r->found = 1;
    }
}

static void json_write_file(void) {
    FILE* f = fopen(JSON_PATH, "w");
    if (!f) { fprintf(stderr, "错误：无法写入 JSON 日志 %s\n", JSON_PATH); return; }
    fprintf(f, "{\n  \"runs\": [\n");
    for (int i = 0; i < json_run_count; i++) {
        JsonRun* r = &json_runs[i];
        fprintf(f, "    {\n      \"a1\": %d,\n", r->a1);
        fprintf(f, "      \"elapsed_ms\": %lld,\n", r->elapsed_ms);
        fprintf(f, "      \"found_promising\": %s,\n", r->found ? "true" : "false");
        fprintf(f, "      \"max_n\": %lld,\n", r->max_n);
        fprintf(f, "      \"nodes_per_level\": [");
        for (int n = 1; n < 64; n++) fprintf(f, "%lld%s", r->nodes[n], (n < 63) ? ", " : "");
        fprintf(f, "],\n      \"fail_total_by_reason\": {");
        int first = 1;
        for (int k = 1; k < 16; k++) {
            if (r->reason_total[k] > 0) {
                fprintf(f, "%s\"%s\": %lld", first ? "" : ", ", reason_names[k], r->reason_total[k]);
                first = 0;
            }
        }
        fprintf(f, "},\n      \"fail_by_level\": [\n");
        for (int n = 1; n < 64; n++) {
            fprintf(f, "        [%lld", r->fail[n][0]);
            for (int k = 1; k < 16; k++) fprintf(f, ", %lld", r->fail[n][k]);
            fprintf(f, "]%s\n", (n < 63) ? "," : "");
        }
        fprintf(f, "      ],\n      \"deepest_state\": {\"n\": %lld, \"m\": %lld, \"Tn\": %lld, \"prefix\": [",
                r->max_n, r->d_m, r->d_Tn);
        for (int k = 0; k < r->d_len; k++) fprintf(f, "%lld%s", r->d_prefix[k], (k < r->d_len - 1) ? ", " : "");
        fprintf(f, "]}\n    }%s\n", (i < json_run_count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    /* 写盘提示静默：避免与 stdout 的"正在测试"交错（两条流独立缓冲/合并显示时序不可控） */
}

void JsonLog::close() { json_write_file(); }

JsonLog g_json_sink;

bool init_json_log(const char* path) {
    if (path) JSON_PATH = path;
    json_total_nodes = 0;
    json_last_report = 0;
    log_register(&g_json_sink);
    log_set_track_node(json_track_node);
    return true;
}
