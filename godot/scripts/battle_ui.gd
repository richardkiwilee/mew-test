class_name BattleUi
extends Control
## ============================================================================
## 战斗阶段 UI（参考 Mewgenics 布局）：
##   顶部条：轮次 / 当前回合 / 提示 / 结束回合
##   回合横幅：每个单位回合开始时居中弹出（"轮到 XX"）
##   左下角：行动面板【移动 / 普通攻击 / 技能列表】
##   底部中央：选中单位信息卡
##   右侧：战斗日志；左上角：地形图例
## =============================================================================

signal end_turn_pressed
signal move_pressed
signal attack_pressed
signal ability_pressed(ability_id: int)
signal cancel_pressed
signal back_to_prep

const TEAM_PLAYER := 1

var _top: PanelContainer
var _actions_panel: PanelContainer
var _log_panel: PanelContainer
var _round_label: Label
var _turn_label: Label
var _hint_label: Label
var _hover_label: Label
var _log: RichTextLabel
# 左下角行动面板
var _actions: VBoxContainer
var _move_btn: Button
var _attack_btn: Button
var _cancel_btn: Button
var _skills_header: Label
var _ability_btns: Array = []
# 底部中央单位卡
var _unit_card: PanelContainer
var _card_name: Label
var _card_hp: ProgressBar
var _card_hp_label: Label
var _card_mana: ProgressBar
var _card_statuses: Label
var _card_actions: Label
var _end_turn_btn: Button
var _legend: PanelContainer
var _banner: Label
var _banner_tween: Tween
var _result_panel: PanelContainer
var _result_label: Label
var _last_seq := 0
var _player_turn := false


func _ready() -> void:
	visible = false
	mouse_filter = Control.MOUSE_FILTER_IGNORE

	# ---- 顶部信息条 ----
	_top = _panel()
	add_child(_top)
	var top_box := HBoxContainer.new()
	_top.add_child(top_box)
	_round_label = _label("第 0 轮", 18)
	top_box.add_child(_round_label)
	_turn_label = _label("", 18, Color(1, 0.9, 0.5))
	top_box.add_child(_turn_label)
	_hint_label = _label("", 15, Color(0.65, 0.85, 1.0))
	top_box.add_child(_hint_label)
	var top_spacer := Control.new()
	top_spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top_box.add_child(top_spacer)
	_end_turn_btn = Button.new()
	_end_turn_btn.text = "结束回合 ✓"
	_end_turn_btn.custom_minimum_size = Vector2(150, 38)
	_end_turn_btn.add_theme_font_size_override("font_size", 17)
	_end_turn_btn.pressed.connect(func(): end_turn_pressed.emit())
	top_box.add_child(_end_turn_btn)

	# ---- 回合横幅（每个单位回合开始时弹出）----
	_banner = _label("", 34, Color(1, 0.92, 0.5))
	_banner.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_banner.visible = false
	add_child(_banner)

	# ---- 悬停信息 ----
	_hover_label = _label("", 14, Color(0.9, 0.95, 1.0))
	_hover_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	add_child(_hover_label)

	# ---- 左下角：行动面板（移动 / 普通攻击 / 技能）----
	_actions_panel = _panel()
	add_child(_actions_panel)
	_actions = VBoxContainer.new()
	_actions_panel.add_child(_actions)
	_actions.add_child(_label("行动", 16, Color(0.85, 0.9, 1.0)))
	var basic_row := HBoxContainer.new()
	_actions.add_child(basic_row)
	_move_btn = _big_button("移动", Color(0.2, 0.5, 0.9), 96, 52)
	_move_btn.pressed.connect(func(): move_pressed.emit())
	basic_row.add_child(_move_btn)
	_attack_btn = _big_button("普通攻击", Color(0.9, 0.35, 0.25), 96, 52)
	_attack_btn.pressed.connect(func(): attack_pressed.emit())
	basic_row.add_child(_attack_btn)
	_cancel_btn = _big_button("取消", Color(0.45, 0.45, 0.5), 96, 52)
	_cancel_btn.pressed.connect(func(): cancel_pressed.emit())
	_actions.add_child(_cancel_btn)
	_skills_header = _label("技能", 16, Color(0.85, 0.9, 1.0))
	_actions.add_child(_skills_header)

	# ---- 底部中央：选中单位信息卡 ----
	_unit_card = _panel()
	add_child(_unit_card)
	var card_box := VBoxContainer.new()
	_unit_card.add_child(card_box)
	_card_name = _label("（未选择单位）", 17, Color(1, 0.9, 0.55))
	card_box.add_child(_card_name)
	_card_hp = ProgressBar.new()
	_card_hp.custom_minimum_size = Vector2(320, 14)
	_card_hp.show_percentage = false
	_card_hp.add_theme_stylebox_override("fill", _bar_style(Color(0.25, 0.8, 0.3)))
	_card_hp.add_theme_stylebox_override("background", _bar_style(Color(0.3, 0.1, 0.1)))
	card_box.add_child(_card_hp)
	_card_hp_label = _label("", 12)
	card_box.add_child(_card_hp_label)
	_card_mana = ProgressBar.new()
	_card_mana.custom_minimum_size = Vector2(320, 14)
	_card_mana.show_percentage = false
	_card_mana.add_theme_stylebox_override("fill", _bar_style(Color(0.3, 0.55, 0.95)))
	_card_mana.add_theme_stylebox_override("background", _bar_style(Color(0.1, 0.15, 0.35)))
	card_box.add_child(_card_mana)
	_card_statuses = _label("", 12, Color(0.9, 0.75, 0.4))
	card_box.add_child(_card_statuses)
	_card_actions = _label("", 12, Color(0.7, 0.8, 0.9))
	card_box.add_child(_card_actions)

	# ---- 日志面板（右侧）----
	_log_panel = _panel()
	add_child(_log_panel)
	var log_box := VBoxContainer.new()
	_log_panel.add_child(log_box)
	log_box.add_child(_label("战斗日志", 16, Color(0.85, 0.9, 1.0)))
	_log = RichTextLabel.new()
	_log.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_log.scroll_following = true
	_log.bbcode_enabled = true
	_log.add_theme_font_size_override("normal_font_size", 13)
	log_box.add_child(_log)

	# ---- 地形图例（左上角）----
	_legend = _panel()
	add_child(_legend)
	var legend_box := VBoxContainer.new()
	_legend.add_child(legend_box)
	legend_box.add_child(_label("地形图例", 14, Color(0.85, 0.9, 1.0)))
	var legend_items := [
		[Color(0.55, 0.45, 0.33), "道路：省步数"], [Color(0.33, 0.55, 0.29), "草丛"],
		[Color(0.42, 0.62, 0.30), "花丛：+2回复"], [Color(0.28, 0.42, 0.20), "荆棘：进-2血"],
		[Color(0.18, 0.42, 0.72), "水域：湿润"], [Color(0.85, 0.30, 0.08), "熔岩：进-5血"],
		[Color(0.45, 0.45, 0.48), "墙：挡路挡视线"],
	]
	for item in legend_items:
		var row := HBoxContainer.new()
		var swatch := ColorRect.new()
		swatch.custom_minimum_size = Vector2(18, 14)
		swatch.color = item[0]
		row.add_child(swatch)
		row.add_child(_label(item[1], 12))
		legend_box.add_child(row)

	# ---- 结算面板 ----
	_result_panel = _panel()
	_result_panel.visible = false
	add_child(_result_panel)
	var rv := VBoxContainer.new()
	_result_panel.add_child(rv)
	_result_label = _label("", 32)
	rv.add_child(_result_label)
	var back := Button.new()
	back.text = "返回准备"
	back.custom_minimum_size = Vector2(180, 46)
	back.add_theme_font_size_override("font_size", 18)
	back.pressed.connect(func(): back_to_prep.emit())
	rv.add_child(back)


func _process(_delta: float) -> void:
	if not visible:
		return
	var vp := get_viewport_rect().size
	if vp.x <= 0 or vp.y <= 0:
		return
	_top.position = Vector2(8, 8)
	_top.size = Vector2(vp.x - 16, 44)
	_banner.position = Vector2(vp.x / 2.0 - 250, 70)
	_banner.size = Vector2(500, 40)
	_hover_label.position = Vector2(vp.x - 420, vp.y - 56)
	_hover_label.size = Vector2(412, 30)
	_actions_panel.position = Vector2(8, vp.y - 300)
	_actions_panel.size = Vector2(228, 292)
	_unit_card.position = Vector2(vp.x / 2.0 - 170, vp.y - 140)
	_unit_card.size = Vector2(340, 132)
	_log_panel.position = Vector2(vp.x - 400, 60)
	_log_panel.size = Vector2(392, vp.y - 200)
	_legend.position = Vector2(8, 60)
	_legend.size = Vector2(228, 180)
	_result_panel.position = Vector2(vp.x / 2.0 - 200, vp.y / 2.0 - 120)
	_result_panel.size = Vector2(400, 240)


func dump_layout() -> void:
	print("[ui] vp=", get_viewport_rect().size,
			" top=", _top.position, _top.size,
			" actions=", _actions_panel.position, _actions_panel.size,
			" card=", _unit_card.position, _unit_card.size,
			" log=", _log_panel.position, _log_panel.size)


func set_round(r: int) -> void:
	_round_label.text = "第 %d 轮" % r


func set_turn_info(text: String, player_turn: bool) -> void:
	_turn_label.text = text
	_player_turn = player_turn
	_end_turn_btn.visible = player_turn


## 回合开始横幅（明显提示轮到谁）
func show_turn_banner(unit_name: String, is_player: bool) -> void:
	_banner.text = "▶ 轮到 %s ◀" % unit_name
	_banner.add_theme_color_override("font_color",
		Color(1.0, 0.9, 0.4) if is_player else Color(1.0, 0.55, 0.45))
	_banner.visible = true
	_banner.modulate.a = 0.0
	if _banner_tween and _banner_tween.is_valid():
		_banner_tween.kill()
	_banner_tween = create_tween()
	_banner_tween.tween_property(_banner, "modulate:a", 1.0, 0.15)
	_banner_tween.tween_interval(1.2)
	_banner_tween.tween_property(_banner, "modulate:a", 0.0, 0.5)
	_banner_tween.tween_callback(func(): _banner.visible = false)


func set_hint(text: String) -> void:
	_hint_label.text = text


func set_hover(text: String) -> void:
	_hover_label.text = text


## 更新选中单位卡片与行动按钮
## info: unit_info 字典；abilities: [{id,name,cost,...}]；can_act: 该单位是否为当前回合的我方单位
func set_selected_card(info: Dictionary, abilities: Array, can_act: bool = false) -> void:
	if info.is_empty():
		_move_btn.text = "移动"
		_attack_btn.text = "普通攻击"
		_card_name.text = "（未选择单位）"
		_card_hp.max_value = 1
		_card_hp.value = 0
		_card_hp_label.text = ""
		_card_mana.max_value = 1
		_card_mana.value = 0
		_card_statuses.text = ""
		_card_actions.text = ""
		_set_actions_enabled(false)
		return
	_card_name.text = "%s  %s" % [info["name"], "（我方）" if info["team"] == TEAM_PLAYER else "（敌方）"]
	var hp: int = info["hp"]
	var mhp: int = info["max_hp"]
	_card_hp.max_value = mhp
	_card_hp.value = hp
	_card_hp_label.text = "生命 %d / %d" % [hp, mhp]
	_card_mana.max_value = maxi(1, info["max_mana"])
	_card_mana.value = info["mana"]
	var st := ""
	for s in info["statuses"]:
		st += "【%s ×%d】" % [s["name"], s["stacks"]]
	_card_statuses.text = st
	var ap: Array = info["action_points"]
	_card_actions.text = "移动×%d  攻击×%d  法力 %d/%d" % [ap[0], ap[1], info["mana"], info["max_mana"]]
	# 行动按钮状态：只有"当前回合的我方单位"才能行动
	if not can_act:
		_set_actions_enabled(false)
		return
	var can_move: bool = info.get("alive", false) and ap[0] > 0
	var can_attack: bool = info.get("alive", false) and ap[1] > 0
	_move_btn.text = "移动 ×%d" % ap[0]
	_attack_btn.text = "普通攻击 ×%d" % ap[1]
	_move_btn.disabled = not can_move
	_attack_btn.disabled = not can_attack
	for i in _ability_btns.size():
		var b: Button = _ability_btns[i]
		var ab: Dictionary = abilities[i] if i < abilities.size() else {}
		if not ab.is_empty():
			b.disabled = info["mana"] < ab.get("cost", 0) or (ap[1] <= 0 and ab.get("spends_action", true))
		else:
			b.disabled = true


## 设置技能按钮（当前单位可用的技能列表，纵向排列在左下角面板）
func set_abilities(abilities: Array) -> void:
	for b in _ability_btns:
		b.queue_free()
	_ability_btns.clear()
	for a in abilities:
		var b := Button.new()
		b.text = "%s（%d蓝 射程%d-%d）" % [a["name"], a["cost"], a["min_range"], a["max_range"]]
		b.tooltip_text = a["desc"]
		b.custom_minimum_size = Vector2(200, 40)
		b.add_theme_font_size_override("font_size", 14)
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		var icon := load("res://assets/icons/ability_%d.svg" % a["id"])
		if icon:
			b.icon = icon
			b.expand_icon = true
		b.pressed.connect(func(): ability_pressed.emit(a["id"]))
		_actions.add_child(b)
		_ability_btns.append(b)
	_skills_header.visible = not abilities.is_empty()


func clear_abilities() -> void:
	for b in _ability_btns:
		b.queue_free()
	_ability_btns.clear()
	_skills_header.visible = false


func _set_actions_enabled(enabled: bool) -> void:
	_move_btn.disabled = not enabled
	_attack_btn.disabled = not enabled
	for b in _ability_btns:
		b.disabled = not enabled


func append_log_entries(entries: Array) -> void:
	for e in entries:
		var seq: int = e["seq"]
		if seq <= _last_seq:
			continue
		_last_seq = seq
		var line: String = e["detail"]
		var color := "#c8c8cc"
		if e["key"].begins_with("Condition") or e["key"].begins_with("Trigger") or e["key"] == "EffectExecuted":
			color = "#8fa8c8"
		elif e["key"] == "DamageApplied" or e["key"] == "HealthDamaged" or e["key"] == "UnitDied" or e["key"] == "UnitDowned":
			color = "#e89a8a"
		elif e["key"] == "HealingApplied" or e["key"] == "ResourceGained" or e["key"] == "TerrainRegen":
			color = "#9ad8a0"
		elif e["key"] == "TurnStarted" or e["key"] == "RoundStarted" or e["key"] == "TurnOrder":
			color = "#e8d88a"
		elif e["key"] == "StatusApplied" or e["key"] == "StatusTicked" or e["key"] == "StatusExpired":
			color = "#d0a8e0"
		_log.append_text("[color=%s]%s[/color]\n" % [color, line.replace("[", "(").replace("]", ")")])


func show_result(victory: bool) -> void:
	_result_label.text = "胜利！" if victory else "战败…"
	_result_label.add_theme_color_override("font_color",
		Color(1.0, 0.85, 0.4) if victory else Color(1.0, 0.45, 0.4))
	_result_panel.visible = true


func hide_result() -> void:
	_result_panel.visible = false


func reset_log() -> void:
	_log.clear()
	_last_seq = 0


func _panel() -> PanelContainer:
	var p := PanelContainer.new()
	var sb := StyleBoxFlat.new()
	sb.bg_color = Color(0.08, 0.09, 0.12, 0.82)
	sb.border_color = Color(0.35, 0.4, 0.5, 0.8)
	sb.set_border_width_all(1)
	sb.set_corner_radius_all(6)
	sb.content_margin_left = 8
	sb.content_margin_right = 8
	sb.content_margin_top = 4
	sb.content_margin_bottom = 4
	p.add_theme_stylebox_override("panel", sb)
	return p


func _big_button(text: String, color: Color, w: float = 112, h: float = 56) -> Button:
	var b := Button.new()
	b.text = text
	b.custom_minimum_size = Vector2(w, h)
	b.add_theme_font_size_override("font_size", 16)
	var sb := StyleBoxFlat.new()
	sb.bg_color = color.darkened(0.35)
	sb.set_corner_radius_all(6)
	sb.content_margin_top = 4
	b.add_theme_stylebox_override("normal", sb)
	var sbd := sb.duplicate()
	sbd.bg_color = Color(0.35, 0.36, 0.4)   # 禁用=浅灰，仍清晰可见
	b.add_theme_stylebox_override("disabled", sbd)
	b.add_theme_color_override("font_disabled_color", Color(0.8, 0.8, 0.82))
	var sbh := sb.duplicate()
	sbh.bg_color = color
	b.add_theme_stylebox_override("hover", sbh)
	var sbp := sb.duplicate()
	sbp.bg_color = color.lightened(0.2)
	b.add_theme_stylebox_override("pressed", sbp)
	return b


func _bar_style(color: Color) -> StyleBoxFlat:
	var sb := StyleBoxFlat.new()
	sb.bg_color = color
	sb.set_corner_radius_all(3)
	return sb


func _label(text: String, size: int, color := Color(0.95, 0.95, 0.95)) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	return l
