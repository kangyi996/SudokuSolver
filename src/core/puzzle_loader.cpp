#include "puzzle_loader.h"
#include <fstream>
#include <sstream>

Board loadFromString(const std::string& s) {
    Board board;
    board.fromString(s);
    return board;
}

// 从文件读取数独（支持单行或多行格式）
Board loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return Board();
    std::ostringstream oss;
    oss << file.rdbuf();
    return loadFromString(oss.str());
}

// 返回 5 个内置示例谜题，覆盖简单到专家级
std::vector<std::pair<std::string, std::string>> samplePuzzles() {
    return {
        {"Easy 01",
         "530070000600195000098000060800060003400803001700020006060000280000419005000080079"},
        {"Medium 01",
         "000260701680070090190004500820100040004602900050003028009300074040050036703018000"},
        {"Hard 01",
         "000000000000003085001020000000507000004000100090000000500000073002010000000040009"},
        {"Expert (Inkala 2012)",
         "800000000003600000070090200050007000000045700000100030001000068008500010090000400"},
        {"Hard 02",
         "4.....8.5.3..........7......2.....6.....8.4......1.......6.3.7.5..2.....1.4......"},
    };
}
