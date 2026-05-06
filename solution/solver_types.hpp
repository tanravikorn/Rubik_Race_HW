#pragma once

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <future>
#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

struct Pos {
    int r, c;
};

struct ParentInfo {
    uint64_t prevKey;
    unsigned char dir;
};

struct CandidateResult {
    bool ok = false;
    std::vector<int> dirs;
};

extern const int DR[4];
extern const int DC[4];
extern const char CMD_FOR_BLANK_DIR[4];

extern int n;
extern std::vector<std::vector<int>> board;
extern std::vector<std::vector<int>> goal;
extern std::vector<std::vector<char>> lockedCell;
extern Pos blankPos;
extern std::string answer;
extern std::chrono::steady_clock::time_point startTime;
extern int timeLimitMs;

bool inBounds(int r, int c);
int manhattan(int r1, int c1, int r2, int c2);
uint64_t packState(int blankId, int tileId);
bool timeoutExceeded();
bool centerMatched();
bool applyBlankMove(int dir);
std::string compressInverseMoves(const std::string& s);
std::string boardSignature(const std::vector<std::vector<int>>& b);
int commandToDir(char cmd);
std::string removeStateLoops(
    const std::vector<std::vector<int>>& initialBoard,
    Pos initialBlank,
    const std::string& moves);
bool isCenterCell(int r, int c);
int mismatchAt(int r, int c, int value);
int centerMismatchCount();
bool planMoveTileWAStar(
    const std::vector<std::vector<char>>& lockedGrid,
    Pos blankStart,
    Pos tileStart,
    Pos tileTarget,
    std::vector<int>& outDirs,
    int expansionLimit,
    int wNum = 3,
    int wDen = 2);
Pos chooseMostConstrainedTarget(const std::vector<std::vector<char>>& blockedTarget);
bool runFallbackSequential(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank);
bool runStochasticFromInitial(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank);
