# Mewgenics 战斗框架 — 实施计划

> 状态图例：`[ ]` 未开始 · `[~]` 进行中 · `[x]` 已完成
> 规则：每完成一个小点立即更新本文件。遇到无法自行解决的问题写入 user.md 请求人工干预。

## 目标总览

1. **C++ 战斗核心**（VS2022 + CMake + build.bat + PCH + Python 反射生成）
   - 按 [mewgenics_inspired_turn_based_combat_design.md](mewgenics_inspired_turn_based_combat_design.md) 的架构实现
   - 完整结构化战斗结算日志（可追踪每一次触发/条件/结算）
2. **Godot demo**（位于本仓库 `godot/` 目录，用 D:/godot 的 Godot 4.6.2 + godot-cpp 调 C++ DLL）
   - 战斗准备阶段：随机 4 只猫（属性/被动随机）+ 玩家装备配置
   - 3 张战斗地图，各有不同地形交互要素
   - 数种敌人，完整战斗流程，结束后返回战斗准备阶段

## 环境清单（已核验）

- [x] VS2022 Professional @ `C:\Program Files\Microsoft Visual Studio\2022\Professional`
- [x] CMake 3.31.6（VS 自带）+ Ninja（VS 自带，`CommonExtensions\Microsoft\CMake`）
- [x] Python 3.14.4
- [x] Godot 4.6.2 @ `D:\godot`，含 `godot-cpp-10.0.0-rc1`（GDExtension 方式调用 C++）
- [x] 参考工程 `D:\pyFramework\3-dmap-2`（45° 等距 3D 战棋：高度图地形、BFS 移动、正交相机、Q/E 旋转）
- [x] 游戏本体 `D:\Steam\steamapps\common\Mewgenics`（Mewgenics.exe + resources.gpak，必要时逆向参考）

---

## Phase 0 — 调研与设计决策

- [x] P0.1 Mewgenics wiki 战斗规则调研（回合/速度、属性、状态、移动、装备）— 完成：7 主属性及公式、先攻/平手规则、地形消耗、状态时机、伤害管线、胜负条件均已核实
- [x] P0.2 调研笔记 `docs/mewgenics_research.md`（wiki 明示值 vs 存疑点已区分）
- [x] P0.3 设计决策 `docs/design_decisions.md`（DD-001~017：斜向移动、半步预算、先攻公式、额外回合、暴击时机、死亡模型、状态 tick、反应链防护、装备槽、回合结算顺序等）

## Phase 1 — C++ 项目骨架

- [x] P1.1 目录结构 + CMakeLists.txt（MSVC、PCH、/W4、静态库 combat_core + 测试 exe）
- [x] P1.2 build.bat（vswhere 定位 VS2022 自带 CMake/Ninja + vcvars64，一键配置+编译+测试）
- [x] P1.3 Python 反射生成器 `tools/codegen/`（schema.py 单一数据源 → 枚举声明/to_string/parse/描述表；内容不变不触碰输出；CMake configure 自动运行 + mew_codegen 目标手动重跑）
- [x] P1.4 编译通过验收（12 个单元测试全绿：RNG 确定性/审计、日志渲染、反射往返）

## Phase 2 — 最小战斗核（设计文档阶段 1）

- [x] P2.1 领域模型：UnitDef/UnitRuntime、BattleContext、LifeState、Team（entity_id 稳定不复用）
- [x] P2.2 GridMap：格子、地形、占用、可通行、半步制消耗（DD-002）、Dijkstra 可达/路径、8 邻域接触
- [x] P2.3 确定性 RNG 服务（splitmix64 + rng_call_id 审计；0/100% 概率不耗序列）
- [x] P2.4 结构化 CombatLog（seq/round/actor/event/message_key/payload/trace，to_text 渲染）
- [x] P2.5 TurnScheduler：轮次/回合队列、先攻=2×SPD、tie-break 开局一次固化（DD-003）、额外回合紧接其后+每轮上限（DD-004）、死亡跳过（TurnSkipped）
- [x] P2.6 移动（逐格事件）+ 基础攻击（Str−5 公式）+ HP + 胜负判定 + 尸体韧性/战后复活（DD-007）
- [x] P2.7 验收：固定 seed + 相同输入 → 日志完全一致（battle_determinism 测试）
- 附：wiki 属性公式全部落地并有单测（HP=CON×4、法力=CHA×3、移动/先攻/暴击/回蓝/普攻公式）

## Phase 3 — 事件与效果基础设施（设计文档阶段 2）

- [x] P3.1 CombatEvent / EventQueue（因果链 depth≤16、每链事件≤64、ChainLimitReached 诊断日志，DD-009）
- [x] P3.2 TriggerDispatcher（phase→priority(小先)→所有者→获得序→效果序→instance_id 稳定排序；匹配/条件/目标/限制全程可追踪日志）
- [x] P3.3 ConditionEvaluator（条件树 + 失败原因；RANDOM_CHANCE 消耗战斗 RNG）
- [x] P3.4 TargetSelector（Self/EventSource/EventTarget/All/Nearest/Random/LowestHp/HighestStat/Radius…稳定顺序）
- [x] P3.5 EffectRegistry + 原子效果（施加/移除状态、增/耗资源、治疗、伤害、额外回合、标签）
- [x] P3.6 验收：法力学者/猎人奖励纯数据触发（不改 ActionResolver）；PER_EVENT 计数按效果独立+窗口重置；无限连锁截断精确可复现（18 次消耗→ChainLimitReached）

## Phase 4 — 属性与状态流水线（设计文档阶段 3）

- [x] P4.1 StatCalculator：烘焙标记（DD-018）+ 分阶段修正 + 叠加模式全枚举 + Clamp + Derived；状态修正按层数缩放
- [x] P4.2 极值交换 transform（固定枚举序 tie-break、仅战斗初始化一次、重算绝不二次交换——测试验证 5 次重算稳定）
- [x] P4.3 StatusSystem：6 种叠层策略、duration 显式规则（施加回合不计，DD-008）、tick 时机（START/END/ROUND_END/IMMEDIATE）、倒地清除、Stun 阻断回合（DD-019）
- [x] P4.4 验收：Lucky+21 + 极值交换（设计文档示例数值精确复现）；Burn/Poison/Bleed 时机与衰减；无 off-by-one；先攻动态重排（未开始单位重排、已开始稳定）；回合开始 DoT 致死取消回合

## Phase 5 — 死亡与连锁反应（设计文档阶段 4）

- [x] P5.1 统一死亡处理（downed/corpse 韧性/DEAD 两级）+ 最后有效伤害来源归属（普攻/补刀/DoT 均验证）
- [x] P5.2 复活统一 API（保留实体 ID、本轮不行动下轮入队、UnitRevived）+ SPAWN_UNIT 效果（环形确定性搜位、SpawnFailed 稳定失败）
- [x] P5.3 反伤（事件标签机制：reflect 伤害不再触发反伤，杜绝无限弹射）、击杀奖励、链限制
- [x] P5.4 验收：反伤互杀（敌人全灭优先判胜 → 倒地猫战后复活，与 wiki 一致）、死亡召唤、无空格召唤失败，全部无递归/无崩溃
- 附：66 个测试全绿

## Phase 6 — 技能、AI 与内容数据（设计文档阶段 5-6）

- [x] P6.1 范围/视线（Bresenham LOS，WALL 遮挡）、地形触发（荆棘/水域/熔岩伤害、Wet 状态、花丛回复、火蒸发水交互）
- [x] P6.2 技能 payload 有序结算（AbilityDef 数据驱动：火球/治疗/电击/毒液/AoE 溅射）+ 敌人 AI（最近目标/施法/普攻/移动逼近，意图经 ActionResolver 验证）
- [x] P6.3 内容数据：8 被动 + 8 状态 + 5 技能 + 6 敌人（猫模板/鼠/蝠/暴徒/火妖/喷毒者）+ 地形交互表
- [x] P6.4 定义校验器扩展（单位引用的被动/技能、技能 payload 引用的状态/单位定义）
- [x] P6.5 战斗日志 to_text 全文导出（结构化字段 + 渲染文本，UI 接口在 Phase 7 C ABI 提供）
- 附：80 个测试全绿（含全 AI 战斗确定性）

## Phase 7 — DLL 导出（Godot 调用层）

- [x] P7.1 采用 GDExtension 作为 DLL 接口（godot-cpp-10.0.0-rc1 + 4.6.2 extension_api 导出）
- [x] P7.2 BattleController 门面类：建战/加猫加敌/开局/查询（单位/状态/日志/技能池/被动池）/行动意图（移动自动寻路/普攻/施法）/敌方回合
- [x] P7.3 build.bat 全流程（codegen → combat_core → 80 测试 → godot-cpp → mew_battle.dll → godot/bin/）

## Phase 8 — Godot 战斗 Demo（`godot/` 目录，参考 3-dmap-2 视觉方案）

- [x] P8.1 Godot 4.6 工程骨架（正交 45° 相机 Q/E 旋转、3D 棋盘、GDExtension 加载）
- [x] P8.2 战斗准备阶段：随机 4 只猫（属性 3-7/2 被动/2 技能随机）+ 5 件装备配置 + 重新随机
- [x] P8.3 3 张地图：草原（道路/花丛/荆棘）、河湾（水域湿润/苔沼/火蒸发水）、熔岩回廊（熔岩/高草/墙/荆棘）——地形规则在 C++ 核心
- [x] P8.4 敌人：鼠/蝠/暴徒/火妖/喷毒者，每图不同配置（近战/远程/AoE/DoT）
- [x] P8.5 战斗流程：点选移动/攻击/技能目标、HP 条、状态徽章、倒地表现、动画、彩色战斗日志面板（触发/伤害/治疗分色）
- [x] P8.6 胜负结算面板 → 返回战斗准备（循环）
- 附：headless 冒烟测试（GDExtension 全战斗）+ UI 流程测试（真实 main.gd 路径：5 轮胜利→结算→返回准备）均 PASS

## Phase 9 — 集成验收

- [x] P9.1 build.bat 一键全流程 ✓（本机实测：codegen → C++ → 80 测试 → GDExtension → godot/bin）
- [x] P9.2 80 个 C++ 单元/集成测试全绿；固定 seed 日志级重放一致性（多组确定性测试）
- [x] P9.3 文档：docs/architecture.md（架构与数据格式、如何新增被动/技能/状态/装备/敌人/地图）
- [x] P9.4 user.md：无阻塞问题（本机工具链齐备，未需人工干预）

---

## 关键设计约束（实现时必守）

1. 所有状态修改走统一 BattleContext / 系统 API；被动不做成独立大类（数据驱动）。
2. 事件/效果排序完全确定：phase → priority(小者先) → 单位顺序 → 被动获得顺序 → 效果定义序 → instance_id。
3. 随机一律走注入的 RngService；演示层随机（UI 抖动等）用独立 RNG。
4. 可无限连锁的技能必须有限制（每行动最大事件数/最大深度 + 诊断日志）。
5. 属性修正顺序：Base → 永久成长 → 装备与被动修正 → 参与变换的临时修正 → 有序变换 → Clamp → Derived。
6. 移动与地形：地图模块只发布 UnitEnteredTile，由地形规则订阅执行伤害/状态。
7. 日志 message_key + 结构化 payload 稳定；中文文案仅用于 UI 展示层。

## 更新记录

| 时间 | 完成项 | 备注 |
|---|---|---|
| 2026-10-14 | 环境核验 + 本计划初稿 | wiki 调研后台进行中 |
| 2026-10-14 | P0 调研完成；P1 骨架全绿 | 12 测试通过；属性表已对齐 Mewgenics 七属性 |
| 2026-10-14 | P2 最小战斗核完成 | 32 测试全绿（含固定 seed 日志级重放一致性） |
| 2026-10-14 | P3 事件效果基建 + P4 属性状态流水线 | 58 测试全绿（含极值交换精确数值、无限连锁截断） |
| 2026-10-14 | P5 死亡与连锁反应 | 66 测试全绿（反伤互杀、击杀归属、死亡召唤） |
| 2026-10-14 | P6 技能/AI/内容 | 80 测试全绿（施法校验/视线/AoE/地形/全 AI 确定性；修复 Bresenham 越界） |
| 2026-10-14 | P7+P8 GDExtension + Godot Demo | 冒烟 PASS（17 轮胜利/914 日志）；UI 流程 PASS（5 轮→结算→返回准备） |
| 2026-10-14 | P9 集成验收 | build.bat 全流程 + 80 C++ 测试 + 双冒烟全绿；文档齐备 |
