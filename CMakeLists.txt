cmake_minimum_required(VERSION 2.8.8)
project(xunused)

find_package(LLVM 10 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(xunused main.cpp)

target_link_libraries(xunused
  PRIVATE
  clangIndex
  clangTooling
  clangFrontend
  clangDriver
  clangSerialization
  clangParse
  clangSema
  clangAnalysis
  clangASTMatchers
  clangAST
  clangEdit
  clangLex
  clangBasic  
)

llvm_map_components_to_libnames(llvm_libs x86asmparser support option profiledata FrontendOpenMP)
target_link_libraries(xunused PRIVATE ${llvm_libs})

install(TARGETS xunused DESTINATION bin)
