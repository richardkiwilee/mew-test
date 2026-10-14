# 设计决策记录

> 规则：wiki 未定义/存疑的行为，采用**最小、确定、可测试**的默认实现，并在此记录。
> 编号格式：DD-xxx。实现代码中引用编号便于追溯。

## DD-001 移动规则：四方向正交移动
wiki 只说移动距离用 Manhattan 形状、近战接触含斜角。本项目决定：**移动按上下左右四方向逐格进行**（BFS/Dijkstra 代价预算），近战攻击接触判定**含 8 邻域斜角**（与 wiki 的接触定义一致）。

## DD-002 移动预算单位：半步（half-step）
地形有 0.5 消耗（Road），战斗数值一律整数。决定：**移动预算 = 2 × 移动范围**（基础预算单位，代码称 `move_budget`），地形消耗：Road=1、普通/草丛/花=2、水域/荆棘=4、厚荆棘=6。范围公式 `max(1, 4 + trunc((Spd−5)/2))` 不变。

## DD-003 先攻：Initiative = 2×Spd（无敌人覆写）
wiki 中敌人可覆写 initiative 但叠加关系未说明。本项目统一：**Initiative = 2×Spd**；敌人想早动/晚动直接在内容数据里调 SPD 或加 IniUp/IniDown 状态（数据驱动，避免第二套规则）。
同先攻 tie-break：战斗开始时用 RngService 为每个单位生成一次稳定值，整场不变（与 wiki 一致）。

## DD-004 额外回合：一律"紧接当前回合之后"
wiki 对不同来源有"尽早/尽晚"与"紧接其后"两种说法且不完整。本项目统一：**GrantExtraTurn 立即插入当前单位回合之后**（插入队首），并声明：额外回合触发完整 TurnStarted/TurnEnded 流水线、刷新行动资源；每单位每轮最多 1 个额外回合（防无限，可配）。

## DD-005 暴击时机：暴击乘区在减伤之后、护盾之前
按 wiki："伤害减免在暴击计算之前，暴击加成在所有计算与减伤之后乘算"。管线：Base → 增伤 → 减伤(Brace/Petrify) → 背刺(+25% 物理) → 暴击(×2) → 护盾(Holy Shield 每实例挡 1 次 → Shield) → HP。

## DD-006 魔法暴击：不暴击（取 wiki 疑似 bug 的合理面）
wiki 自注"魔法暴击恒 2 倍可能是 bug"。本项目：**魔法伤害不能暴击**（只有物理可暴击/背刺），保持伤害类型语义清晰。

## DD-007 死亡模型：Downed（倒地）→ 尸体韧性 → 移除
- HP≤0 → `Downed`：清除绝大多数状态、不可行动、不再触发被动；可被复活（复活 HP = 战后回复值）。
- 倒地后再次受击按"尸体韧性"（玩家猫 3 层）计，归零 → `Dead`（本场不可复活）。
- 敌人无尸体韧性（倒地即 Dead）。胜利条件 = 全部必需敌人 Dead；失败 = 全部玩家猫 Downed/Dead。
- 本项目暂不实现战后伤势系统（Injuries），但 LifeState 已预留扩展。

## DD-008 状态持续时间与 tick
- 持续时间单位：默认 `ACTOR_TURNS`（持有者回合），另有 ROUNDS/PERMANENT/EVENT_COUNT。
- 施加当回合是否计数：`starts_counting = NEXT_TICK`（默认施加回合不计，下一规定时机起减）。
- tick 时机与 wiki 对齐：Burn（StartOfTurn：先结算伤害、再 −1 层）、Poison（StartOfTurn 只伤害）、Bleed（RoundEnd）、Stun/Blind/Charmed 等（EndOfTurn 减层）。
- 叠层默认 `STACK_AND_REFRESH`；Stun 类不可叠层用 `REPLACE_STRONGER`。

## DD-009 反应链防护
每个因果链（root_action）最多 64 个事件、最大深度 16、每效果实例每事件最多触发 1 次；
超限时输出 ERROR 级日志并终止该链（设计文档 §12.4）。

## DD-010 装备槽
Weapon / Head / Face / Neck / Trinket（含消耗品）。装备修正进入 `EQUIPMENT_AND_PASSIVE` 属性阶段；每回合 1 次使用动作（Weapon/Trinket 槽位）。demo 简化：不实现套装加成与诅咒。

## DD-011 回合结束结算顺序（设计文档 §6.5 落地）
1. TurnEnding 事件 → 2. END_OF_TURN_PRE_REGEN 状态 tick → 3. 回蓝 max(Int,0)+加成、回血 Health Regen → 4. END_OF_TURN_POST_REGEN 效果 → 5. 推进 EndOfTurn tick 的状态时长 → 6. 排空反应队列 → 7. TurnEnded。

## DD-012 回合开始结算顺序（设计文档 §6.4 落地）
1. TurnStarted 事件 → 2. 刷新 4 类动作资源（Move/BasicAttack/Weapon/Trinket，Refresh 语义：0→1）→ 3. START_OF_TURN 状态 tick（Burn 等）→ 4. TurnStarted 订阅被动 → 5. 排空反应队列 → 6. 确认仍存活/未被跳过 → 7. 移交玩家/AI。

## DD-013 近战攻击距离与接触
近战普攻范围 = 8 邻域（含斜角）；"接触"事件（Contact）在相邻时发布，供被动订阅。Reach 属性暂不实现。

## DD-014 随机性来源
战斗内一切随机走 RngService（seed 化）；展示层随机（UI）用 Godot 侧 RNG，不进 C++ 战斗层。
AI 决策随机也走战斗 RNG（保证重放一致）。

## DD-015 数值与取整
全整数（int32 存储 / int64 运算）；除法取整规则：**半程进位**（div_round_half_away，basic_types.h）；基点乘法 apply_bp。wiki 的 floor/trunc 公式按原样实现（floor((Spd−5)/2) 等）。

## DD-016 敌人生成与地图
战斗棋盘 10×10（demo 可配 12×12 变体）；玩家 4 只猫出生在预设 4 格随机分配；敌人按地图配置出生。
3 张 demo 地图的地形差异见 godot 内容配置（Phase 8）。

## DD-018 属性初始化烘焙标记
战斗开始时设置 passive/status 实例序号标记：标记之前的被动/状态修正烘焙进 `effective_base`（参与一次性变换），标记之后新施加的实例动态叠加（声明式、可重复求值）。初始化总是从 base 重算（幂等），绝不重复叠加修正；一次性变换绝不重跑。状态修正（ADD）按层数缩放；MUL/SET 类状态修正按"存在即生效一次"处理。

## DD-019 Stun 模型
wiki：Stun 立即跳过回合，除施加回合外每回合结束 −1 层。本项目采用 ACTOR_TURNS 时长模型：duration 1 = 跳过接下来 1 个己方回合（被跳过的回合末推进时长并过期）；施加当回合不计时（DD-008）。

## DD-017 demo 内容范围
- 猫被动池（随机 2 个/猫）：Lucky+21（装备阶段 LCK+21）、极值交换（BattleStarted 一次性）、Agile（+2 移动）、Brave（STR+2）、Scholar（回蓝+2）、Regen（回血+1）等 —— 全部数据驱动。
- 敌人：Rat（近战）、Bat（快、弱）、Thug（高血近战）、Mage Imp（远程火）、Spitter（远程毒）等。
- 装备：商店式列表（+属性/回蓝/移动等），配置于准备阶段。
