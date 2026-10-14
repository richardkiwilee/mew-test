# -*- coding: utf-8 -*-
"""战斗核心枚举定义的单一数据源（Single Source of Truth）。

C++ 代码只 include 生成产物 `src/generated/reflection_gen.h`，不在别处手写枚举。
新增枚举/枚举值：修改本文件 → 重新运行 generate.py（build.bat 在 CMake configure
阶段会自动执行）→ 全项目即可使用，并获得 to_string / parse_enum / 名称与描述表。

约定：
- 枚举值自动从 1 开始递增（新值追加在末尾，保证已有值稳定）；
- 每个值可带一句中文描述（供日志渲染与 Godot UI 展示）；
- 不要在这里放战斗数值（数值属于内容数据，不属代码枚举）。
"""

from __future__ import annotations


class EnumDef:
    """一个 C++ 枚举的定义。entries: list[(NAME, desc)]。"""

    def __init__(self, cpp_name: str, doc: str, entries: list):
        self.cpp_name = cpp_name
        self.doc = doc
        self.entries = entries  # list of (NAME, desc)
        # 校验：名称必须是合法 C++ 标识符
        for name, _ in entries:
            assert name.isidentifier() and not name[0].isdigit(), f"bad enum ident: {name}"
        seen = set()
        for name, _ in entries:
            assert name not in seen, f"duplicate enum value: {name}"
            seen.add(name)

    def value_of(self, index: int) -> int:
        return index + 1  # 从 1 开始，0 保留为 INVALID


# ======================================================================
# 属性（StatType）——Mewgenics 主属性（wiki: Stats 页）
# 出生基准 3~7，5 为中性；派生属性见 DERIVED_STAT_TYPE
# ======================================================================
STAT_TYPE = EnumDef("StatType", "主属性（Mewgenics 七属性）", [
    ("STR", "力量：近战普攻 = Str-5（最低1），近战技能 = (Str-5)/2"),
    ("DEX", "敏捷：远程攻击/技能 = (Dex-5)/2"),
    ("CON", "体质：最大生命 = CON×4；战后回复 = 5+(CON-5)/2"),
    ("INT", "智力：回合结束回蓝 = max(Int,0)"),
    ("CHA", "魅力：最大法力 = CHA×3；起始法力 = CHA"),
    ("SPD", "速度：移动范围 = max(1,4+trunc((Spd-5)/2))；先攻 = 2×Spd"),
    ("LCK", "幸运：暴击率 +2%/点；随机判定按 Luck 骰公式"),
])

DERIVED_STAT_TYPE = EnumDef("DerivedStatType", "派生属性（由主属性/修正计算）", [
    ("MAX_HP",        "最大生命 = CON×4"),
    ("MAX_MANA",      "最大法力 = CHA×3"),
    ("INITIATIVE",    "先攻 = 2×SPD（+修正）"),
    ("MOVE_RANGE",    "移动范围 = max(1,4+trunc((SPD-5)/2))"),
    ("CRIT_CHANCE",   "暴击率（基点）= max(0, base + 2×LCK)"),
    ("MANA_REGEN",    "回合结束回蓝 = max(INT,0)+加成"),
    ("HEALTH_REGEN",  "回合结束回血"),
    ("DODGE_CHANCE",  "闪避率（基点）"),
    ("BACKSTAB_MULT", "背刺倍率（默认 12500 = +25%）"),
])

RESOURCE_TYPE = EnumDef("ResourceType", "可增减/消耗的资源类型", [
    ("HP",    "生命"),
    ("MANA",  "法力"),
])

ACTION_SLOT = EnumDef("ActionSlot", "每回合动作槽（各持独立 action point，wiki: Keywords/Refresh）", [
    ("MOVE",         "移动"),
    ("BASIC_ATTACK", "基础攻击"),
    ("WEAPON",       "武器"),
    ("TRINKET",      "饰品/消耗品"),
])

DAMAGE_TYPE = EnumDef("DamageType", "伤害类型", [
    ("PHYSICAL", "物理伤害（受 DEF 减免）"),
    ("MAGICAL",  "魔法伤害（受 RES 减免）"),
    ("TRUE",     "真实伤害（不受减免）"),
])

TEAM = EnumDef("Team", "阵营", [
    ("PLAYER",  "玩家（猫）"),
    ("ENEMY",   "敌方"),
    ("NEUTRAL", "中立（地形/环境）"),
])

LIFE_STATE = EnumDef("LifeState", "单位生命状态", [
    ("ALIVE",   "存活"),
    ("DOWNED",  "倒地（可被复活，不再行动）"),
    ("DEAD",    "死亡"),
    ("REMOVED", "已移出战场"),
])

# ======================================================================
# 战斗事件（EventType）——已发生事实 / 即将发生的机会，语义见设计文档 §4.4
# ======================================================================
EVENT_TYPE = EnumDef("EventType", "战斗事件类型", [
    ("BattleStarted",      "战斗开始"),
    ("BattleEnded",        "战斗结束"),
    ("RoundStarted",       "轮次开始"),
    ("RoundEnded",         "轮次结束"),
    ("TurnStarted",        "单位回合开始（机会已发生）"),
    ("TurnEnding",         "单位回合即将结束"),
    ("TurnEnded",          "单位回合结束"),
    ("ExtraTurnQueued",    "额外回合入队"),
    ("ActionDeclared",     "行动宣告"),
    ("ActionRejected",     "行动被拒绝（校验失败）"),
    ("ActionStarted",      "行动开始执行"),
    ("ActionCompleted",    "行动完成"),
    ("ActionCancelled",    "行动被取消"),
    ("MovementStarted",    "移动开始"),
    ("UnitMoved",          "单位完成一步移动"),
    ("UnitEnteredTile",    "单位踏入格子（地形规则订阅此事件）"),
    ("UnitLeftTile",       "单位离开格子"),
    ("MovementCompleted",  "移动完成"),
    ("AttackDeclared",     "攻击宣告"),
    ("AttackHit",          "攻击命中（即使最终伤害为 0 也触发）"),
    ("AttackMissed",       "攻击未命中"),
    ("BeforeDamage",       "伤害提交前（可修改/取消候选伤害）"),
    ("DamageCalculated",   "伤害计算完成"),
    ("DamageApplied",      "伤害已提交（扣盾/扣血已落地）"),
    ("DamagePrevented",    "伤害被阻止"),
    ("ShieldDamaged",      "护盾受损"),
    ("HealthDamaged",      "实际生命损失"),
    ("HealingApplied",     "治疗已应用"),
    ("ResourceGained",     "资源获得"),
    ("ResourceSpent",      "资源消耗"),
    ("StatusApplied",      "状态已施加"),
    ("StatusStackChanged", "状态层数变化"),
    ("StatusTicked",       "状态 tick 触发"),
    ("StatusRemoved",      "状态被移除（驱散等）"),
    ("StatusExpired",      "状态自然过期"),
    ("BeforeUnitDeath",    "死亡提交前（允许复活/替死干预）"),
    ("UnitDowned",         "单位倒地（有尸体韧性，仍可被攻击/复活）"),
    ("UnitDied",           "死亡已提交"),
    ("AfterUnitDeath",     "死亡结算后"),
    ("UnitRevived",        "单位复活"),
    ("UnitSpawned",        "单位被召唤"),
    ("UnitRemoved",        "单位移出战场"),
    ("AbilityCastStarted", "技能施放开始"),
    ("AbilityCastCompleted", "技能施放完成"),
])

EVENT_PHASE = EnumDef("EventPhase", "事件阶段语义（Before=机会 / Commit=已提交 / After=结果后）", [
    ("BEFORE", "事件发生前（可干预）"),
    ("COMMIT", "事件正在提交"),
    ("AFTER",  "事件发生后（只能响应结果）"),
])

# ======================================================================
# 效果（EffectType）——原子效果，组合即技能/被动，见设计文档 §5.4
# ======================================================================
EFFECT_TYPE = EnumDef("EffectType", "原子效果类型", [
    ("ADD_STAT_MODIFIER",       "添加属性修正"),
    ("REMOVE_STAT_MODIFIER",    "移除属性修正"),
    ("ADD_STAT_TRANSFORM",      "添加属性变换"),
    ("SWAP_EXTREME_STAT_VALUES","交换最高/最低属性值（一次性变换）"),
    ("APPLY_STATUS",            "施加状态"),
    ("REMOVE_STATUS",           "移除状态"),
    ("MODIFY_STATUS_STACKS",    "修改状态层数"),
    ("CLEANSE_STATUS",          "驱散状态（按标签）"),
    ("DEAL_DAMAGE",             "造成伤害"),
    ("MODIFY_PENDING_DAMAGE",   "修改候选伤害"),
    ("PREVENT_DAMAGE",          "阻止伤害"),
    ("REFLECT_DAMAGE",          "反射伤害"),
    ("HEAL",                    "治疗"),
    ("MODIFY_PENDING_HEALING",  "修改候选治疗"),
    ("GAIN_RESOURCE",           "获得资源"),
    ("SPEND_RESOURCE",          "消耗资源"),
    ("REFUND_RESOURCE",         "退还资源"),
    ("GRANT_ACTION",            "授予行动资源"),
    ("CONSUME_ACTION",          "消耗行动资源"),
    ("GRANT_EXTRA_TURN",        "授予额外回合"),
    ("CHANGE_INITIATIVE",       "改变先攻/速度顺序"),
    ("MOVE_UNIT",               "移动单位"),
    ("PUSH_UNIT",               "推离单位"),
    ("PULL_UNIT",               "拉近单位"),
    ("TELEPORT_UNIT",           "传送单位"),
    ("SPAWN_UNIT",              "召唤单位"),
    ("CHANGE_TARGET",           "改变目标"),
    ("TAUNT",                   "嘲讽"),
    ("APPLY_CONTROL",           "施加控制效果"),
    ("ADD_TAG",                 "添加标签"),
    ("REMOVE_TAG",              "移除标签"),
    ("MARK_TARGET",             "标记目标"),
])

TARGET_SELECTOR_TYPE = EnumDef("TargetSelectorType", "受控目标选择器", [
    ("SELF",              "所有者自身"),
    ("EVENT_SOURCE",      "事件来源单位"),
    ("EVENT_TARGET",      "事件目标单位"),
    ("CHOSEN_TARGET",     "行动选定目标"),
    ("ALL_ALLIES",        "所有友军"),
    ("ALL_ENEMIES",       "所有敌人"),
    ("ALL_UNITS",         "所有单位"),
    ("NEAREST_ENEMY",     "最近敌人"),
    ("RANDOM_ENEMY",      "随机敌人"),
    ("LOWEST_HP_ALLY",    "最低生命友军"),
    ("HIGHEST_STAT_UNIT", "指定属性最高单位"),
    ("UNITS_IN_RADIUS",   "半径范围内单位"),
    ("UNITS_ON_TILES",    "指定格子上的单位"),
    ("LAST_EFFECTIVE_DAMAGE_SOURCE", "最后有效伤害来源"),
])

CONDITION_OP = EnumDef("ConditionOp", "受控条件操作（用于条件树）", [
    ("ALWAYS",                              "恒真"),
    ("ALL",                                 "所有子条件成立（AND）"),
    ("ANY",                                 "任一子条件成立（OR）"),
    ("NOT",                                 "子条件取反"),
    ("SOURCE_IS_ALLY_OF_OWNER",             "事件来源是所有者友军"),
    ("EVENT_TARGET_IS_ENEMY_OF_OWNER",      "事件目标是所有者敌人"),
    ("OWNER_IS_LAST_EFFECTIVE_DAMAGE_SOURCE", "所有者是最后有效伤害来源"),
    ("EVENT_AMOUNT_GE",                     "事件数值 >= 阈值"),
    ("OWNER_HP_RATIO_LE",                   "所有者生命比例 <= 阈值"),
    ("EVENT_TARGET_HP_RATIO_LE",            "事件目标生命比例 <= 阈值"),
    ("OWNER_HAS_STATUS",                    "所有者拥有某状态"),
    ("EVENT_TARGET_HAS_STATUS",             "事件目标拥有某状态"),
    ("EVENT_RESOURCE_IS",                   "事件资源类型匹配"),
    ("EVENT_HAS_TAG",                       "事件携带某标签"),
    ("EVENT_LACKS_TAG",                     "事件不携带某标签（如反伤不触发反伤）"),
    ("RANDOM_CHANCE",                       "概率判定（消耗战斗 RNG）"),
])

# ======================================================================
# 状态系统（§10）
# ======================================================================
STACK_POLICY = EnumDef("StackPolicy", "状态叠层策略（§10.2）", [
    ("INDEPENDENT_INSTANCES", "每次施加创建独立实例，各自计时"),
    ("STACK_AND_REFRESH",     "层数 +1 并刷新持续时间"),
    ("STACK_WITHOUT_REFRESH", "层数 +1 但不刷新持续时间"),
    ("REFRESH_ONLY",          "不增层数，仅刷新持续时间"),
    ("REPLACE_STRONGER",      "只保留强度更高者"),
    ("UNIQUE",                "目标身上最多一个实例"),
])

DURATION_UNIT = EnumDef("DurationUnit", "状态持续时间的计时单位（§10.3）", [
    ("PERMANENT",   "永久"),
    ("ACTOR_TURNS", "按持有者回合数计"),
    ("ROUNDS",      "按全局轮次计"),
    ("EVENT_COUNT", "按固定事件次数计"),
])

TICK_TIMING = EnumDef("TickTiming", "状态 tick 时机（§10.3）", [
    ("IMMEDIATE",    "施加后立即触发一次"),
    ("START_OF_TURN", "持有者回合开始时"),
    ("END_OF_TURN",   "持有者回合结束时"),
    ("ROUND_END",     "全局轮次结束时（如 Bleed）"),
    ("EVENT_DRIVEN",  "仅由事件驱动"),
])

# ======================================================================
# 属性计算（§9）
# ======================================================================
STAT_MOD_OP = EnumDef("StatModifierOp", "属性修正运算", [
    ("ADD", "加法"),
    ("MUL", "乘法（倍率，10000 = 100%）"),
    ("SET", "覆盖"),
])

STACKING_MODE = EnumDef("StackingMode", "修正叠加模式（§9.5）", [
    ("ADDITIVE",        "数值相加"),
    ("MULTIPLICATIVE",  "倍率相乘"),
    ("OVERRIDE",        "按优先级选一个覆盖值"),
    ("MINIMUM",         "取最小值"),
    ("MAXIMUM",         "取最大值"),
    ("UNIQUE_BY_SOURCE","同一来源最多生效一次"),
    ("DIMINISHING_RETURNS", "逐步递减（明确公式）"),
])

STAT_PHASE = EnumDef("StatPhase", "属性修正所处计算阶段（§9.2 顺序即枚举顺序）", [
    ("PERMANENT_PROGRESSION",   "永久成长修正"),
    ("EQUIPMENT_AND_PASSIVE",   "装备与被动修正"),
    ("PRE_TRANSFORM_TEMPORARY", "参与变换的临时修正"),
    ("POST_TRANSFORM",          "变换后临时修正"),
])

TRIGGER_LIMIT_SCOPE = EnumDef("TriggerLimitScope", "触发次数限制范围", [
    ("PER_EVENT",  "每次事件最多触发一次"),
    ("PER_ACTION", "每次行动最多触发一次"),
    ("PER_TURN",   "每个单位回合最多触发一次"),
    ("PER_ROUND",  "每轮最多触发一次"),
    ("PER_BATTLE", "每场战斗最多触发一次"),
])

# ======================================================================
# 回合与行动调度（§6）
# ======================================================================
TURN_KIND = EnumDef("TurnKind", "回合种类", [
    ("MAIN",     "主回合"),
    ("EXTRA",    "额外回合"),
    ("SCRIPTED", "脚本/规则插入回合"),
])

ACTION_TYPE = EnumDef("ActionType", "行动类型", [
    ("MOVE",          "移动"),
    ("BASIC_ATTACK",  "基础攻击"),
    ("CAST_ABILITY",  "施放技能"),
    ("USE_ITEM",      "使用道具"),
])

MOVE_KIND = EnumDef("MoveKind", "位移来源语义（供被动区分主动移动/强制位移）", [
    ("ACTIVE",  "主动移动"),
    ("FORCED",  "被强制位移（推/拉）"),
    ("TELEPORT","传送"),
])

# ======================================================================
# 地图（§11）
# ======================================================================
TERRAIN_TYPE = EnumDef("TerrainType", "格子地形类型（wiki: Tiles 页，消耗见 DD-002 半步制）", [
    ("EMPTY",       "空地（消耗 2 半步）"),
    ("ROAD",        "道路（消耗 1 半步）"),
    ("GRASS",       "草丛（消耗 2；元素反应载体）"),
    ("TALL_GRASS",  "高草丛（消耗 2；站内 +50% 闪避）"),
    ("FLOWER",      "花丛（消耗 2；+2 回血 +2 回蓝）"),
    ("TALL_FLOWER", "高花丛（消耗 2；+3/+3 回复、50% 闪避）"),
    ("BRAMBLE",     "荆棘（消耗 4；每进入一次 2 伤害）"),
    ("THICK_BRAMBLE", "厚荆棘（消耗 6；每进入一次 4 伤害）"),
    ("WATER",       "水域（消耗 4；施加 Wet；导电）"),
    ("WATER_CURRENT", "激流（消耗 4；Wet；轮末顺流推 1 格）"),
    ("CREEP",       "苔沼（消耗 4；Wet；进入/回合开始 1 伤害）"),
    ("LAVA",        "熔岩（不可通行；demo 地形）"),
    ("PIT",         "深坑（不可通行）"),
    ("WALL",        "墙（不可通行，不可占据）"),
])

# ======================================================================
# 枚举注册表（生成器按此顺序输出）
# ======================================================================
ALL_ENUMS = [
    STAT_TYPE, DERIVED_STAT_TYPE, RESOURCE_TYPE, ACTION_SLOT,
    DAMAGE_TYPE, TEAM, LIFE_STATE,
    EVENT_TYPE, EVENT_PHASE,
    EFFECT_TYPE, TARGET_SELECTOR_TYPE, CONDITION_OP,
    STACK_POLICY, DURATION_UNIT, TICK_TIMING,
    STAT_MOD_OP, STACKING_MODE, STAT_PHASE, TRIGGER_LIMIT_SCOPE,
    TURN_KIND, ACTION_TYPE, MOVE_KIND,
    TERRAIN_TYPE,
]
