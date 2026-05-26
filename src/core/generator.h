#pragma once

#include "board.h"
#include <string>

enum class Difficulty { EASY, MEDIUM, HARD, EXPERT };

// 生成一个完整有效的随机数独终盘，再按难度挖空保证唯一解
Board generatePuzzle(Difficulty diff);

// 难度对应的提示文字
const char* difficultyName(Difficulty diff);
// 难度对应的预估空格数
int difficultyEmptyCells(Difficulty diff);
