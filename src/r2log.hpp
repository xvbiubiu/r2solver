/*
 * r2log.hpp —— R2Solver 日志接口（C++17 版）
 *
 * 由 C 版 r2log.h 翻译：LogSink 函数指针结构 -> 抽象基类；
 * 注册表仍支持多个 sink 同时挂载（std::vector<LogSink*>）。
 */
#pragma once

#include "r2solver.hpp"
#include <vector>

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void write(FailureReason reason, const State& s, const char* where) = 0;
    virtual void flush() {}
    virtual void close() {}
};

/* ---------------- 注册表 ---------------- */
void log_register(LogSink* sink);
void log_unregister(LogSink* sink);
void log_clear(void);
void log_attempt(FailureReason reason, const State& s, const char* where);
void log_flush_all(void);
void log_close_all(void);

/* ---------------- 节点跟踪钩子 ---------------- */
using TrackNodeFn = void (*)(const State& s);
void log_set_track_node(TrackNodeFn fn);
void log_track_node(const State& s);

/* ---------------- 内置日志模块（各自独立 .cpp） ---------------- */
class NullLog final : public LogSink {
public:
    void write(FailureReason, const State&, const char*) override {}
};
extern NullLog g_null_sink;

class DetailedLog final : public LogSink {
public:
    void write(FailureReason reason, const State& s, const char* where) override;
    void flush() override;
    void close() override;
};
extern DetailedLog g_detailed_sink;
bool init_detailed_log(const char* path);

class StatsLog final : public LogSink {
public:
    void write(FailureReason reason, const State& s, const char* where) override;
    void flush() override {}
    void close() override;
};
extern StatsLog g_stats_sink;

class JsonLog final : public LogSink {
public:
    void write(FailureReason reason, const State& s, const char* where) override;
    void flush() override {}
    void close() override;
};
extern JsonLog g_json_sink;
bool init_json_log(const char* path);
void json_reset(int a1);
void json_finish_run(void);
