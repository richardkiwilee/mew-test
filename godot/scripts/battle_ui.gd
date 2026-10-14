class_name BattleUi
extends Control
## ============================================================================
## 战斗阶段 UI：回合信息 / 行动按钮 / 技能条 / 战斗日志 / 结算面板
## =============================================================================

signal end_turn_pressed
signal ability_pressed(ability_id: int)
signal back_to_prep

var _round_label: Label
var _turn_label: Label
var _hint_label: Label
var _log: RichTextLabel
var _abilities_row: HBoxContainer
var _end_turn_btn: Button
var _result_panel: PanelContainer
var _result_label: Label
var _last_seq := 0


func _ready() -> void:
	visible = false
	var root := VBoxContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(root)

	var top := HBoxContainer.new()
	top.custom_minimum_size = Vector2(0, 44)
	root.add_child(top)
	_round_label = _label("第 0 轮", 18)
	top.add_child(_round_label)
	_turn_label = _label("", 18, Color(1, 0.9, 0.5))
	top.add_child(_turn_label)
	_hint_label = _label("", 15, Color(0.65, 0.85, 1.0))
	top.add_child(_hint_label)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	top.add_child(spacer)
	_end_turn_btn = Button.new()
	_end_turn_btn.text = "结束回合"
	_end_turn_btn.custom_minimum_size = Vector2(130, 40)
	_end_turn_btn.pressed.connect(func(): end_turn_pressed.emit())
	top.add_child(_end_turn_btn)

	# 技能条
	_abilities_row = HBoxContainer.new()
	root.add_child(_abilities_row)

	# 日志
	_log = RichTextLabel.new()
	_log.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_log.scroll_following = true
	_log.bbcode_enabled = true
	_log.add_theme_font_size_override("normal_font_size", 13)
	root.add_child(_log)

	# 结算面板
	_result_panel = PanelContainer.new()
	_result_panel.set_anchors_preset(Control.PRESET_CENTER)
	_result_panel.offset_left = -200
	_result_panel.offset_top = -120
	_result_panel.offset_right = 200
	_result_panel.offset_bottom = 120
	_result_panel.visible = false
	add_child(_result_panel)
	var rv := VBoxContainer.new()
	_result_panel.add_child(rv)
	_result_label = _label("", 30)
	rv.add_child(_result_label)
	var back := Button.new()
	back.text = "返回准备"
	back.custom_minimum_size = Vector2(160, 44)
	back.pressed.connect(func(): back_to_prep.emit())
	rv.add_child(back)


func set_round(r: int) -> void:
	_round_label.text = "第 %d 轮" % r


func set_turn_info(text: String, player_turn: bool) -> void:
	_turn_label.text = text
	_end_turn_btn.visible = player_turn


func set_hint(text: String) -> void:
	_hint_label.text = text


func set_abilities(abilities: Array) -> void:
	for c in _abilities_row.get_children():
		c.queue_free()
	for a in abilities:
		var b := Button.new()
		b.text = "%s(%d蓝)" % [a["name"], a["cost"]]
		b.custom_minimum_size = Vector2(120, 36)
		b.pressed.connect(func(): ability_pressed.emit(a["id"]))
		_abilities_row.add_child(b)


func clear_abilities() -> void:
	for c in _abilities_row.get_children():
		c.queue_free()


func append_log_entries(entries: Array) -> void:
	for e in entries:
		var seq: int = e["seq"]
		if seq <= _last_seq:
			continue
		_last_seq = seq
		var line: String = e["detail"]
		var color := "#cccccc"
		if e["key"].begins_with("Condition") or e["key"].begins_with("Trigger") or e["key"] == "EffectExecuted":
			color = "#8fa8c8"
		elif e["key"] == "DamageApplied" or e["key"] == "HealthDamaged" or e["key"] == "UnitDied" or e["key"] == "UnitDowned":
			color = "#e89a8a"
		elif e["key"] == "HealingApplied" or e["key"] == "ResourceGained":
			color = "#9ad8a0"
		elif e["key"] == "TurnStarted" or e["key"] == "RoundStarted":
			color = "#e8d88a"
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


func _label(text: String, size: int, color := Color(0.95, 0.95, 0.95)) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", color)
	return l
