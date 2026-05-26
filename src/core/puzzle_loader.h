#pragma once

#include "board.h"
#include <vector>
#include <string>

// 从 81 字符串加载数独（支持 . 和 0 表示空格，忽略空白和分隔线）
Board loadFromString(const std::string& s);
// 从文件加载数独
Board loadFromFile(const std::string& filepath);

// 内置示例谜题，返回 [{名称, 81字符谜题}, ...]
std::vector<std::pair<std::string, std::string>> samplePuzzles();
