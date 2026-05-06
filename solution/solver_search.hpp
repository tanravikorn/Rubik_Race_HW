#pragma once

#include "solver_types.hpp"

bool planMoveTileWAStar(
    const std::vector<std::vector<char>>& lockedGrid,
    Pos blankStart,
    Pos tileStart,
    Pos tileTarget,
    std::vector<int>& outDirs,
    int expansionLimit,
    int wNum,
    int wDen) {
    outDirs.clear();
    if (tileStart.r == tileTarget.r && tileStart.c == tileTarget.c) return true;

    const auto cellId = [](int r, int c) { return r * n + c; };

    const int bStart = cellId(blankStart.r, blankStart.c);
    const int tStart = cellId(tileStart.r, tileStart.c);
    const uint64_t startKey = packState(bStart, tStart);

    struct Node {
        int fScaled;
        int g;
        int br, bc;
        int tr, tc;
        int lastDir;
    };
    struct Cmp {
        bool operator()(const Node& a, const Node& b) const {
            return a.fScaled > b.fScaled;
        }
    };

    std::priority_queue<Node, std::vector<Node>, Cmp> pq;
    std::unordered_map<uint64_t, int> bestG;
    std::unordered_map<uint64_t, ParentInfo> parent;
    bestG.reserve(static_cast<size_t>(expansionLimit) * 2U + 64U);
    parent.reserve(static_cast<size_t>(expansionLimit) * 2U + 64U);

    const int h0 = manhattan(tileStart.r, tileStart.c, tileTarget.r, tileTarget.c) * 2 +
                   manhattan(blankStart.r, blankStart.c, tileStart.r, tileStart.c);
    pq.push({wNum * h0, 0, blankStart.r, blankStart.c, tileStart.r, tileStart.c, -1});
    bestG[startKey] = 0;

    bool found = false;
    uint64_t goalKey = 0;
    int expanded = 0;

    while (!pq.empty()) {
        if (timeoutExceeded()) return false;

        const Node cur = pq.top();
        pq.pop();

        const int bId = cellId(cur.br, cur.bc);
        const int tId = cellId(cur.tr, cur.tc);
        const uint64_t curKey = packState(bId, tId);

        const auto itBest = bestG.find(curKey);
        if (itBest == bestG.end() || itBest->second != cur.g) continue;

        if (cur.tr == tileTarget.r && cur.tc == tileTarget.c) {
            found = true;
            goalKey = curKey;
            break;
        }

        if (++expanded > expansionLimit) break;

        for (int d = 0; d < 4; ++d) {
            if (cur.lastDir != -1 && d == (cur.lastDir ^ 1)) continue;

            const int nbr = cur.br + DR[d];
            const int nbc = cur.bc + DC[d];
            if (!inBounds(nbr, nbc)) continue;
            if (lockedGrid[nbr][nbc]) continue;

            int ntr = cur.tr;
            int ntc = cur.tc;
            if (nbr == cur.tr && nbc == cur.tc) {
                ntr = cur.br;
                ntc = cur.bc;
            }

            const int nbId = cellId(nbr, nbc);
            const int ntId = cellId(ntr, ntc);
            const uint64_t nKey = packState(nbId, ntId);
            const int ng = cur.g + 1;

            const auto it = bestG.find(nKey);
            if (it != bestG.end() && ng >= it->second) continue;

            bestG[nKey] = ng;
            parent[nKey] = {curKey, static_cast<unsigned char>(d)};
            const int h = manhattan(ntr, ntc, tileTarget.r, tileTarget.c) * 2 +
                          manhattan(nbr, nbc, ntr, ntc);
            const int fScaled = ng * wDen + wNum * h;
            pq.push({fScaled, ng, nbr, nbc, ntr, ntc, d});
        }
    }

    if (!found) return false;

    std::vector<int> rev;
    for (uint64_t k = goalKey; k != startKey;) {
        const auto it = parent.find(k);
        if (it == parent.end()) return false;
        rev.push_back(static_cast<int>(it->second.dir));
        k = it->second.prevKey;
    }
    std::reverse(rev.begin(), rev.end());
    outDirs = std::move(rev);
    return true;
}

Pos chooseMostConstrainedTarget(const std::vector<std::vector<char>>& blockedTarget) {
    std::unordered_map<int, int> remainingNeed;
    std::unordered_map<int, int> available;
    std::vector<Pos> pending;
    pending.reserve((n - 2) * (n - 2));

    for (int r = 1; r <= n - 2; ++r) {
        for (int c = 1; c <= n - 2; ++c) {
            if (!lockedCell[r][c] && !blockedTarget[r][c]) {
                pending.push_back({r, c});
                remainingNeed[goal[r - 1][c - 1]]++;
            }
        }
    }
    if (pending.empty()) return {-1, -1};

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            if (!lockedCell[r][c] && board[r][c] != -1) available[board[r][c]]++;
        }
    }

    Pos best = pending.front();
    int bestSlack = INT_MAX;
    int bestBlankDist = INT_MAX;

    for (const Pos& p : pending) {
        const int need = goal[p.r - 1][p.c - 1];
        const int slack = (available.count(need) ? available[need] : 0) - remainingNeed[need];
        const int dBlank = manhattan(blankPos.r, blankPos.c, p.r, p.c);

        if (slack < bestSlack || (slack == bestSlack && dBlank < bestBlankDist)) {
            bestSlack = slack;
            bestBlankDist = dBlank;
            best = p;
        }
    }
    return best;
}
