#include "generator.h"
#include <algorithm>
#include <random>
#include <vector>

static std::mt19937 rng(std::random_device{}());

// ---- 工具 ----
static void shuffleVec(std::vector<int>& v) {
    std::shuffle(v.begin(), v.end(), rng);
}

// ---- 阶段一：生成完整终盘 ----
// 用随机化回溯填满棋盘
static bool fillBoard(Board& board) {
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (board.grid[r][c] != 0) continue;
            auto cand = board.getCandidates(r, c);
            std::vector<int> vals;
            for (int v = 1; v <= 9; v++)
                if (cand.test(v)) vals.push_back(v);
            shuffleVec(vals);

            for (int v : vals) {
                if (board.place(r, c, v)) {
                    if (fillBoard(board)) return true;
                    board.erase(r, c);
                }
            }
            return false;
        }
    }
    return true; // 全部填满
}

// ---- 阶段二：计算解的数量（最多算到 limit 就停止） ----
static int countSolutions(Board& board, int limit) {
    // 找 MRV 空格
    int bestR = -1, bestC = -1, bestCnt = 10;
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (board.grid[r][c] != 0) continue;
            int cnt = board.candidateCount(r, c);
            if (cnt == 0) return 0; // 矛盾
            if (cnt < bestCnt) { bestCnt = cnt; bestR = r; bestC = c; }
        }
    }
    if (bestR == -1) return 1; // 填满了，找到 1 个解

    int total = 0;
    auto cand = board.getCandidates(bestR, bestC);
    for (int v = 1; v <= 9 && total < limit; v++) {
        if (!cand.test(v)) continue;
        board.place(bestR, bestC, v);
        total += countSolutions(board, limit - total);
        board.erase(bestR, bestC);
    }
    return total;
}

// ---- 阶段三：挖空 ----
// 随机顺序尝试挖空格子，每次挖完后验证解唯一
static void digHoles(Board& board, int targetEmpty) {
    std::vector<int> indices(81);
    for (int i = 0; i < 81; i++) indices[i] = i;
    shuffleVec(indices);

    int currentEmpty = 0;
    for (int idx : indices) {
        if (currentEmpty >= targetEmpty) break;
        int r = idx / 9, c = idx % 9;
        if (board.grid[r][c] == 0) continue;

        int backup = board.grid[r][c];
        board.erase(r, c);

        Board test = board;
        int sols = countSolutions(test, 2);
        if (sols == 1) {
            currentEmpty++;
        } else {
            board.place(r, c, backup); // 恢复：挖掉后不唯一
        }
    }
}

// ---- 公开接口 ----
Board generatePuzzle(Difficulty diff) {
    Board board;

    // 先填 3 个对角宫（它们互相独立，加速生成）
    int boxes[3] = {0, 4, 8}; // 宫 0, 4, 8
    for (int bi : boxes) {
        int br = (bi / 3) * 3, bc = (bi % 3) * 3;
        std::vector<int> nums = {1,2,3,4,5,6,7,8,9};
        shuffleVec(nums);
        for (int i = 0; i < 9; i++) {
            int r = br + i / 3, c = bc + i % 3;
            board.place(r, c, nums[i]);
        }
    }

    // 填完剩余格子
    fillBoard(board);

    // 标记所有格为 "given"（初始），之后挖掉的会取消标记
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            board.given[r][c] = true;

    // 挖空
    digHoles(board, difficultyEmptyCells(diff));

    // 更新 given 标记：只有非空格子才是 given
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            board.given[r][c] = (board.grid[r][c] != 0);

    return board;
}

const char* difficultyName(Difficulty diff) {
    switch (diff) {
        case Difficulty::EASY:   return "简单";
        case Difficulty::MEDIUM: return "中等";
        case Difficulty::HARD:   return "困难";
        case Difficulty::EXPERT: return "专家";
    }
    return "";
}

int difficultyEmptyCells(Difficulty diff) {
    switch (diff) {
        case Difficulty::EASY:   return 38;
        case Difficulty::MEDIUM: return 46;
        case Difficulty::HARD:   return 52;
        case Difficulty::EXPERT: return 56;
    }
    return 46;
}
