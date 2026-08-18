/*
 * log_detailed.cpp —— R2Solver 详细文件日志模块（C++17 版，对照 legacy/c/log_detailed.c）
 */
#include "r2log.hpp"
#include <cstdio>

static FILE* detailed_file = nullptr;

void DetailedLog::write(FailureReason reason, const State& s, const char* where) {
    if (!detailed_file) return;
    fprintf(detailed_file, "[%s] a1=%lld, n=%lld, m=%lld, Tn=%lld  位置: %s\n",
            failure_reason_str(reason), s.a[1], s.n, s.m, s.Tn, where);
    fprintf(detailed_file, "  前缀: ");
    print_sequence_values(s, detailed_file, 20);
}

void DetailedLog::flush() {
    if (detailed_file) fflush(detailed_file);
}

void DetailedLog::close() {
    if (detailed_file) { fclose(detailed_file); detailed_file = nullptr; }
}

DetailedLog g_detailed_sink;

bool init_detailed_log(const char* path) {
    detailed_file = fopen(path, "w");
    if (!detailed_file) return false;
    setvbuf(detailed_file, nullptr, _IOFBF, 4 << 20);
    log_register(&g_detailed_sink);
    return true;
}
