#include "solver.hpp"
#include "solver_modes.hpp"
#include "solver_search.hpp"
#include "solver_utils.hpp"

const int DR[4] = {-1, 1, 0, 0};
const int DC[4] = {0, 0, -1, 1};
const char CMD_FOR_BLANK_DIR[4] = {'D', 'U', 'R', 'L'};

int n = 0;
std::vector<std::vector<int>> board;
std::vector<std::vector<int>> goal;
std::vector<std::vector<char>> lockedCell;
Pos blankPos{-1, -1};
std::string answer;
std::chrono::steady_clock::time_point startTime;
int timeLimitMs = 4000;

int runSolver() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cin >> n;
    board.assign(n, std::vector<int>(n));
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            std::cin >> board[r][c];
            if (board[r][c] == -1) blankPos = {r, c};
        }
    }

    goal.assign(n - 2, std::vector<int>(n - 2));
    for (int r = 0; r < n - 2; ++r) {
        for (int c = 0; c < n - 2; ++c) std::cin >> goal[r][c];
    }

    const std::vector<std::vector<int>> initialBoard = board;
    const Pos initialBlank = blankPos;

    if (centerMatched()) {
        std::cout << "S\n";
        return 0;
    }

    bool solved = false;
    if (n <= 9) {
        solved = runFallbackSequential(initialBoard, initialBlank);
        if (!solved) solved = runStochasticFromInitial(initialBoard, initialBlank);
    } else {
        lockedCell.assign(n, std::vector<char>(n, 0));
        answer.clear();
        answer.reserve(250000);

        startTime = std::chrono::steady_clock::now();
        timeLimitMs = (n <= 15 ? 6000 : 4000);

        const int totalCenter = (n - 2) * (n - 2);
        int lockedCount = 0;
        std::vector<Pos> lockHistory;
        lockHistory.reserve(totalCenter);
        int rounds = 0;
        const int maxRounds = totalCenter * 6 + 400;

        while (lockedCount < totalCenter && rounds < maxRounds && !timeoutExceeded()) {
            rounds++;
            bool placedOne = false;
            std::vector<std::vector<char>> blockedTarget(n, std::vector<char>(n, 0));
            const int maxTryThisRound = std::min(totalCenter - lockedCount, 12);

            for (int trIt = 0; trIt < maxTryThisRound && !placedOne && !timeoutExceeded(); ++trIt) {
                const Pos target = chooseMostConstrainedTarget(blockedTarget);
                if (target.r == -1) break;
                const int tr = target.r;
                const int tc = target.c;
                const int need = goal[tr - 1][tc - 1];

                if (board[tr][tc] == need) {
                    lockedCell[tr][tc] = 1;
                    lockHistory.push_back(target);
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

                const int candidateLimit = std::min<int>(8, candidates.size());
                const int expansionLimit = (n <= 15 ? 70000 : 30000);

                std::vector<Pos> evalCandidates(candidates.begin(), candidates.begin() + candidateLimit);
                std::vector<CandidateResult> results(candidateLimit);

                unsigned int hc = std::thread::hardware_concurrency();
                if (hc == 0) hc = 4;
                const bool useParallel = (candidateLimit >= 3 && hc > 1);

                if (useParallel) {
                    std::vector<std::future<CandidateResult>> futures;
                    futures.reserve(candidateLimit);
                    for (int i = 0; i < candidateLimit; ++i) {
                        const Pos src = evalCandidates[i];
                        const Pos tg = target;
                        const Pos b0 = blankPos;
                        futures.push_back(std::async(std::launch::async, [&, src, tg, b0]() -> CandidateResult {
                            CandidateResult res;
                            std::vector<int> dirs;
                            res.ok = planMoveTileWAStar(lockedCell, b0, src, tg, dirs, expansionLimit);
                            if (res.ok) res.dirs = std::move(dirs);
                            return res;
                        }));
                    }
                    for (int i = 0; i < candidateLimit; ++i) {
                        results[i] = futures[i].get();
                    }
                } else {
                    for (int i = 0; i < candidateLimit; ++i) {
                        std::vector<int> dirs;
                        const bool ok = planMoveTileWAStar(
                            lockedCell, blankPos, evalCandidates[i], target, dirs, expansionLimit);
                        results[i].ok = ok;
                        if (ok) results[i].dirs = std::move(dirs);
                    }
                }

                int bestIdx = -1;
                size_t bestLen = static_cast<size_t>(-1);
                for (int i = 0; i < candidateLimit; ++i) {
                    if (!results[i].ok) continue;
                    if (results[i].dirs.size() < bestLen) {
                        bestLen = results[i].dirs.size();
                        bestIdx = i;
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
                for (const int d : results[bestIdx].dirs) {
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
                lockHistory.push_back(target);
                lockedCount++;
                placedOne = true;
            }

            if (placedOne) continue;

            if (!lockHistory.empty()) {
                const Pos u = lockHistory.back();
                lockHistory.pop_back();
                if (lockedCell[u.r][u.c]) {
                    lockedCell[u.r][u.c] = 0;
                    lockedCount--;
                }
            } else {
                break;
            }
        }

        solved = centerMatched();
        if (!solved && n <= 15) {
            solved = runFallbackSequential(initialBoard, initialBlank);
        }
        if (!solved && n <= 15) {
            solved = runStochasticFromInitial(initialBoard, initialBlank);
        }
    }

    if (solved) {
        answer = removeStateLoops(initialBoard, initialBlank, answer);
        answer = compressInverseMoves(answer);
        answer.push_back('S');
        std::cout << answer << '\n';
    } else {
        std::cout << "S\n";
    }
    return 0;
}
