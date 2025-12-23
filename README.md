# xunused

`xunused` is a tool to find unused C/C++ functions and methods across source files in the whole project.
It is built upon clang to parse the source code (in parallel). It then shows all functions that had
a definition but no use. Templates, virtual functions, constructors, functions with `static` linkage are
all taken into account. If you find an issue, please open a issue on https://github.com/mgehre/xunused or file a pull request.

xunused is compatible with LLVM and Clang versions 14 to 18.

## Building and Installation

First download or build the necessary versions of LLVM and Clang with development headers.
On Debian and Ubuntu, this can easily be done via [http://apt.llvm.org](http://apt.llvm.org) and `apt install llvm-18-dev libclang-18-dev`.
Then build via

```bash
mkdir build
cd build
cmake ..
make
```

If you'd like to build xunused as part of the LLVM build system, make sure to add `-DLLVM_EXTERNAL_PROJECTS=xunused
-DLLVM_EXTERNAL_XUNUSED_SOURCE_DIR=/path/to/xunused` to the cmake options.

## Run it

To run the tool, provide a [compilation database](https://clang.llvm.org/docs/JSONCompilationDatabase.html).
By default, it will analyze all files that are mentioned in it.

```bash
cd build
./xunused /path/to/your/project/compile_commands.json
```

You can specify the option `-filter` together with a regular expressions. Only files who's path is matching the regular
expression will be analyzed. You might want to exclude your test's source code to find functions that are only used by tests but not any other code.

If `xunused` complains about missing include files such as `stddef.h`, try adding `-extra-arg=-I/usr/include/clang/17/include` (or similar) to the arguments.

## Development

### Running Tests

xunused includes a test suite using LLVM's lit testing framework. To run the tests:

```bash
cmake --build build --target check
```

For more information on running and writing tests, see [tests/README.md](tests/README.md).
