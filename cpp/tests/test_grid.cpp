#include "pch.h"
#include "test_framework.h"

#include "combat/map/grid_map.h"

using namespace mew::combat;

namespace {

// 5×5 全空地
GridMap make_grid(int32_t w = 5, int32_t h = 5) {
    GridMap grid(w, h);
    return grid;
}

}  // namespace

// 移动消耗（DD-002 半步制）
TEST_CASE(grid_move_costs) {
    GridMap grid = make_grid();
    grid.tile({1, 0}).terrain = TerrainType::ROAD;
    grid.tile({2, 0}).terrain = TerrainType::WATER;
    grid.tile({3, 0}).terrain = TerrainType::BRAMBLE;
    grid.tile({4, 0}).terrain = TerrainType::THICK_BRAMBLE;
    CHECK_EQ(grid.move_cost_half_steps({0, 0}), 2);
    CHECK_EQ(grid.move_cost_half_steps({1, 0}), 1);
    CHECK_EQ(grid.move_cost_half_steps({2, 0}), 4);
    CHECK_EQ(grid.move_cost_half_steps({3, 0}), 4);
    CHECK_EQ(grid.move_cost_half_steps({4, 0}), 6);
}

// 不可通行地形
TEST_CASE(grid_impassable) {
    GridMap grid = make_grid();
    grid.tile({2, 2}).terrain = TerrainType::WALL;
    CHECK(!grid.terrain_passable({2, 2}));
    CHECK(!grid.enterable({2, 2}));
    grid.tile({3, 3}).terrain = TerrainType::PIT;
    CHECK(!grid.terrain_passable({3, 3}));
    grid.tile({4, 4}).terrain = TerrainType::LAVA;
    CHECK(!grid.terrain_passable({4, 4}));
    CHECK(!grid.in_bounds({-1, 0}));
    CHECK(!grid.in_bounds({5, 5}));
}

// Dijkstra 可达性（预算 4 半步 = 移动范围 2）
TEST_CASE(grid_reachable) {
    GridMap grid = make_grid();
    const auto tiles = grid.reachable_tiles({0, 0}, 4, false);
    CHECK_EQ(tiles.size(), size_t(6));  // 原点 + 半径2内：曼哈顿距离 0/1/2
    const auto costs = grid.reachable_costs({0, 0}, 4, false);
    CHECK_EQ(costs[grid.index_of({0, 0})], 0);
    CHECK_EQ(costs[grid.index_of({1, 0})], 2);
    CHECK_EQ(costs[grid.index_of({2, 0})], 4);
    CHECK_EQ(costs[grid.index_of({3, 0})], -1);  // 超预算
}

// 道路降低消耗：预算 4 沿路可走 2 格 + 普通 1 格
TEST_CASE(grid_road_cheaper) {
    GridMap grid = make_grid();
    grid.tile({1, 0}).terrain = TerrainType::ROAD;
    const auto costs = grid.reachable_costs({0, 0}, 4, false);
    // (1,0)=1、(2,0)=3、(3,0)=5 超预算
    CHECK_EQ(costs[grid.index_of({1, 0})], 1);
    CHECK_EQ(costs[grid.index_of({2, 0})], 3);
    CHECK_EQ(costs[grid.index_of({3, 0})], -1);
}

// 占用阻止进入
TEST_CASE(grid_occupancy) {
    GridMap grid = make_grid();
    grid.place_unit(7, {1, 0});
    CHECK(!grid.enterable({1, 0}));
    CHECK_EQ(grid.occupant({1, 0}), 7u);
    // 预算内可达集合排除被占格
    const auto tiles = grid.reachable_tiles({0, 0}, 4, false);
    bool has_occupied = false;
    for (Vec2i t : tiles) {
        if (t == Vec2i{1, 0}) has_occupied = true;
    }
    CHECK(!has_occupied);
    grid.clear_occupant({1, 0});
    CHECK(grid.enterable({1, 0}));
}

// 路径：预算内最短路径
TEST_CASE(grid_path) {
    GridMap grid = make_grid();
    const auto path = grid.path_to({0, 0}, {2, 2}, 8, false);
    CHECK(!path.empty());
    CHECK_EQ(path.front(), (Vec2i{0, 0}));
    CHECK_EQ(path.back(), (Vec2i{2, 2}));
    CHECK_EQ(path.size(), size_t(5));  // 4 步
    // 超预算不可达
    CHECK(grid.path_to({0, 0}, {2, 2}, 2, false).empty());
    // 被墙挡住
    grid.tile({1, 1}).terrain = TerrainType::WALL;
    CHECK(!grid.path_to({0, 0}, {2, 2}, 8, false).empty());  // 可以绕路
    CHECK(grid.path_to({0, 0}, {2, 2}, 4, false).empty());   // 预算不够绕路
}

// 距离与 8 邻域（DD-001）
TEST_CASE(grid_distance) {
    CHECK_EQ(GridMap::manhattan({0, 0}, {2, 3}), 5);
    CHECK(GridMap::adjacent8({1, 1}, {0, 0}));  // 斜角也算接触
    CHECK(GridMap::adjacent8({1, 1}, {2, 2}));
    CHECK(!GridMap::adjacent8({1, 1}, {3, 1}));
    CHECK(!GridMap::adjacent8({1, 1}, {1, 1}));
}
