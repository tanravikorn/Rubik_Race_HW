#pragma once

#include "solver_types.hpp"

bool runFallbackSequential(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank) {
    board = initialBoard;
    blankPos = initialBlank;
    lockedCell.assign(n, std::vector<char>(n, 0));
    answer.clear();
    answer.reserve(300000);

    startTime = std::chrono::steady_clock::now();
    timeLimitMs = (n <= 9 ? 12000 : (n <= 15 ? 35000 : 5000));

    const int totalCenter = (n - 2) * (n - 2);
    int lockedCount = 0;
    struct LockSnapshot {
        std::vector<std::vector<int>> boardState;
        Pos blankState;
        size_t answerLen = 0;
        Pos lockedPos;
    };
    std::vector<LockSnapshot> lockHistory;
    lockHistory.reserve(totalCenter);

    int rounds = 0;
    const int maxRounds = totalCenter * 30 + 1200;

    while (lockedCount < totalCenter && rounds < maxRounds && !timeoutExceeded()) {
        rounds++;

        for (int r = 1; r <= n - 2; ++r) {
            for (int c = 1; c <= n - 2; ++c) {
                if (!lockedCell[r][c] && board[r][c] == goal[r - 1][c - 1]) {
                    lockHistory.push_back({board, blankPos, answer.size(), {r, c}});
                    lockedCell[r][c] = 1;
                    lockedCount++;
                }
            }
        }
        if (lockedCount >= totalCenter) break;

        bool placedOne = false;
        std::vector<std::vector<char>> blockedTarget(n, std::vector<char>(n, 0));
        const int maxTryThisRound = std::min(totalCenter - lockedCount, 20);

        for (int trIt = 0; trIt < maxTryThisRound && !placedOne && !timeoutExceeded(); ++trIt) {
            const Pos target = chooseMostConstrainedTarget(blockedTarget);
            if (target.r == -1) break;
            const int tr = target.r;
            const int tc = target.c;
            const int need = goal[tr - 1][tc - 1];

            if (board[tr][tc] == need) {
                lockHistory.push_back({board, blankPos, answer.size(), target});
                lockedCell[tr][tc] = 1;
                lockedCount++;
                placedOne = true;
                break;
            }

            std::vector<Pos> candidates;
            candidates.reserve(n);
            for (int r = 0; r < n; ++r) {
                for (int c = 0; c < n; ++c) {
                    if (!lockedCell[r][c] && board[r][c] == need) {
                        candidates.push_back({r, c});
                    }
                }
            }
            if (candidates.empty()) {
                blockedTarget[tr][tc] = 1;
                continue;
            }

            std::sort(candidates.begin(), candidates.end(), [&](const Pos& a, const Pos& b) {
                const int sa = manhattan(a.r, a.c, tr, tc) * 3 +
                               manhattan(blankPos.r, blankPos.c, a.r, a.c);
                const int sb = manhattan(b.r, b.c, tr, tc) * 3 +
                               manhattan(blankPos.r, blankPos.c, b.r, b.c);
                return sa < sb;
            });

            const int candidateLimit = std::min<int>(n <= 9 ? 20 : 16, candidates.size());
            const int expansionLimit = (n <= 9 ? 220000 : (n <= 15 ? 220000 : 60000));

            int bestIdx = -1;
            std::vector<int> bestDirs;
            for (int i = 0; i < candidateLimit; ++i) {
                std::vector<int> dirs;
                const bool ok = planMoveTileWAStar(
                    lockedCell, blankPos, candidates[i], target, dirs, expansionLimit, 1, 1);
                if (!ok) continue;
                if (bestIdx == -1 || dirs.size() < bestDirs.size()) {
                    bestIdx = i;
                    bestDirs = std::move(dirs);
                }
            }

            if (bestIdx == -1) {
                blockedTarget[tr][tc] = 1;
                continue;
            }

            auto savedBoard = board;
            const Pos savedBlank = blankPos;
            const size_t savedLen = answer.size();

            bool ok = true;
            for (const int d : bestDirs) {
                if (!applyBlankMove(d)) {
                    ok = false;
                    break;
                }
            }

            if (!ok || board[tr][tc] != need) {
                board = std::move(savedBoard);
                blankPos = savedBlank;
                answer.resize(savedLen);
                blockedTarget[tr][tc] = 1;
                continue;
            }

            lockedCell[tr][tc] = 1;
            lockHistory.push_back({savedBoard, savedBlank, savedLen, target});
            lockedCount++;
            placedOne = true;
        }

        if (placedOne) continue;

        if (lockHistory.empty()) break;

        LockSnapshot snap = std::move(lockHistory.back());
        lockHistory.pop_back();
        if (lockedCell[snap.lockedPos.r][snap.lockedPos.c]) {
            board = std::move(snap.boardState);
            blankPos = snap.blankState;
            answer.resize(snap.answerLen);
            lockedCell[snap.lockedPos.r][snap.lockedPos.c] = 0;
            lockedCount--;
        }
    }

    return centerMatched();
}

bool runStochasticFromInitial(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank) {
    startTime = std::chrono::steady_clock::now();
    timeLimitMs = (n <= 9 ? 12000 : (n <= 15 ? 25000 : 6000));

    std::mt19937 rng(static_cast<uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    const int attempts = (n <= 9 ? 20 : 18);
    const int baseSteps = (n <= 9 ? 30000 : 38000);

    for (int attempt = 0; attempt < attempts && !timeoutExceeded(); ++attempt) {
        board = initialBoard;
        blankPos = initialBlank;
        answer.clear();
        answer.reserve(350000);

        const int warmup = std::min(baseSteps / 20, attempt * (n + 9));
        for (int w = 0; w < warmup; ++w) {
            std::vector<int> opts;
            for (int d = 0; d < 4; ++d) {
                const int nr = blankPos.r + DR[d];
                const int nc = blankPos.c + DC[d];
                if (inBounds(nr, nc)) opts.push_back(d);
            }
            if (opts.empty()) break;
            const int d = opts[std::uniform_int_distribution<int>(0, static_cast<int>(opts.size()) - 1)(rng)];
            if (!applyBlankMove(d)) break;
        }

        int score = centerMismatchCount();
        if (score == 0) return true;

        int lastDir = -1;
        int stall = 0;
        const int stallLimit = std::max(120, n * n * 3);

        for (int step = 0; step < baseSteps && !timeoutExceeded(); ++step) {
            if (score == 0) return true;

            std::vector<int> dirs;
            dirs.reserve(4);
            for (int d = 0; d < 4; ++d) {
                const int nr = blankPos.r + DR[d];
                const int nc = blankPos.c + DC[d];
                if (inBounds(nr, nc)) dirs.push_back(d);
            }
            if (dirs.empty()) break;

            const auto inv = [](int d) { return d ^ 1; };
            const bool randomMode = (step % 29 == 0) ||
                                    (std::uniform_int_distribution<int>(0, 99)(rng) < 9) ||
                                    (stall > stallLimit && std::uniform_int_distribution<int>(0, 99)(rng) < 70);

            int chosen = -1;
            int bestScore = INT_MAX;

            if (randomMode) {
                chosen = dirs[std::uniform_int_distribution<int>(0, static_cast<int>(dirs.size()) - 1)(rng)];
            } else {
                for (const int d : dirs) {
                    if (lastDir != -1 && d == inv(lastDir) && dirs.size() > 1 &&
                        std::uniform_int_distribution<int>(0, 99)(rng) < 85) {
                        continue;
                    }

                    const int nr = blankPos.r + DR[d];
                    const int nc = blankPos.c + DC[d];
                    const int tileVal = board[nr][nc];

                    const int before = mismatchAt(blankPos.r, blankPos.c, -1) +
                                       mismatchAt(nr, nc, tileVal);
                    const int after = mismatchAt(blankPos.r, blankPos.c, tileVal) +
                                      mismatchAt(nr, nc, -1);
                    int candScore = score + (after - before);
                    if (lastDir != -1 && d == inv(lastDir)) candScore += 1;

                    if (candScore < bestScore ||
                        (candScore == bestScore &&
                         std::uniform_int_distribution<int>(0, 1)(rng) == 1)) {
                        bestScore = candScore;
                        chosen = d;
                    }
                }
                if (chosen == -1) {
                    chosen = dirs[std::uniform_int_distribution<int>(0, static_cast<int>(dirs.size()) - 1)(rng)];
                }
            }

            const int nr = blankPos.r + DR[chosen];
            const int nc = blankPos.c + DC[chosen];
            const int tileVal = board[nr][nc];
            const int before = mismatchAt(blankPos.r, blankPos.c, -1) + mismatchAt(nr, nc, tileVal);
            const int after = mismatchAt(blankPos.r, blankPos.c, tileVal) + mismatchAt(nr, nc, -1);
            score += (after - before);

            if (!applyBlankMove(chosen)) break;
            lastDir = chosen;

            if (after < before) stall = 0;
            else stall++;

            if (stall <= stallLimit * 2) continue;

            const int shake = std::max(14, n * 2);
            for (int k = 0; k < shake; ++k) {
                std::vector<int> opts;
                for (int d = 0; d < 4; ++d) {
                    const int rr = blankPos.r + DR[d];
                    const int cc = blankPos.c + DC[d];
                    if (inBounds(rr, cc)) opts.push_back(d);
                }
                if (opts.empty()) break;
                const int d = opts[std::uniform_int_distribution<int>(0, static_cast<int>(opts.size()) - 1)(rng)];
                const int rr = blankPos.r + DR[d];
                const int cc = blankPos.c + DC[d];
                const int tv = board[rr][cc];
                const int bfr = mismatchAt(blankPos.r, blankPos.c, -1) + mismatchAt(rr, cc, tv);
                const int afr = mismatchAt(blankPos.r, blankPos.c, tv) + mismatchAt(rr, cc, -1);
                score += (afr - bfr);
                if (!applyBlankMove(d)) break;
                lastDir = d;
            }
            stall = 0;
        }
        if (centerMatched()) return true;
    }
    return centerMatched();
}
