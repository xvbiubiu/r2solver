# Bug 报告：fill_segment 漏解（每块只尝试字典序最小的填充）

**报告日期**：2026-08-17
**文件**：`search.c::fill_segment`（R2Solver v1.2）
**严重程度**：高（搜索漏解，历史结论"a1=7 无解"存疑）
**状态**：已修复（v1.3），证据链可独立复核

---

## 1. 结论

v1.2 的 `fill_segment` 找到一个新增块的合法填充并递归 `try_extend` 后，**若深层失败直接返回 false，跳过该块剩余所有合法填充**。正确行为应为失败后继续枚举。

后果：搜索沿"每块字典序最小填充"单一路径推进，非最小填充及其全部深层子树从未被搜索 → **漏解**。"a1=2..7 无 promising"的历史结论基于此 bug，不可信。

---

## 2. 缺陷代码（v1.2）

```c
if (f->pos == L) {
    if (f->need_sum == 0) {
        /* ...保存/修改状态... */
        bool ok = try_extend(s);
        /* ...恢复状态... */
        free(stack);
        return ok;        /* <== 缺陷：ok==false 也直接返回，不枚举其他填充 */
    }
    ...
}
```

**修复后（v1.3）**：

```c
if (ok) { free(stack); return true; }
top--;        /* 弹掉末帧，继续枚举本块其他合法填充 */
continue;
```

---

## 3. 证据链（供独立复核）

1. **独立随机探测器**（`R2Solver/tools/count_r2.c::probe_random_depth`，不复用 solverer 填充代码，仅复刻剪枝条件）：a1=7 在 69 万次随机游走中**发现 4 条活到 n=22 的路径**（promising 门槛 n>20）；而 solverer v1.2 全搜索报告 max_n=16、无 promising。
2. **路径合法性验证**（`R2Solver/tools/analyze_paths.ps1`）：对 36 条长路径逐层独立验证 solverer 全部约束（初始段、L 一致性、REFINED、MIN_SUM、L_EXTRA、全局上下界、m/Tn 连续性、块和、块内差、块内倍增上界）——**36/36 合法，0 非法**。
3. **修复前后行为差异**：a1=5 节点数 1143（v1.2 单填充路径）→ 17 亿+（v1.3 全填充树）。

---

## 4. 校验方法（5 步）

1. 静态审查缺陷代码与修复后代码的控制流差异。
2. `gcc -O2 -std=gnu99 -o count_r2.exe R2Solver/tools/count_r2.c -lm && count_r2.exe 7` → 随机深度探测应出现最大深度 ≥ 20（n=23 路径写入 `r2_paths.txt`）。
3. `powershell -File R2Solver/tools/analyze_paths.ps1` → "合法路径 36 条，非法 0 条"。
4. 修复版 solverer 冒烟：`max_a1=4`（Release），秒级跑完且节点数远大于旧版 15。
5. 若确认：a1=7 存在活到 n=22 的合法路径，修复后有望找到 promising（n>20）序列。

---

## 5. 附带影响

- "a1=2..7 均无 promising"不能作为无解证明。
- 修复后搜索空间为全部填充的组合树（a1=5 ≈ 17 亿节点），大 a1 裸 DFS 不可行，需更强剪枝（如多层前瞻，count_r2 模拟 5 层前瞻可剪约 78% 填充）。

*报告：AI 编码代理，2026-08-17。所有结论基于可复现的独立验证，欢迎复核。*
