#pragma once

#include "solver_types.hpp"

bool inBounds(int r, int c) {
    return r >= 0 && r < n && c >= 0 && c < n;
}

int manhattan(int r1, int c1, int r2, int c2) {
    return std::abs(r1 - r2) + std::abs(c1 - c2);
}

uint64_t packState(int blankId, int tileId) {
    return (static_cast<uint64_t>(blankId) << 32) | static_cast<uint32_t>(tileId);
}

bool timeoutExceeded() {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
    return ms > timeLimitMs;
}

bool centerMatched() {
    for (int r = 1; r <= n - 2; ++r) {
        for (int c = 1; c <= n - 2; ++c) {
            if (board[r][c] != goal[r - 1][c - 1]) return false;
        }
    }
    return true;
}

bool applyBlankMove(int dir) {
    const int nr = blankPos.r + DR[dir];
    const int nc = blankPos.c + DC[dir];
    if (!inBounds(nr, nc)) return false;
    std::swap(board[blankPos.r][blankPos.c], board[nr][nc]);
    blankPos = {nr, nc};
    answer.push_back(CMD_FOR_BLANK_DIR[dir]);
    return true;
}

std::string compressInverseMoves(const std::string& s) {
    const auto inv = [](char a, char b) {
        return (a == 'U' && b == 'D') || (a == 'D' && b == 'U') ||
               (a == 'L' && b == 'R') || (a == 'R' && b == 'L');
    };
    std::string out;
    out.reserve(s.size());
    for (const char ch : s) {
        if (!out.empty() && inv(out.back(), ch)) out.pop_back();
        else out.push_back(ch);
    }
    return out;
}

std::string boardSignature(const std::vector<std::vector<int>>& b) {
    std::string sig;
    sig.reserve(static_cast<size_t>(n) * static_cast<size_t>(n) * 4 + 16);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            sig += std::to_string(b[r][c]);
            sig.push_back(',');
        }
    }
    return sig;
}

int commandToDir(char cmd) {
    if (cmd == 'D') return 0;
    if (cmd == 'U') return 1;
    if (cmd == 'R') return 2;
    if (cmd == 'L') return 3;
    return -1;
}

std::string removeStateLoops(
    const std::vector<std::vector<int>>& initialBoard,
    Pos initialBlank,
    const std::string& moves) {
    std::vector<std::vector<int>> simBoard = initialBoard;
    Pos simBlank = initialBlank;

    std::vector<char> moveStack;
    moveStack.reserve(moves.size());

    std::vector<std::string> sigStack;
    sigStack.reserve(moves.size() + 1);

    std::unordered_map<std::string, int> seen;
    seen.reserve(moves.size() * 2 + 8);

    const std::string startSig = boardSignature(simBoard);
    sigStack.push_back(startSig);
    seen[startSig] = 0;

    for (const char cmd : moves) {
        const int dir = commandToDir(cmd);
        if (dir < 0) continue;

        const int nr = simBlank.r + DR[dir];
        const int nc = simBlank.c + DC[dir];
        if (!inBounds(nr, nc)) continue;

        std::swap(simBoard[simBlank.r][simBlank.c], simBoard[nr][nc]);
        simBlank = {nr, nc};

        const std::string sig = boardSignature(simBoard);
        const auto it = seen.find(sig);
        if (it != seen.end()) {
            const int keepMoves = it->second;
            while (static_cast<int>(moveStack.size()) > keepMoves) {
                const std::string remSig = sigStack.back();
                sigStack.pop_back();
                seen.erase(remSig);
                moveStack.pop_back();
            }
            continue;
        }
        moveStack.push_back(cmd);
        sigStack.push_back(sig);
        seen[sig] = static_cast<int>(moveStack.size());
    }

    return std::string(moveStack.begin(), moveStack.end());
}

bool isCenterCell(int r, int c) {
    return r >= 1 && r <= n - 2 && c >= 1 && c <= n - 2;
}

int mismatchAt(int r, int c, int value) {
    if (!isCenterCell(r, c)) return 0;
    return value != goal[r - 1][c - 1];
}

int centerMismatchCount() {
    int bad = 0;
    for (int r = 1; r <= n - 2; ++r) {
        for (int c = 1; c <= n - 2; ++c) {
            bad += (board[r][c] != goal[r - 1][c - 1]);
        }
    }
    return bad;
}
