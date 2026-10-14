#pragma once
// ============================================================================
// GridMap — 战斗棋盘（设计文档 §11.2 / wiki: 10×10 网格、每格 5 层）
// ============================================================================

#include <vector>

#include "combat/core/basic_types.h"
#include "combat/core/vec2i.h"
#include "generated/reflection_gen.h"
// ============================================================================
// - 移动预算用"半步"单位（DD-002）：预算 = 2 × 移动范围；消耗：路 1 / 普通 2 /
//   水域·荆棘 4 / 厚荆棘 6；移动按四方向逐格（DD-001）。
// - 本模块只负责格子/占用/通行/距离/路径，不直接造成伤害：
//   踩入地形由上层发布 UnitEnteredTile 事件，由地形规则订阅执行（Phase 6）。
// - 每格至多 1 个单位；陷阱等占用独立层（Phase 6 起用 trap 字段）。
// ============================================================================

namespace mew::combat {

struct TileCell {
    TerrainType terrain = TerrainType::EMPTY;
    uint32_t trap_id = 0;                  // 0 = 无陷阱（陷阱层，Phase 6 启用）
    bool is_blocked = false;               // 剧情/物件阻挡（除地形外）
};

class GridMap {
public:
    GridMap() = default;
    GridMap(int32_t w, int32_t h) { resize(w, h); }

    void resize(int32_t w, int32_t h) {
        w_ = w;
        h_ = h;
        tiles_.assign(static_cast<size_t>(w) * h, TileCell{});
        occupants_.assign(static_cast<size_t>(w) * h, kInvalidEntity);
    }

    int32_t width() const { return w_; }
    int32_t height() const { return h_; }

    bool in_bounds(Vec2i p) const { return p.x >= 0 && p.x < w_ && p.y >= 0 && p.y < h_; }
    size_t index_of(Vec2i p) const { return static_cast<size_t>(p.y) * w_ + p.x; }

    const TileCell& tile(Vec2i p) const { return tiles_[index_of(p)]; }
    TileCell& tile(Vec2i p) { return tiles_[index_of(p)]; }

    EntityId occupant(Vec2i p) const { return occupants_[index_of(p)]; }

    // 放置/移除单位（调用方负责合法性检查）
    void place_unit(EntityId id, Vec2i p) { occupants_[index_of(p)] = id; }
    void clear_occupant(Vec2i p) { occupants_[index_of(p)] = kInvalidEntity; }

    // 地形是否可通行（不含占用检查）
    bool terrain_passable(Vec2i p) const {
        const auto& t = tile(p);
        if (t.is_blocked) return false;
        switch (t.terrain) {
            case TerrainType::WALL:
            case TerrainType::PIT:
            case TerrainType::LAVA:
                return false;
            default:
                return true;
        }
    }

    // 是否可移动进入：地形可通行且无单位占据
    bool enterable(Vec2i p) const {
        return in_bounds(p) && terrain_passable(p) && occupant(p) == kInvalidEntity;
    }

    // 移动消耗（半步单位，DD-002）
    int32_t move_cost_half_steps(Vec2i p) const {
        switch (tile(p).terrain) {
            case TerrainType::ROAD:            return 1;
            case TerrainType::BRAMBLE:
            case TerrainType::WATER:
            case TerrainType::WATER_CURRENT:
            case TerrainType::CREEP:           return 4;
            case TerrainType::THICK_BRAMBLE:   return 6;
            default:                           return 2;  // 空地/草丛/花丛等
        }
    }

    // 曼哈顿距离（wiki：移动距离恒用 Manhattan）
    static int32_t manhattan(Vec2i a, Vec2i b) {
        return std::abs(a.x - b.x) + std::abs(a.y - b.y);
    }

    // 8 邻域接触（DD-001：近战接触含斜角）；自身不算相邻
    static bool adjacent8(Vec2i a, Vec2i b) {
        const int32_t dx = std::abs(a.x - b.x);
        const int32_t dy = std::abs(a.y - b.y);
        return (dx != 0 || dy != 0) && std::max(dx, dy) <= 1;
    }

    // 视线：Bresenham 直线（8 向），WALL 遮挡（端点本身不检查）
    bool has_line_of_sight(Vec2i from, Vec2i to) const;

    static constexpr Vec2i kDirs4[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    // ------------------------------------------------------------------
    // Dijkstra 可达性（四方向，半步预算）
    // 返回每个格子的最小消耗；-1 = 不可达（起点预算内恒为 0）。
    // ignore_occupants = true 时仅按地形计算（预览/传送落点用）。
    // ------------------------------------------------------------------
    std::vector<int32_t> reachable_costs(Vec2i from, int32_t budget, bool ignore_occupants) const;

    // 起点到目标的预算内最短路径（含起点与终点）；不可达返回空。
    std::vector<Vec2i> path_to(Vec2i from, Vec2i to, int32_t budget, bool ignore_occupants) const;

    // 预算内可进入的格子列表（Dijkstra 序，确定性）
    std::vector<Vec2i> reachable_tiles(Vec2i from, int32_t budget, bool ignore_occupants) const;

private:
    int32_t w_ = 0;
    int32_t h_ = 0;
    std::vector<TileCell> tiles_;
    std::vector<EntityId> occupants_;
};

}  // namespace mew::combat
