extends Node
## ============================================================================
## Main — Demo 流程控制：准备 → 战斗 → 结算 → 返回准备（循环）
## 战斗逻辑全部在 C++ 核心（BattleController），本脚本只做展示与输入转发。
## =============================================================================

const TEAM_PLAYER := 1

enum State { PREP, BATTLE, RESULT }

var _ctrl: BattleController  # GDExtension 战斗核心
var _board: Board
var _prep: PrepUi
var _battle_ui: BattleUi
var _state := State.PREP
var _selected_unit := -1
var _targeting_ability := -1
var _ability_lookup := {}
var _map_index := 0


func _ready() -> void:
	# 战斗核心控制器（GDExtension 注册的类）
	_ctrl = BattleController.new()
	add_child(_ctrl)

	# 3D 棋盘
	_board = Board.new()
	add_child(_board)

	# 两个 UI 层
	_prep = PrepUi.new()
	add_child(_prep)
	_battle_ui = BattleUi.new()
	add_child(_battle_ui)

	_prep.start_requested.connect(_on_start_battle)
	_battle_ui.end_turn_pressed.connect(_on_end_turn)
	_battle_ui.ability_pressed.connect(_on_ability_pressed)
	_battle_ui.back_to_prep.connect(_on_back_to_prep)
	_board.tile_clicked.connect(_on_tile_clicked)
	_board.unit_clicked.connect(_on_unit_clicked)

	_enter_prep()


func _enter_prep() -> void:
	_state = State.PREP
	_selected_unit = -1
	_targeting_ability = -1
	_battle_ui.visible = false
	_battle_ui.hide_result()
	_board.clear_units()
	_board.clear_highlights()
	_prep.setup(_ctrl.passive_pool(), _ctrl.ability_pool())
	_prep.visible = true


func _on_start_battle(map_index: int, cat_configs: Array) -> void:
	_map_index = map_index
	var map: Dictionary = Maps.get_map(map_index)
	var seed := randi()
	_ctrl.create_battle(seed, 10, 10, Maps.terrain_codes(map["rows"]))
	for i in cat_configs.size():
		var cfg: Dictionary = cat_configs[i]
		var pos: Vector2i = map["cats"][i]
		_ctrl.add_cat(cfg["stats"], cfg["passives"], cfg["abilities"], pos.x, pos.y, cfg["name"])
	for e in map["enemies"]:
		_ctrl.add_enemy(e[0], e[1], e[2])

	# 技能名/费用查询表（UI 用）
	_ability_lookup = {}
	for a in _ctrl.ability_pool():
		_ability_lookup[a["id"]] = a

	_board.build_terrain(Maps.terrain_codes(map["rows"]))
	_board.clear_units()
	_battle_ui.reset_log()
	_battle_ui.visible = true
	_battle_ui.set_hint("战斗开始！点击己方猫选择，点击格子移动，点击敌人攻击")
	_ctrl.start_battle()
	_state = State.BATTLE
	_selected_unit = -1
	_refresh_all()
	_run_enemy_turns_if_needed()


func _refresh_all() -> void:
	_board.refresh_units(_ctrl.all_units())
	_battle_ui.append_log_entries(_ctrl.log_entries())
	_battle_ui.set_round(_ctrl.round_index())

	var cur := _ctrl.current_unit()
	var player_turn := false
	if cur > 0:
		var info := _ctrl.unit_info(cur)
		player_turn = info["team"] == TEAM_PLAYER
		_battle_ui.set_turn_info("当前：%s" % info["name"], player_turn)
		_battle_ui.set_abilities(_abilities_of(cur) if player_turn else [])
	else:
		_battle_ui.set_turn_info("", false)
		_battle_ui.clear_abilities()

	# 选中单位的高亮
	if _selected_unit > 0:
		var info := _ctrl.unit_info(_selected_unit)
		if info.get("team", 0) == TEAM_PLAYER and info.get("alive", false):
			_board.set_reachable(_ctrl.reachable_tiles(_selected_unit))
		else:
			_selected_unit = -1
			_board.clear_highlights()
	else:
		_board.clear_highlights()


func _abilities_of(unit_id: int) -> Array:
	var out: Array = []
	for aid in _ctrl.unit_info(unit_id)["abilities"]:
		out.append(_ability_lookup.get(aid, {"id": aid, "name": "?", "cost": 0}))
	return out


func _run_enemy_turns_if_needed() -> void:
	if _state != State.BATTLE or _ctrl.battle_over():
		return
	var cur := _ctrl.current_unit()
	if cur <= 0:
		_check_battle_end()
		return
	if _ctrl.unit_info(cur)["team"] != TEAM_PLAYER:
		await _run_enemy_turns()
	_check_battle_end()


func _run_enemy_turns() -> void:
	while _state == State.BATTLE and not _ctrl.battle_over():
		var cur := _ctrl.current_unit()
		if cur <= 0:
			break
		if _ctrl.unit_info(cur)["team"] == TEAM_PLAYER:
			break
		await get_tree().create_timer(0.3).timeout
		_ctrl.run_enemy_turn()
		_refresh_all()


func _on_tile_clicked(cell: Vector2i) -> void:
	if _state != State.BATTLE or _ctrl.battle_over():
		return
	if _targeting_ability >= 0:
		var res: Dictionary = _ctrl.try_cast(_selected_unit, _targeting_ability, 0, cell.x, cell.y)
		_targeting_ability = -1
		if not res["ok"]:
			_battle_ui.set_hint("无法施放：%s" % res["reject"])
		_refresh_all()
		return
	if _selected_unit <= 0:
		return
	var path: PackedVector2Array = _ctrl.path_to(_selected_unit, cell.x, cell.y)
	if path.size() < 2:
		return
	var res: Dictionary = _ctrl.try_move_to(_selected_unit, cell.x, cell.y)
	if res["ok"]:
		_board.animate_move(_selected_unit, path)
		_refresh_all()
	else:
		_battle_ui.set_hint("无法移动：%s" % res["reject"])


func _on_unit_clicked(unit_id: int) -> void:
	if _state != State.BATTLE or _ctrl.battle_over():
		return
	var info: Dictionary = _ctrl.unit_info(unit_id)
	if _targeting_ability >= 0:
		var res: Dictionary = _ctrl.try_cast(_selected_unit, _targeting_ability, unit_id, 0, 0)
		_targeting_ability = -1
		if not res["ok"]:
			_battle_ui.set_hint("无法施放：%s" % res["reject"])
		_refresh_all()
		return
	if info["team"] == TEAM_PLAYER and info["alive"]:
		_selected_unit = unit_id
		_battle_ui.set_hint("已选中 %s —— 点击格子移动 / 点击敌人攻击 / 使用下方技能" % info["name"])
		_refresh_all()
	elif info["team"] != TEAM_PLAYER and _selected_unit > 0:
		var res: Dictionary = _ctrl.try_attack(_selected_unit, unit_id)
		if res["ok"]:
			_board.animate_attack(_selected_unit, unit_id)
			_refresh_all()
		else:
			_battle_ui.set_hint("无法攻击：%s" % res["reject"])


func _on_ability_pressed(ability_id: int) -> void:
	if _selected_unit <= 0:
		_battle_ui.set_hint("请先选择己方猫")
		return
	_targeting_ability = ability_id
	var a: Dictionary = _ability_lookup.get(ability_id, {"name": "?"})
	_battle_ui.set_hint("选择【%s】的目标（点击单位或格子）" % a["name"])


func _on_end_turn() -> void:
	if _state != State.BATTLE:
		return
	_targeting_ability = -1
	_ctrl.end_turn()
	_selected_unit = -1
	_refresh_all()
	await _run_enemy_turns_if_needed()


func _check_battle_end() -> void:
	if _ctrl.battle_over():
		_state = State.RESULT
		_battle_ui.set_turn_info("", false)
		_battle_ui.clear_abilities()
		_battle_ui.show_result(_ctrl.winner() == TEAM_PLAYER)
		_board.clear_highlights()


func _on_back_to_prep() -> void:
	_enter_prep()
