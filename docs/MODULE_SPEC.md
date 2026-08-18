# R2Solver —— 模块规范（MODULE_SPEC）

> 本文档定义 `R2Solver/src/` 下各模块的职责、依赖、接口契约与"自由挂载"规范。
> 目标：搜索核心与日志、剪枝解耦——**新增日志模块或剪枝时无需改动搜索核心**。

---

## 1. 目录与模块职责

```
R2Solver/
├── src/
│   ├── r2solver.h       核心公共头：State / FailureReason / 阈值宏 / 各模块接口声明
│   ├── state.c          状态内存管理与通用工具（promising 快照、失败原因名）
│   ├── search.c         搜索核心：try_extend / fill_segment / enumerate_initial
│   ├── prune.c          剪枝：全局上下界 + 可挂载剪枝注册表（内置 3 剪枝）
│   ├── r2log.h          日志接口：LogSink 契约 + 注册表 + 节点跟踪钩子
│   ├── log.c            日志注册表实现（广播 / 注册 / 卸载）
│   ├── log_null.c       空日志模块（无操作）
│   ├── log_detailed.c   详细文件日志模块（每失败点一行）
│   ├── log_stats.c      统计日志模块（失败原因聚合）
│   ├── log_json.c       JSON 统计日志模块（r2_stats.json + 进度打印）
│   └── r2solver_main.c  组装入口（选日志、注册剪枝、跑搜索）
├── tools/               count_r2.c / analyze_paths.ps1 / analyze_r2.py
├── docs/                MODULE_SPEC.md / README.md / REQUIREMENTS.md
└── legacy/              solverer_CHANGELOG.md / solverer_BUGREPORT.md（历史文档）
```

**依赖方向（只允许单向）**：

```
r2solver_main.c ──→ 所有模块
search.c ──→ r2solver.h, prune接口, r2log.h
prune.c ──→ r2solver.h
log_*.c ──→ r2log.h, r2solver.h
log.c ──→ r2log.h, r2solver.h
state.c ──→ r2solver.h
```

约束：**任何模块不得反向依赖**（如 log_*.c 不得 include search.c 的东西）。

---

## 2. 核心头契约

### 2.1 r2solver.h
- `State`：搜索状态（`a[]` 序列、`m` 索引、`n` 步数、`Tn` 和）。
- `FailureReason`：失败原因枚举。**新增剪枝失败原因时在此扩展**，并同步 `state.c::failure_reason_str()` 与各日志模块的统计字段。
- 阈值宏：`LIMIT / MAX_VAL / PROMISING_N_THRESHOLD / PROMISING_ITEM_THRESHOLD`（可用 `-D` 覆盖）。
- 接口：`ensure_capacity / save_promising_sequence / print_sequence_values`（state.c）、`global_lower_bound / global_upper_bound_original`（prune.c）、`try_extend / fill_segment / enumerate_initial`（search.c）、`Prune` 类型与 `prune_register* / prune_run_all`。

### 2.2 r2log.h
- `LogSink`：日志实现的最小契约（`write` / `flush` / `close`）。
- 注册表：`log_register / log_unregister / log_clear`，**支持任意多个 sink 同时挂载**。
- 广播：`log_attempt(reason, s, where)` 分发给所有已注册 sink。
- 节点跟踪：`log_set_track_node(fn)` / `log_track_node(s)`——搜索核心每访问一个节点调用一次（默认空），供 JSON 等模块计数/打进度。
- 生命周期：`log_flush_all / log_close_all` 在程序结束前调用。

---

## 3. 日志模块规范（如何新增/挂载）

### 3.1 挂载方式（在 solverer_main.c）

```c
init_json_log("C:/Users/21797/Desktop/Helloworld/r2_stats.json");  /* 挂载 JSON */
init_detailed_log("C:/Users/21797/Desktop/Helloworld/r2.log");     /* 再挂 detailed */
log_register(&stats_sink);                                        /* 再挂 stats */
```

多个模块可同时工作：`log_attempt` 广播给全部已注册 sink。

### 3.2 写一个新日志模块（5 步）

1. 新建 `log_xxx.c`，`#include "r2log.h"`。
2. 实现 3 个静态函数：
   ```c
   static void xxx_write(FailureReason r, const State *s, const char *where);
   static void xxx_flush(void);
   static void xxx_close(void);
   ```
3. 定义 sink 实例：`const LogSink xxx_sink = { .write=xxx_write, .flush=xxx_flush, .close=xxx_close };`
4. 提供 `init_xxx_log(...)`（可选）：做资源初始化 + `log_register(&xxx_sink)`（+ 需要时 `log_set_track_node(xxx_track)`）。
5. 把文件加入 CMake 的 `add_executable(solverer ...)` 列表，在 `solverer_main.c` 挂载。

**write 语义**：`reason` 为失败原因（`OK` 不调用）；`s` 为当前状态（可读 `a/n/m/Tn`，**禁止修改**）；`where` 为调用点描述串。**write 内部不得调用搜索/剪枝函数**（会破坏状态与性能）。

### 3.3 节点跟踪

需要"每访问一个节点"的回调（如 JSON 的计数/进度）时，在模块的 init 中：
```c
log_set_track_node(xxx_track_node);   /* void fn(const State*) */
```
搜索核心每进一次 `try_extend` 调一次 `log_track_node`。多模块都想要时，由 main 串接（例如在 main 里设置一个汇总回调）。

---

## 4. 剪枝模块规范（如何新增/挂载）

### 4.1 现有剪枝（prune.c 内置，`prune_register_builtin()` 一次挂载）

| 名称 | reason | 作用 |
|---|---|---|
| `REFINED_INEQ` | `FAIL_REFINED_INEQ` | 精细必要条件 |
| `L_EXTRA` | `FAIL_L_EXTRA` | L 取上界额外限制 |
| `MIN_SUM_EXCEED` | `FAIL_MIN_SUM_EXCEED` | 新增段最小和 ≤ Tn |

注册顺序 = 检查顺序（当前：L_extra → refined → min_sum，与单文件版基准一致，保证失败原因标签逐字节对齐）。

**try_extend gate 区内联剪枝（v1.5，用户推导，不走注册表）**：

| 名称 | 作用 | 位置 |
|---|---|---|
| `L_{k+1}<2L_k` | 相邻段平均值必须严格递增 ⟹ 块长严格小于 2 倍前一块；全量检查 k=1..m-2，提前发现未来层违反 | `search.cpp::try_extend`（`prune_run_all` 之后） |
| 相邻步长二次上界 | `L_{n+1}² + L_{n+1}(Tn/Ln+1) ≤ 2Tn`，正根上界（比 2L_n 更紧）；链式 3 层 | 同上 |

两处均为严格必要条件（`X ≥ Tn/Ln` 下界 + 平均递增长度），`bound_floor` 向下取整保守，**不漏解**。

### 4.2 写一个新剪枝（3 步）

1. 写检查函数（**必须是不漏解的必要条件**）：
   ```c
   static bool prune_xxx(const State *s, ll L) {
       /* 返回 true=通过(合法)；false=剪掉 */
   }
   ```
2. 组装并挂载（可在 prune.c 内置，或在 main 里注册自定义剪枝）：
   ```c
   static const Prune PRUNE_XXX = { "XXX", FAIL_XXX, prune_xxx };
   prune_register(&PRUNE_XXX);   /* 可在 main 中调用 */
   ```
3. 若引入新失败原因：扩展 `FailureReason` 枚举 + `failure_reason_str` + 各日志模块统计字段。

**注意**：
- `prune_run_all(s, L, &why)` 依注册顺序执行，首个失败即返回并写出 `why`。
- 剪枝必须是**严格必要条件**——若误剪掉真正解即引入漏解 bug（本项目已有前车之鉴，见 v1.3 修复）。
- 顺序敏感：把"开销小、剪枝强"的放前面可加速（当前 MIN_SUM 是主力，考虑前移需先验证语义不变）。

---

## 5. 搜索核心说明（search.c）

| 函数 | 职责 | 备注 |
|---|---|---|
| `enumerate_initial` | 递归枚举初始段 a_1..a_{a1}，完成即调 `try_extend` | 深度 ≤ a1 |
| `try_extend` | 单步扩展：promising 判定 → 全局上下界 → 剪枝（prune_run_all）→ fill_segment | 递归深度 = 步数 n |
| `fill_segment` | 迭代回溯填充新增块（显式栈，防栈溢出） | 每个完整填充后递归 try_extend |

**不要改动 search.c 的搜索语义**；需要新约束时走剪枝模块（第 4 节）。`log_attempt` / `log_track_node` 是 search.c 与日志的唯一耦合点。

---

## 6. 构建

```bash
# 根目录 CMakeLists 已含 solverer 目标（模块列表）
cmake -B build -G Ninja
cmake --build build --target solverer
```

**加减模块**：编辑根 CMakeLists 的 `add_executable(solverer ...)` 源文件列表。
- 不需要某个日志模块 → 从列表删除对应 .c（main 中相应挂载行也应删除/注释）。
- 加日志模块 → 见 3.2 第 5 步。

---

## 7. 数据流与统计口径

```
main
 ├─ json_reset(a1)                       （开始记录一个 a1）
 ├─ enumerate_initial → try_extend
 │    ├─ log_track_node(s)               （节点计数：每进 try_extend 一次）
 │    ├─ promising 判定 → save_promising_sequence
 │    ├─ prune_run_all → log_attempt(why)（失败原因）
 │    └─ fill_segment →（递归）try_extend
 ├─ json_finish_run()                    （结束 a1，写盘 r2_stats.json）
 └─ log_close_all()
```

**统计口径（r2_stats.json）**：
- `nodes_per_level`：63 元素数组，**索引 i（0 起）对应 n=i+1 层**。`nodes_per_level[5]` = n=6 层节点数。
- `max_n` = 达到的最大 n；若 `nodes_per_level[max_n-1] > 0` 且 `nodes_per_level[max_n] == 0`，表示第 max_n 层全部剪枝失败，无第 max_n+1 层——**这是正常的，不是矛盾**。
- `deepest_state` = 首次达到 max_n 时的状态快照（m/Tn/前 40 项前缀）。

---

## 8. 已知约定与注意事项

- **promising 语义**：`n > PROMISING_N_THRESHOLD`（默认 20）或 `Tn > LIMIT` 或末项 > 阈值即宣告 promising 并停止搜索（启发式判据，非数学证明）。
- **搜索空间**：修复 fill_segment 漏解后，搜索枚举**全部合法填充**，树随 a1 超指数膨胀（a1=5 已 17 亿节点）。大 a1 必须依赖更强剪枝（如多层前瞻）。
- **历史**：v1.2 单文件 C 版（含漏解 bug）与 C 版源码已删除，迁移记录见 `legacy/solverer_CHANGELOG.md`。

*规范版本：1.0（2026-08-17）*
