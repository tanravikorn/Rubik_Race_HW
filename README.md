# How to Run sol.cpp

## Compilation
Compile the program with -O2 optimization for better performance:
```bash
g++ -O2 -o sol sol.cpp
```

Or with other compilers:
```bash
clang++ -O2 -o sol sol.cpp
```

**Note:** The `-O2` flag enables compiler optimizations that reduce runtime and memory usage, which is important for staying within cost and resource budgets during execution.

## Execution
Run the compiled program:
```bash
./sol < input_file > output_file
```

Or on Windows:
```bash
sol.exe < input_file > output_file
```

### With Budget Configuration
Set environment variables before running (see CONFIG.md):
```powershell
$env:SOLVE_TIME = 300        # Time budget (seconds)
$env:MEMORY_LIMIT = 512      # Memory budget (MB)
$env:COST_BUDGET = 100000    # Move count budget
./sol < testcase/1.in > out1.txt
```

## Input/Output
- The program reads input from standard input (stdin)
- Output is written to standard output (stdout)
- Input file format: Board size N, initial board state (N×N), target pattern ((N-2)×(N-2))
- Output format: Move sequence (U/D/L/R commands) followed by S

### Computation Time Budget
- **Large board sizes** (N up to 67) require efficient algorithms
- **Recommended approach**: A* with Manhattan distance heuristic
- Compile with `-O2` to maximize performance
- Since this is output-only, expensive offline computation is acceptable

### Memory Considerations
- Track visited states efficiently (consider bitmasking for smaller boards)
- Use priority queues for heuristic search
- Compile with `-O2` for memory optimization

## Requirements
- C++11 or later
- Standard C++ library
- Sufficient RAM for state exploration (varies by board size)

## Notes
- Ensure sol.cpp is in the same directory before compilation
- The output files (out1.txt through out6.txt) and compiled .exe files are ignored by version control
- This is a sliding puzzle optimization problem — aim to find solutions with move counts ≤ BASE for each test case
- **main.cpp** is a solver designed for the **5×5 board** test case (1.in)
