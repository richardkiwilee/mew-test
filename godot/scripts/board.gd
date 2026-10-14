class_name Board
extends Node3D
## ============================================================================
## 3D 棋盘：地形渲染 + 单位显示 + 高亮 + 拾取（参考 3-dmap-2 的等距方案）
## 纯展示层：只读取战斗核心返回的状态，不修改任何战斗数据。
## =============================================================================

signal tile_clicked(cell: Vector2i)
signal unit_clicked(unit_id: int)

const TILE := 2.0                 # 格距（世界单位）
const GRID_W := 10
const GRID_H := 10
const CAM_DIST := 26.0
const CAM_PITCH := 45.0

const TERRAIN_STYLE := {
	1:  {"color": Color(0.62, 0.60, 0.55), "h": 0.05, "name": "空地"},
	2:  {"color": Color(0.55, 0.45, 0.33), "h": 0.05, "name": "道路"},
	3:  {"color": Color(0.33, 0.55, 0.29), "h": 0.05, "name": "草丛"},
	4:  {"color": Color(0.22, 0.45, 0.20), "h": 0.05, "name": "高草丛", "bush": true},
	5:  {"color": Color(0.42, 0.62, 0.30), "h": 0.05, "name": "花丛", "flower": true},
	6:  {"color": Color(0.36, 0.58, 0.26), "h": 0.05, "name": "高花丛", "bush": true, "flower": true},
	7:  {"color": Color(0.28, 0.42, 0.20), "h": 0.05, "name": "荆棘", "bush": true},
	8:  {"color": Color(0.22, 0.35, 0.16), "h": 0.05, "name": "厚荆棘", "bush": true},
	9:  {"color": Color(0.18, 0.42, 0.72), "h": -0.18, "name": "水域"},
	10: {"color": Color(0.15, 0.38, 0.75), "h": -0.18, "name": "激流"},
	11: {"color": Color(0.28, 0.35, 0.28), "h": -0.10, "name": "苔沼"},
	12: {"color": Color(0.85, 0.30, 0.08), "h": -0.05, "name": "熔岩", "glow": true},
	13: {"color": Color(0.08, 0.08, 0.10), "h": -0.55, "name": "深坑"},
	14: {"color": Color(0.45, 0.45, 0.48), "h": 1.4, "name": "墙"},
}

const ENEMY_STYLE := {  # def_id → 颜色/形状
	2: {"color": Color(0.62, 0.40, 0.35), "shape": "cap", "name": "老鼠"},
	3: {"color": Color(0.50, 0.42, 0.62), "shape": "sphere", "name": "蝙蝠"},
	4: {"color": Color(0.55, 0.30, 0.28), "shape": "cap", "name": "暴徒"},
	5: {"color": Color(0.80, 0.42, 0.15), "shape": "cone", "name": "火妖"},
	6: {"color": Color(0.40, 0.58, 0.30), "shape": "cap", "name": "喷毒者"},
}

const STATUS_STYLE := {
	3: {"text": "火", "color": Color(1.0, 0.4, 0.2)},
	4: {"text": "毒", "color": Color(0.3, 0.9, 0.3)},
	5: {"text": "血", "color": Color(0.8, 0.1, 0.1)},
	6: {"text": "晕", "color": Color(0.9, 0.8, 0.2)},
	7: {"text": "弱", "color": Color(0.6, 0.5, 0.7)},
	8: {"text": "湿", "color": Color(0.3, 0.6, 1.0)},
}

var _board_root: Node3D
var _camera: Camera3D
var _tiles: Dictionary = {}          # Vector2i -> MeshInstance3D
var _units: Dictionary = {}          # unit_id -> Node3D
var _reachable: Array[Vector2i] = []
var _selected_cell := Vector2i(-1, -1)
var _selected_marker: MeshInstance3D
var _unit_hint: Dictionary = {}      # unit_id -> Label3D（状态徽章）
var _rot := 0                        # 0..3（Q/E 90° 步进）
var _input_locked := false


func _ready() -> void:
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
	_selected_marker = _make_tile_marker(Color(1, 0.85, 0.2), 0.4)
	_selected_marker.visible = false
	_board_root.add_child(_selected_marker)
	# 环境底色
	var world_env := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.12, 0.14, 0.18)
	world_env.environment = env
	add_child(world_env)


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
	_unit_hint.clear()


func refresh_units(units: Array) -> void:
	# 同步单位节点（增量创建/更新/删除）
	var seen := {}
	for data in units:
		var uid: int = data["id"]
		seen[uid] = true
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


func grid_to_world(cell: Vector2i) -> Vector3:
	return Vector3((cell.x - GRID_W / 2.0 + 0.5) * TILE, 0, (cell.y - GRID_H / 2.0 + 0.5) * TILE)


func world_to_grid(pos: Vector3) -> Vector2i:
	var c := Vector2i(roundi(pos.x / TILE + GRID_W / 2.0 - 0.5),
			roundi(pos.z / TILE + GRID_H / 2.0 - 0.5))
	if c.x < 0 or c.x >= GRID_W or c.y < 0 or c.y >= GRID_H:
		return Vector2i(-1, -1)
	return c


func set_reachable(cells: Array) -> void:
	_reachable.clear()
	for c in cells:
		_reachable.append(Vector2i(c.x, c.y))
	_redraw_highlights()


func clear_highlights() -> void:
	_reachable.clear()
	_redraw_highlights()


func set_selected(cell: Vector2i) -> void:
	_selected_cell = cell
	if cell.x < 0:
		_selected_marker.visible = false
	else:
		_selected_marker.visible = true
		_selected_marker.position = grid_to_world(cell) + Vector3(0, 0.03, 0)


func set_input_locked(locked: bool) -> void:
	_input_locked = locked


func animate_move(unit_id: int, path: Array) -> void:
	# path: Array[Vector2i]（含起点）——逐格补间
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
	var origin := node.position
	var dir := (tgt.position - origin).normalized()
	var tween := create_tween()
	tween.tween_property(node, "position", origin + dir * 0.5, 0.08)
	tween.tween_property(node, "position", origin, 0.12)
	tween.finished.connect(func(): set_input_locked(false))


func _unhandled_input(event: InputEvent) -> void:
	if _input_locked:
		return
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
		elif event.button_index == MOUSE_BUTTON_LEFT:
			_handle_click(event.position)


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


func _handle_click(screen_pos: Vector2) -> void:
	var from := _camera.project_ray_origin(screen_pos)
	var dir := _camera.project_ray_normal(screen_pos)
	# 单位优先（近似：对每个单位节点做射线-包围球检测）
	var best_id := -1
	var best_t := INF
	for uid in _units:
		var node: Node3D = _units[uid]
		var sphere_center := node.position + Vector3(0, 0.6, 0)
		var radius := 0.7
		var t := _ray_sphere(from, dir, sphere_center, radius)
		if t < best_t:
			best_t = t
			best_id = uid
	if best_id >= 0 and best_t < 200.0:
		unit_clicked.emit(best_id)
		return
	if dir.y < -0.001:
		var t := -from.y / dir.y
		var hit := from + dir * t
		var cell := world_to_grid(hit)
		if cell.x >= 0:
			tile_clicked.emit(cell)


func _ray_sphere(origin: Vector3, dir: Vector3, center: Vector3, radius: float) -> float:
	var oc := origin - center
	var b := oc.dot(dir)
	var c := oc.dot(oc) - radius * radius
	var disc := b * b - c
	if disc < 0.0:
		return INF
	var t := -b - sqrt(disc)
	return t if t > 0.0 else INF


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


func _make_tile_marker(color: Color, alpha: float) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(TILE * 0.96, 0.06, TILE * 0.96)
	var mat := StandardMaterial3D.new()
	mat.albedo_color = color
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color.a = alpha
	box.material = mat
	mi.mesh = box
	return mi


func _redraw_highlights() -> void:
	for c in _tiles:
		var tile: MeshInstance3D = _tiles[c]
		var in_reach := _reachable.has(c)
		var mat: StandardMaterial3D = tile.mesh.surface_get_material(0) as StandardMaterial3D
		if in_reach:
			var m := mat.duplicate() as StandardMaterial3D
			m.emission_enabled = true
			m.emission = Color(0.15, 0.5, 0.9)
			m.emission_energy_multiplier = 0.55
			tile.mesh.surface_set_material(0, m)
		else:
			var m := mat.duplicate() as StandardMaterial3D
			m.emission_enabled = false
			tile.mesh.surface_set_material(0, m)


func _make_unit(data: Dictionary) -> Node3D:
	var root := Node3D.new()
	var style: Dictionary
	var is_player: bool = data["team"] == 1  # Team::PLAYER
	if is_player:
		style = {"color": Color(0.95, 0.65, 0.25), "shape": "cap", "name": "猫"}
	else:
		style = ENEMY_STYLE.get(data.get("def", 0), ENEMY_STYLE[2])
	# 记住 def id 用于样式（unit_info 里没有 def 字段——用名字映射不到，改用颜色缓存）
	root.set_meta("team", data["team"])
	root.set_meta("color", style["color"])
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

	# 血条（红底 + 绿条）
	var bg := MeshInstance3D.new()
	var bgb := BoxMesh.new()
	bgb.size = Vector3(1.0, 0.1, 0.08)
	var bgm := StandardMaterial3D.new()
	bgm.albedo_color = Color(0.25, 0.1, 0.1)
	bgb.material = bgm
	bg.mesh = bgb
	bg.position = Vector3(0, 1.35, 0)
	root.add_child(bg)
	var fg := MeshInstance3D.new()
	var fgb := BoxMesh.new()
	fgb.size = Vector3(1.0, 0.1, 0.1)
	var fgm := StandardMaterial3D.new()
	fgm.albedo_color = Color(0.2, 0.85, 0.3)
	fgb.material = fgm
	fg.mesh = fgb
	fg.position = Vector3(0, 1.36, 0)
	fg.name = "hp_fg"
	root.add_child(fg)

	# 状态徽章（Label3D 集合，挂 root 子节点）
	var badges := Node3D.new()
	badges.name = "badges"
	root.add_child(badges)
	return root


func _update_unit(node: Node3D, data: Dictionary) -> void:
	var cell := Vector2i(data["x"], data["y"])
	var target := grid_to_world(cell) + Vector3(0, 0.02, 0)
	node.position = target
	node.visible = not data.get("dead_removed", false)
	var hp: float = maxf(float(data["hp"]), 0.0)
	var max_hp: float = maxf(float(data["max_hp"]), 1.0)
	var fg: MeshInstance3D = node.get_node("hp_fg")
	var ratio := clampf(hp / max_hp, 0.0, 1.0)
	fg.scale.x = ratio
	fg.position.x = -(1.0 - ratio) * 0.5
	var mat := fg.mesh.surface_get_material(0) as StandardMaterial3D
	var color := Color(0.2, 0.85, 0.3)
	if ratio < 0.35:
		color = Color(0.9, 0.25, 0.2)
	elif ratio < 0.65:
		color = Color(0.9, 0.8, 0.2)
	if mat.albedo_color != color:
		var m := mat.duplicate() as StandardMaterial3D
		m.albedo_color = color
		fg.mesh.surface_set_material(0, m)
	# 倒地：转灰 + 躺倒
	var body: MeshInstance3D = node.get_child(0)
	if data.get("downed", false):
		body.rotation_degrees = Vector3(90, 0, 0)
		body.position.y = 0.15
		var bm := body.mesh.surface_get_material(0) as StandardMaterial3D
		if bm.albedo_color != Color(0.5, 0.5, 0.5):
			var m := bm.duplicate() as StandardMaterial3D
			m.albedo_color = Color(0.5, 0.5, 0.5)
			body.mesh.surface_set_material(0, m)
	else:
		body.rotation_degrees = Vector3.ZERO
		body.position.y = 0.55
	# 状态徽章
	var badges: Node3D = node.get_node("badges")
	for b in badges.get_children():
		b.queue_free()
	var idx := 0
	for s in data["statuses"]:
		var style: Dictionary = STATUS_STYLE.get(s["id"], {})
		if style.is_empty():
			continue
		var label := Label3D.new()
		label.text = "%s%s" % [style["text"], s["stacks"] if s["stacks"] > 1 else ""]
		label.font_size = 34
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.modulate = style["color"]
		label.position = Vector3(-0.45 + idx * 0.45, 1.55, 0)
		badges.add_child(label)
		idx += 1
