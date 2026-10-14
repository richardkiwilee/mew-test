extends SceneTree
## 调试：启动战斗并在游戏内直接截取视口（排除屏幕/窗口因素）
## 运行：godot --path . -s res://scripts/test_capture.gd

func _initialize() -> void:
	var main: Node = load("res://scripts/main.gd").new()
	root.add_child(main)
	for i in 6:
		await process_frame
	var configs: Array = []
	for i in 4:
		configs.append({
			"name": "猫%d" % i, "class": "mage",
			"stats": PackedInt32Array([6, 5, 5, 5, 5, 6, 5]),
			"passives": PackedInt32Array([104, 108]),
			"abilities": PackedInt32Array([201, 202, 205]),
		})
	main._on_start_battle(0, configs)
	for i in 8:
		await process_frame
	# 视口截图（真实渲染帧）
	var img: Image = root.get_viewport().get_texture().get_image()
	img.save_png("res://../battle_view.png")
	print("captured state=", main._state, " battle_ui visible=", main._battle_ui.visible)
	print("actions panel children=", main._battle_ui.get_child_count())
	quit(0)
