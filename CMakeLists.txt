# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project(CTegra-Swizzle CXX)
add_library(CTegra-Swizzle STATIC "src/tegra_swizzle/arrays.h" "src/tegra_swizzle/blockdepth.h" "src/tegra_swizzle/blockheight.h" "src/tegra_swizzle/lib.h" "src/tegra_swizzle/lib.cpp" "src/tegra_swizzle/surface.h" "src/tegra_swizzle/swizzle.h")

target_include_directories(CTegra-Swizzle PUBLIC src)