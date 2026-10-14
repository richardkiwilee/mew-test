extends SceneTree
## 无头冒烟测试：完整走一遍 准备→战斗→结算 流程（不经 UI）
## 运行：godot --headless --path . -s res://scripts/test_smoke.gd

func _initialize() -> void:
	print("=== smoke test: battle flow ===")
	var ctrl := BattleController.new()
	var map: Dictionary = Maps.get_map(0)
	ctrl.create_battle(12345, 10, 10, Maps.terrain_codes(map["rows"]))

	# 4 只猫
	var cat_ids: Array = []
	for i in 4:
		var pos: Vector2i = map["cats"][i]
		var stats := PackedInt32Array([6, 5, 5, 5, 5, 6, 5])
		stats[i % 7] += 2
		var pid: int = ctrl.add_cat(stats, PackedInt32Array([104, 105]), PackedInt32Array([201, 203]),
				pos.x, pos.y, "测试猫%d" % i)
		cat_ids.append(pid)
		print("cat ", pid, " spawned at ", pos)
	# 敌人
	for e in map["enemies"]:
		var eid: int = ctrl.add_enemy(e[0], e[1], e[2])
		print("enemy ", eid, " def=", e[0])

	var first: int = ctrl.start_battle()
	print("battle started, first unit=", first)

	var guard := 0
	while not ctrl.battle_over() and guard < 400:
		guard += 1
		var cur: int = ctrl.current_unit()
		if cur <= 0:
			break
		var info: Dictionary = ctrl.unit_info(cur)
		if info["team"] == 1:
			# 玩家猫：攻击相邻敌人，否则向最近敌人移动（简单脚本）
			var target := _nearest_enemy(ctrl, cur)
			if target > 0:
				var ti: Dictionary = ctrl.unit_info(target)
				var d := absi(info["x"] - ti["x"]) + absi(info["y"] - ti["y"])
				if d == 1:
					ctrl.try_attack(cur, target)
				else:
					ctrl.try_move_to(cur, ti["x"], ti["y"])
			ctrl.end_turn()
		else:
			ctrl.run_enemy_turn()

	var result := "VICTORY" if ctrl.winner() == 1 else "DEFEAT"
	print("battle over: winner=", ctrl.winner(), " (", result, ") rounds=", ctrl.round_index())
	print("--- log tail ---")
	var entries: Array = ctrl.log_entries()
	for e in entries.slice(maxi(entries.size() - 12, 0)):
		print(e["detail"])
	# 校验日志完整性
	print("total log entries: ", entries.size())
	var ok := ctrl.battle_over() and entries.size() > 20
	print("SMOKE ", "PASS" if ok else "FAIL")
	quit(0 if ok else 1)


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
