# xunused
`xunused` is a tool to find unused C/C++ functions and methods across source files in the whole project.
It is built upon clang to parse the source code (in parallel). It then shows all functions that had
a definition but no use. Templates, virtual functions, constructors, functions with `static` linkage are
all taken into account. If you find an issue, please open a issue on https://github.com/mgehre/xunused or fill a pull request.

## Building and Installation
First download or build LLVM 8 and Clang 8 with development headers.
On Ubuntu, this can easily be done via http://apt.llvm.org/ and `apt install llvm-8-dev libclang-8-dev`.
Then build via
```
mkdir build
cmake ..
make
```

## Run it
```
cd build
./xunused /path/to/your/project/compile_commands.json
```
You can specify the option `-filter` together with a regular expressions. Only files who's path is matching the regular
expression will be analyzed. You might want to exclude your test's source code to find functions that are only used by tests but not any other code.

If `xunused` complains about missing include files such as `stddef.h`, try adding `-extra-arg=-I/usr/include/clang/8/include` (or similar) to the arguments.