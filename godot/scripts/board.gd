class_name Board
extends Node3D
## ============================================================================
## 3D 棋盘：地形渲染 + 单位显示 + 多层高亮（移动/攻击/施法/悬停/选中/当前回合）
## 纯展示层：只读取战斗核心返回的状态，不修改任何战斗数据。
## =============================================================================

signal tile_clicked(cell: Vector2i)
signal unit_clicked(unit_id: int)
signal right_clicked
signal hover_changed(text: String)

const TILE := 2.0                 # 格距（世界单位）
const CAT_SPRITE_HEIGHT := 2.4      # 玩家猫精灵目标高度（米，≈1.2 格）
const ENEMY_SPRITE_HEIGHT := 1.8    # 敌人精灵目标高度（米，≈0.9 格）
const STATUS_ICON_SIZE := 0.55      # 状态图标目标尺寸（米）
const GRID_W := 10
const GRID_H := 10
const CAM_DIST := 26.0
const CAM_PITCH := 45.0

const TERRAIN_STYLE := {
	1:  {"color": Color(0.62, 0.60, 0.55), "h": 0.05, "name": "空地"},
	2:  {"color": Color(0.55, 0.45, 0.33), "h": 0.05, "name": "道路（省步数）"},
	3:  {"color": Color(0.33, 0.55, 0.29), "h": 0.05, "name": "草丛"},
	4:  {"color": Color(0.22, 0.45, 0.20), "h": 0.05, "name": "高草丛", "bush": true},
	5:  {"color": Color(0.42, 0.62, 0.30), "h": 0.05, "name": "花丛（回合末+2回复）", "flower": true},
	6:  {"color": Color(0.36, 0.58, 0.26), "h": 0.05, "name": "高花丛（+3回复）", "bush": true, "flower": true},
	7:  {"color": Color(0.28, 0.42, 0.20), "h": 0.05, "name": "荆棘（进入-2血）", "bush": true},
	8:  {"color": Color(0.22, 0.35, 0.16), "h": 0.05, "name": "厚荆棘（进入-4血）", "bush": true},
	9:  {"color": Color(0.18, 0.42, 0.72), "h": -0.18, "name": "水域（施加湿润）"},
	10: {"color": Color(0.15, 0.38, 0.75), "h": -0.18, "name": "激流（施加湿润）"},
	11: {"color": Color(0.28, 0.35, 0.28), "h": -0.10, "name": "苔沼（-1血+湿润）"},
	12: {"color": Color(0.85, 0.30, 0.08), "h": -0.05, "name": "熔岩（进入-5血）", "glow": true},
	13: {"color": Color(0.08, 0.08, 0.10), "h": -0.55, "name": "深坑（不可通行）"},
	14: {"color": Color(0.45, 0.45, 0.48), "h": 1.4, "name": "墙（不可通行/挡视线）"},
}

const ENEMY_STYLE := {  # def_id → 颜色/形状/名字
	2: {"color": Color(0.62, 0.40, 0.35), "shape": "cap", "name": "老鼠"},
	3: {"color": Color(0.50, 0.42, 0.62), "shape": "sphere", "name": "蝙蝠"},
	4: {"color": Color(0.55, 0.30, 0.28), "shape": "cap", "name": "暴徒"},
	5: {"color": Color(0.80, 0.42, 0.15), "shape": "cone", "name": "火妖"},
	6: {"color": Color(0.40, 0.58, 0.30), "shape": "cap", "name": "喷毒者"},
}

const STATUS_STYLE := {
	1: {"text": "全", "color": Color(1.0, 0.95, 0.4)},   # 全属性提升
	2: {"text": "力", "color": Color(0.95, 0.5, 0.3)},   # 力量提升
	3: {"text": "火", "color": Color(1.0, 0.4, 0.2)},
	4: {"text": "毒", "color": Color(0.3, 0.9, 0.3)},
	5: {"text": "血", "color": Color(0.8, 0.1, 0.1)},
	6: {"text": "晕", "color": Color(0.9, 0.8, 0.2)},
	7: {"text": "弱", "color": Color(0.6, 0.5, 0.7)},
	8: {"text": "湿", "color": Color(0.3, 0.6, 1.0)},
}

# def_id → 静态精灵兜底（wiki level-editor PNG；动画素材缺失时用）
const SPRITE_MAP := {
	2: "beaniesrat_level_editor", 3: "bat_level_editor",
	4: "bigdemon_level_editor", 5: "bgcandle_level_editor",
	6: "belcher_level_editor",
}
# 敌人动画（ENEMY_<名>_{Idle,Attack,Death}.gif → 组合帧）
const ENEMY_ANIM_KEYS := {
	2: "lil__rat", 3: "bat", 4: "lumpy", 5: "cat_caller", 6: "glass_spitter",
}

# 高亮层：cell -> 覆盖物节点
var _board_root: Node3D
var _camera: Camera3D
var _tiles: Dictionary = {}          # Vector2i -> MeshInstance3D
var _units: Dictionary = {}          # unit_id -> Node3D
var _unit_infos: Dictionary = {}     # unit_id -> Dictionary（最近一次刷新数据）
var _overlays: Array = []            # 所有高亮覆盖物
var _selected_unit := -1
var _current_unit := -1
var _turn_marker: Sprite3D
var _turn_tween: Tween
var _turn_ring: MeshInstance3D
var _turn_ring_tween: Tween
var _sel_ring: MeshInstance3D
var _hover_overlay: MeshInstance3D
var _hover_cell := Vector2i(-1, -1)
var _rot := 0                        # 0..3（Q/E 90° 步进）
var _input_locked := false
var _sprite_frames: Dictionary = {}  # 精灵名 -> SpriteFrames（wiki 素材，可选）
var _unit_sprite_keys: Dictionary = {}  # unit_id -> 精灵名（玩家猫职业，main 指定）


func _ready() -> void:
	_load_sprite_manifest()
	_board_root = Node3D.new()
	_board_root.name = "BoardRoot"
	add_child(_board_root)
	_camera = Camera3D.new()
	_camera.projection = Camera3D.PROJECTION_ORTHOGONAL
	_camera.size = 22.0
	_camera.fov = 45.0
	add_child(_camera)
	_apply_camera()
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-55, -30, 0)
	add_child(light)
	var world_env := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.12, 0.14, 0.18)
	world_env.environment = env
	add_child(world_env)
	# 悬停/选中/当前回合标记
	_hover_overlay = _make_overlay(Color(1, 1, 1, 0.55), 0.05)
	_hover_overlay.visible = false
	_board_root.add_child(_hover_overlay)
	_turn_marker = Sprite3D.new()
	_turn_marker.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_turn_marker.no_depth_test = true
	_turn_marker.visible = false
	_board_root.add_child(_turn_marker)
	_turn_ring = _make_ring(Color(1.0, 0.85, 0.2), 0.82)
	_turn_ring.visible = false
	_board_root.add_child(_turn_ring)
	_sel_ring = _make_ring(Color(0.25, 0.9, 1.0), 0.62)
	_sel_ring.visible = false
	_board_root.add_child(_sel_ring)


func build_terrain(codes: PackedInt32Array) -> void:
	for child in _tiles.values():
		child.queue_free()
	_tiles.clear()
	for y in GRID_H:
		for x in GRID_W:
			var code: int = codes[y * GRID_W + x]
			_tiles[Vector2i(x, y)] = _make_tile(Vector2i(x, y), code)


func clear_units() -> void:
	for n in _units.values():
		n.queue_free()
	_units.clear()
	_unit_infos.clear()
	_selected_unit = -1
	_current_unit = -1
	_turn_marker.visible = false
	_turn_ring.visible = false
	_sel_ring.visible = false


func refresh_units(units: Array) -> void:
	var seen := {}
	for data in units:
		var uid: int = data["id"]
		seen[uid] = true
		_unit_infos[uid] = data
		var node: Node3D = _units.get(uid)
		if node == null:
			node = _make_unit(data)
			_units[uid] = node
			_board_root.add_child(node)
		_update_unit(node, data)
	for uid in _units.keys():
		if not seen.has(uid):
			_units[uid].queue_free()
			_units.erase(uid)
			_unit_infos.erase(uid)
	_update_markers()


func set_current_unit(uid: int) -> void:
	_current_unit = uid
	_update_markers()


func set_selected_unit(uid: int) -> void:
	_selected_unit = uid
	_update_markers()


func selected_unit() -> int:
	return _selected_unit




func set_terrain_names(names: Dictionary) -> void:
	_terrain_names = names


## 指定单位的精灵（玩家猫职业，如 "class_mage_start"）；空字符串清除
func set_unit_sprite(unit_id: int, sprite_key: String) -> void:
	if sprite_key.is_empty():
		_unit_sprite_keys.erase(unit_id)
	else:
		_unit_sprite_keys[unit_id] = sprite_key
	if _units.has(unit_id):
		_units[unit_id].queue_free()
		_units.erase(unit_id)   # 下次 refresh 用新精灵重建


var _terrain_names: Dictionary = {}


# ---------------------------------------------------------------------------
# 高亮层 API（每次调用清空重建；确定性顺序：移动→施法→攻击）
# ---------------------------------------------------------------------------
func show_move_highlights(cells: Array) -> void:
	_clear_overlays()
	for c in cells:
		_add_overlay(Vector2i(c.x, c.y), Color(0.2, 0.55, 1.0, 0.45), 0.05, "move")


func show_cast_range(cells: Array, valid: Array) -> void:
	_clear_overlays()
	for c in cells:
		_add_overlay(Vector2i(c.x, c.y), Color(0.95, 0.6, 0.1, 0.4), 0.05, "cast")
	for c in valid:
		_add_overlay(Vector2i(c.x, c.y), Color(1.0, 0.4, 0.2, 0.75), 0.05, "castv")


func show_attack_highlights(unit_ids: Array) -> void:
	for uid in unit_ids:
		var node: Node3D = _units.get(int(uid))
		if node:
			_add_overlay_at(node.position, Color(1.0, 0.2, 0.15, 0.7), 0.05, "attack")


func clear_highlights() -> void:
	_clear_overlays()


func set_input_locked(locked: bool) -> void:
	_input_locked = locked


func animate_move(unit_id: int, path: Array) -> void:
	var node: Node3D = _units.get(unit_id)
	if node == null or path.size() < 2:
		return
	set_input_locked(true)
	var tween := create_tween()
	for i in range(1, path.size()):
		var target := grid_to_world(Vector2i(path[i].x, path[i].y))
		tween.tween_property(node, "position", target, 0.14)
	tween.finished.connect(func(): set_input_locked(false))


func animate_attack(unit_id: int, target_id: int) -> void:
	var node: Node3D = _units.get(unit_id)
	var tgt: Node3D = _units.get(target_id)
	if node == null or tgt == null:
		return
	set_input_locked(true)
	var spr: AnimatedSprite3D = node.get_node_or_null("sprite")
	var origin := node.position
	var dir := (tgt.position - origin).normalized()
	if spr and spr.sprite_frames.has_animation("attack"):
		spr.play("attack")
	var tween := create_tween()
	tween.tween_property(node, "position", origin + dir * 0.5, 0.08)
	tween.tween_property(node, "position", origin, 0.12)
	tween.tween_callback(func():
		set_input_locked(false)
		if spr and spr.sprite_frames.has_animation("idle"):
			spr.play("idle"))


func grid_to_world(cell: Vector2i) -> Vector3:
	return Vector3((cell.x - GRID_W / 2.0 + 0.5) * TILE, 0, (cell.y - GRID_H / 2.0 + 0.5) * TILE)


func world_to_grid(pos: Vector3) -> Vector2i:
	var c := Vector2i(roundi(pos.x / TILE + GRID_W / 2.0 - 0.5),
			roundi(pos.z / TILE + GRID_H / 2.0 - 0.5))
	if c.x < 0 or c.x >= GRID_W or c.y < 0 or c.y >= GRID_H:
		return Vector2i(-1, -1)
	return c


# ---------------------------------------------------------------------------
# 输入
# ---------------------------------------------------------------------------
func _unhandled_input(event: InputEvent) -> void:
	if event.is_action_pressed("rotate_left"):
		_rot = (_rot + 3) % 4
		_apply_camera()
	elif event.is_action_pressed("rotate_right"):
		_rot = (_rot + 1) % 4
		_apply_camera()
	elif event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_camera.size = clampf(_camera.size * 0.9, 8.0, 40.0)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_camera.size = clampf(_camera.size * 1.1, 8.0, 40.0)
		elif event.button_index == MOUSE_BUTTON_LEFT and not _input_locked:
			_handle_click(event.position)
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			right_clicked.emit()
	elif event is InputEventMouseMotion and not _input_locked:
		_update_hover(event.position)


func _update_hover(screen_pos: Vector2) -> void:
	var from := _camera.project_ray_origin(screen_pos)
	var dir := _camera.project_ray_normal(screen_pos)
	# 单位悬停优先
	var best_id := -1
	var best_t := INF
	for uid in _units:
		var node: Node3D = _units[uid]
		var t := _ray_sphere(from, dir, node.position + Vector3(0, 0.6, 0), 0.7)
		if t < best_t:
			best_t = t
			best_id = uid
	if best_id >= 0 and best_t < 200.0:
		_hover_cell = Vector2i(-1, -1)
		_hover_overlay.visible = false
		var info: Dictionary = _unit_infos.get(best_id, {})
		var hp := str(info.get("hp", "?"))
		var mhp := str(info.get("max_hp", "?"))
		var statuses: Array = info.get("statuses", [])
		var st := ""
		for s in statuses:
			st += " %s×%d" % [s["name"], s["stacks"]]
		hover_changed.emit("%s  HP %s/%s%s" % [info.get("name", "?"), hp, mhp, st])
		return
	if dir.y < -0.001:
		var t := -from.y / dir.y
		var cell := world_to_grid(from + dir * t)
		if cell.x >= 0:
			if cell != _hover_cell:
				_hover_cell = cell
				_hover_overlay.visible = true
				_hover_overlay.position = grid_to_world(cell) + Vector3(0, 0.03, 0)
			hover_changed.emit("格子 (%d,%d)  %s" % [cell.x, cell.y, _terrain_names.get(cell, "?")])
			return
	_hover_cell = Vector2i(-1, -1)
	_hover_overlay.visible = false


func _handle_click(screen_pos: Vector2) -> void:
	var from := _camera.project_ray_origin(screen_pos)
	var dir := _camera.project_ray_normal(screen_pos)
	var best_id := -1
	var best_t := INF
	for uid in _units:
		var node: Node3D = _units[uid]
		var t := _ray_sphere(from, dir, node.position + Vector3(0, 0.6, 0), 0.7)
		if t < best_t:
			best_t = t
			best_id = uid
	if best_id >= 0 and best_t < 200.0:
		unit_clicked.emit(best_id)
		return
	if dir.y < -0.001:
		var t := -from.y / dir.y
		var cell := world_to_grid(from + dir * t)
		if cell.x >= 0:
			tile_clicked.emit(cell)


func _apply_camera() -> void:
	var center := Vector3(0, 0, 0)
	var yaw := -45.0 - _rot * 90.0
	var rad_yaw := deg_to_rad(yaw)
	var rad_pitch := deg_to_rad(CAM_PITCH)
	var offset := Vector3(
		sin(rad_yaw) * cos(rad_pitch) * CAM_DIST,
		sin(rad_pitch) * CAM_DIST,
		cos(rad_yaw) * cos(rad_pitch) * CAM_DIST)
	_camera.position = center + offset
	_camera.look_at(center, Vector3.UP)


func _ray_sphere(origin: Vector3, dir: Vector3, center: Vector3, radius: float) -> float:
	var oc := origin - center
	var b := oc.dot(dir)
	var c := oc.dot(oc) - radius * radius
	var disc := b * b - c
	if disc < 0.0:
		return INF
	var t := -b - sqrt(disc)
	return t if t > 0.0 else INF


# ---------------------------------------------------------------------------
# 高亮覆盖物
# ---------------------------------------------------------------------------
func _make_overlay(color: Color, y_off: float) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(TILE * 0.94, 0.08, TILE * 0.94)
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	box.material = mat
	mi.mesh = box
	mi.position.y = y_off
	return mi


func _add_overlay(cell: Vector2i, color: Color, y_off: float, tag: String) -> void:
	var ov := _make_overlay(color, y_off)
	ov.position = grid_to_world(cell) + Vector3(0, y_off, 0)
	ov.set_meta("tag", tag)
	_board_root.add_child(ov)
	_overlays.append(ov)


func _add_overlay_at(pos: Vector3, color: Color, y_off: float, tag: String) -> void:
	var ov := _make_overlay(color, y_off)
	ov.position = pos + Vector3(0, y_off, 0)
	ov.set_meta("tag", tag)
	_board_root.add_child(ov)
	_overlays.append(ov)


func _clear_overlays() -> void:
	for ov in _overlays:
		ov.queue_free()
	_overlays.clear()


func _make_cone(color: Color) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var cm := CylinderMesh.new()
	cm.top_radius = 0.05
	cm.bottom_radius = 0.28
	cm.height = 0.6
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.emission_enabled = true
	mat.emission = color
	mat.emission_energy_multiplier = 0.8
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	cm.material = mat
	mi.mesh = cm
	return mi


func _make_ring(color: Color, radius: float) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var tm := TorusMesh.new()
	tm.inner_radius = radius
	tm.outer_radius = radius + 0.14
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.emission_enabled = true
	mat.emission = color
	mat.emission_energy_multiplier = 1.0
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	tm.material = mat
	mi.mesh = tm
	mi.rotation_degrees = Vector3(90, 0, 0)
	return mi


func _update_markers() -> void:
	# 选中环
	if _selected_unit > 0 and _units.has(_selected_unit):
		_sel_ring.visible = true
		_sel_ring.position = _units[_selected_unit].position + Vector3(0, 0.05, 0)
	else:
		_sel_ring.visible = false
	# 当前回合：原版回合图标（浮动）+ 脉冲地环 —— 明显提示轮到谁
	if _current_unit > 0 and _units.has(_current_unit):
		var info: Dictionary = _unit_infos.get(_current_unit, {})
		var is_player: bool = info.get("team", 1) == 1
		var icon := load("res://assets/icons/turn_ally.svg" if is_player else "res://assets/icons/turn_enemy.svg")
		if icon:
			_turn_marker.texture = icon
			_turn_marker.pixel_size = _pixel_size_for(icon, 1.0)
		var base: Vector3 = _units[_current_unit].position + Vector3(0, 2.3, 0)
		_turn_marker.visible = true
		_turn_marker.position = base
		if _turn_tween and _turn_tween.is_valid():
			_turn_tween.kill()
		_turn_tween = create_tween().set_loops()
		_turn_tween.tween_property(_turn_marker, "position:y", base.y + 0.3, 0.45)
		_turn_tween.tween_property(_turn_marker, "position:y", base.y, 0.45)
		# 脉冲地环
		_turn_ring.visible = true
		_turn_ring.position = _units[_current_unit].position + Vector3(0, 0.06, 0)
		if _turn_ring_tween and _turn_ring_tween.is_valid():
			_turn_ring_tween.kill()
		_turn_ring_tween = create_tween().set_loops()
		_turn_ring_tween.tween_property(_turn_ring, "scale", Vector3(1.18, 1, 1.18), 0.45)
		_turn_ring_tween.tween_property(_turn_ring, "scale", Vector3(1.0, 1, 1.0), 0.45)
	else:
		_turn_marker.visible = false
		_turn_ring.visible = false


# ---------------------------------------------------------------------------
# 地形与单位构建
# ---------------------------------------------------------------------------
func _make_tile(cell: Vector2i, code: int) -> MeshInstance3D:
	var style: Dictionary = TERRAIN_STYLE.get(code, TERRAIN_STYLE[1])
	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	var h: float = style["h"]
	box.size = Vector3(TILE * 0.98, absf(h) if h > 0.3 else 0.2, TILE * 0.98)
	if h < 0.0:
		box.size.y = 0.2
	var mat := StandardMaterial3D.new()
	mat.albedo_color = style["color"]
	if style.get("glow", false):
		mat.emission_enabled = true
		mat.emission = Color(1.0, 0.3, 0.05)
		mat.emission_energy_multiplier = 1.6
	box.material = mat
	mi.mesh = box
	mi.position = grid_to_world(cell) + Vector3(0, minf(h, 0.0), 0)
	mi.name = "tile_%d_%d" % [cell.x, cell.y]
	_board_root.add_child(mi)
	# wiki 地形图标：贴片覆盖在格子上（像素风；缺失时用纯色）
	var tex := load("res://assets/icons/tile_%d.svg" % code)
	if tex:
		var quad := MeshInstance3D.new()
		var qm := QuadMesh.new()
		qm.size = Vector2(TILE * 0.98, TILE * 0.98)
		var qmat := StandardMaterial3D.new()
		qmat.albedo_texture = tex
		qmat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
		qmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		qmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		qm.material = qmat
		quad.mesh = qm
		quad.position = mi.position + Vector3(0, 0.05, 0)
		quad.rotation_degrees = Vector3(-90, 0, 0)
		quad.name = "tile_icon_%d_%d" % [cell.x, cell.y]
		_board_root.add_child(quad)
	if style.get("bush", false) or style.get("flower", false):
		var deco := MeshInstance3D.new()
		var dbox := BoxMesh.new()
		dbox.size = Vector3(TILE * 0.4, 0.35, TILE * 0.4)
		var dmat := StandardMaterial3D.new()
		dmat.albedo_color = Color(1.0, 0.75, 0.85) if style.get("flower", false) else Color(0.3, 0.6, 0.25)
		dbox.material = dmat
		deco.mesh = dbox
		deco.position = mi.position + Vector3(0, 0.22, 0)
		_board_root.add_child(deco)
	return mi


func _load_sprite_manifest() -> void:
	# wiki 精灵（若存在）：res://assets/sprites/manifest.json
	var m := load("res://assets/sprites/manifest.json")
	if m == null:
		return
	var data: Dictionary = m.data
	for key in data:
		var entry: Dictionary = data[key]
		var frames: Array = entry.get("frames", [])
		if frames.is_empty():
			continue
		var sf := SpriteFrames.new()
		for f in frames:
			var tex := load("res://assets/sprites/%s" % f)
			if tex:
				sf.add_frame("default", tex)
		if sf.get_frame_count("default") > 0:
			sf.set_animation_speed("default", float(entry.get("fps", 8)))
			_sprite_frames[key] = sf


## 帧纹理 → 像素尺寸（使不同素材统一到目标世界高度）
func _pixel_size_for(tex: Texture2D, desired_height: float) -> float:
	var h := tex.get_size().y
	return desired_height / maxf(h, 1.0)


func _enemy_frames(def_id: int) -> SpriteFrames:
	# ENEMY_<名>_{Idle,Attack,Death}.gif → 组合成一个 SpriteFrames（多动画）
	var base: String = ENEMY_ANIM_KEYS.get(def_id, "")
	if base.is_empty():
		return null
	var sf := SpriteFrames.new()
	var any_frame := false
	for anim in ["idle", "attack", "death"]:
		var key := "enemy_%s_%s" % [base, anim]
		if _sprite_frames.has(key):
			sf.add_animation(anim)
			for i in _sprite_frames[key].get_frame_count("default"):
				sf.add_frame(anim, _sprite_frames[key].get_frame_texture("default", i))
			sf.set_animation_speed(anim, _sprite_frames[key].get_animation_speed("default"))
			sf.set_animation_loop(anim, anim != "death")
			any_frame = true
	if not any_frame:
		return null
	return sf


func _make_unit(data: Dictionary) -> Node3D:
	var root := Node3D.new()
	var is_player: bool = data["team"] == 1  # Team::PLAYER
	var style: Dictionary
	if is_player:
		style = {"color": Color(0.95, 0.65, 0.25), "shape": "cap", "name": "猫"}
	else:
		style = ENEMY_STYLE.get(data.get("def", 0), ENEMY_STYLE[2])
	root.set_meta("team", data["team"])
	# wiki 精灵优先（billboard 动画），无素材回退几何体
	var spr_frames: SpriteFrames = null
	var sprite_key: String = _unit_sprite_keys.get(data.get("id", 0), "")
	if sprite_key.is_empty() and not is_player:
		spr_frames = _enemy_frames(data.get("def", 0))
	if spr_frames == null and not sprite_key.is_empty() and _sprite_frames.has(sprite_key):
		spr_frames = _sprite_frames[sprite_key]
	if spr_frames == null and not is_player:
		var fallback: String = SPRITE_MAP.get(data.get("def", 0), "")
		spr_frames = _sprite_frames.get(fallback)
	if spr_frames != null:
		var spr := AnimatedSprite3D.new()
		spr.sprite_frames = spr_frames
		spr.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		var ref_anim := "idle" if spr_frames.has_animation("idle") else "default"
		var ref_tex: Texture2D = spr_frames.get_frame_texture(ref_anim, 0)
		var desired := CAT_SPRITE_HEIGHT if is_player else ENEMY_SPRITE_HEIGHT
		spr.pixel_size = _pixel_size_for(ref_tex, desired)
		spr.position.y = desired / 2.0   # 脚底贴格面
		spr.name = "sprite"
		spr.play(ref_anim)
		root.add_child(spr)
		root.set_meta("has_sprite", true)
	var body := MeshInstance3D.new()
	var mesh: PrimitiveMesh
	match style["shape"]:
		"cone":
			var cm := CylinderMesh.new()
			cm.top_radius = 0.05
			cm.bottom_radius = 0.42
			cm.height = 1.1
			mesh = cm
		"sphere":
			var sm := SphereMesh.new()
			sm.radius = 0.42
			sm.height = 0.9
			mesh = sm
		_:
			var cpm := CapsuleMesh.new()
			cpm.radius = 0.36
			cpm.height = 1.1
			mesh = cpm
	var mat := StandardMaterial3D.new()
	mat.albedo_color = style["color"]
	mesh.material = mat
	body.mesh = mesh
	body.position.y = 0.55
	root.add_child(body)
	# 有精灵时隐藏几何体（精灵作为形象；血条/徽章仍保留）
	if root.get_meta("has_sprite", false):
		body.visible = false

	# 血条（红底 + 彩条 + 数值标签）
	var bg := MeshInstance3D.new()
	var bgb := BoxMesh.new()
	bgb.size = Vector3(1.0, 0.1, 0.08)
	var bgm := StandardMaterial3D.new()
	bgm.albedo_color = Color(0.25, 0.1, 0.1)
	bgm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	bgb.material = bgm
	bg.mesh = bgb
	bg.position = Vector3(0, 1.35, 0)
	root.add_child(bg)
	var fg := MeshInstance3D.new()
	var fgb := BoxMesh.new()
	fgb.size = Vector3(1.0, 0.1, 0.1)
	var fgm := StandardMaterial3D.new()
	fgm.albedo_color = Color(0.2, 0.85, 0.3)
	fgm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	fgb.material = fgm
	fg.mesh = fgb
	fg.position = Vector3(0, 1.36, 0)
	fg.name = "hp_fg"
	root.add_child(fg)
	var hp_label := Label3D.new()
	hp_label.name = "hp_label"
	hp_label.font_size = 30
	hp_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	hp_label.no_depth_test = true
	hp_label.position = Vector3(0, 1.62, 0)
	root.add_child(hp_label)

	var badges := Node3D.new()
	badges.name = "badges"
	root.add_child(badges)
	return root


func _update_unit(node: Node3D, data: Dictionary) -> void:
	var cell := Vector2i(data["x"], data["y"])
	var target := grid_to_world(cell) + Vector3(0, 0.02, 0)
	node.position = target
	node.visible = data.get("on_board", false)
	var hp: float = maxf(float(data["hp"]), 0.0)
	var max_hp: float = maxf(float(data["max_hp"]), 1.0)
	var fg: MeshInstance3D = node.get_node("hp_fg")
	var ratio := clampf(hp / max_hp, 0.0, 1.0)
	fg.scale.x = ratio
	fg.position.x = -(1.0 - ratio) * 0.5
	var color := Color(0.2, 0.85, 0.3)
	if ratio < 0.35:
		color = Color(0.9, 0.25, 0.2)
	elif ratio < 0.65:
		color = Color(0.9, 0.8, 0.2)
	var mat := fg.mesh.surface_get_material(0) as StandardMaterial3D
	if mat.albedo_color != color:
		var m := mat.duplicate() as StandardMaterial3D
		m.albedo_color = color
		fg.mesh.surface_set_material(0, m)
	var hp_label: Label3D = node.get_node("hp_label")
	hp_label.text = "%d/%d" % [int(hp), int(max_hp)]
	# 倒地：播放 death 动画（一次）或转灰躺倒（几何体单位）
	var spr: AnimatedSprite3D = node.get_node_or_null("sprite")
	if spr and data.get("downed", false) and spr.sprite_frames.has_animation("death"):
		if not node.get_meta("death_played", false):
			node.set_meta("death_played", true)
			spr.play("death")
			spr.animation_finished.connect(func():
				spr.stop())
		return
	var body: Node3D = node.get_child(0)
	var mb := body as MeshInstance3D
	if mb == null:
		return
	if data.get("downed", false):
		mb.rotation_degrees = Vector3(90, 0, 0)
		mb.position.y = 0.15
		var bm := mb.mesh.surface_get_material(0) as StandardMaterial3D
		if bm.albedo_color != Color(0.5, 0.5, 0.5):
			var m := bm.duplicate() as StandardMaterial3D
			m.albedo_color = Color(0.5, 0.5, 0.5)
			mb.mesh.surface_set_material(0, m)
	else:
		mb.rotation_degrees = Vector3.ZERO
		mb.position.y = 0.55
	# 状态徽章：wiki 图标 billboard（缺失回退文字）
	var badges: Node3D = node.get_node("badges")
	for b in badges.get_children():
		b.queue_free()
	var idx := 0
	for s in data["statuses"]:
		var icon := load("res://assets/icons/status_%d.svg" % s["id"])
		if icon:
			var icon_spr := Sprite3D.new()
			icon_spr.texture = icon
			icon_spr.billboard = BaseMaterial3D.BILLBOARD_ENABLED
			icon_spr.no_depth_test = true
			icon_spr.pixel_size = _pixel_size_for(icon, STATUS_ICON_SIZE)
			icon_spr.position = Vector3(-0.55 + idx * 0.5, 2.0, 0)
			badges.add_child(icon_spr)
			if s["stacks"] > 1:
				var cnt := Label3D.new()
				cnt.text = "×%d" % s["stacks"]
				cnt.font_size = 26
				cnt.billboard = BaseMaterial3D.BILLBOARD_ENABLED
				cnt.no_depth_test = true
				cnt.modulate = Color(1, 0.9, 0.4)
				cnt.position = icon_spr.position + Vector3(0.18, 0, 0)
				badges.add_child(cnt)
		else:
			var st: Dictionary = STATUS_STYLE.get(s["id"], {})
			var label := Label3D.new()
			label.text = "%s%d" % [st.get("text", "?"), s["stacks"] if s["stacks"] > 1 else 0]
			label.font_size = 34
			label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
			label.no_depth_test = true
			label.modulate = st.get("color", Color.WHITE)
			label.position = Vector3(-0.5 + idx * 0.5, 1.85, 0)
			badges.add_child(label)
		idx += 1
