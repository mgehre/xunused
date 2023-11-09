cmake_minimum_required(VERSION 3.5)
project(xunused)

find_package(LLVM  ${LLVM_VERSION_REQUIRED} CONFIG REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

if (LLVM_VERSION_MAJOR GREATER_EQUAL 16)
    find_package(Clang ${LLVM_VERSION_MAJOR} CONFIG REQUIRED)
    message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")
endif (LLVM_VERSION_MAJOR GREATER_EQUAL 16)

option(XUNUSED_LINK_LLVM_DYLIB
       "Link against the LLVM dynamic library"
       ${LLVM_LINK_LLVM_DYLIB})

option(XUNUSED_LINK_CLANG_DYLIB
       "Link against the clang dynamic library"
       ${CLANG_LINK_CLANG_DYLIB})

message(STATUS "Link against the LLVM dynamic library: ${XUNUSED_LINK_LLVM_DYLIB}")
message(STATUS "Link against the Clang dynamic library: ${XUNUSED_LINK_CLANG_DYLIB}")

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})

add_definitions(${LLVM_DEFINITIONS})
add_definitions(${CLANG_DEFINITIONS})

link_directories(${LLVM_LIBRARY_DIRS})
link_directories(${CLANG_LIBRARY_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(xunused main.cpp)

if (XUNUSED_LINK_CLANG_DYLIB)
    set(XUNUSED_CLANG_LIBS "clang-cpp")
else (XUNUSED_LINK_CLANG_DYLIB)
    set(XUNUSED_CLANG_LIBS "clangIndex"
                           "clangTooling"
                           "clangFrontend"
                           "clangDriver"
                           "clangSerialization"
                           "clangParse"
                           "clangSema"
                           "clangAnalysis"
                           "clangASTMatchers"
                           "clangAST"
                           "clangEdit"
                           "clangLex"
                           "clangBasic")
    if (LLVM_VERSION_MAJOR GREATER_EQUAL 15)
        list(APPEND XUNUSED_CLANG_LIBS "clangSupport")
    endif (LLVM_VERSION_MAJOR GREATER_EQUAL 15)
endif (XUNUSED_LINK_CLANG_DYLIB)

if (XUNUSED_LINK_LLVM_DYLIB)
    set(XUNUSED_LLVM_LIBS  "LLVM")
else (XUNUSED_LINK_LLVM_DYLIB)
    set(XUNUSED_LLVM_LIBS  "LLVMX86AsmParser"
                           "LLVMSupport"
                           "LLVMOption"
                           "LLVMProfileData"
                           "LLVMFrontendOpenMP")
    if (LLVM_VERSION_MAJOR EQUAL 15)
        list(APPEND XUNUSED_LLVM_LIBS "LLVMWindowsDriver")
    endif (LLVM_VERSION_MAJOR EQUAL 15)
endif (XUNUSED_LINK_LLVM_DYLIB)

target_link_libraries(xunused PRIVATE ${XUNUSED_CLANG_LIBS} ${XUNUSED_LLVM_LIBS})

install(TARGETS xunused DESTINATION bin)
