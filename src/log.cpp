/*
 * log.cpp —— R2Solver 日志注册表实现（C++17 版，对照 legacy/c/log.c）
 */
#include "r2log.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>

static std::vector<LogSink*> g_sinks;
static TrackNodeFn g_track_node = nullptr;

void log_register(LogSink* sink) {
    if (!sink) return;
    if (std::find(g_sinks.begin(), g_sinks.end(), sink) == g_sinks.end())
        g_sinks.push_back(sink);
}

void log_unregister(LogSink* sink) {
    g_sinks.erase(std::remove(g_sinks.begin(), g_sinks.end(), sink), g_sinks.end());
}

void log_clear(void) {
    g_sinks.clear();
    g_track_node = nullptr;
}

void log_attempt(FailureReason reason, const State& s, const char* where) {
    for (LogSink* p : g_sinks)
        if (p) p->write(reason, s, where);
}

void log_flush_all(void) {
    for (LogSink* p : g_sinks)
        if (p) p->flush();
}

void log_close_all(void) {
    for (LogSink* p : g_sinks)
        if (p) p->close();
}

void log_set_track_node(TrackNodeFn fn) { g_track_node = fn; }

void log_track_node(const State& s) { if (g_track_node) g_track_node(s); }
