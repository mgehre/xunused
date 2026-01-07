cmake_minimum_required(VERSION 3.7)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(XUNUSED_STANDALONE_BUILD ON)
    message(STATUS "xunused: standalone build")
else()
    set(XUNUSED_STANDALONE_BUILD OFF)
    message(STATUS "xunused: build as part of LLVM")
endif()

if (XUNUSED_STANDALONE_BUILD)
    project(xunused)

    # Let find_package(...) search for the latest version when multiple versions of
    # the same library are available. Therefore ...
    #
    #   1) set natural sort order to compare contiguous digits as whole numbers, and
    #   2) do ordering in descending mode to test latest found folder first.
    #
    set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
    set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)

    find_package(LLVM ${LLVM_VERSION_REQUIRED} CONFIG REQUIRED)
    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

    if (LLVM_VERSION_MAJOR GREATER_EQUAL 16)
        if (LLVM_VERSION_MAJOR GREATER_EQUAL 18)
            find_package(Clang ${LLVM_VERSION_MAJOR}.${LLVM_VERSION_MINOR} CONFIG REQUIRED)
        else()
            find_package(Clang ${LLVM_VERSION_MAJOR} CONFIG REQUIRED)
        endif()
        message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")
    endif (LLVM_VERSION_MAJOR GREATER_EQUAL 16)

    set(XUNUSED_LLVM_DIR ${LLVM_DIR})
    set(XUNUSED_INCLUDE_DIRS
        ${LLVM_INCLUDE_DIRS}
        ${CLANG_INCLUDE_DIRS}
    )
else()
    set(XUNUSED_LLVM_DIR ${CMAKE_SOURCE_DIR})
    set(XUNUSED_INCLUDE_DIRS
        ${LLVM_SOURCE_DIR}/include
        ${LLVM_EXTERNAL_CLANG_SOURCE_DIR}/include
        ${LLVM_BINARY_DIR}/include
        ${LLVM_BINARY_DIR}/tools/clang/include
    )

    if (UNIX AND NOT LLVM_ENABLE_RTTI)
        # When building as part of LLVM on Unix, LLVM and Clang can be built with
        # -fno-rtti, so we need to disable RTTI for xunused as well.
        # https://github.com/mgehre/xunused/issues/1
        add_compile_options(-fno-rtti)
    endif()
endif()

message(STATUS
  "xunused: configuring for LLVM ${LLVM_VERSION} from ${XUNUSED_LLVM_DIR}")

# The good default is given by the llvm toolchain installation itself, but still
# in case both static and shared libraries are available, allow to override that
# default.
option(XUNUSED_LINK_LLVM_DYLIB
       "Link against the LLVM dynamic library"
       ${LLVM_LINK_LLVM_DYLIB})

option(XUNUSED_LINK_CLANG_DYLIB
       "Link against the clang dynamic library"
       ${CLANG_LINK_CLANG_DYLIB})

message(STATUS "Link against the LLVM dynamic library: ${XUNUSED_LINK_LLVM_DYLIB}")
message(STATUS "Link against the Clang dynamic library: ${XUNUSED_LINK_CLANG_DYLIB}")

include_directories(${XUNUSED_INCLUDE_DIRS})

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
endif (XUNUSED_LINK_LLVM_DYLIB)

target_link_libraries(xunused PRIVATE ${XUNUSED_CLANG_LIBS} ${XUNUSED_LLVM_LIBS})

install(TARGETS xunused DESTINATION bin)

# Add a check target to run lit tests
# Find lit.py in the LLVM installation
if (XUNUSED_STANDALONE_BUILD)
    set(LIT_PY "${LLVM_LIBRARY_DIRS}/../build/utils/lit/lit.py")
    set(CHECK_TARGET_NAME "check")
else()
    set(LIT_PY "${XUNUSED_LLVM_DIR}/utils/lit/lit.py")
    set(CHECK_TARGET_NAME "check-xunused")
endif()
if(EXISTS "${LIT_PY}")
    message(STATUS "Found lit.py at: ${LIT_PY}")

    find_package(Python3 COMPONENTS Interpreter)

    # Configure the site-specific configuration file
    set(XUNUSED_TOOLS_DIR "${CMAKE_BINARY_DIR}")
    set(TARGET_TRIPLE "${LLVM_DEFAULT_TARGET_TRIPLE}")
    set(CMAKE_CURRENT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/tests")
    configure_file(
        ${CMAKE_SOURCE_DIR}/tests/lit.site.cfg.py.in
        ${CMAKE_BINARY_DIR}/tests/lit.site.cfg.py
        @ONLY
    )

    add_custom_target(${CHECK_TARGET_NAME}
        COMMAND ${Python3_EXECUTABLE} ${LIT_PY} -v ${CMAKE_BINARY_DIR}/tests
        DEPENDS xunused
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running lit tests"
    )
else()
    message(STATUS "lit.py not found in LLVM installation, check target will not be available")
endif()
