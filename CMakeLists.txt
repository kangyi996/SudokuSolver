cmake_minimum_required(VERSION 3.15)
project(SudokuSolver LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")

add_executable(sudoku_solver
    # Core
    src/core/board.cpp
    src/core/solver.cpp
    src/core/puzzle_loader.cpp
    src/core/generator.cpp
    # ImGui core
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/imgui_tables.cpp
    # ImGui backends
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
    # App
    src/main.cpp
    src/app.rc
)

target_include_directories(sudoku_solver PRIVATE
    src
    third_party/imgui
    third_party/imgui/backends
)

target_link_libraries(sudoku_solver PRIVATE
    opengl32
    gdi32
    gdiplus
    comctl32
    dwmapi
)
