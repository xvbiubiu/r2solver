# R2Solver —— 独立一键编译工程（C++17 版）
# 用法：
#   CLion: File → Open → 选 R2Solver 文件夹（自动识别本文件）→ ▶️
#   命令行: cmake -B build && cmake --build build
#
# 说明：
#   - solverer 为 C++17 实现（src/*.cpp），逻辑与 legacy/c/ 的 C 版逐模块一致
#   - count_r2 为独立验证工具，保留 C 实现（tools/count_r2.c）
cmake_minimum_required(VERSION 3.14)
project(R2Solver CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# 搜索器（C++17 主程序）
add_executable(solverer
  src/r2solver_main.cpp
  src/state.cpp
  src/prune.cpp
  src/search.cpp
  src/log.cpp
  src/log_null.cpp
  src/log_detailed.cpp
  src/log_stats.cpp
  src/log_json.cpp
)

# 验证工具（count_r2 等）不属于仓库分发内容；本地存在时才构建
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/count_r2.c")
  add_executable(count_r2 tools/count_r2.c)
endif()

# 数学库（sqrtl/ceill/log2l 等；MSVC 的 CRT 自带，无需链接）
if(NOT MSVC)
  target_link_libraries(solverer m)
  if(TARGET count_r2)
    target_link_libraries(count_r2 m)
  endif()
endif()
