#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

class Solver
{
public:
    void run()
    {
        readInput();
        solve();
        std::cout << moves_ << 'S' << '\n';
    }

private:
    int n_ = 0;
    int blank_r_ = -1;
    int blank_c_ = -1;
    std::vector<std::vector<int>> board_;
    std::vector<std::vector<int>> initial_board_;
    std::vector<std::vector<int>> target_;
    std::vector<char> fixed_;
    std::string moves_;

    std::vector<int> bfs_parent_;
    std::vector<char> bfs_cmd_;
    std::vector<int> bfs_seen_;
    int bfs_stamp_ = 1;

    std::vector<int> tile_dist_;
    std::vector<int> remaining_need_;
    std::vector<int> available_count_;
    std::vector<int> initial_remaining_need_;
    std::vector<int> initial_available_count_;
    int max_color_ = 0;
    int initial_blank_r_ = -1;
    int initial_blank_c_ = -1;

    struct Strategy
    {
        int fill_mode; // เปลี่ยนจาก bool center_out เป็น int เพื่อรองรับหลายรูปแบบ
        bool planned_route;
        bool advanced_score;
        int w_dist;
        int w_preserve;
        int w_linear;
        int w_zone;
        int w_blank;
        int scarcity_boost;
        int source_top_k;
    };

    static constexpr std::array<int, 4> DR = {-1, 1, 0, 0};
    static constexpr std::array<int, 4> DC = {0, 0, -1, 1};

    int idx(int r, int c) const { return r * n_ + c; }

    bool inside(int r, int c) const { return r >= 0 && r < n_ && c >= 0 && c < n_; }

    bool isTargetCell(int r, int c) const { return r >= 1 && r <= n_ - 2 && c >= 1 && c <= n_ - 2; }

    int targetValueAt(int r, int c) const { return target_[r - 1][c - 1]; }

    bool isCorrectTargetCell(int r, int c) const
    {
        return isTargetCell(r, c) && board_[r][c] == targetValueAt(r, c);
    }

    int manhattan(int r1, int c1, int r2, int c2) const { return std::abs(r1 - r2) + std::abs(c1 - c2); }

    bool isOpposite(char a, char b) const
    {
        return (a == 'U' && b == 'D') || (a == 'D' && b == 'U') ||
               (a == 'L' && b == 'R') || (a == 'R' && b == 'L');
    }

    std::string reduceMoves(const std::string &input) const
    {
        std::string out;
        out.reserve(input.size());
        for (char cmd : input)
        {
            if (!out.empty() && isOpposite(out.back(), cmd))
            {
                out.pop_back();
            }
            else
            {
                out.push_back(cmd);
            }
        }
        return out;
    }

    int readTimeBudgetMs() const
    {
        const char *env = std::getenv("SOLVER_TIME_MS");
        if (env != nullptr)
        {
            const int val = std::atoi(env);
            if (val > 0)
                return val;
        }
        if (n_ >= 51)
            return 330000;
        if (n_ >= 33)
            return 150000;
        if (n_ >= 23)
            return 60000;
        return 15000;
    }

    char cmdForBlankMove(int from_r, int from_c, int to_r, int to_c) const
    {
        if (to_r == from_r - 1 && to_c == from_c)
            return 'D';
        if (to_r == from_r + 1 && to_c == from_c)
            return 'U';
        if (to_r == from_r && to_c == from_c - 1)
            return 'R';
        if (to_r == from_r && to_c == from_c + 1)
            return 'L';
        throw std::runtime_error("Invalid blank move");
    }

    char cmdForTileMove(int from_r, int from_c, int to_r, int to_c) const
    {
        if (to_r == from_r - 1 && to_c == from_c)
            return 'U';
        if (to_r == from_r + 1 && to_c == from_c)
            return 'D';
        if (to_r == from_r && to_c == from_c - 1)
            return 'L';
        if (to_r == from_r && to_c == from_c + 1)
            return 'R';
        throw std::runtime_error("Invalid tile move");
    }

    void readInput()
    {
        std::ios::sync_with_stdio(false);
        std::cin.tie(nullptr);

        if (!(std::cin >> n_))
        {
            throw std::runtime_error("Failed to read N");
        }

        board_.assign(n_, std::vector<int>(n_, 0));
        target_.assign(n_ - 2, std::vector<int>(n_ - 2, 0));
        fixed_.assign(n_ * n_, 0);

        for (int r = 0; r < n_; ++r)
        {
            for (int c = 0; c < n_; ++c)
            {
                std::cin >> board_[r][c];
                if (board_[r][c] == -1)
                {
                    blank_r_ = r;
                    blank_c_ = c;
                }
                else if (board_[r][c] > max_color_)
                {
                    max_color_ = board_[r][c];
                }
            }
        }
        if (blank_r_ < 0)
        {
            throw std::runtime_error("No blank cell");
        }

        for (int r = 0; r < n_ - 2; ++r)
        {
            for (int c = 0; c < n_ - 2; ++c)
            {
                std::cin >> target_[r][c];
                if (target_[r][c] > max_color_)
                {
                    max_color_ = target_[r][c];
                }
            }
        }

        const int cells = n_ * n_;
        bfs_parent_.assign(cells, -1);
        bfs_cmd_.assign(cells, 0);
        bfs_seen_.assign(cells, 0);
        tile_dist_.assign(cells, -1);
        remaining_need_.assign(max_color_ + 1, 0);
        available_count_.assign(max_color_ + 1, 0);

        for (int r = 1; r <= n_ - 2; ++r)
        {
            for (int c = 1; c <= n_ - 2; ++c)
            {
                ++remaining_need_[targetValueAt(r, c)];
            }
        }
        for (int r = 0; r < n_; ++r)
        {
            for (int c = 0; c < n_; ++c)
            {
                if (board_[r][c] != -1)
                {
                    ++available_count_[board_[r][c]];
                }
            }
        }

        initial_board_ = board_;
        initial_blank_r_ = blank_r_;
        initial_blank_c_ = blank_c_;
        initial_remaining_need_ = remaining_need_;
        initial_available_count_ = available_count_;
    }

    void applyMove(char cmd)
    {
        int nr = blank_r_;
        int nc = blank_c_;
        if (cmd == 'U')
            nr += 1;
        else if (cmd == 'D')
            nr -= 1;
        else if (cmd == 'L')
            nc += 1;
        else if (cmd == 'R')
            nc -= 1;
        else
            throw std::runtime_error("Unknown command");

        if (!inside(nr, nc))
        {
            throw std::runtime_error("Move out of bounds");
        }

        std::swap(board_[blank_r_][blank_c_], board_[nr][nc]);
        blank_r_ = nr;
        blank_c_ = nc;
        moves_.push_back(cmd);
    }

    bool bfsBlankPath(int tr, int tc, int blocked_id, std::vector<char> &out_path)
    {
        out_path.clear();
        if (blank_r_ == tr && blank_c_ == tc)
            return true;

        const int start = idx(blank_r_, blank_c_);
        const int goal = idx(tr, tc);
        ++bfs_stamp_;
        if (bfs_stamp_ == std::numeric_limits<int>::max())
        {
            std::fill(bfs_seen_.begin(), bfs_seen_.end(), 0);
            bfs_stamp_ = 1;
        }

        std::queue<int> q;
        bfs_seen_[start] = bfs_stamp_;
        bfs_parent_[start] = -1;
        q.push(start);

        bool found = false;
        while (!q.empty())
        {
            const int cur = q.front();
            q.pop();
            if (cur == goal)
            {
                found = true;
                break;
            }

            const int r = cur / n_;
            const int c = cur % n_;
            for (int d = 0; d < 4; ++d)
            {
                const int nr = r + DR[d];
                const int nc = c + DC[d];
                if (!inside(nr, nc))
                    continue;
                const int nid = idx(nr, nc);
                if (nid == blocked_id)
                    continue;
                if (fixed_[nid])
                    continue;
                if (bfs_seen_[nid] == bfs_stamp_)
                    continue;

                bfs_seen_[nid] = bfs_stamp_;
                bfs_parent_[nid] = cur;
                bfs_cmd_[nid] = cmdForBlankMove(r, c, nr, nc);
                q.push(nid);
            }
        }

        if (!found)
            return false;

        int cur = goal;
        while (cur != start)
        {
            out_path.push_back(bfs_cmd_[cur]);
            cur = bfs_parent_[cur];
        }
        std::reverse(out_path.begin(), out_path.end());
        return true;
    }

    int bfsBlankLen(int start_id, int goal_id, int blocked_id)
    {
        if (start_id == goal_id)
            return 0;
        ++bfs_stamp_;
        if (bfs_stamp_ == std::numeric_limits<int>::max())
        {
            std::fill(bfs_seen_.begin(), bfs_seen_.end(), 0);
            bfs_stamp_ = 1;
        }

        std::queue<int> q;
        bfs_seen_[start_id] = bfs_stamp_;
        bfs_parent_[start_id] = 0;
        q.push(start_id);

        while (!q.empty())
        {
            const int cur = q.front();
            q.pop();
            const int r = cur / n_;
            const int c = cur % n_;
            const int nd = bfs_parent_[cur] + 1;

            for (int d = 0; d < 4; ++d)
            {
                const int nr = r + DR[d];
                const int nc = c + DC[d];
                if (!inside(nr, nc))
                    continue;
                const int nid = idx(nr, nc);
                if (nid == blocked_id)
                    continue;
                if (fixed_[nid])
                    continue;
                if (bfs_seen_[nid] == bfs_stamp_)
                    continue;
                if (nid == goal_id)
                {
                    return nd;
                }
                bfs_seen_[nid] = bfs_stamp_;
                bfs_parent_[nid] = nd;
                q.push(nid);
            }
        }
        return -1;
    }

    void buildTileDist(int tr, int tc)
    {
        std::fill(tile_dist_.begin(), tile_dist_.end(), -1);

        const int start = idx(tr, tc);
        if (fixed_[start])
        {
            return;
        }

        std::queue<int> q;
        tile_dist_[start] = 0;
        q.push(start);

        while (!q.empty())
        {
            const int cur = q.front();
            q.pop();
            const int r = cur / n_;
            const int c = cur % n_;
            const int nd = tile_dist_[cur] + 1;

            for (int d = 0; d < 4; ++d)
            {
                const int nr = r + DR[d];
                const int nc = c + DC[d];
                if (!inside(nr, nc))
                    continue;
                const int nid = idx(nr, nc);
                if (fixed_[nid])
                    continue;
                if (tile_dist_[nid] != -1)
                    continue;
                tile_dist_[nid] = nd;
                q.push(nid);
            }
        }
    }

    void resetState()
    {
        board_ = initial_board_;
        blank_r_ = initial_blank_r_;
        blank_c_ = initial_blank_c_;
        fixed_.assign(n_ * n_, 0);
        moves_.clear();
        remaining_need_ = initial_remaining_need_;
        available_count_ = initial_available_count_;
    }

    std::vector<std::pair<int, int>> buildOrder(int fill_mode) const
    {
        std::vector<std::pair<int, int>> order;

        if (fill_mode == 0 || fill_mode == 1) // โหมด 0: วงแหวนในออกนอก, 1: วงแหวนนอกเข้าใน
        {
            const int center = n_ / 2;
            const int max_radius = (n_ - 2) / 2;

            for (int radius = 0; radius <= max_radius; ++radius)
            {
                const int min_r = std::max(1, center - radius);
                const int max_r = std::min(n_ - 2, center + radius);
                const int min_c = std::max(1, center - radius);
                const int max_c = std::min(n_ - 2, center + radius);

                for (int r = min_r; r <= max_r; ++r)
                {
                    for (int c = min_c; c <= max_c; ++c)
                    {
                        const int cheb = std::max(std::abs(r - center), std::abs(c - center));
                        if (cheb == radius)
                        {
                            order.emplace_back(r, c);
                        }
                    }
                }
            }
            if (fill_mode == 1)
            {
                std::reverse(order.begin(), order.end());
            }
        }
        else if (fill_mode == 2) // โหมด 2: ทำจากแถวบนสุด ไล่ลงมาล่างสุด
        {
            for (int r = 1; r <= n_ - 2; ++r)
            {
                for (int c = 1; c <= n_ - 2; ++c)
                {
                    order.emplace_back(r, c);
                }
            }
        }
        else if (fill_mode == 3) // โหมด 3: ทำจากแถวล่างสุด ไล่ขึ้นบนสุด
        {
            for (int r = n_ - 2; r >= 1; --r)
            {
                for (int c = 1; c <= n_ - 2; ++c)
                {
                    order.emplace_back(r, c);
                }
            }
        }
        else if (fill_mode == 4) // โหมด 4: ทำจากคอลัมน์ซ้ายสุด ไล่ไปขวาสุด
        {
            for (int c = 1; c <= n_ - 2; ++c)
            {
                for (int r = 1; r <= n_ - 2; ++r)
                {
                    order.emplace_back(r, c);
                }
            }
        }

        return order;
    }

    int linearConflictPenaltyForSource(int sr, int sc, int tr, int tc) const
    {
        int penalty = 0;
        if (sr == tr && isTargetCell(sr, sc))
        {
            const int lo = std::min(sc, tc);
            const int hi = std::max(sc, tc);
            for (int c = lo; c <= hi; ++c)
            {
                if (c == sc || c == tc)
                    continue;
                if (isCorrectTargetCell(sr, c))
                    ++penalty;
            }
        }
        if (sc == tc && isTargetCell(sr, sc))
        {
            const int lo = std::min(sr, tr);
            const int hi = std::max(sr, tr);
            for (int r = lo; r <= hi; ++r)
            {
                if (r == sr || r == tr)
                    continue;
                if (isCorrectTargetCell(r, sc))
                    ++penalty;
            }
        }
        return penalty;
    }

    bool planTileRoute(int sr, int sc, int tr, int tc, std::vector<int> &out_route, int &out_cost)
    {
        out_route.clear();
        out_cost = std::numeric_limits<int>::max();
        const int src_id = idx(sr, sc);
        const int dst_id = idx(tr, tc);
        const int steps = tile_dist_[src_id];
        if (steps < 0)
            return false;
        if (steps == 0)
        {
            out_route.push_back(src_id);
            out_cost = 0;
            return true;
        }

        struct State
        {
            int prev;   // previous tile position; -1 means initial blank
            int cur;    // current tile position
            int cost;   // cumulative movement cost
            int parent; // index in previous layer
        };

        std::vector<std::vector<State>> layers(steps + 1);
        layers[0].push_back({-1, src_id, 0, -1});
        const int blank_start = idx(blank_r_, blank_c_);

        for (int step = 0; step < steps; ++step)
        {
            std::unordered_map<long long, int> next_best;
            auto &cur_layer = layers[step];
            auto &next_layer = layers[step + 1];

            for (int si = 0; si < static_cast<int>(cur_layer.size()); ++si)
            {
                const State &st = cur_layer[si];
                const int cur_id = st.cur;
                const int cur_r = cur_id / n_;
                const int cur_c = cur_id % n_;
                const int cur_d = tile_dist_[cur_id];
                if (cur_d <= 0)
                    continue;

                const int blank_start_id = (st.prev == -1 ? blank_start : st.prev);
                for (int d = 0; d < 4; ++d)
                {
                    const int nr = cur_r + DR[d];
                    const int nc = cur_c + DC[d];
                    if (!inside(nr, nc))
                        continue;
                    const int nxt_id = idx(nr, nc);
                    if (fixed_[nxt_id])
                        continue;
                    if (tile_dist_[nxt_id] != cur_d - 1)
                        continue;

                    const int blank_len = bfsBlankLen(blank_start_id, nxt_id, cur_id);
                    if (blank_len < 0)
                        continue;

                    const int new_cost = st.cost + blank_len + 1;
                    const long long key = (static_cast<long long>(cur_id) << 32) ^
                                          static_cast<unsigned int>(nxt_id);
                    auto it = next_best.find(key);
                    if (it == next_best.end())
                    {
                        const int ni = static_cast<int>(next_layer.size());
                        next_layer.push_back({cur_id, nxt_id, new_cost, si});
                        next_best[key] = ni;
                    }
                    else
                    {
                        State &old = next_layer[it->second];
                        if (new_cost < old.cost)
                        {
                            old.cost = new_cost;
                            old.parent = si;
                        }
                    }
                }
            }
            if (next_layer.empty())
            {
                return false;
            }
        }

        int best_i = -1;
        int best_cost = std::numeric_limits<int>::max();
        auto &last_layer = layers[steps];
        for (int i = 0; i < static_cast<int>(last_layer.size()); ++i)
        {
            if (last_layer[i].cur != dst_id)
                continue;
            if (last_layer[i].cost < best_cost)
            {
                best_cost = last_layer[i].cost;
                best_i = i;
            }
        }
        if (best_i < 0)
            return false;
        out_cost = best_cost;

        out_route.assign(steps + 1, -1);
        int li = steps;
        int si = best_i;
        while (li >= 1)
        {
            const State &st = layers[li][si];
            out_route[li] = st.cur;
            si = st.parent;
            --li;
        }
        out_route[0] = src_id;
        return true;
    }

    void executePlannedRoute(const std::vector<int> &route)
    {
        std::vector<char> blank_path;
        for (int i = 0; i + 1 < static_cast<int>(route.size()); ++i)
        {
            const int cur_id = route[i];
            const int nxt_id = route[i + 1];
            const int cur_r = cur_id / n_;
            const int cur_c = cur_id % n_;
            const int nxt_r = nxt_id / n_;
            const int nxt_c = nxt_id % n_;

            if (!bfsBlankPath(nxt_r, nxt_c, cur_id, blank_path))
            {
                throw std::runtime_error("Cannot realize planned blank path");
            }
            for (char cmd : blank_path)
            {
                applyMove(cmd);
            }
            applyMove(cmdForTileMove(cur_r, cur_c, nxt_r, nxt_c));
        }
    }

    void moveTileToTargetPlanned(int sr, int sc, int tr, int tc)
    {
        std::vector<int> route;
        int route_cost = std::numeric_limits<int>::max();
        if (!planTileRoute(sr, sc, tr, tc, route, route_cost))
        {
            throw std::runtime_error("Cannot plan tile route");
        }
        executePlannedRoute(route);
    }

    void moveTileToTargetGreedy(int sr, int sc, int tr, int tc)
    {
        int cr = sr;
        int cc = sc;
        std::vector<char> blank_path;

        while (cr != tr || cc != tc)
        {
            const int cur_dist = tile_dist_[idx(cr, cc)];
            if (cur_dist <= 0)
            {
                throw std::runtime_error("Invalid tile distance state");
            }

            struct StepCandidate
            {
                int nr;
                int nc;
                int score;
            };
            std::vector<StepCandidate> candidates;

            for (int d = 0; d < 4; ++d)
            {
                const int nr = cr + DR[d];
                const int nc = cc + DC[d];
                if (!inside(nr, nc))
                    continue;
                const int nid = idx(nr, nc);
                if (fixed_[nid])
                    continue;
                if (tile_dist_[nid] != cur_dist - 1)
                    continue;
                const int score = manhattan(blank_r_, blank_c_, nr, nc);
                candidates.push_back({nr, nc, score});
            }

            if (candidates.empty())
            {
                throw std::runtime_error("No tile step candidate");
            }

            std::sort(candidates.begin(), candidates.end(), [](const StepCandidate &a, const StepCandidate &b)
                      { return a.score < b.score; });

            bool stepped = false;
            for (const auto &cand : candidates)
            {
                if (!bfsBlankPath(cand.nr, cand.nc, idx(cr, cc), blank_path))
                {
                    continue;
                }
                for (char cmd : blank_path)
                {
                    applyMove(cmd);
                }
                applyMove(cmdForTileMove(cr, cc, cand.nr, cand.nc));
                cr = cand.nr;
                cc = cand.nc;
                stepped = true;
                break;
            }

            if (!stepped)
            {
                throw std::runtime_error("Cannot move blank to support tile step");
            }
        }
    }

    void runSingleStrategy(const Strategy &strategy)
    {
        const auto order = buildOrder(strategy.fill_mode);

        for (const auto &[tr, tc] : order)
        {
            const int target_value = targetValueAt(tr, tc);
            if (board_[tr][tc] != target_value)
            {
                buildTileDist(tr, tc);

                struct SourceCandidate
                {
                    int r;
                    int c;
                    int dist;
                    int blank;
                    long long score;
                };
                std::vector<SourceCandidate> candidates;
                candidates.reserve(64);

                for (int r = 0; r < n_; ++r)
                {
                    for (int c = 0; c < n_; ++c)
                    {
                        if (fixed_[idx(r, c)])
                            continue;
                        if (board_[r][c] != target_value)
                            continue;
                        const int d = tile_dist_[idx(r, c)];
                        if (d < 0)
                            continue;

                        const int blank_score = manhattan(blank_r_, blank_c_, r, c);
                        long long score = static_cast<long long>(d) * 10000LL + blank_score;
                        if (strategy.advanced_score)
                        {
                            const int preserve_penalty =
                                (isCorrectTargetCell(r, c) && !(r == tr && c == tc)) ? 1 : 0;
                            const int linear_penalty = linearConflictPenaltyForSource(r, c, tr, tc);
                            const int in_target_zone_penalty = isTargetCell(r, c) ? 1 : 0;
                            const int slack = available_count_[target_value] - remaining_need_[target_value];
                            const int scarcity_weight = std::max(0, 20 - std::min(slack, 20));
                            score =
                                static_cast<long long>(d) * strategy.w_dist +
                                static_cast<long long>(preserve_penalty) *
                                    (strategy.w_preserve + strategy.scarcity_boost * scarcity_weight) +
                                static_cast<long long>(linear_penalty) * strategy.w_linear +
                                static_cast<long long>(in_target_zone_penalty) *
                                    (strategy.w_zone + (strategy.scarcity_boost * scarcity_weight) / 4) +
                                static_cast<long long>(blank_score) * strategy.w_blank;
                        }
                        candidates.push_back({r, c, d, blank_score, score});
                    }
                }

                if (candidates.empty())
                {
                    throw std::runtime_error("No reachable source tile for target");
                }

                std::sort(candidates.begin(), candidates.end(), [](const SourceCandidate &a, const SourceCandidate &b)
                          {
                    if (a.score != b.score) return a.score < b.score;
                    if (a.dist != b.dist) return a.dist < b.dist;
                    return a.blank < b.blank; });

                int best_r = candidates[0].r;
                int best_c = candidates[0].c;

                if (strategy.planned_route)
                {
                    const int top_k = std::max(1, std::min(strategy.source_top_k, static_cast<int>(candidates.size())));
                    int best_route_cost = std::numeric_limits<int>::max();
                    long long best_route_score = std::numeric_limits<long long>::max();
                    std::vector<int> best_route;
                    for (int i = 0; i < top_k; ++i)
                    {
                        std::vector<int> route;
                        int route_cost = std::numeric_limits<int>::max();
                        if (!planTileRoute(candidates[i].r, candidates[i].c, tr, tc, route, route_cost))
                        {
                            continue;
                        }
                        if (route_cost < best_route_cost ||
                            (route_cost == best_route_cost && candidates[i].score < best_route_score))
                        {
                            best_route_cost = route_cost;
                            best_route_score = candidates[i].score;
                            best_r = candidates[i].r;
                            best_c = candidates[i].c;
                            best_route = std::move(route);
                        }
                    }
                    if (!best_route.empty())
                    {
                        executePlannedRoute(best_route);
                    }
                    else
                    {
                        moveTileToTargetPlanned(best_r, best_c, tr, tc);
                    }
                }
                else
                {
                    moveTileToTargetGreedy(best_r, best_c, tr, tc);
                }
            }

            fixed_[idx(tr, tc)] = 1;
            --remaining_need_[target_value];
            --available_count_[target_value];
        }

        for (int r = 1; r <= n_ - 2; ++r)
        {
            for (int c = 1; c <= n_ - 2; ++c)
            {
                if (board_[r][c] != targetValueAt(r, c))
                {
                    throw std::runtime_error("Failed to match target center");
                }
            }
        }
    }

    void solve()
    {
        const std::array<Strategy, 8> base_strategies = {{
            {0, false, false, 10000, 1800, 250, 120, 10, 80, 1}, // 0 = วงแหวนในออกนอก
            {0, true, false, 10000, 2200, 400, 160, 10, 150, 1},
            {1, false, false, 10000, 1800, 250, 120, 10, 80, 1}, // 1 = วงแหวนนอกเข้าใน
            {1, true, false, 10000, 2200, 400, 160, 10, 150, 1},
            {0, true, true, 10000, 2200, 400, 160, 10, 150, 2},
            {1, true, true, 10000, 2200, 400, 160, 10, 150, 2},
            {0, true, true, 10000, 2600, 520, 220, 8, 180, 3},
            {1, true, true, 10000, 2600, 520, 220, 8, 180, 3},
        }};

        bool found = false;
        std::string best_moves;
        for (const auto &st : base_strategies)
        {
            resetState();
            try
            {
                runSingleStrategy(st);

                const std::string reduced = reduceMoves(moves_);
                if (!found || reduced.size() < best_moves.size())
                {
                    found = true;
                    best_moves = reduced;
                }
            }
            catch (const std::runtime_error &)
            {
            }
        }

        const int budget_ms = readTimeBudgetMs();
        const auto start = std::chrono::steady_clock::now();
        std::mt19937 rng(static_cast<unsigned int>(n_ * 10007 + max_color_ * 97 + 12345));
        int attempts = 0;

        while (true)
        {
            const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - start)
                                        .count();
            // หยุดการทำงานเมื่อหมดเวลาที่ตั้งไว้เท่านั้น
            if (elapsed_ms >= budget_ms)
                break;

            Strategy st;

            // เปลี่ยนมาสุ่มลำดับ 5 รูปแบบ! (0, 1, 2, 3, 4)
            st.fill_mode = rng() % 5;
            st.planned_route = ((rng() % 100) < 90);
            st.advanced_score = true;

            st.w_dist = 10000;
            st.w_preserve = static_cast<int>(rng() % 8000);
            st.w_linear = static_cast<int>(rng() % 4000);
            st.w_zone = static_cast<int>(rng() % 2000);
            st.w_blank = static_cast<int>(rng() % 1000);

            st.scarcity_boost = static_cast<int>(rng() % 1000);
            st.source_top_k = 1 + static_cast<int>(rng() % 6);

            resetState();
            try
            {
                runSingleStrategy(st);
                const std::string reduced = reduceMoves(moves_);
                if (!found || reduced.size() < best_moves.size())
                {
                    found = true;
                    best_moves = reduced;
                }
            }
            catch (const std::runtime_error &)
            {
            }
            ++attempts;

            // ลบโค้ดบรรทัด if (attempts >= 1024 && n_ < 51) break; ทิ้งไป
            // เพื่อให้มันสุ่มต่อไปเรื่อยๆ จนกว่าจะหมดเวลา Time Budget
        }

        if (!found)
        {
            throw std::runtime_error("No successful strategy");
        }
        moves_ = best_moves;
    }
};

int main()
{
    Solver solver;
    solver.run();
    return 0;
}
