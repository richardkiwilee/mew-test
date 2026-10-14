class_name PrepUi
extends Control
## ============================================================================
## 战斗准备阶段 UI：随机 4 只猫（属性/被动/技能随机）+ 装备配置 + 地图选择
## 猫生成用的随机数属"阵容生成 RNG"（DD-014：与战斗 RNG 分离，不影响重放）。
## =============================================================================

signal start_requested(map_index: int, cat_configs: Array)

const STAT_NAMES := ["力量", "敏捷", "体质", "智力", "魅力", "速度", "幸运"]
const CAT_NAMES := ["煤球", "雪球", "琥珀", "奶糖", "斑斑", "虎子", "豆豆", "年糕",
	"芝麻", "橘子", "米粒", "布丁", "汤圆", "花卷", "饼干", "可乐"]
const ABILITY_POOL := [201, 202, 203, 205]   # 火球/治疗/电击/烈焰扫荡
# 猫职业（wiki CLASS_* 动画素材的 13 个职业；随机分配并作为棋盘精灵）
const CLASSES := [
	["butcher", "屠夫"], ["cleric", "牧师"], ["collarless", "无项圈"], ["druid", "德鲁伊"],
	["fighter", "战士"], ["hunter", "猎人"], ["jester", "小丑"], ["mage", "法师"],
	["monk", "武僧"], ["necromancer", "死灵"], ["psychic", "通灵"], ["tank", "坦克"],
	["thief", "盗贼"],
]
const EQUIPMENTS := [  # id → 名称
	[110, "幸运护符(+3幸运)"], [111, "力量腰带(+2力量)"], [112, "疾风之靴(+2速度)"],
	[113, "学者之帽(+2智力)"], [114, "生命护符(+2体质)"],
]

var _rng := RandomNumberGenerator.new()
var _passive_pool: Array = []
var _ability_pool: Array = []
var _cats: Array = []            # 每只猫：{name, stats, passives, abilities, equip_id}
var _card_equip: Array = []      # OptionButton 引用
var _selected_map := 0
var _map_buttons: Array = []
var _root: VBoxContainer
var _cards_row: HBoxContainer
var _map_desc: Label


func setup(passive_pool: Array, ability_pool: Array) -> void:
	_passive_pool = passive_pool
	_ability_pool = ability_pool
	visible = true


func _ready() -> void:
	_rng.randomize()
	var bg := ColorRect.new()
	bg.color = Color(0.13, 0.15, 0.19)
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(bg)
	_root = VBoxContainer.new()
	_root.set_anchors_preset(Control.PRESET_FULL_RECT)
	_root.offset_left = 24
	_root.offset_top = 16
	_root.offset_right = -24
	_root.offset_bottom = -16
	add_child(_root)
	_root.add_child(_label("猫猫战斗 Demo —— 战斗准备", 30))
	_root.add_child(_label("随机生成的 4 只猫，请为它们配置装备并选择地图", 15, Color(0.7, 0.75, 0.8)))

	# 地图选择
	_root.add_child(_label("选择地图", 18))
	var map_row := HBoxContainer.new()
	for i in Maps.MAPS.size():
		var b := Button.new()
		b.text = Maps.MAPS[i]["name"]
		b.toggle_mode = true
		b.custom_minimum_size = Vector2(150, 40)
		b.pressed.connect(func(): _select_map(i))
		map_row.add_child(b)
		_map_buttons.append(b)
	_root.add_child(map_row)
	_map_desc = _label(Maps.MAPS[0]["desc"], 13, Color(0.6, 0.7, 0.6))
	_root.add_child(_map_desc)

	# 猫卡片
	_root.add_child(_label("队伍（点击重新随机）", 18))
	_cards_row = HBoxContainer.new()
	for i in 4:
		var card := _make_cat_card(i)
		_cards_row.add_child(card)
		_reroll_cat(i)   # 卡片入容器后再填内容
	_root.add_child(_cards_row)

	var start_btn := Button.new()
	start_btn.text = "开始战斗"
	start_btn.custom_minimum_size = Vector2(220, 52)
	start_btn.add_theme_font_size_override("font_size", 22)
	start_btn.pressed.connect(_on_start)
	_root.add_child(start_btn)


func _select_map(i: int) -> void:
	_selected_map = i
	for k in _map_buttons.size():
		_map_buttons[k].button_pressed = (k == i)
	_map_desc.text = Maps.MAPS[i]["desc"]


func _make_cat_card(index: int) -> Control:
	var card := PanelContainer.new()
	card.custom_minimum_size = Vector2(285, 380)
	var vbox := VBoxContainer.new()
	card.add_child(vbox)

	var reroll := Button.new()
	reroll.text = "重新随机"
	reroll.pressed.connect(func(): _reroll_cat(index))
	vbox.add_child(reroll)

	var name_label := _label("", 20)
	name_label.name = "name"
	vbox.add_child(name_label)

	var class_label := _label("", 13, Color(0.75, 0.85, 1.0))
	class_label.name = "class"
	vbox.add_child(class_label)

	var stats_label := RichTextLabel.new()
	stats_label.bbcode_enabled = true
	stats_label.fit_content = true
	stats_label.name = "stats"
	stats_label.add_theme_font_size_override("normal_font_size", 14)
	vbox.add_child(stats_label)

	vbox.add_child(_label("被动", 14, Color(0.65, 0.65, 0.7)))
	var passives_label := _label("", 13, Color(0.9, 0.8, 0.5))
	passives_label.name = "passives"
	passives_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	vbox.add_child(passives_label)

	vbox.add_child(_label("技能", 14, Color(0.65, 0.65, 0.7)))
	var abilities_label := _label("", 13, Color(0.5, 0.8, 0.9))
	abilities_label.name = "abilities"
	abilities_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	vbox.add_child(abilities_label)

	vbox.add_child(_label("装备", 14, Color(0.65, 0.65, 0.7)))
	var equip := OptionButton.new()
	equip.add_item("无装备", 0)
	for e in EQUIPMENTS:
		equip.add_item(e[1], e[0])
	equip.item_selected.connect(func(_i: int): _on_equip(index, equip))
	vbox.add_child(equip)
	_card_equip.append(equip)
	return card


func _reroll_cat(index: int) -> void:
	# 随机 4 只猫：属性 3..7、2 被动、2 技能
	var stats := PackedInt32Array()
	for _s in 7:
		stats.append(_rng.randi_range(3, 7))
	var passives: Array = []
	var pool: Array = _passive_pool.duplicate()
	for _p in 2:
		if pool.is_empty():
			break
		var pick: Dictionary = pool.pop_at(_rng.randi_range(0, pool.size() - 1))
		passives.append(pick["id"])
	var abilities: Array = []
	var apool := ABILITY_POOL.duplicate()
	for _a in 2:
		if apool.is_empty():
			break
		abilities.append(apool.pop_at(_rng.randi_range(0, apool.size() - 1)))
	var cls: Array = CLASSES[_rng.randi_range(0, CLASSES.size() - 1)]
	var cat := {
		"name": CAT_NAMES[_rng.randi_range(0, CAT_NAMES.size() - 1)],
		"class": cls[0], "class_name": cls[1],
		"stats": stats, "passives": passives, "abilities": abilities, "equip": 0,
	}
	if _cats.size() <= index:
		_cats.append(cat)
	else:
		_cats[index] = cat
	_update_card_display(index)


func _update_card_display(index: int) -> void:
	var panel := _cards_row.get_child(index) as PanelContainer
	var vbox := panel.get_child(0) as VBoxContainer
	var cat: Dictionary = _cats[index]
	(vbox.get_node("name") as Label).text = cat["name"]
	(vbox.get_node("class") as Label).text = "职业：%s" % cat["class_name"]
	var stats_text := ""
	for i in 7:
		var v: int = cat["stats"][i]
		var color := "#cccccc"
		if v >= 6:
			color = "#7fd87f"
		elif v <= 4:
			color = "#e88a7a"
		stats_text += "[color=%s]%s %d[/color]  " % [color, STAT_NAMES[i], v]
	(vbox.get_node("stats") as RichTextLabel).text = stats_text
	(vbox.get_node("passives") as Label).text = _names_of(cat["passives"], _passive_pool)
	(vbox.get_node("abilities") as Label).text = _names_of(cat["abilities"], _ability_pool)
	_card_equip[index].select(0)
	cat["equip"] = 0


func _names_of(ids: Array, pool: Array) -> String:
	var parts: Array = []
	for id in ids:
		for entry in pool:
			if entry["id"] == id:
				parts.append(entry["name"])
	return "、".join(parts) if not parts.is_empty() else "—"


func _on_equip(index: int, option: OptionButton) -> void:
	_cats[index]["equip"] = option.get_item_id(option.selected)


func _on_start() -> void:
	var configs: Array = []
	for cat in _cats:
		var passives: Array = cat["passives"].duplicate()
		if cat["equip"] != 0:
			passives.append(cat["equip"])
		configs.append({
			"name": cat["name"], "class": cat["class"], "class_name": cat["class_name"],
			"stats": cat["stats"],
			"passives": passives, "abilities": cat["abilities"],
		})
	visible = false
	start_requested.emit(_selected_map, configs)


func _label(text: String, size: int, color := Color(0.95, 0.95, 0.95)) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	return l
