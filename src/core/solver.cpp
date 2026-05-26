#include "solver.h"
#include <algorithm>
#include <sstream>

Solver::Solver(Board& b) : board(b) {}

// ======== 约束传播 ========

bool Solver::tryNakedSingle(Step& outStep) {
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (board.grid[r][c] != 0) continue;
            auto cand = board.getCandidates(r, c);
            if (cand.count() == 1) {
                int val = 0;
                for (int v = 1; v <= 9; v++)
                    if (cand.test(v)) { val = v; break; }

                outStep.type = StepType::NAKED_SINGLE;
                outStep.row = r; outStep.col = c; outStep.value = val;

                std::ostringstream oss;
                oss << "R" << (r+1) << "C" << (c+1) << " = " << val
                    << "  【唯余法】该格仅剩 " << val << " 可填";
                outStep.description = oss.str();
                board.place(r, c, val);
                return true;
            }
        }
    }
    return false;
}

bool Solver::tryHiddenSingle(Step& outStep) {
    // 行
    for (int r = 0; r < 9; r++) {
        for (int v = 1; v <= 9; v++) {
            if (board.usedInRow(r, v)) continue;
            int foundCol = -1;
            for (int c = 0; c < 9; c++) {
                if (board.grid[r][c] != 0) continue;
                if (board.isValid(r, c, v)) {
                    if (foundCol == -1) foundCol = c;
                    else { foundCol = -2; break; }
                }
            }
            if (foundCol >= 0) {
                outStep.type = StepType::HIDDEN_SINGLE;
                outStep.row = r; outStep.col = foundCol; outStep.value = v;
                std::ostringstream oss;
                oss << "R" << (r+1) << "C" << (foundCol+1) << " = " << v
                    << "  【隐式唯一 行" << (r+1) << "】数字 " << v << " 仅此格可填";
                outStep.description = oss.str();
                board.place(r, foundCol, v);
                return true;
            }
        }
    }
    // 列
    for (int c = 0; c < 9; c++) {
        for (int v = 1; v <= 9; v++) {
            if (board.usedInCol(c, v)) continue;
            int foundRow = -1;
            for (int r = 0; r < 9; r++) {
                if (board.grid[r][c] != 0) continue;
                if (board.isValid(r, c, v)) {
                    if (foundRow == -1) foundRow = r;
                    else { foundRow = -2; break; }
                }
            }
            if (foundRow >= 0) {
                outStep.type = StepType::HIDDEN_SINGLE;
                outStep.row = foundRow; outStep.col = c; outStep.value = v;
                std::ostringstream oss;
                oss << "R" << (foundRow+1) << "C" << (c+1) << " = " << v
                    << "  【隐式唯一 列" << (c+1) << "】数字 " << v << " 仅此格可填";
                outStep.description = oss.str();
                board.place(foundRow, c, v);
                return true;
            }
        }
    }
    // 宫
    for (int br = 0; br < 3; br++) {
        for (int bc = 0; bc < 3; bc++) {
            int bi = br * 3 + bc;
            for (int v = 1; v <= 9; v++) {
                if (board.usedInBox(bi, v)) continue;
                int foundR = -1, foundC = -1;
                for (int r = br * 3; r < br * 3 + 3 && foundR != -2; r++) {
                    for (int c = bc * 3; c < bc * 3 + 3; c++) {
                        if (board.grid[r][c] != 0) continue;
                        if (board.isValid(r, c, v)) {
                            if (foundR == -1) { foundR = r; foundC = c; }
                            else { foundR = -2; break; }
                        }
                    }
                }
                if (foundR >= 0) {
                    outStep.type = StepType::HIDDEN_SINGLE;
                    outStep.row = foundR; outStep.col = foundC; outStep.value = v;
                    std::ostringstream oss;
                    oss << "R" << (foundR+1) << "C" << (foundC+1) << " = " << v
                        << "  【隐式唯一 宫" << (bi+1) << "】数字 " << v << " 仅此格可填";
                    outStep.description = oss.str();
                    board.place(foundR, foundC, v);
                    return true;
                }
            }
        }
    }
    return false;
}

void Solver::propagate(std::vector<Step>& outSteps) {
    Step s;
    while (tryNakedSingle(s) || tryHiddenSingle(s))
        outSteps.push_back(s);
}

// ======== 单步推理入口 ========

bool Solver::step(Step& outStep) {
    if (tryNakedSingle(outStep)) return true;
    if (tryHiddenSingle(outStep)) return true;
    if (board.isComplete()) {
        outStep.type = StepType::DONE;
        outStep.description = "求解完成！";
        return true;
    }
    return backtrackStep(outStep);
}

// ======== 回溯搜索 ========

bool Solver::findMRVCell(int& outRow, int& outCol, std::vector<int>& outCandidates) {
    int bestCount = 10;
    outRow = -1;
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (board.grid[r][c] != 0) continue;
            auto cand = board.getCandidates(r, c);
            int cnt = (int)cand.count();
            if (cnt == 0) {
                outRow = r; outCol = c;
                outCandidates.clear();
                return true;
            }
            if (cnt < bestCount) {
                bestCount = cnt;
                outRow = r; outCol = c;
            }
        }
    }
    if (outRow >= 0) {
        auto cand = board.getCandidates(outRow, outCol);
        for (int v = 1; v <= 9; v++)
            if (cand.test(v)) outCandidates.push_back(v);
        return true;
    }
    return false;
}

bool Solver::backtrackStep(Step& outStep) {
    if (btStack.empty()) {
        int r, c;
        std::vector<int> cands;
        if (!findMRVCell(r, c, cands)) {
            outStep.type = StepType::STUCK;
            outStep.description = "无法继续";
            return false;
        }
        if (cands.empty()) {
            outStep.type = StepType::BACKTRACK_FAIL;
            outStep.description = "矛盾：某格无候选数";
            return false;
        }
        BacktrackFrame frame;
        frame.snapshot = board;
        frame.row = r; frame.col = c;
        frame.candidates = cands;
        frame.triedIndex = 0;
        btStack.push(frame);
    }

    BacktrackFrame& top = btStack.top();

    if (top.triedIndex < (int)top.candidates.size()) {
        int val = top.candidates[top.triedIndex++];
        board = top.snapshot;
        board.place(top.row, top.col, val);

        outStep.type = StepType::BACKTRACK_GUESS;
        outStep.row = top.row; outStep.col = top.col; outStep.value = val;

        std::ostringstream oss;
        oss << "R" << (top.row+1) << "C" << (top.col+1)
            << " = " << val << "  【回溯猜测 #" << top.triedIndex
            << ", 深度 " << btStack.size() << "】";
        outStep.description = oss.str();

        std::vector<Step> propagated;
        propagate(propagated);
        if (!propagated.empty())
            outStep.description += " → 触发约束传播 " + std::to_string(propagated.size()) + " 步";

        return true;
    } else {
        outStep.type = StepType::BACKTRACK_FAIL;
        board = top.snapshot;
        std::ostringstream oss;
        oss << "R" << (top.row+1) << "C" << (top.col+1)
            << " 回溯，深度 " << (btStack.size() - 1);
        outStep.description = oss.str();
        btStack.pop();
        if (btStack.empty()) {
            outStep.description += " — 该谜题无解！";
            return false;
        }
        return backtrackStep(outStep);
    }
}

// ======== 批处理求解 ========

bool Solver::backtrackSolve(std::vector<Step>& outSteps) {
    if (board.isComplete()) return true;

    int r, c;
    std::vector<int> cands;
    if (!findMRVCell(r, c, cands)) return true;
    if (cands.empty()) return false;

    for (int val : cands) {
        Board saved = board;
        {
            Step s;
            s.type = StepType::BACKTRACK_GUESS;
            s.row = r; s.col = c; s.value = val;
            std::ostringstream oss;
            oss << "R" << (r+1) << "C" << (c+1) << " = " << val
                << "  【回溯猜测 " << cands.size() << "选1】";
            s.description = oss.str();
            outSteps.push_back(s);
        }
        board.place(r, c, val);
        propagate(outSteps);
        if (backtrackSolve(outSteps)) return true;
        board = saved;
        Step fs;
        fs.type = StepType::BACKTRACK_FAIL;
        fs.row = r; fs.col = c; fs.value = val;
        fs.description = "R" + std::to_string(r+1) + "C" + std::to_string(c+1)
                       + " = " + std::to_string(val) + " 失败，回溯";
        outSteps.push_back(fs);
    }
    return false;
}

bool Solver::solveAll(std::vector<Step>& outSteps) {
    outSteps.clear();
    propagate(outSteps);
    if (board.isComplete()) {
        Step s;
        s.type = StepType::DONE;
        s.description = "仅通过约束传播即完成求解！";
        outSteps.push_back(s);
        return true;
    }
    bool ok = backtrackSolve(outSteps);
    Step s;
    s.type = StepType::DONE;
    s.description = ok ? "求解完成！" : "该谜题无解！请检查输入是否有冲突。";
    outSteps.push_back(s);
    return ok;
}

// 对棋盘计数解的数量（上限 limit）
int countSolutions(Board board, int limit) {
    // 先约束传播
    {
        Solver s(board);
        std::vector<Step> dummy;
        s.propagate(dummy);
    }
    if (board.isComplete()) return 1;

    // MRV 选格
    int r = -1, c = -1;
    std::vector<int> cands;
    {
        Solver s(board);
        if (!s.findMRVCell(r, c, cands) || cands.empty()) return 0;
    }

    int count = 0;
    for (int val : cands) {
        Board copy = board;
        copy.place(r, c, val);
        int sub = countSolutions(copy, limit - count);
        count += sub;
        if (count >= limit) break;
    }
    return count;
}
