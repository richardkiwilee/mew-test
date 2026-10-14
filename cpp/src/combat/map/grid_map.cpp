#include "pch.h"
#include "combat/map/grid_map.h"

namespace mew::combat {

namespace {

struct DijkstraState {
    std::vector<int32_t> dist;
    std::vector<Vec2i> prev;
};

DijkstraState run_dijkstra(const GridMap& grid, Vec2i from, int32_t budget, bool ignore_occupants) {
    DijkstraState st;
    const size_t n = static_cast<size_t>(grid.width()) * grid.height();
    st.dist.assign(n, std::numeric_limits<int32_t>::max());
    st.prev.assign(n, {-1, -1});
    if (!grid.in_bounds(from)) return st;

    // 小地图用二叉堆；比较器只比较代价（Vec2i 无全序，不能默认 pair 比较）
    auto cmp = [](const std::pair<int32_t, Vec2i>& a, const std::pair<int32_t, Vec2i>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<int32_t, Vec2i>, std::vector<std::pair<int32_t, Vec2i>>,
                        decltype(cmp)> pq(cmp);
    st.dist[grid.index_of(from)] = 0;
    pq.push({0, from});

    while (!pq.empty()) {
        auto [cost, p] = pq.top();
        pq.pop();
        if (cost != st.dist[grid.index_of(p)]) continue;  // 过期项
        for (Vec2i d : GridMap::kDirs4) {
            const Vec2i np = p + d;
            if (!grid.in_bounds(np)) continue;
            if (!grid.terrain_passable(np)) continue;
            if (!ignore_occupants && grid.occupant(np) != kInvalidEntity) continue;
            const int32_t ncost = cost + grid.move_cost_half_steps(np);
            if (ncost > budget) continue;
            if (ncost < st.dist[grid.index_of(np)]) {
                st.dist[grid.index_of(np)] = ncost;
                st.prev[grid.index_of(np)] = p;
                pq.push({ncost, np});
            }
        }
    }
    return st;
}

}  // namespace

std::vector<int32_t> GridMap::reachable_costs(Vec2i from, int32_t budget, bool ignore_occupants) const {
    auto st = run_dijkstra(*this, from, budget, ignore_occupants);
    std::vector<int32_t> out;
    out.reserve(st.dist.size());
    for (size_t i = 0; i < st.dist.size(); ++i) {
        out.push_back(st.dist[i] == std::numeric_limits<int32_t>::max() ? -1 : st.dist[i]);
    }
    return out;
}

std::vector<Vec2i> GridMap::path_to(Vec2i from, Vec2i to, int32_t budget, bool ignore_occupants) const {
    if (!in_bounds(to)) return {};
    auto st = run_dijkstra(*this, from, budget, ignore_occupants);
    if (st.dist[index_of(to)] == std::numeric_limits<int32_t>::max()) return {};
    std::vector<Vec2i> path;
    Vec2i cur = to;
    while (cur != from) {
        path.push_back(cur);
        cur = st.prev[index_of(cur)];
    }
    path.push_back(from);
    std::reverse(path.begin(), path.end());
    return path;
}

bool GridMap::has_line_of_sight(Vec2i from, Vec2i to) const {
    if (!in_bounds(from) || !in_bounds(to)) return false;
    // 标准 Bresenham（8 向）：WALL 遮挡（起点与终点本身不检查）
    int32_t x0 = from.x, y0 = from.y;
    const int32_t x1 = to.x, y1 = to.y;
    const int32_t dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    const int32_t sx = x0 < x1 ? 1 : (x0 > x1 ? -1 : 0);
    const int32_t sy = y0 < y1 ? 1 : (y0 > y1 ? -1 : 0);
    int32_t err = dx + dy;
    while (x0 != x1 || y0 != y1) {
        if (!(x0 == from.x && y0 == from.y)
            && tile({x0, y0}).terrain == TerrainType::WALL) {
            return false;
        }
        const int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
    return true;
}

std::vector<Vec2i> GridMap::reachable_tiles(Vec2i from, int32_t budget, bool ignore_occupants) const {
    auto st = run_dijkstra(*this, from, budget, ignore_occupants);
    std::vector<Vec2i> out;
    for (int32_t y = 0; y < h_; ++y) {
        for (int32_t x = 0; x < w_; ++x) {
            const size_t i = static_cast<size_t>(y) * w_ + x;
            if (st.dist[i] != std::numeric_limits<int32_t>::max()) {
                out.push_back({x, y});
            }
        }
    }
    return out;
}

}  // namespace mew::combat
