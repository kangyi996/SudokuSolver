#pragma once

#include <bitset>
#include <string>

// 9x9 数独棋盘，维护行列宫占用表以 O(1) 检查约束
class Board {
public:
    int grid[9][9];       // 0 = 空格
    bool given[9][9];     // 标记初始给定数字，不可修改

    Board();
    void clear();

    // 填入数字，若违反约束则返回 false
    bool place(int row, int col, int value);
    // 强制设置数字（绕过冲突检查，更新约束表；用于用户输入）
    void setCell(int row, int col, int value);
    // 擦除格内数字
    void erase(int row, int col);
    // 检查某格是否与同行/列/宫的其他格冲突（有重复数字）
    bool hasConflict(int row, int col) const;
    // 从 grid 完全重建行列宫约束表（修复因冲突/擦除导致的不一致）
    void rebuildConstraints();
    // 检查 (row,col) 填入 value 是否合法
    bool isValid(int row, int col, int value) const;

    // 计算某格的候选数字集合
    std::bitset<10> getCandidates(int row, int col) const;
    // 候选数个数（MRV 启发式用）
    int candidateCount(int row, int col) const;
    // 棋盘是否已填满
    bool isComplete() const;

    // 从 81 字符串加载 / 导出（. 或 0 = 空格）
    void fromString(const std::string& s);
    std::string toString() const;

    // 供 Solver 快速查询某数字是否在某行/列/宫中
    bool usedInRow(int r, int v) const { return rowUsed[r][v]; }
    bool usedInCol(int c, int v) const { return colUsed[c][v]; }
    bool usedInBox(int b, int v) const { return boxUsed[b][v]; }

private:
    bool rowUsed[9][10] = {};
    bool colUsed[9][10] = {};
    bool boxUsed[9][10] = {};

    static int boxIndex(int row, int col) {
        return (row / 3) * 3 + (col / 3);
    }
};
