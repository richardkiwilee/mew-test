# Mewgenics 战斗规则调研笔记

> 来源：mewgenics.wiki.gg（Stats / Battle / Speed / Damage / Damage Mechanics / Status Effects / Abilities / Tiles / Traps / Knockback / Range and Area / Keywords / Items 等页面）。
> 标注约定：**wiki 明示数值**直接引用；"存疑"处见 docs/design_decisions.md 中本项目做出的决策。

## 1. 主属性（7 项，出生基准 3~7，5 为中性）

| 属性 | 效果 |
|---|---|
| STR | 近战普攻 = `Str − 5`（最低 1）；近战技能 = `(Str−5)/2` |
| DEX | 远程攻击/技能 = `(Dex−5)/2` |
| CON | 最大生命 = `CON × 4`（最低 1）；战后回复 = `5 + (CON−5)/2` |
| INT | 回合结束回蓝 = `max(Int, 0)` |
| CHA | 最大法力 = `CHA × 3`；起始法力 = CHA |
| SPD | 移动范围 = `max(1, 4 + trunc((Spd−5)/2))`；先攻 = `base + 2×Spd` |
| LCK | 暴击率 `+2%/点`；一切随机判定按 Luck 骰公式（见下） |

**Luck 骰公式**（L≥5 时 S = 1 − f^n·(1−a·s)，L<5 时 S = s^n·(1−a·f)，
其中 s=基础成功率、f=1−s、k=|L−5|/10、n=1+⌊k⌋、a=k−⌊k⌋ —— 每偏离 5 一点多掷一骰取最好/最差）。
暴击率不受此公式影响，是平加。

## 2. 派生属性

- HP_max = CON×4；Mana_max = CHA×3；起始法力 = CHA
- Movement Range = max(1, 4 + trunc((Spd−5)/2))（猫 base=4；Agile 被动 +2 移动 ≈ SPD+4）
- Initiative = base_initiative + 2×Spd
- Crit Chance = max(0, base_crit + 2×Luck) + CritUp（**只有玩家猫等少数单位能随机暴击**，敌人 base=−999）
- 无命中率属性；miss 来源 = Dodge Chance（独立多次判定）、Blind（30%）、Miss Chance 状态

## 3. 轮 / 回合 / 先攻

- 战斗由 Round 组成（从 1 开始，无上限）；**每个单位每轮至少行动一次**。
- 顺序按 Initiative 排序；**同先攻者战斗开始时随机决定一次，整场不变**。
- 每回合默认：1 次移动 + 1 次基础攻击 + 武器 1 次 + 饰品/消耗品 1 次（各自独立 action point）。
- 回合开始：刷新各动作；回合结束：回蓝（=Mana Regen）+回血（=Health Regen），触发对应时机效果。
- Refresh 语义：动作剩余 0 次时补 1 点；Bonus Move/Attack 未用完可跨回合保留。
- 额外回合：Psychic 的 "Extra Turn?" 紧接当前回合后（施法者被 Stun、无回蓝）；其他来源"尽量最早/最晚"。
- Round 10 起疲劳（Exhaustion 每回合 +1 层，回合开始每层 1 点不可格挡伤害）。

## 4. 棋盘与移动

- **10×10 网格**；每格 5 层：Units → Gas → Pickups → Traps → Tile-Type。
- 移动距离恒用 **Manhattan**；移动按格计步；近战接触判定**含斜角**。
- 替代移动动作：Jump / Teleport / Dig / Shadowstep / Dash Attack / Trample。

## 5. 地形（Tiles，移动消耗 × 效果）

| 地形 | 消耗 | 效果 |
|---|---|---|
| Empty/Grass | 1 | 无（元素反应载体） |
| Road | 0.5 | 无 |
| Tall Grass | 1 | 站内 +50% 闪避 |
| Flower | 1 | +2 HP Regen +2 Mana Regen |
| Tall Flower | 1 | +3/+3 Regen、50% 闪避 |
| Bramble | 2 | 每进入一次 2 伤害 |
| Thick Bramble | 3 | 每进入一次 4 伤害 |
| Water / Water Current | 2 | 施加 Wet；导电；Current 轮末顺流推 1 格 |
| Creep | 2 | 施加 Wet；进入/回合开始 1 伤害 |

陷阱占独立层：Bear Trap（5 物理+Immobile 1+断移动）、Web Trap（Webbed 1+断移动）、Spike Trap（8/13）、Land Mine（5）。

## 6. 攻击 / 伤害管线

- 基础攻击示例：Melee = Str；Short Shot = 5+(Dex−5)/2；Magic Missile = 3+(Int−5)/2（必中）；Cleric Heal/Attack = Str。
- **伤害管线顺序**：减伤（Brace 每层 −1 / Petrify 恒 1）→ 暴击（×2，**最后应用**，可突破限伤）→ 背刺 +25%（仅物理，向下取整最低 +1）→ 护盾（Holy Shield 每次挡 1 个实例，先于 Shield）→ HP。
- 暴击默认 2 倍；必中标记（Marked/Magic Weakness）可破闪避。
- 击退：撞到单位双方各受 1 点 Knockback 伤害；撞边=撞自己。

## 7. 关键状态时机（wiki 明示 Behavior）

| 状态 | 时机 |
|---|---|
| Burn | 回合开始：每层 1 伤害，然后 −1 层；无视盾 |
| Poison | 回合开始：每层 1 伤害；不衰减；被治疗 −1 治疗量并消耗 1 层 |
| Bleed | **轮结束**：受 =层数 伤害，然后 +1 层；无视盾 |
| Stun | 立即跳过回合；除施加回合外每回合结束 −1 层 |
| Sleep | 跳过下回合；被攻击有几率醒 |
| Freeze | 免疫一切伤害/状态；每起始回合减层；耗尽变 Wet |
| Marked | 受物理伤害 100% 暴击、无视盾/圣盾、必中；被标记者回合结束 −1 |
| Bruise | 受所有物理伤害 +1/层 |
| Weakness | STR/DEX/SPD 各 −2 |
| Blind | 30% miss，每回合结束 −1 |
| Init Up/Down | ±1 先攻/层，**回合开始时移除** |
| Bonus Move/Attack | 额外 1 次动作，未用可跨回合保留 |
| Exhaustion | 回合开始每层 1 不可格挡伤害，然后 +1；不可清除 |

- Stat Up/Down 是同一 buff 的正负显示，抵消后消失；临时与永久版本互相独立。
- 不可叠层类（如 Stun）重复施加取较高值。
- 倒地清除绝大多数状态。

## 8. 装备与被动

- 槽位：武器 / 头部 / 面部 / 颈部 / 饰品（含消耗品同槽）。
- 被动 Agile：+2 移动（升级 +4）；Block（技能）：+2 Shield；Toad Style：移动替换为 Jump（半 STR 伤害）——"被动直接替换行动"模式。
- 属性修正类被动/物品以加法或倍率挂在对应属性上（如 +LCK 装备、All Stats Up 状态）。

## 9. 胜负

- 败：**所有玩家猫倒地**。胜：所有必需击杀的敌人死亡。
- 胜利结算：倒地猫按 `5 + (CON−5)/2` 复活回血；所有状态清除；奖励按回合数/难度/Luck 结算。

## 10. 存疑点（已转设计决策）

1. 敌人 initiative 覆写值与 `base + 2×Spd` 的叠加关系 —— wiki 未说明。
2. 移动是否禁止斜向 —— wiki 只说距离用 Manhattan、近战含斜角。
3. 额外回合"尽早/尽晚"与"紧接当前回合"在不同来源的差异。
4. 魔法暴击恒 2 倍 —— wiki 自注可能是 bug。
