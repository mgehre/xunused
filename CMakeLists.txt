cmake_minimum_required(VERSION 3.7)
project(xunused)

# Let find_package(...) search for the latest version when multiple versions of
# the same library are available. Therefore ...
#
#   1) set natural sort order to compare contiguous digits as whole numbers, and
#   2) do ordering in descending mode to test latest found folder first.
#
set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

find_package(LLVM  ${LLVM_VERSION_REQUIRED} CONFIG REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

find_package(Python3 COMPONENTS Interpreter)

if (LLVM_VERSION_MAJOR GREATER_EQUAL 16)
    if (LLVM_VERSION_MAJOR GREATER_EQUAL 18)
        find_package(Clang ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR} CONFIG REQUIRED)
    else()
        find_package(Clang ${LLVM_VERSION_MAJOR} CONFIG REQUIRED)
    endif()
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

# Add a check target to run lit tests
# Find lit.py in the LLVM installation
file(GLOB_RECURSE LIT_PY_PATHS "${LLVM_LIBRARY_DIRS}/../build/utils/lit/lit.py")
list(LENGTH LIT_PY_PATHS LIT_PY_COUNT)
if(LIT_PY_COUNT GREATER 0)
    list(GET LIT_PY_PATHS 0 LIT_PY)
    message(STATUS "Found lit.py at: ${LIT_PY}")
    
    # Find LLVM tools directory (for FileCheck, etc.)
    get_filename_component(LLVM_TOOLS_DIR "${LLVM_DIR}/../bin" ABSOLUTE)
    
    add_custom_target(check
        COMMAND ${CMAKE_COMMAND} -E env 
            "PATH=$<TARGET_FILE_DIR:xunused>:${LLVM_TOOLS_DIR}:$ENV{PATH}"
            ${Python3_EXECUTABLE} ${LIT_PY} -v ${CMAKE_SOURCE_DIR}/tests
        DEPENDS xunused
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running lit tests"
    )
else()
    message(STATUS "lit.py not found in LLVM installation, check target will not be available")
endif()
