extends SceneTree
## UI 流程测试：实例化 Main，走 _on_start_battle → 战斗驱动 → 结算 的真实代码路径
## 运行：godot --headless --path . -s res://scripts/test_ui_flow.gd

func _initialize() -> void:
	print("=== ui flow test ===")
	Engine.time_scale = 8.0   # 加速敌方回合节奏（0.3s → 0.0375s）
	var main: Node = load("res://scripts/main.gd").new()
	root.add_child(main)
	await process_frame
	await process_frame

	var configs: Array = []
	for i in 4:
		configs.append({
			"name": "猫%d" % i,
			"stats": PackedInt32Array([6, 5, 5, 5, 5, 6, 5]),
			"passives": PackedInt32Array([104, 108]),
			"abilities": PackedInt32Array([201, 203]),
		})
	main._on_start_battle(1, configs)   # 河湾地图（水域地形交互）
	await process_frame

	var acted := 0
	for i in 3000:
		await process_frame
		if main._state == main.State.RESULT:
			break
		if main._state != main.State.BATTLE:
			continue
		var cur: int = main._ctrl.current_unit()
		if cur <= 0:
			continue
		var info: Dictionary = main._ctrl.unit_info(cur)
		if info["team"] != 1:
			continue   # 敌方回合由 main 自动推进
		# 玩家回合：攻击相邻敌人，否则逼近
		var target := _nearest_enemy(main._ctrl, cur)
		if target > 0:
			var ti: Dictionary = main._ctrl.unit_info(target)
			var d := absi(info["x"] - ti["x"]) + absi(info["y"] - ti["y"])
			if d == 1:
				main._ctrl.try_attack(cur, target)
			else:
				main._ctrl.try_move_to(cur, ti["x"], ti["y"])
		main._on_end_turn()
		acted += 1

	var over: bool = main._ctrl.battle_over()
	print("state=", main._state, " battle_over=", over, " winner=", main._ctrl.winner(),
			" rounds=", main._ctrl.round_index(), " player_acts=", acted)
	print("log entries=", main._ctrl.log_entries().size())
	# 回到准备（流程闭环）
	main._on_back_to_prep()
	await process_frame
	print("back to prep visible=", main._prep.visible)
	print("UI FLOW ", "PASS" if over and main._prep.visible else "FAIL")
	quit(0 if over else 1)


func _nearest_enemy(ctrl, my_id: int) -> int:
	var my: Dictionary = ctrl.unit_info(my_id)
	var best := -1
	var best_d := 999
	for u in ctrl.all_units():
		if u["team"] != 2 or not u["alive"] or not u["on_board"]:
			continue
		var d := absi(my["x"] - u["x"]) + absi(my["y"] - u["y"])
		if d < best_d:
			best_d = d
			best = u["id"]
	return best
