extends Node
## ============================================================================
## Main — Demo 流程控制：准备 → 战斗 → 结算 → 返回准备（循环）
## 战斗逻辑全部在 C++ 核心（BattleController），本脚本只做展示与输入转发。
## 操作模式：NONE（默认，自动高亮移动范围）/ MOVE / ATTACK / CAST(技能)
## =============================================================================

const TEAM_PLAYER := 1

enum State { PREP, BATTLE, RESULT }
enum Mode { NONE, MOVE, ATTACK, CAST }

var _ctrl: BattleController  # GDExtension 战斗核心
var _board: Board
var _prep: PrepUi
var _battle_ui: BattleUi
var _state := State.PREP
var _mode := Mode.NONE
var _selected_unit := -1
var _cast_ability_id := -1
var _ability_lookup := {}   # id -> {name,cost,min_range,max_range,target,desc}
var _map_index := 0
var _terrain_names := {}
var _last_turn_unit := 0   # 回合变化检测（横幅提示）


func _ready() -> void:
	_fit_window_to_screen()
	_ctrl = BattleController.new()
	add_child(_ctrl)

	_board = Board.new()
	add_child(_board)

	_prep = PrepUi.new()
	add_child(_prep)
	_battle_ui = BattleUi.new()
	add_child(_battle_ui)

	_prep.start_requested.connect(_on_start_battle)
	_battle_ui.end_turn_pressed.connect(_on_end_turn)
	_battle_ui.move_pressed.connect(_on_move_pressed)
	_battle_ui.attack_pressed.connect(_on_attack_pressed)
	_battle_ui.ability_pressed.connect(_on_ability_pressed)
	_battle_ui.cancel_pressed.connect(_on_cancel)
	_battle_ui.back_to_prep.connect(_on_back_to_prep)
	_board.tile_clicked.connect(_on_tile_clicked)
	_board.unit_clicked.connect(_on_unit_clicked)
	_board.right_clicked.connect(_on_cancel)
	_board.hover_changed.connect(_battle_ui.set_hover)

	_enter_prep()
	if OS.get_environment("AUTOSTART") == "1":
		_autostart_debug.call_deferred()


## 窗口适配：不超过屏幕、居中，避免底部面板被裁掉
func _fit_window_to_screen() -> void:
	var screen: Vector2i = DisplayServer.screen_get_size()
	var scale: float = DisplayServer.screen_get_max_scale()
	var avail: Vector2i = Vector2i(int(screen.x / scale), int(screen.y / scale))
	var size := Vector2i(mini(1280, avail.x), mini(720, avail.y - 60))
	var pos := Vector2i(maxi(0, (screen.x - int(size.x * scale)) / 2),
			maxi(0, (screen.y - int(size.y * scale)) / 2))
	get_window().size = size
	get_window().position = pos


## 调试/截图用：自动开一场战斗（AUTOSTART=1 时），并把真实视口保存下来
func _autostart_debug() -> void:
	var configs: Array = []
	for i in 4:
		configs.append({
			"name": "猫%d" % i, "class": "mage",
			"stats": PackedInt32Array([6, 5, 5, 5, 5, 6, 5]),
			"passives": PackedInt32Array([104, 108]),
			"abilities": PackedInt32Array([201, 202, 205]),
		})
	_on_start_battle(0, configs)
	await get_tree().create_timer(2.5).timeout
	get_viewport().get_texture().get_image().save_png("res://../autostart_capture.png")
	print("[autostart] captured")


# ===========================================================================
# 流程
# ===========================================================================
func _enter_prep() -> void:
	_state = State.PREP
	_mode = Mode.NONE
	_selected_unit = -1
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
		var cat_id: int = _ctrl.add_cat(cfg["stats"], cfg["passives"], cfg["abilities"], pos.x, pos.y, cfg["name"])
		if cat_id > 0 and cfg.has("class"):
			_board.set_unit_sprite(cat_id, "class_%s_start" % cfg["class"])
	for e in map["enemies"]:
		_ctrl.add_enemy(e[0], e[1], e[2])

	_ability_lookup = {}
	for a in _ctrl.ability_pool():
		_ability_lookup[a["id"]] = a

	# 地形名表（悬停显示用）
	_terrain_names = {}
	var codes: PackedInt32Array = Maps.terrain_codes(map["rows"])
	for y in 10:
		for x in 10:
			var code: int = codes[y * 10 + x]
			_terrain_names[Vector2i(x, y)] = Board.TERRAIN_STYLE.get(code, {}).get("name", "?")

	_board.set_terrain_names(_terrain_names)
	_board.build_terrain(codes)
	_board.clear_units()
	_battle_ui.reset_log()
	_battle_ui.visible = true
	_battle_ui.dump_layout.call_deferred()
	_battle_ui.set_hint("轮到你的猫行动：蓝色格=可移动，红色环=可攻击的敌人，下方按钮施放技能")
	_ctrl.start_battle()
	print("[main] window=", get_window().size, " viewport=", get_viewport().get_visible_rect().size,
			" screen=", DisplayServer.screen_get_size(), " scale=", DisplayServer.screen_get_max_scale())
	_state = State.BATTLE
	_mode = Mode.NONE
	_selected_unit = -1
	_auto_select_current()
	_refresh_all()
	_run_enemy_turns_if_needed()


func _auto_select_current() -> void:
	# 玩家回合自动选中当前猫（让可操作项立即可见）
	var cur := _ctrl.current_unit()
	if cur > 0:
		var info: Dictionary = _ctrl.unit_info(cur)
		if info.get("team", 0) == TEAM_PLAYER and info.get("alive", false):
			_selected_unit = cur
			_mode = Mode.NONE


func _refresh_all() -> void:
	_board.refresh_units(_ctrl.all_units())
	_battle_ui.append_log_entries(_ctrl.log_entries())
	_battle_ui.set_round(_ctrl.round_index())

	var cur := _ctrl.current_unit()
	var player_turn := false
	if cur > 0:
		var info := _ctrl.unit_info(cur)
		player_turn = info["team"] == TEAM_PLAYER
		_battle_ui.set_turn_info("当前回合：%s" % info["name"], player_turn)
		_board.set_current_unit(cur)
		_battle_ui.set_abilities(_abilities_of(cur))
		# 回合切换横幅（明显提示轮到谁）
		if cur != _last_turn_unit:
			_battle_ui.show_turn_banner(info["name"], player_turn)
			_last_turn_unit = cur
	else:
		_battle_ui.set_turn_info("", false)
		_battle_ui.clear_abilities()
		_board.set_current_unit(-1)
		_last_turn_unit = 0

	_update_selection_ui()
	_update_highlights()


func _update_selection_ui() -> void:
	_board.set_selected_unit(_selected_unit)
	if _selected_unit > 0:
		var info: Dictionary = _ctrl.unit_info(_selected_unit)
		var cur := _ctrl.current_unit()
		var can_act: bool = _selected_unit == cur and info.get("team", 0) == TEAM_PLAYER \
				and info.get("alive", false)
		var abilities: Array = _abilities_of(_selected_unit) if can_act else []
		_battle_ui.set_selected_card(info, abilities, can_act)
	else:
		_battle_ui.set_selected_card({}, [], false)


func _update_highlights() -> void:
	if _selected_unit <= 0:
		_board.clear_highlights()
		return
	var info: Dictionary = _ctrl.unit_info(_selected_unit)
	if not info.get("alive", false):
		_board.clear_highlights()
		return
	match _mode:
		Mode.CAST:
			var a: Dictionary = _ability_lookup.get(_cast_ability_id, {})
			var cells: Array = _cells_in_range(info["x"], info["y"], a.get("min_range", 1), a.get("max_range", 1))
			var valid: Array = _cast_target_tiles(a)
			_board.show_cast_range(cells, valid)
		Mode.ATTACK:
			_board.clear_highlights()
			_board.show_attack_highlights(_attackable_enemies())
		_:
			# 默认：显示移动范围（蓝色）+ 可攻击敌人（红色环）
			_board.show_move_highlights(_ctrl.reachable_tiles(_selected_unit))
			_board.show_attack_highlights(_attackable_enemies())


func _attackable_enemies() -> Array:
	var out: Array = []
	if _selected_unit <= 0:
		return out
	var info: Dictionary = _ctrl.unit_info(_selected_unit)
	if info.get("action_points", [0])[1] < 1:
		return out
	for u in _ctrl.all_units():
		if u["team"] == TEAM_PLAYER or not u["on_board"]:
			continue
		if u.get("alive", false) or u.get("downed", false):
			if absi(info["x"] - u["x"]) + absi(info["y"] - u["y"]) == 1:
				out.append(u["id"])
	return out


func _cells_in_range(cx: int, cy: int, min_r: int, max_r: int) -> Array:
	var cells: Array = []
	for y in 10:
		for x in 10:
			var d := absi(cx - x) + absi(cy - y)
			if d >= min_r and d <= max_r:
				cells.append(Vector2i(x, y))
	return cells


func _cast_target_tiles(a: Dictionary) -> Array:
	var cells: Array = []
	var info: Dictionary = _ctrl.unit_info(_selected_unit)
	var target_type: String = a.get("target", "enemy")
	for u in _ctrl.all_units():
		if not u["on_board"]:
			continue
		var d := absi(info["x"] - u["x"]) + absi(info["y"] - u["y"])
		if d < a.get("min_range", 1) or d > a.get("max_range", 1):
			continue
		var ok := false
		match target_type:
			"enemy":
				ok = u["team"] != TEAM_PLAYER and (u.get("alive", false) or u.get("downed", false))
			"ally":
				ok = u["team"] == TEAM_PLAYER and u.get("alive", false)
			_:
				ok = u.get("alive", false)
		if ok:
			cells.append(Vector2i(u["x"], u["y"]))
	return cells


# ===========================================================================
# 敌方回合自动推进
# ===========================================================================
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
		if _state == State.BATTLE:
			_auto_select_current()
			_refresh_all()


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


func _check_battle_end() -> void:
	if _ctrl.battle_over():
		_state = State.RESULT
		_mode = Mode.NONE
		_selected_unit = -1
		_battle_ui.set_turn_info("", false)
		_battle_ui.clear_abilities()
		_battle_ui.set_selected_card({}, [])
		_battle_ui.show_result(_ctrl.winner() == TEAM_PLAYER)
		_board.clear_highlights()


# ===========================================================================
# 操作模式与输入
# ===========================================================================
func _on_move_pressed() -> void:
	if _selected_unit <= 0:
		return
	_mode = Mode.MOVE
	_battle_ui.set_hint("点击蓝色格子移动（右键/Esc 取消）")
	_update_highlights()


func _on_attack_pressed() -> void:
	if _selected_unit <= 0:
		return
	_mode = Mode.ATTACK
	_battle_ui.set_hint("点击红色高亮的敌人进行攻击（右键取消）")
	_update_highlights()


func _on_ability_pressed(ability_id: int) -> void:
	if _selected_unit <= 0:
		_battle_ui.set_hint("请先选择己方猫")
		return
	_mode = Mode.CAST
	_cast_ability_id = ability_id
	var a: Dictionary = _ability_lookup.get(ability_id, {"name": "?"})
	_battle_ui.set_hint("选择【%s】的目标：橙色=射程范围，深红=可命中的目标（右键取消）" % a["name"])
	_update_highlights()


func _on_cancel() -> void:
	if _state != State.BATTLE:
		return
	_mode = Mode.NONE
	_cast_ability_id = -1
	_battle_ui.set_hint("已取消。蓝色格=可移动，红色环=可攻击的敌人")
	_update_highlights()


func _on_tile_clicked(cell: Vector2i) -> void:
	if _state != State.BATTLE or _ctrl.battle_over():
		return
	if _selected_unit <= 0:
		return
	match _mode:
		Mode.MOVE, Mode.NONE:
			var path: PackedVector2Array = _ctrl.path_to(_selected_unit, cell.x, cell.y)
			if path.size() < 2:
				return
			var res: Dictionary = _ctrl.try_move_to(_selected_unit, cell.x, cell.y)
			if res["ok"]:
				_board.animate_move(_selected_unit, path)
				_mode = Mode.NONE
				_refresh_all()
			else:
				_battle_ui.set_hint("无法移动：%s" % res["reject"])
		Mode.ATTACK:
			_battle_ui.set_hint("点击敌人攻击（红色高亮），或右键取消")
		Mode.CAST:
			var res: Dictionary = _ctrl.try_cast(_selected_unit, _cast_ability_id, 0, cell.x, cell.y)
			if not res["ok"]:
				_battle_ui.set_hint("无法施放：%s" % res["reject"])
			else:
				_mode = Mode.NONE
				_refresh_all()


func _on_unit_clicked(unit_id: int) -> void:
	if _state != State.BATTLE or _ctrl.battle_over():
		return
	var info: Dictionary = _ctrl.unit_info(unit_id)
	match _mode:
		Mode.CAST:
			var res: Dictionary = _ctrl.try_cast(_selected_unit, _cast_ability_id, unit_id, 0, 0)
			if not res["ok"]:
				_battle_ui.set_hint("无法施放：%s" % res["reject"])
			else:
				_mode = Mode.NONE
				_refresh_all()
			return
		Mode.ATTACK:
			if info["team"] != TEAM_PLAYER and _selected_unit > 0:
				var res: Dictionary = _ctrl.try_attack(_selected_unit, unit_id)
				if res["ok"]:
					_board.animate_attack(_selected_unit, unit_id)
					_mode = Mode.NONE
					_refresh_all()
				else:
					_battle_ui.set_hint("无法攻击：%s" % res["reject"])
			return
		_:
			pass
	# NONE 模式：己方猫 → 选中；敌方 → 查看信息或直接攻击
	if info["team"] == TEAM_PLAYER:
		if info.get("alive", false):
			_selected_unit = unit_id
			_mode = Mode.NONE
			_battle_ui.set_hint("已选中 %s：蓝格移动 / 点红色环敌人攻击 / 下方按钮施法" % info["name"])
			_refresh_all()
	else:
		var cur := _ctrl.current_unit()
		if cur > 0 and _ctrl.unit_info(cur)["team"] == TEAM_PLAYER and _selected_unit <= 0:
			_selected_unit = cur  # 玩家回合点敌人：先选中当前猫
		if _selected_unit > 0:
			var sel: Dictionary = _ctrl.unit_info(_selected_unit)
			if absi(sel["x"] - info["x"]) + absi(sel["y"] - info["y"]) == 1:
				var res: Dictionary = _ctrl.try_attack(_selected_unit, unit_id)
				if res["ok"]:
					_board.animate_attack(_selected_unit, unit_id)
					_refresh_all()
					return
		# 否则仅展示敌方信息卡
		_board.set_selected_unit(-1)
		_battle_ui.set_selected_card(info, [])


func _on_end_turn() -> void:
	if _state != State.BATTLE:
		return
	_mode = Mode.NONE
	_cast_ability_id = -1
	_ctrl.end_turn()
	_selected_unit = -1
	_refresh_all()
	await _run_enemy_turns_if_needed()


func _on_back_to_prep() -> void:
	_enter_prep()


func _abilities_of(unit_id: int) -> Array:
	var out: Array = []
	for aid in _ctrl.unit_info(unit_id)["abilities"]:
		out.append(_ability_lookup.get(aid, {"id": aid, "name": "?", "cost": 0, "min_range": 1, "max_range": 1, "desc": ""}))
	return out
