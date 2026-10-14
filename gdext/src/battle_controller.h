#pragma once
// ============================================================================
// BattleController — Godot 侧战斗门面（GDExtension）
// 持有唯一 BattleContext；所有 GDScript 调用经统一 API 进入战斗核心。
// UI/动画只读取状态与日志，绝不直接修改战斗状态。
// ============================================================================

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>

#include <memory>

namespace godot {
class Array;
class Dictionary;
class PackedInt32Array;
class PackedVector2Array;
class String;
}  // namespace godot

namespace mew::combat {
class BattleContext;
}

namespace mew_gdext {

class BattleController : public godot::Node {
    GDCLASS(BattleController, godot::Node)

private:
    std::unique_ptr<mew::combat::BattleContext> battle_;
    int64_t next_cat_def_id_ = 1000;

protected:
    static void _bind_methods();

public:
    BattleController();
    ~BattleController();

    // ---------------- 准备阶段 ----------------
    void create_battle(int64_t seed, int64_t w, int64_t h, const godot::PackedInt32Array& terrain);
    int64_t add_cat(const godot::PackedInt32Array& stats,
                    const godot::PackedInt32Array& passive_ids,
                    const godot::PackedInt32Array& ability_ids, int64_t x, int64_t y,
                    const godot::String& name);
    int64_t add_enemy(int64_t def_id, int64_t x, int64_t y);
    int64_t start_battle();

    // ---------------- 状态查询 ----------------
    godot::Dictionary unit_info(int64_t id) const;
    godot::Array all_units() const;
    int64_t current_unit() const;
    bool battle_over() const;
    int64_t winner() const;
    int64_t round_index() const;

    // ---------------- 行动意图 ----------------
    godot::PackedVector2Array reachable_tiles(int64_t id) const;
    godot::PackedVector2Array path_to(int64_t id, int64_t x, int64_t y) const;
    godot::Dictionary try_move_to(int64_t id, int64_t x, int64_t y);
    godot::Dictionary try_attack(int64_t id, int64_t target_id);
    godot::Dictionary try_cast(int64_t id, int64_t ability_id, int64_t target_id, int64_t tx,
                               int64_t ty);
    void end_turn();
    bool run_enemy_turn();

    // ---------------- 战斗日志 ----------------
    godot::Array log_entries() const;
    godot::String log_text() const;

    // ---------------- 内容查询（准备阶段 UI 用） ----------------
    godot::Array ability_pool() const;    // 全部技能 [{id, name, desc, cost, min, max}]
    godot::Array passive_pool() const;    // 全部被动 [{id, name, desc}]
    godot::Array enemy_pool() const;      // 全部敌人定义 [{id, name, stats}]
};

}  // namespace mew_gdext
