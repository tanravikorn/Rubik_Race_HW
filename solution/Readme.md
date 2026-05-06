# Rubik Race / Large Sliding Puzzle Solver

## 1) Problem summary
We solve a large sliding-puzzle variant:
- Board size can be large (`N` up to ~67).
- One blank cell (`-1`) moves by swapping with a neighbor.
- Only the center region (`1..N-2`, `1..N-2`) must match the target matrix.
- Goal: minimize moves, output move string ending with `S`.

Main difficulty:
1. Huge state space.
2. Duplicate colors (many candidate source tiles for one target cell).
3. Local-greedy moves can trap future placements.

---

## 2) First idea and why it was not enough
### First idea
Greedy constructive solving:
1. Choose a target center cell.
2. Pick a matching-color tile (usually nearest).
3. Move that tile to target.
4. Lock cell and repeat.

### Limitation
Greedy local decisions often increase total moves later:
- Bad blank position after placement.
- Wrong tile choice among duplicates.
- Deadlocks around locked cells.

---

## 3) Optimization path
1. **Constrained target selection**  
   Prioritize difficult targets first (low slack colors).

2. **Weighted A\*** (`planMoveTileWAStar`)  
   Search on compact state `(blank_pos, moving_tile_pos)` with lock constraints.

3. **Multi-candidate evaluation (parallel when useful)**  
   Try top candidate source tiles and pick shortest successful route.

4. **Fallback + diversification**
   - Deterministic sequential fallback with rollback.
   - Stochastic local-improvement mode for stuck states.

5. **Path cleanup**
   - Remove immediate inverse pairs.
   - Remove loops that return to previous board states.

---

## 4) Main idea of final solution
**Hybrid strategy**:
1. Construct center solution cell-by-cell.
2. For each placement, run bounded informed search (Weighted A\*).
3. Keep progress with lock/rollback.
4. Use fallback stochastic search if constructive search stalls.
5. Compress final move sequence.

This balances:
- move quality,
- scalability,
- and robustness.

---

## 5) Full function reference (every function)

## File: `main.cpp`

### `int main()`
- **Purpose:** Thin entrypoint.
- **Behavior:** Calls `runSolver()` and returns its code.
- **Input/Output:** No direct parsing here.
- **Side effects:** None except delegating execution.

---

## File: `solver.hpp`

### `int runSolver();`
- **Purpose:** Public solver entry function declaration.
- **Defined in:** `solver.cpp`.

---

## File: `solver.cpp`

### Global constants
### `const int DR[4] = {-1, 1, 0, 0};`
### `const int DC[4] = {0, 0, -1, 1};`
### `const char CMD_FOR_BLANK_DIR[4] = {'D', 'U', 'R', 'L'};`
- **Purpose:** Direction vectors and output command mapping for blank moves.

### Global runtime state
### `int n`
### `std::vector<std::vector<int>> board`
### `std::vector<std::vector<int>> goal`
### `std::vector<std::vector<char>> lockedCell`
### `Pos blankPos`
### `std::string answer`
### `std::chrono::steady_clock::time_point startTime`
### `int timeLimitMs`
- **Purpose:** Shared mutable solver state used by all modules.

### `int runSolver()`
- **Purpose:** Main orchestration of full solve process.
- **Input parsing:**
  1. Reads `n`.
  2. Reads full board.
  3. Detects blank position.
  4. Reads center target `(n-2) x (n-2)`.
- **Main flow:**
  1. Early-exit if center already matched.
  2. If `n <= 9`: run `runFallbackSequential`, then `runStochasticFromInitial`.
  3. Else run constructive large-board solver:
     - initialize lock grid and time budget,
     - iteratively select constrained target,
     - build candidate source list for needed color,
     - evaluate candidates via `planMoveTileWAStar` (parallel if beneficial),
     - apply best route, lock successful target,
     - if no placement in round, unlock last lock (rollback).
  4. Optional fallback for small boards if constructive path failed.
  5. Final cleanup: `removeStateLoops` + `compressInverseMoves`, append `S`.
- **Output:** Prints move string ending with `S`, or `S` alone if unsolved.

---

## File: `solver_utils.hpp`

### `bool inBounds(int r, int c)`
- Checks if `(r,c)` is inside board `[0, n) x [0, n)`.

### `int manhattan(int r1, int c1, int r2, int c2)`
- Returns Manhattan distance `|r1-r2| + |c1-c2|`.

### `uint64_t packState(int blankId, int tileId)`
- Packs two 32-bit cell IDs into one 64-bit key for hash maps.

### `bool timeoutExceeded()`
- Compares elapsed wall-clock time (`now - startTime`) against `timeLimitMs`.

### `bool centerMatched()`
- Returns `true` only if every center cell equals target value.

### `bool applyBlankMove(int dir)`
- Tries to move blank using `DR/DC[dir]`.
- If out of bounds: returns `false`.
- If valid: swaps blank with neighbor, updates `blankPos`, appends command to `answer`, returns `true`.

### `std::string compressInverseMoves(const std::string& s)`
- Cancels immediate opposite pairs:
  - `U` with `D`, `L` with `R`.
- Produces shorter equivalent command string.

### `std::string boardSignature(const std::vector<std::vector<int>>& b)`
- Serializes board to comma-separated string.
- Used by loop-removal logic for state deduplication.

### `int commandToDir(char cmd)`
- Maps output move chars to direction index:
  - `D->0`, `U->1`, `R->2`, `L->3`.
- Returns `-1` for invalid command.

### `std::string removeStateLoops(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank, const std::string& moves)`
- Re-simulates the move sequence from initial state.
- Keeps a map from board signature to first seen move depth.
- If a signature repeats, removes the loop segment between occurrences.
- Returns loop-pruned move string.

### `bool isCenterCell(int r, int c)`
- Returns whether cell is in center objective region.

### `int mismatchAt(int r, int c, int value)`
- For center cells, returns `1` if `value` mismatches target at `(r,c)`, else `0`.
- For non-center cells returns `0`.

### `int centerMismatchCount()`
- Counts total mismatched center cells in current board.
- Used as stochastic mode score.

---

## File: `solver_search.hpp`

### `bool planMoveTileWAStar(const std::vector<std::vector<char>>& lockedGrid, Pos blankStart, Pos tileStart, Pos tileTarget, std::vector<int>& outDirs, int expansionLimit, int wNum, int wDen)`
- **Purpose:** Plan route to move one chosen tile to one target.
- **State model:** `(blank position, moving tile position)`.
- **Transitions:** Move blank one step; if blank enters tile cell, tile is pushed.
- **Search type:** Weighted A\* with priority `f = g + w*h` (scaled as integers via `wNum/wDen`).
- **Heuristic:** Tile-to-target Manhattan (weighted) + blank-to-tile Manhattan.
- **Constraints:**
  - Cannot cross locked cells.
  - Expansion cap (`expansionLimit`).
  - Timeout check via `timeoutExceeded()`.
  - Avoid immediate reverse direction to reduce oscillation.
- **Output:** `outDirs` as direction sequence if success.
- **Return:** `true` if route found, else `false`.

### `Pos chooseMostConstrainedTarget(const std::vector<std::vector<char>>& blockedTarget)`
- **Purpose:** Pick next target center cell to place.
- **Method:**
  1. Build pending target list (not locked and not blocked this round).
  2. Count remaining demand by color in pending targets.
  3. Count available unlocked tiles by color on board.
  4. Compute `slack = available[color] - remainingNeed[color]`.
  5. Choose minimum slack (most constrained), tie-break by blank distance.
- **Return:** Best target position or `{-1,-1}` if none.

---

## File: `solver_modes.hpp`

### `bool runFallbackSequential(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank)`
- **Purpose:** Deterministic fallback constructive solver (stronger search budget).
- **Behavior:**
  1. Reset board/blank/locks.
  2. Loop through rounds under timeout.
  3. Auto-lock already-correct center cells.
  4. For chosen constrained target:
     - collect matching-color candidates,
     - sort by target distance + blank distance,
     - run `planMoveTileWAStar(..., w=1)` on top candidates,
     - execute shortest successful route.
  5. If blocked, rollback one lock snapshot and retry.
- **Return:** `true` if center solved, else `false`.

### `bool runStochasticFromInitial(const std::vector<std::vector<int>>& initialBoard, Pos initialBlank)`
- **Purpose:** Randomized local-improvement fallback for escaping deterministic traps.
- **Behavior:**
  1. Multiple attempts until timeout.
  2. Reset to initial state each attempt.
  3. Optional random warmup moves for diversification.
  4. Hill-climb on mismatch score:
     - evaluate legal blank moves by local mismatch delta,
     - prefer improving/non-reversing moves,
     - occasionally randomize to escape local minima.
  5. If prolonged stall, perform random shake moves.
- **Return:** `true` when center mismatch becomes zero, else `false`.

---

## File: `solver_types.hpp`

### `struct Pos`
- `(r,c)` coordinate pair.

### `struct ParentInfo`
- Parent link record for path reconstruction in A\*:
  - `prevKey`: previous packed state.
  - `dir`: direction used to reach current state.

### `struct CandidateResult`
- Candidate planning result in parallel evaluation:
  - `ok`: search success flag.
  - `dirs`: resulting direction sequence.

### `extern ...` declarations
- Share constants, globals, and function declarations across modules.

---

## 6) Notes on local helper lambdas
Some lambdas are intentionally local (not global functions):
- `cellId` inside `planMoveTileWAStar`: row/col -> linear ID.
- `inv` inside stochastic mode: opposite direction (`d ^ 1`).
- Candidate sorting lambdas: rank source tiles per current target.

These are scoped to where they are used to keep global API small.

