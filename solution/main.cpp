#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

using namespace std;

int N;
vector<vector<int>> target_region;
// เก็บตำแหน่งเป้าหมายของเลขแต่ละตัวไว้ใน vector เพื่อความรวดเร็ว
vector<vector<pair<int, int>>> target_pos(101);

int calculate_h(const vector<vector<int>> &b)
{
    int h = 0;
    for (int i = 1; i < N - 1; ++i)
    {
        for (int j = 1; j < N - 1; ++j)
        {
            int val = b[i][j];
            if (val != -1)
            {
                // ดึงตำแหน่งของเลข val ใน target มาคำนวณระยะห่าง
                // หากเลขนั้นมีหลายตำแหน่ง ให้เลือกอันที่ใกล้ที่สุด
                int min_d = 1e9;
                for (auto &p : target_pos[val])
                {
                    int d = abs(i - (p.first + 1)) + abs(j - (p.second + 1));
                    if (d < min_d)
                        min_d = d;
                }
                h += min_d;
            }
        }
    }
    return h;
}

bool is_goal(const vector<vector<int>> &b)
{
    for (int i = 0; i < N - 2; ++i)
        for (int j = 0; j < N - 2; ++j)
            if (b[i + 1][j + 1] != target_region[i][j])
                return false;
    return true;
}

string path;
int dr[] = {1, -1, 0, 0};
int dc[] = {0, 0, 1, -1};
char moves[] = {'U', 'D', 'L', 'R'};
int min_f; // สำหรับข้าม threshold

bool dfs(vector<vector<int>> &b, int g, int threshold, int r, int c, int last_move_idx)
{
    int h = calculate_h(b);
    int f = g + h;
    if (f > threshold)
    {
        min_f = min(min_f, f);
        return false;
    }
    if (is_goal(b))
        return true;

    for (int i = 0; i < 4; ++i)
    {
        if ((last_move_idx == 0 && i == 1) || (last_move_idx == 1 && i == 0) ||
            (last_move_idx == 2 && i == 3) || (last_move_idx == 3 && i == 2))
            continue;

        int nr = r + dr[i], nc = c + dc[i];
        if (nr >= 0 && nr < N && nc >= 0 && nc < N)
        {
            swap(b[r][c], b[nr][nc]);
            path.push_back(moves[i]);
            if (dfs(b, g + 1, threshold, nr, nc, i))
                return true;
            path.pop_back();
            swap(b[r][c], b[nr][nc]);
        }
    }
    return false;
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cin >> N;
    vector<vector<int>> board(N, vector<int>(N));
    int br, bc;
    for (int i = 0; i < N; ++i)
    {
        for (int j = 0; j < N; ++j)
        {
            cin >> board[i][j];
            if (board[i][j] == -1)
            {
                br = i;
                bc = j;
            }
        }
    }
    target_region.assign(N - 2, vector<int>(N - 2));
    for (int i = 0; i < N - 2; ++i)
    {
        for (int j = 0; j < N - 2; ++j)
        {
            cin >> target_region[i][j];
            target_pos[target_region[i][j]].push_back({i, j});
        }
    }

    int threshold = calculate_h(board);
    while (threshold < 1000)
    {
        min_f = 1e9;
        if (dfs(board, 0, threshold, br, bc, -1))
        {
            cout << path.length() << ' ' << path << "S" << endl;
            return 0;
        }
        if (min_f == 1e9)
            break; // ไม่เจอทางออก
        threshold = min_f;
    }
    return 0;
}