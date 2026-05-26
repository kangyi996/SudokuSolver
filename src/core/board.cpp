#include "board.h"
#include <cstring>
#include <cctype>

Board::Board() {
    clear();
}

// 重置棋盘为全空状态
void Board::clear() {
    std::memset(grid, 0, sizeof(grid));
    std::memset(given, 0, sizeof(given));
    std::memset(rowUsed, 0, sizeof(rowUsed));
    std::memset(colUsed, 0, sizeof(colUsed));
    std::memset(boxUsed, 0, sizeof(boxUsed));
}

// 在 (row,col) 填入数字，若该格已有数字或违反数独约束则返回 false
bool Board::place(int row, int col, int value) {
    if (value < 1 || value > 9) return false;
    if (grid[row][col] != 0) return false;

    int b = boxIndex(row, col);
    if (rowUsed[row][value] || colUsed[col][value] || boxUsed[b][value])
        return false;

    grid[row][col] = value;
    rowUsed[row][value] = true;
    colUsed[col][value] = true;
    boxUsed[b][value] = true;
    return true;
}

// 强制设置数字——先擦再写，不检查约束（用于用户直接输入）
void Board::setCell(int row, int col, int value) {
    erase(row, col);
    if (value < 1 || value > 9) return;
    grid[row][col] = value;
    int b = boxIndex(row, col);
    rowUsed[row][value] = true;
    colUsed[col][value] = true;
    boxUsed[b][value] = true;
}

// 从 grid 完全重建约束表——修复因冲突/擦除可能造成的行列宫占用不一致
void Board::rebuildConstraints() {
    std::memset(rowUsed, 0, sizeof(rowUsed));
    std::memset(colUsed, 0, sizeof(colUsed));
    std::memset(boxUsed, 0, sizeof(boxUsed));
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int val = grid[r][c];
            if (val == 0) continue;
            int b = boxIndex(r, c);
            rowUsed[r][val] = true;
            colUsed[c][val] = true;
            boxUsed[b][val] = true;
        }
    }
}

// 扫描同行/列/宫是否有重复数字
bool Board::hasConflict(int row, int col) const {
    int val = grid[row][col];
    if (val == 0) return false;
    for (int c = 0; c < 9; c++)
        if (c != col && grid[row][c] == val) return true;
    for (int r = 0; r < 9; r++)
        if (r != row && grid[r][col] == val) return true;
    int br = (row / 3) * 3, bc = (col / 3) * 3;
    for (int r = br; r < br + 3; r++)
        for (int c = bc; c < bc + 3; c++)
            if ((r != row || c != col) && grid[r][c] == val) return true;
    return false;
}

void Board::erase(int row, int col) {
    int value = grid[row][col];
    if (value == 0) return;

    int b = boxIndex(row, col);
    grid[row][col] = 0;
    rowUsed[row][value] = false;
    colUsed[col][value] = false;
    boxUsed[b][value] = false;
}

// O(1) 检查在 (row,col) 填入 value 是否违反行列宫约束
bool Board::isValid(int row, int col, int value) const {
    if (value < 1 || value > 9) return false;
    int b = boxIndex(row, col);
    return !rowUsed[row][value] && !colUsed[col][value] && !boxUsed[b][value];
}

// 计算 (row,col) 的所有候选数字（排除同行列宫已填的）
std::bitset<10> Board::getCandidates(int row, int col) const {
    if (grid[row][col] != 0) return std::bitset<10>();
    std::bitset<10> cand;
    for (int v = 1; v <= 9; v++)
        if (isValid(row, col, v)) cand.set(v);
    return cand;
}

// 返回候选数字个数，供 MRV 启发式选择最优搜索分支
int Board::candidateCount(int row, int col) const {
    if (grid[row][col] != 0) return 0;
    int count = 0;
    for (int v = 1; v <= 9; v++)
        if (isValid(row, col, v)) count++;
    return count;
}

bool Board::isComplete() const {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (grid[r][c] == 0) return false;
    return true;
}

// 从 81 字符解析数独（跳过空格/换行/分隔线字符）
void Board::fromString(const std::string& s) {
    clear();
    int idx = 0;
    for (char ch : s) {
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t'
            || ch == '|' || ch == '-' || ch == '+')
            continue;
        if (idx >= 81) break;
        int row = idx / 9, col = idx % 9;
        if (ch >= '1' && ch <= '9') {
            place(row, col, ch - '0');
            given[row][col] = true;
        }
        idx++;
    }
}

std::string Board::toString() const {
    std::string result;
    result.reserve(81);
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            result += (grid[r][c] == 0) ? '.' : char('0' + grid[r][c]);
    return result;
}
