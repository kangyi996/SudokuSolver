#include <windows.h>
#include <GL/gl.h>

// MinGW GL/gl.h 只定义了 GL_VERSION_1_1 但不提供 PFNGL* 类型，
// 导致 ImGui 的 loader 跳过 GL 1.1 段落的 typedef。手动补齐。
#ifndef GL_VERSION_1_1
#error "Expected GL_VERSION_1_1 to be defined"
#endif
#undef GL_VERSION_1_1
#ifndef GLsizeiptr
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#endif

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cfloat>
#include <chrono>

#include "core/board.h"
#include "core/solver.h"
#include "core/generator.h"

#pragma comment(lib, "opengl32.lib")

// ======== 棋盘颜色（applyTheme 中设定）=======
ImU32 C_BG, C_BOARD, C_THIN, C_THICK, C_GIVEN, C_SOLVED,
      C_SEL, C_REL, C_HOVER, C_CAND, C_CONFLICT_BG, C_CONFLICT_FG;
ImFont* g_font = nullptr;  // 中文字体，用于棋盘数字渲染

// ======== 状态 ========
struct AppState {
    Board initial, current;
    Solver* solver = nullptr;
    int selR = -1, selC = -1;
    int hoverR = -1, hoverC = -1;
    bool showCandidates = false;
    bool conflict[9][9] = {};
    std::vector<Step> steps;
    int lastStepCount = 0;
    char statusMsg[256] = "点击「生成新谜题」开始，或直接在棋盘上填数后求解。";
    int difficulty = 0;

    // 深色模式
    bool darkMode = false;

    // 计时器（0=停止, 1=运行中, 2=暂停）
    using Clock = std::chrono::steady_clock;
    int timerState = 0;            // 0=stopped, 1=running, 2=paused
    Clock::time_point segStart;    // 当前段开始时间
    double accumSec = 0.0;         // 已累计秒数（不含当前段）

    double getElapsed() const {
        if (timerState == 1)
            return accumSec + std::chrono::duration<double>(Clock::now() - segStart).count();
        return accumSec;
    }
    void timerStart() {
        if (timerState == 0) accumSec = 0.0;
        segStart = Clock::now(); timerState = 1;
    }
    void timerPause() {
        if (timerState != 1) return;
        accumSec += std::chrono::duration<double>(Clock::now() - segStart).count();
        timerState = 2;
    }
    void timerStop()  { accumSec = 0.0; timerState = 0; }
    void timerResume(){ segStart = Clock::now(); timerState = 1; }
};
static AppState g;

// ======== 动态布局（每帧根据可用空间重算）=======
static float g_cell  = 52.0f;
static float g_thin  = 1.0f;
static float g_thick = 2.5f;

float cellX(int c) {
    float x = 0;
    for (int i = 0; i < c; i++) x += g_cell + ((i % 3 == 2) ? g_thick : g_thin);
    return x;
}
float cellY(int r) {
    float y = 0;
    for (int i = 0; i < r; i++) y += g_cell + ((i % 3 == 2) ? g_thick : g_thin);
    return y;
}
float boardW() { return g_cell * 9 + g_thin * 6 + g_thick * 4; }
float boardH() { return boardW(); }

bool hitCell(float mx, float my, int& r, int& c) {
    for (r = 0; r < 9; r++)
        for (c = 0; c < 9; c++)
            if (mx >= cellX(c) && mx < cellX(c) + g_cell &&
                my >= cellY(r) && my < cellY(r) + g_cell)
                return true;
    return false;
}

void computeLayout(float availW, float availH) {
    float pad = 20.0f;
    float avail = std::min(availW, availH) - pad * 2;
    if (avail < 200.0f) avail = 200.0f;
    g_cell = (avail - g_thin * 6 - g_thick * 4) / 9.0f;
    if (g_cell < 28.0f) g_cell = 28.0f;
    if (g_cell > 90.0f) g_cell = 90.0f;
    // 细线至少 1px，粗线按比例但至少 2px
    g_thin  = std::max(1.0f, g_cell / 52.0f);
    g_thick = std::max(2.0f, g_cell / 22.0f);
}

void resetSolver() {
    delete g.solver;
    g.current = g.initial;
    g.current.rebuildConstraints(); // 修复因用户输入/擦除可能导致的约束表不一致
    g.solver = new Solver(g.current);
    g.steps.clear();
    g.lastStepCount = 0;
}

// 扫描整个棋盘，更新冲突标记
void updateConflicts() {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            g.conflict[r][c] = g.initial.hasConflict(r, c);
}

// 检查整个棋盘是否有冲突
bool hasAnyConflict() {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (g.conflict[r][c]) return true;
    return false;
}

// 检查棋盘是否全空
bool isBoardEmpty() {
    for (int r = 0; r < 9; r++)
        for (int c = 0; c < 9; c++)
            if (g.initial.grid[r][c] != 0) return false;
    return true;
}

void loadPuzzle(const Board& b, const char* msg) {
    g.initial = b;
    g.initial.rebuildConstraints();
    updateConflicts();
    resetSolver();
    g.selR = g.selC = -1;
    snprintf(g.statusMsg, sizeof(g.statusMsg), "%s", msg);
}

// ======== 深色/浅色主题（同时更新 ImGui 样式和棋盘颜色）=======
void applyTheme(bool dark) {
    ImGuiStyle& st = ImGui::GetStyle();
    if (dark) {
        ImGui::StyleColorsDark();
        st.Colors[ImGuiCol_WindowBg]        = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
        st.Colors[ImGuiCol_ChildBg]         = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
        st.Colors[ImGuiCol_Button]          = ImVec4(0.25f, 0.40f, 0.70f, 1.0f);
        st.Colors[ImGuiCol_ButtonHovered]   = ImVec4(0.32f, 0.50f, 0.80f, 1.0f);
        st.Colors[ImGuiCol_ButtonActive]    = ImVec4(0.20f, 0.32f, 0.62f, 1.0f);
        st.Colors[ImGuiCol_Text]            = ImVec4(0.90f, 0.90f, 0.94f, 1.0f);
        st.Colors[ImGuiCol_Header]          = ImVec4(0.30f, 0.42f, 0.70f, 0.6f);
        st.Colors[ImGuiCol_HeaderHovered]   = ImVec4(0.35f, 0.50f, 0.75f, 0.7f);
        st.Colors[ImGuiCol_FrameBg]         = ImVec4(0.22f, 0.22f, 0.28f, 1.0f);
        st.Colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.28f, 0.28f, 0.35f, 1.0f);
        st.Colors[ImGuiCol_CheckMark]       = ImVec4(0.35f, 0.65f, 1.0f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarBg]     = ImVec4(0.08f, 0.08f, 0.10f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarGrab]   = ImVec4(0.25f, 0.25f, 0.32f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.42f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.40f, 0.48f, 1.0f);
        st.Colors[ImGuiCol_Separator]       = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);
        st.Colors[ImGuiCol_TitleBg]         = ImVec4(0.06f, 0.06f, 0.08f, 1.0f);
        st.Colors[ImGuiCol_TitleBgActive]   = ImVec4(0.10f, 0.10f, 0.14f, 1.0f);
        st.Colors[ImGuiCol_PopupBg]         = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
        // 棋盘颜色 - 深色
        C_BG       = IM_COL32(30,30,35,255);
        C_BOARD    = IM_COL32(42,42,50,255);
        C_THIN     = IM_COL32(70,70,80,255);
        C_THICK    = IM_COL32(180,180,190,255);
        C_GIVEN    = IM_COL32(225,225,235,255);
        C_SOLVED   = IM_COL32(90,160,255,255);
        C_SEL      = IM_COL32(55,65,95,255);
        C_REL      = IM_COL32(48,52,70,255);
        C_HOVER    = IM_COL32(100,140,220,255);
        C_CAND     = IM_COL32(130,140,160,255);
        C_CONFLICT_BG = IM_COL32(140,50,55,255);
        C_CONFLICT_FG = IM_COL32(255,140,140,255);
    } else {
        ImGui::StyleColorsLight();
        st.Colors[ImGuiCol_WindowBg]        = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
        st.Colors[ImGuiCol_ChildBg]         = ImVec4(0.97f, 0.97f, 0.99f, 1.0f);
        st.Colors[ImGuiCol_Button]          = ImVec4(0.40f, 0.55f, 0.90f, 1.0f);
        st.Colors[ImGuiCol_ButtonHovered]   = ImVec4(0.30f, 0.45f, 0.85f, 1.0f);
        st.Colors[ImGuiCol_ButtonActive]    = ImVec4(0.25f, 0.38f, 0.78f, 1.0f);
        st.Colors[ImGuiCol_Text]            = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
        st.Colors[ImGuiCol_Header]          = ImVec4(0.55f, 0.65f, 0.90f, 0.6f);
        st.Colors[ImGuiCol_HeaderHovered]   = ImVec4(0.45f, 0.55f, 0.85f, 0.7f);
        st.Colors[ImGuiCol_FrameBg]         = ImVec4(0.75f, 0.75f, 0.80f, 1.0f);
        st.Colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.65f, 0.70f, 0.80f, 1.0f);
        st.Colors[ImGuiCol_CheckMark]       = ImVec4(0.15f, 0.45f, 0.90f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarBg]     = ImVec4(0.93f, 0.93f, 0.95f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarGrab]   = ImVec4(0.70f, 0.70f, 0.75f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
        st.Colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
        st.Colors[ImGuiCol_Separator]       = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
        st.Colors[ImGuiCol_TitleBg]         = ImVec4(0.90f, 0.90f, 0.93f, 1.0f);
        st.Colors[ImGuiCol_TitleBgActive]   = ImVec4(0.85f, 0.85f, 0.88f, 1.0f);
        st.Colors[ImGuiCol_PopupBg]         = ImVec4(0.97f, 0.97f, 0.99f, 1.0f);
        // 棋盘颜色 - 浅色
        C_BG       = IM_COL32(245,245,250,255);
        C_BOARD    = IM_COL32(255,255,255,255);
        C_THIN     = IM_COL32(195,195,205,255);
        C_THICK    = IM_COL32(45,45,55,255);
        C_GIVEN    = IM_COL32(20,20,30,255);
        C_SOLVED   = IM_COL32(37,99,235,255);
        C_SEL      = IM_COL32(210,225,255,255);
        C_REL      = IM_COL32(240,244,255,255);
        C_HOVER    = IM_COL32(180,200,240,255);
        C_CAND     = IM_COL32(150,160,180,255);
        C_CONFLICT_BG = IM_COL32(255,220,220,255);
        C_CONFLICT_FG = IM_COL32(200,40,40,255);
    }
    st.WindowRounding = 6;
    st.ChildRounding = 6;
    st.FrameRounding = 4;
    st.GrabRounding = 4;
    st.ScrollbarRounding = 6;
}

// ======== 计时器格式化 ========
std::string formatTime(double sec) {
    if (sec < 60) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f 秒", sec);
        return buf;
    }
    int m = (int)(sec / 60);
    int s = (int)(sec) % 60;
    if (m < 60) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d 分 %d 秒", m, s);
        return buf;
    }
    int h = m / 60;
    m %= 60;
    char buf[48];
    snprintf(buf, sizeof(buf), "%d 时 %d 分 %d 秒", h, m, s);
    return buf;
}



// ======== 绘制棋盘 ========
void drawBoard(ImDrawList* dl, ImVec2 org) {
    dl->AddRectFilled(org, ImVec2(org.x + boardW(), org.y + boardH()), C_BOARD, 4.0f);

    // 冲突高亮
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            if (g.conflict[r][c]) {
                dl->AddRectFilled(
                    ImVec2(org.x + cellX(c) + 1, org.y + cellY(r) + 1),
                    ImVec2(org.x + cellX(c) + g_cell, org.y + cellY(r) + g_cell),
                    C_CONFLICT_BG);
            }
        }
    }

    // 选中格的行列宫高亮
    if (g.selR >= 0 && g.selC >= 0) {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (r == g.selR && c == g.selC) continue;
                bool related = (r == g.selR) || (c == g.selC) ||
                               (r / 3 == g.selR / 3 && c / 3 == g.selC / 3);
                if (related && !g.conflict[r][c])
                    dl->AddRectFilled(
                        ImVec2(org.x + cellX(c) + 1, org.y + cellY(r) + 1),
                        ImVec2(org.x + cellX(c) + g_cell, org.y + cellY(r) + g_cell),
                        C_REL);
            }
        }
        if (!g.conflict[g.selR][g.selC])
            dl->AddRectFilled(
                ImVec2(org.x + cellX(g.selC) + 1, org.y + cellY(g.selR) + 1),
                ImVec2(org.x + cellX(g.selC) + g_cell, org.y + cellY(g.selR) + g_cell),
                C_SEL);
    }

    // hover 边框
    if (g.hoverR >= 0 && g.hoverC >= 0 &&
        !(g.hoverR == g.selR && g.hoverC == g.selC)) {
        dl->AddRect(
            ImVec2(org.x + cellX(g.hoverC) + 1, org.y + cellY(g.hoverR) + 1),
            ImVec2(org.x + cellX(g.hoverC) + g_cell, org.y + cellY(g.hoverR) + g_cell),
            C_HOVER, 0, 0, 2.0f);
    }

    // 数字 + 候选数（主数字大号，候选数小号）
    float numSize  = g_cell * 0.58f;   // 主数字字号
    float candSize = g_cell * 0.23f;   // 候选数字号
    for (int r = 0; r < 9; r++) {
        for (int c = 0; c < 9; c++) {
            int val = g.current.grid[r][c];
            float cx = org.x + cellX(c), cy = org.y + cellY(r);
            if (val) {
                bool isInitial = (g.initial.grid[r][c] != 0);
                bool isConflict = g.conflict[r][c];
                ImU32 color = isConflict ? C_CONFLICT_FG :
                              (isInitial ? C_GIVEN : C_SOLVED);
                char buf[2] = {char('0' + val), 0};
                ImVec2 ts = g_font->CalcTextSizeA(numSize, FLT_MAX, -1, buf);
                dl->AddText(g_font, numSize,
                    ImVec2(cx + (g_cell - ts.x) * 0.5f, cy + (g_cell - ts.y) * 0.5f),
                    color, buf);
            } else if (g.showCandidates) {
                auto cand = g.current.getCandidates(r, c);
                for (int v = 1; v <= 9; v++) {
                    if (!cand.test(v)) continue;
                    int cr = (v - 1) / 3, cc = (v - 1) % 3;
                    char buf[2] = {char('0' + v), 0};
                    ImVec2 ts = g_font->CalcTextSizeA(candSize, FLT_MAX, -1, buf);
                    dl->AddText(g_font, candSize,
                        ImVec2(cx + cc * g_cell / 3 + (g_cell / 3 - ts.x) * 0.5f,
                               cy + cr * g_cell / 3 + (g_cell / 3 - ts.y) * 0.5f),
                        C_CAND, buf);
                }
            }
        }
    }

    // 网格线
    for (int i = 0; i <= 9; i++) {
        bool thick = (i % 3 == 0);
        ImU32 lc = thick ? C_THICK : C_THIN;
        float lw = thick ? g_thick : g_thin;

        float y = (i == 0) ? 0 : cellY(i - 1) + g_cell + g_thin / 2;
        if (i == 9) y = boardH();
        dl->AddLine(ImVec2(org.x, org.y + y), ImVec2(org.x + boardW(), org.y + y),
                    lc, lw);

        float x = (i == 0) ? 0 : cellX(i - 1) + g_cell + g_thin / 2;
        if (i == 9) x = boardW();
        dl->AddLine(ImVec2(org.x + x, org.y), ImVec2(org.x + x, org.y + boardH()),
                    lc, lw);
    }
}

// ======== 主 UI ========
void renderUI() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowSize(vp);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Main", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);

    // 右侧面板宽度：取窗口 38% 但控制在 340~500 之间
    float rightW = vp.x * 0.38f;
    if (rightW < 340) rightW = 340;
    if (rightW > 500) rightW = 500;
    float leftW = vp.x - rightW;

    // === 左侧：棋盘 ===
    ImGui::BeginChild("LeftPanel", ImVec2(leftW, 0), true);
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        computeLayout(avail.x, avail.y);

        float bx = (avail.x - boardW()) * 0.5f;
        float by = (avail.y - boardH()) * 0.5f;
        if (bx < 4) bx = 4;
        if (by < 4) by = 4;
        ImVec2 org = ImGui::GetCursorScreenPos();
        org.x += bx; org.y += by;

        drawBoard(ImGui::GetWindowDrawList(), org);

        if (ImGui::IsWindowHovered()) {
            ImVec2 mp = ImGui::GetMousePos();
            hitCell(mp.x - org.x, mp.y - org.y, g.hoverR, g.hoverC);
        } else {
            g.hoverR = g.hoverC = -1;
        }

        // 鼠标点击选中
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImVec2 mp = ImGui::GetMousePos();
            int r, c;
            if (hitCell(mp.x - org.x, mp.y - org.y, r, c)) {
                if (g.initial.given[r][c]) { g.selR = g.selC = -1; }
                else { g.selR = r; g.selC = c; }
            } else {
                g.selR = g.selC = -1;
            }
        }

        // 键盘输入 —— 使用 setCell 确保约束表一致
        if (g.selR >= 0 && g.selC >= 0 && !g.initial.given[g.selR][g.selC]) {
            for (int vk = '1'; vk <= '9'; vk++) {
                ImGuiKey k1 = (ImGuiKey)(ImGuiKey_0 + (vk - '0'));
                ImGuiKey k2 = (ImGuiKey)(ImGuiKey_Keypad0 + (vk - '0'));
                if (ImGui::IsKeyPressed(k1) || ImGui::IsKeyPressed(k2)) {
                    int val = vk - '0';
                    g.current.setCell(g.selR, g.selC, val);
                    g.initial.setCell(g.selR, g.selC, val);
                    // 不设 given——用户自己输入的数字可随时修改
                    updateConflicts();
                    resetSolver();
                    if (g.conflict[g.selR][g.selC])
                        snprintf(g.statusMsg, sizeof(g.statusMsg),
                                 "R%dC%d = %d  ⚠ 与同行/列/宫数字冲突！",
                                 g.selR + 1, g.selC + 1, val);
                    else
                        snprintf(g.statusMsg, sizeof(g.statusMsg),
                                 "R%dC%d = %d", g.selR + 1, g.selC + 1, val);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                g.current.erase(g.selR, g.selC);
                g.initial.erase(g.selR, g.selC);
                g.initial.given[g.selR][g.selC] = false;
                updateConflicts();
                resetSolver();
                snprintf(g.statusMsg, sizeof(g.statusMsg),
                         "R%dC%d 已擦除", g.selR + 1, g.selC + 1);
            }
        }
        // 方向键移动
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)  && g.selC > 0) g.selC--;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && g.selC < 8) g.selC++;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)    && g.selR > 0) g.selR--;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)  && g.selR < 8) g.selR++;

        ImGui::Dummy(ImVec2(boardW(), boardH()));
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // === 右侧：控制面板 ===
    {
        ImGui::BeginChild("RightPanel", ImVec2(rightW, 0), true);

        // ---- 深色模式 ----
        bool dark = g.darkMode;
        if (ImGui::Checkbox("深色模式", &dark)) {
            g.darkMode = dark;
            applyTheme(dark);
        }

        ImGui::Spacing();

        // ---- 计时器 ----
        double elapsed = g.getElapsed();
        ImGui::Text("计时：%s", formatTime(elapsed).c_str());
        ImGui::SameLine();
        if (g.timerState == 0 || g.timerState == 2) {
            if (ImGui::Button(g.timerState == 0 ? "开始" : "继续", ImVec2(60, 24))) {
                if (g.timerState == 0) g.timerStart();
                else g.timerResume();
            }
        } else {
            if (ImGui::Button("暂停", ImVec2(60, 24))) g.timerPause();
        }
        ImGui::SameLine();
        if (ImGui::Button("归零", ImVec2(60, 24))) g.timerStop();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 难度 + 生成
        ImGui::Text("难度选择：");
        ImGui::SameLine();
        const char* diffs[] = {"简单", "中等", "困难", "专家"};
        ImGui::SetNextItemWidth(90);
        ImGui::Combo("##diff", &g.difficulty, diffs, 4);
        ImGui::SameLine();
        if (ImGui::Button("生成新谜题", ImVec2(105, 0))) {
            Difficulty d = (Difficulty)g.difficulty;
            Board b = generatePuzzle(d);
            char msg[128];
            snprintf(msg, sizeof(msg), "已生成「%s」谜题，%d 个提示数。",
                     difficultyName(d), 81 - difficultyEmptyCells(d));
            loadPuzzle(b, msg);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 操作按钮
        ImGui::Text("操作：");
        if (ImGui::Button("逐步推理", ImVec2(95, 30))) {
            if (hasAnyConflict()) {
                snprintf(g.statusMsg, sizeof(g.statusMsg),
                         "棋盘存在冲突（红色标记），请先修正后再求解。");
            } else if (g.current.isComplete()) {
                snprintf(g.statusMsg, sizeof(g.statusMsg), "已完成！");
            } else {
                Step step;
                if (g.solver->step(step)) {
                    g.steps.push_back(step);
                    snprintf(g.statusMsg, sizeof(g.statusMsg), "%s", step.description.c_str());
                } else {
                    snprintf(g.statusMsg, sizeof(g.statusMsg),
                             "无法继续。可能存在矛盾或无解，请检查输入。");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("一键求解", ImVec2(95, 30))) {
            if (hasAnyConflict()) {
                snprintf(g.statusMsg, sizeof(g.statusMsg),
                         "棋盘存在冲突（红色标记），请先修正后再求解。");
            } else if (isBoardEmpty()) {
                snprintf(g.statusMsg, sizeof(g.statusMsg),
                         "棋盘为空，请先生成谜题或手动填入数字。");
            } else {
                Board copy = g.current;
                copy.rebuildConstraints();
                Solver batch(copy);
                std::vector<Step> allSteps;
                bool ok = batch.solveAll(allSteps);
                if (ok) {
                    g.current = copy;
                    delete g.solver;
                    g.solver = new Solver(g.current);
                    g.steps = allSteps;
                    int solCnt = countSolutions(g.initial, 2);
                    if (solCnt > 1)
                        snprintf(g.statusMsg, sizeof(g.statusMsg),
                                 "求解完成（警告：该谜题有多个解！），共 %zu 步推理。", allSteps.size());
                    else
                        snprintf(g.statusMsg, sizeof(g.statusMsg),
                                 "求解完成（唯一解），共 %zu 步推理。", allSteps.size());
                } else {
                    snprintf(g.statusMsg, sizeof(g.statusMsg),
                             "该谜题无解！请检查输入是否有冲突或错误。");
                }
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("重置", ImVec2(88, 28))) {
            resetSolver();
            g.selR = g.selC = -1;
            snprintf(g.statusMsg, sizeof(g.statusMsg), "已重置到初始谜题。");
        }
        ImGui::SameLine();
        if (ImGui::Button("清空", ImVec2(88, 28))) {
            g.initial.clear();
            for (int r = 0; r < 9; r++)
                for (int c = 0; c < 9; c++)
                    g.conflict[r][c] = false;
            resetSolver();
            g.selR = g.selC = -1;
            snprintf(g.statusMsg, sizeof(g.statusMsg), "棋盘已清空，请填入数字后求解。");
        }
        ImGui::SameLine();

        // 候选数复选框
        ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.55f, 0.55f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.45f, 0.55f, 0.75f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_CheckMark,      ImVec4(0.15f, 0.45f, 0.90f, 1.0f));
        ImGui::Checkbox("候选数", &g.showCandidates);
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 状态消息
        ImGui::TextWrapped("%s", g.statusMsg);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 推理日志
        ImGui::Text("推理日志：");
        ImGui::BeginChild("LogArea", ImVec2(0, -10), true);
        for (auto& st : g.steps)
            ImGui::TextWrapped("%s", st.description.c_str());
        if ((int)g.steps.size() > g.lastStepCount) {
            ImGui::SetScrollHereY(1.0f);
            g.lastStepCount = (int)g.steps.size();
        }
        ImGui::EndChild();

        ImGui::EndChild();
    }

    ImGui::End();
}

// ======== Win32 + OpenGL 初始化 ========
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
    case WM_SIZE: {
        RECT rc; GetClientRect(hwnd, &rc);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // 窗口
    WNDCLASSEXW wc = {sizeof(wc), CS_OWNDC, WndProc, 0, 0, hInst,
                      LoadIcon(hInst, MAKEINTRESOURCE(101)),
                      nullptr, nullptr, nullptr, L"SudokuImGui", nullptr};
    RegisterClassExW(&wc);

    RECT wr = {0, 0, 1150, 720};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"数独求解器",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInst, nullptr);

    // OpenGL 上下文
    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0, 0, 0, 0, 0};
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);

    // ImGui 初始化
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // 风格
    applyTheme(false);

    // 加载中文字体
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
    };
    // 加载中文字体——两组大小：UI 用 19px，棋盘数字用 48px（防止放大模糊）
    ImFont* uiFont = nullptr;
    for (const char* fp : fontPaths) {
        FILE* f = fopen(fp, "rb");
        if (f) {
            fclose(f);
            uiFont = io.Fonts->AddFontFromFileTTF(fp, 19.0f, nullptr,
                io.Fonts->GetGlyphRangesChineseFull());
            g_font = io.Fonts->AddFontFromFileTTF(fp, 48.0f, nullptr,
                io.Fonts->GetGlyphRangesChineseFull());
            break;
        }
    }
    if (!uiFont) { g_font = io.Fonts->AddFontDefault(); }
    io.Fonts->Build();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");


    snprintf(g.statusMsg, sizeof(g.statusMsg),
             "欢迎！请生成谜题或直接在棋盘上填数，然后点击求解。");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 主循环
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        renderUI();

        ImGui::Render();
        RECT rc; GetClientRect(hwnd, &rc);
        glViewport(0, 0, rc.right - rc.left, rc.bottom - rc.top);
        glClearColor(g.darkMode ? 0.08f : 0.95f,
                     g.darkMode ? 0.08f : 0.95f,
                     g.darkMode ? 0.10f : 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }

    // 清理
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hglrc);
    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    delete g.solver;
    return 0;
}
