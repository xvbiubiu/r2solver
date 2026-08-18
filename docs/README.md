# R2Solver

**R2Solver**（原"序列搜索器 r=2 / solverer"）—— 等比部分和序列搜索器。

搜索严格递增正整数序列 \(a_1 < a_2 < a_3 < \cdots\)（相邻项差 ≥ 2），使部分和序列在"按序列值取前缀"的意义下成等比：\(T_{n+1} = 2\,T_n\)，其中
\[
T_n = \sum_{i=1}^{a_n} a_i .
\]
当序列能达到足够深的层数（默认 \(n > 20\)，或 \(T_n > 2^{60}\)，或末项 > 1 亿）时宣告为 **promising**（项目定义的"可无限延伸"启发式判据）。

**C++17 实现**（`src/`），零第三方依赖。模块化设计：**搜索核心 / 剪枝 / 日志 三块解耦，可自由挂载**。C 版实现已完成历史使命并删除（迁移记录见 `legacy/solverer_CHANGELOG.md`）。

---

## 1. 功能一览

| 功能 | 说明 |
|---|---|
| 深度优先回溯搜索 | `enumerate_initial`（初始段）→ `try_extend`（单步扩展）→ `fill_segment`（迭代回溯填充新增块，显式栈防栈溢出） |
| Promising 判定 | 三阈值：`n > 20` / `Tn > 2^60` / 末项 > 1 亿（`PROMISING_*` 宏可 `-D` 覆盖） |
| 剪枝系统（可挂载） | 内置：全局上下界、精细不等式（REFINED）、新增段最小和（MIN_SUM）、L 取上界额外限制（L_EXTRA）；填充内边界（倍增上界、剩余最小和） |
| 日志系统（可挂载） | LogSink 注册表，**多个 sink 可同时挂载**：空 / 详细文本文件 / 失败统计 / JSON 统计 |
| JSON 机器日志 | `r2_stats.json`：按层节点数、层×失败原因、最大深度、最深状态快照、耗时；每完成一个 a1 即写盘 |
| 进度输出 | 每 1 亿节点向 stderr 打印 `[进度] a1=? max_n=? 节点=? 耗时=?` |
| 配套工具 | `count_r2`（独立统计：第一层填充数、第二层存活率、前瞻剪枝模拟、随机深度探测）、`analyze_paths.ps1`（路径合法性验证） |

---

## 2. 目录结构

```
R2Solver/
├── src/           核心源码（r2solver.h / r2log.h / state.c / search.c / prune.c / log*.c / r2solver_main.c）
├── tools/         配套工具（count_r2.c / analyze_paths.ps1 / analyze_r2.py）
├── docs/          文档（MODULE_SPEC.md / README.md / REQUIREMENTS.md）
└── legacy/        历史文档（solverer_CHANGELOG.md / solverer_BUGREPORT.md）
```

---

## 3. 构建与运行

### 3.1 CMake（推荐，CLion 可直接打开）

```bash
cmake -B build -G Ninja
cmake --build build --target solverer
./build/solverer            # 或 CLion 里选 solverer 目标 ▶️
```

构建配置建议 `Release`（Debug 的 -O0 慢 5~10 倍）。

### 3.2 直接 g++

```bash
g++ -O2 -std=c++17 -o solverer R2Solver/src/r2solver_main.cpp R2Solver/src/state.cpp \
    R2Solver/src/prune.cpp R2Solver/src/search.cpp R2Solver/src/log.cpp R2Solver/src/log_null.cpp \
    R2Solver/src/log_detailed.cpp R2Solver/src/log_stats.cpp R2Solver/src/log_json.cpp -lm
```

### 3.3 运行参数

当前为**编译期配置**（无命令行参数）：
- 搜索范围：`r2solver_main.c` 中 `ll max_a1 = 5;`（临时值，改回 50 需谨慎——搜索空间指数膨胀）
- 阈值宏：`LIMIT / MAX_VAL / PROMISING_N_THRESHOLD / PROMISING_ITEM_THRESHOLD`（`-D` 覆盖）
- JSON 输出路径：`r2solver_main.cpp` 中 `init_json_log(...)` 实参（当前 `R2Solver/data/r2_stats.json`，与 `r2_paths.txt` 同目录）

---

## 4. 接口与调用方式

### 4.1 搜索核心（search.c）

```c
bool try_extend(State *s);               /* 单步扩展；命中 promising 返回 true 并保存快照 */
bool fill_segment(State *s, ll old_m, ll L, ll pos, ll need_sum, ll min_val);
bool enumerate_initial(State *s, ll A, ll cur_idx, ll need_terms, ll min_val, ll sum_so_far);
```

调用链：`enumerate_initial` →（初始段完成）`try_extend` →（剪枝通过）`fill_segment` →（每个完整填充）递归 `try_extend`。

### 4.2 挂载日志（r2solver_main.c 示例）

```c
init_json_log("C:/Users/21797/Desktop/Helloworld/r2_stats.json"); /* JSON */
init_detailed_log("r2.log");   /* 再加详细日志 */
log_register(&stats_sink);     /* 再加统计 —— 三者同时工作 */
```

### 4.3 挂载剪枝

```c
prune_register_builtin();                        /* 内置三剪枝（顺序=检查顺序） */
static bool prune_xxx(const State *s, ll L) { ... }
static const Prune P = { "XXX", FAIL_XXX, prune_xxx };
prune_register(&P);                              /* 新增剪枝，不改搜索核心 */
```

详见 `MODULE_SPEC.md`（模块规范：日志 5 步、剪枝 3 步、统计口径、依赖图）。

### 4.4 关键数据结构

```c
typedef struct {
    ll *a;      /* 序列值，索引从 1 起 */
    ll m;       /* 已填充最大索引（= a_n） */
    ll n;       /* 当前步数（已满足 T_n 的个数） */
    ll Tn;      /* 当前和 T_n */
    size_t cap;
} State;
```

---

## 5. 输出与统计口径

`r2_stats.json` 关键字段：

| 字段 | 含义 |
|---|---|
| `nodes_per_level` | 63 元素数组，**索引 i（0 起）对应 n=i+1 层**；`[i]` = 第 n 层访问的节点数 |
| `fail_by_level` | 63 × 16 数组：各层 × 各失败原因 计数 |
| `fail_total_by_reason` | 失败原因总计数 |
| `max_n` | 达到的最大层 |
| `deepest_state` | 首次达到 max_n 时的状态（m / Tn / 前 40 项前缀） |
| `found_promising` | 是否找到 promising |

> 注意：`nodes_per_level[max_n-1] > 0` 且 `nodes_per_level[max_n] == 0` 表示第 max_n 层全部剪枝失败——**这是正常的**，不是统计矛盾。

---

## 6. 更新内容（v1.2 → v1.3）

### 2026-08-17 v1.3
1. **【重大】fill_segment 漏解修复**：原实现每块只尝试字典序最小的填充，深层失败即返回，跳过其余合法填充 → 漏解。修复后失败时继续枚举。详见 `solverer_BUGREPORT.md`。
2. **模块化重构**：单文件 → `R2Solver/src/` 多模块（搜索/剪枝/日志解耦，可自由挂载）。
3. **正式命名 R2Solver**。
4. **JSON 统计日志**（`log_json.c`）+ 每 1 亿节点进度输出 + 每 a1 写盘。
5. **剪枝注册表**：新增剪枝 = 写函数 + `prune_register`，不改搜索核心。

### 2026-08-15 v1.2（交接基线）
- 迭代回溯版 fill_segment（显式栈）；日志三模式（null/detailed/stats）；promising 快照。
- **含漏解 bug（后修复）**。

---

## 7. 配套工具

| 工具 | 文件 | 用途 |
|---|---|---|
| count_r2 | `R2Solver/tools/count_r2.c` | 独立统计：初始段数、第一层填充数、第二层存活率、k 层前瞻剪枝模拟、随机深度探测（路径写入 `r2_paths.txt`） |
| analyze_paths | `analyze_paths.ps1` | 对 `r2_paths.txt` 逐层验证 solverer 全部约束，判定路径真假 |
| 分析脚本 | `analyze_r2.py` | 汇总 `r2_stats.json`，自动判定 a1=8 状态 |

---

## 8. 已知问题与限制

- **搜索空间超指数膨胀**：修复漏解后搜索枚举全部合法填充，a1=5 已达 17 亿节点、a1=7 裸 DFS 不可行。大 a1 依赖更强剪枝（如多层前瞻，见 count_r2 模拟：5 层前瞻可剪约 78% 填充）。
- **promising 是启发式判据**：`n>20` 或 `Tn>2^60` 命中的序列，数学上是否可无限延展未经严格证明。
- 无命令行参数（编译期配置）；无检查点/断点续跑。

---

## 9. 相关文档

- `R2Solver/docs/MODULE_SPEC.md` —— 模块规范（接口契约、挂载规范、统计口径）
- `R2Solver/docs/REQUIREMENTS.md` —— 需求规格说明书（代码审查对照用）
- `R2Solver/legacy/solverer_CHANGELOG.md` —— 更新日志
- `R2Solver/legacy/solverer_BUGREPORT.md` —— fill_segment 漏解 bug 报告（含证据链与校验方法）

## 10. 迁移与回归验证（C → C++17）

**迁移方式**：`src/*.cpp` 由 C 版（已删除）逐模块翻译，语义不变；仅语言特性替换：
手写动态数组 → `std::vector`；宏 → `constexpr`；LogSink 函数指针结构 → 抽象基类；剪枝注册表 → `std::vector`。

**回归验证（保证"没有 bug"）**：
1. 编译通过（Release）。
2. **确定性对照**：C++ 版跑 `max_a1=4`，`r2_stats.json` 的 `nodes_per_level / fail_by_level / max_n / deepest_state` 必须与 C 版逐字节一致（基准：a1=4 = 15,12,21,197,6236,25168 节点，max_n=6，MIN_SUM 21658 / L_EXTRA 6296 / INITIAL_RANGE_EMPTY 4）。
3. a1=5 跑 1~2 分钟，进度输出的 `max_n` 与节点率应与 C 版一致（基准：max_n=6）。
4. 独立工具不受影响：`count_r2`（C，tools/）与 `analyze_paths.ps1` 继续用于交叉验证。

*R2Solver v1.4（C++17 迁移）· 2026-08-17*
