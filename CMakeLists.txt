cmake_minimum_required(VERSION 3.13)
project(BorrowCheckPlugin LANGUAGES C CXX)

# Set plugin output to be a shared library
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti")
set(CMAKE_CXX_VISIBILITY_PRESET default)
set(CMAKE_VISIBILITY_INLINES_HIDDEN OFF)

# Point to your custom-built LLVM/Clang
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

# Set up include and link paths
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

# Use LLVM/Clang's CMake flags
add_definitions(${LLVM_DEFINITIONS})

# Output a shared library
add_library(BorrowCheckPlugin SHARED BorrowCheckPlugin.cpp)

# Link against Clang/LLVM
target_link_libraries(BorrowCheckPlugin
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  LLVMCore
)
