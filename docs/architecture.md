# 架构与数据说明

> Mewgenics 风格回合制战斗框架 —— 实现概览。设计规范见 mewgenics_inspired_turn_based_combat_design.md，
> wiki 调研见 docs/mewgenics_research.md，未定义行为的设计决策见 docs/design_decisions.md（DD-xxx）。

## 1. 目录结构

```
d:\mew-test\
├── build.bat                    # 一键构建（codegen → C++ → 测试 → GDExtension → godot/bin）
├── PLAN.md                      # 实施计划（每完成一点即时更新）
├── cpp/                         # C++ 战斗核心（引擎无关）
│   ├── CMakeLists.txt           # combat_core 静态库 + 测试 + gdext 子目录；PCH；configure 时跑 codegen
│   ├── tools/codegen/           # Python 反射生成器（schema.py 单一数据源 → src/generated/）
│   ├── src/
│   │   ├── pch.h                # 预编译头（编译时间控制）
│   │   ├── generated/           # 生成物：枚举声明 + to_string/parse/描述表（勿手改）
│   │   └── combat/
│   │       ├── core/            # BattleContext（权威状态+统一API）、基础类型
│   │       ├── domain/          # 定义（UnitDef/PassiveDef/AbilityDef/StatusDef）与运行时
│   │       ├── definitions/     # DefinitionCatalog（注册/查询/校验）
│   │       ├── scheduling/      # TurnScheduler（轮次/先攻/tie-break/额外回合）
│   │       ├── resolution/      # ActionResolver（移动/普攻/施法）、DamageResolver、DeathResolver
│   │       ├── effects/         # EventQueue、TriggerDispatcher、ConditionEvaluator、TargetSelector、EffectRegistry
│   │       ├── stats/           # StatCalculator（修正管线 + 一次性变换）
│   │       ├── statuses/        # StatusSystem（叠层/时长/tick/交互）
│   │       ├── map/             # GridMap（半步制 Dijkstra、Bresenham 视线）
│   │       ├── ai/              # EnemyAi（意图驱动）
│   │       ├── infrastructure/  # RngService、CombatLog
│   │       └── content/         # builtin_content：单位/状态/被动/技能/地形交互表
│   └── tests/                   # 80 个单元/集成测试（零依赖测试框架）
├── gdext/                       # GDExtension 包装（BattleController 门面）
└── godot/                       # Godot 4.6 Demo（GDScript 展示层）
    ├── mew_battle.gdextension   # DLL 加载清单
    ├── bin/mew_battle.dll       # 构建产物（build.bat 自动复制）
    └── scripts/                 # main / board / prep_ui / battle_ui / maps / test_*
```

## 2. 关键机制速览

| 机制 | 位置 | 说明 |
|---|---|---|
| 行动顺序 | TurnScheduler | 先攻=2×SPD；同值开局随机一次固化；额外回合紧接其后、每单位每轮 1 次（DD-003/004） |
| 行动经济 | UnitRuntime.action_points | 4 槽（移动/普攻/武器/饰品）；Refresh=0→1（DD-012） |
| 伤害管线 | DamageResolver | 减伤 → 背刺 → 暴击×2（仅物理、最后乘算）→ 护盾 → HP（DD-005/006） |
| 属性管线 | StatCalculator | base → 修正（4 阶段/7 叠加模式）→ 初始化一次性变换 → effective_base → 动态修正 → 派生（DD-018） |
| 事件反应 | EventQueue + TriggerDispatcher | 排序：phase→priority(小先)→所有者→获得序→效果序→instance_id；深度 16/链 64 上限（DD-009） |
| 状态 | StatusSystem | 6 叠层策略、施加回合不计时长（DD-008）、START/END/ROUND_END/IMMEDIATE tick |
| 死亡 | DeathResolver | 倒地(尸体韧性)→死亡两级；敌人全灭优先判胜；互杀=胜利+战后复活 |
| 随机 | RngService | splitmix64、调用审计、0/100% 不耗序列；演示层随机用 Godot RNG（DD-014） |

## 3. 如何新增内容（全部数据驱动，不改核心代码）

### 新增被动（被动 = 静态属性修正 + 触发效果）
1. `cpp/src/combat/content/builtin_content.h` 的 `passive_ids` 加 ID 常量。
2. `builtin_content.cpp` 的 register 段加 `PassiveDef`：
   - 属性修正：`stat_modifiers`（stat/operation/stacking_mode/phase/source_id）
   - 触发效果：`effects`（trigger 事件 / phase 窗口 / priority / condition 条件树 / target 选择器 / 原子效果参数 / limit）
3. 原子效果不足时：`schema.py` 加 EffectType → `effect_registry.cpp` 注册 handler → 更新 `docs/design_decisions.md`。
4. 条件不足时：`schema.py` 加 ConditionOp → `condition_evaluator.cpp` 实现。
5. 测试：`cpp/tests/` 加触发/边界用例；`PLAN.md` 记录。

### 新增状态
同上，改 `status_ids` + `StatusDef`（stack_policy/duration_unit/tick_timing/层数变化/blocks_turn/stat_modifiers/effects）。

### 新增技能
`ability_ids` + `AbilityDef`（成本/射程/视线/目标策略/payload 效果列表，定义序=结算序）；
单位通过 `UnitDef.ability_ids` 拥有。目标选择器 `CHOSEN_TARGET` 即施法目标。

### 新增敌人 / 装备
敌人：`unit_ids` + `make_def(...)`（七属性 + 移动 + 尸体韧性 + 能否暴击 + ability_ids）。
装备：以被动形式定义（EQUIPMENT_AND_PASSIVE 阶段修正），准备阶段 UI 挂到猫上。

### 新增地图（Godot 侧）
`godot/scripts/maps.gd`：字符画地形行（编码表见文件头注释）+ 猫出生格 + 敌人配置。
地形伤害/状态规则在 C++ `terrain_enter_damage`/`terrain_applies_wet`（`builtin_content.cpp`）。

## 4. 构建与运行

```
build.bat            # 或 build.bat debug（含 wiki 素材 → godot/assets 转换）
godot godot/project.godot   # 打开 Demo（godot/bin/mew_battle.dll 由 build.bat 部署）
godot --headless --path godot -s res://scripts/test_smoke.gd   # 无头冒烟
godot --headless --path godot -s res://scripts/test_ui_flow.gd  # UI 流程测试
AUTOSTART=1 godot godot/project.godot   # 跳过准备界面直接开战（调试用）
```

## 5. wiki 素材管线（wiki-asset/）

- `wiki-asset/wiki-pages/*.wiki`：参考页存档（Stats/Battle/Status_Effects/Tiles/敌人页等 39 页）。
- `wiki-asset/sprites/`：wiki 原始素材 9,099 个文件（379MB）：敌人动画 GIF（`ENEMY_<名>_{Idle,Attack,Death}.gif`，约 307 敌人存在全集）、猫职业动画（`CLASS_*`）、技能图标 1,316、状态图标 246、地形图标 32、物品/物件/地图图标等。`MANIFEST.md` + `sprite-manifest.csv` 为完整清单。
- `tools/convert_sprites.py`：GIF/PNG → 帧序列 PNG + manifest.json（Pillow）。
- `tools/prepare_godot_assets.py`：白名单转换（5 敌人×3 动画 + 13 职业×3 动画 + level-editor 兜底 + 5 技能/8 状态/14 地形图标），build.bat 第 5 步自动执行；godot/assets 为派生产物（gitignore）。
- Godot 集成：玩家猫 = 职业动画；敌人 = 三态动画（idle/attack/death 自动切换）；地形 = wiki 图标贴片（像素风 NEAREST 过滤）；状态徽章 = wiki 图标 billboard + 层数；技能按钮 = SVG 图标。
- demo 敌人 ↔ wiki 精灵映射：老鼠→Lil' Rat、蝙蝠→Bat、大块头→Lumpy、唤猫者→Cat Caller、玻璃喷吐者→Glass Spitter（`prepare_godot_assets.py` 的 ENEMY_ANIMS 表）。

## 6. 日志格式（验收要点）

## 6. 日志格式（验收要点）

每条日志：`[序号] 轮次 行动者 [类别] message_key 字段...`；内部 key 稳定（测试不依赖中文文案）；
含 TurnOrder（先攻排序）、StatsInitialized（属性快照）、TriggerMatched/ConditionPassed/Failed、
EffectExecuted、DamageCalculated/Applied、StatusApplied/Ticked/Expired、ChainLimitReached 等。
`CombatLog::to_text()` 全文导出；Godot UI 按类别着色显示。
