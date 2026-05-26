#pragma once

#include "board.h"
#include <vector>
#include <string>
#include <stack>

enum class StepType {
    NAKED_SINGLE,     // 唯余法：某格只有一个候选数
    HIDDEN_SINGLE,    // 隐式唯一：某数字在某行/列/宫只出现在一格
    BACKTRACK_GUESS,  // 回溯猜测
    BACKTRACK_FAIL,   // 回溯失败（当前路径矛盾）
    DONE,             // 求解完成
    STUCK             // 约束传播无法推进
};

// 单步推理记录
struct Step {
    StepType type;
    int row = -1, col = -1, value = 0;
    std::string description;
};

// 数独求解器：约束传播 + MRV 回溯搜索
// 支持逐步推理 (step) 和批处理求解 (solveAll)
class Solver {
public:
    explicit Solver(Board& board);

    // 执行一步推理（优先约束传播，其次回溯猜测）
    // 返回 false 表示无法继续（矛盾或无解）
    bool step(Step& outStep);

    // 完整求解，一次性返回所有推理步骤
    bool solveAll(std::vector<Step>& outSteps);

    size_t backtrackDepth() const { return btStack.size(); }

    // 找候选数最少的空格（MRV 启发式）
    bool findMRVCell(int& outRow, int& outCol, std::vector<int>& outCandidates);
    // 循环执行约束传播直到无法推进
    void propagate(std::vector<Step>& outSteps);

private:
    Board& board;

    // ---- 约束传播 ----
    // 尝试找一个 Naked Single（唯余法）
    bool tryNakedSingle(Step& outStep);
    // 尝试找一个 Hidden Single（隐式唯一）
    bool tryHiddenSingle(Step& outStep);

    // ---- 回溯搜索 ----
    struct BacktrackFrame {
        Board snapshot;              // 猜测前的棋盘快照
        int row, col;                // 当前猜测的格子
        std::vector<int> candidates; // 候选数字列表
        int triedIndex = 0;          // 下次尝试的候选数索引
    };
    std::stack<BacktrackFrame> btStack;

    // 逐步模式的回溯：每次猜测一个候选数
    bool backtrackStep(Step& outStep);
    // 批量模式的回溯：递归求解
    bool backtrackSolve(std::vector<Step>& outSteps);
};

// 对棋盘计数解的数量（上限 limit），用于检测多解
int countSolutions(Board board, int limit);
