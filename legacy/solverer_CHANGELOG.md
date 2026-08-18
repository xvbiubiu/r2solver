# R2Solver 更新日志（CHANGELOG）

**项目**：R2Solver（原"序列搜索器 r=2 / solverer"）
**当前版本**：v1.3（2026-08-17）
**历史版本**：v1.2（2026-08-15，交接基线，含漏解 bug）

---

## v1.2 → v1.3（2026-08-17）

### 1.【核心修复】fill_segment 漏解修复
- **位置**：`fill_segment`，`f->pos == L && f->need_sum == 0` 分支
- **旧行为**：找到一个合法填充并递归 `try_extend` 后，无论成败都返回——失败时跳过本块其余所有合法填充（每块只试字典序最小填充，漏解）
- **新行为**：`try_extend` 失败时 `top--; continue;` 继续枚举；仅某填充深层返回 true 才返回 true；全部枚举完才返回 false
- **证据链与校验方法**：见 `solverer_BUGREPORT.md`

### 2. 模块化重构
- 单文件 `solverer.c` → `R2Solver/src/` 多模块（搜索 / 剪枝 / 日志解耦）
- 日志升级为**注册表**（LogSink 多挂载），剪枝升级为**可注册列表**
- 见 `R2Solver/docs/MODULE_SPEC.md`

### 3. 正式命名 R2Solver
- 目录 `R2Solver/`（src / tools / docs / legacy 四子目录）

### 4. 新增 JSON 统计日志
- `log_json.c`：按层节点数、层×失败原因、max_n、最深状态快照、耗时，输出 `r2_stats.json`
- 每完成一个 a1 写盘；每 1 亿节点打印进度

### 5. 配套工具归拢
- `R2Solver/tools/`：count_r2.c（独立统计/随机深度探测）、analyze_paths.ps1（路径验证）、analyze_r2.py（JSON 汇总）

---

## v1.1 → v1.2（2026-08-15，交接基线）
- 迭代回溯版 fill_segment（显式栈，防栈溢出）
- 日志三模式（null / detailed / stats）
- promising 快照保存
- **含漏解 bug（v1.3 修复）**

（v1.1 及更早无记录）
