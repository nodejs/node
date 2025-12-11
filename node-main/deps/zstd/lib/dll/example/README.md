# ZSTD Windows binary package

## The package contents

- `zstd.exe` : Command Line Utility, supporting gzip-like arguments
- `dll\libzstd.dll` : The ZSTD dynamic library (DLL)
- `dll\libzstd.lib` : The import library of the ZSTD dynamic library (DLL) for Visual C++
- `example\` : The example of usage of the ZSTD library
- `include\` : Header files required by the ZSTD library
- `static\libzstd_static.lib` : The static ZSTD library (LIB)

## Usage of Command Line Interface

Command Line Interface (CLI) supports gzip-like arguments.
By default CLI takes an input file and compresses it to an output file:

    Usage: zstd [arg] [input] [output]

The full list of commands for CLI can be obtained with `-h` or `-H`. The ratio can
be improved with commands from `-3` to `-16` but higher levels also have slower
compression. CLI includes in-memory compression benchmark module with compression
levels starting from `-b` and ending with `-e` with iteration time of `-i` seconds.
CLI supports aggregation of parameters i.e. `-b1`, `-e18`, and `-i1` can be joined
into `-b1e18i1`.

## The example of usage of static and dynamic ZSTD libraries with gcc/MinGW

Use `cd example` and `make` to build `fullbench-dll` and `fullbench-lib`.
`fullbench-dll` uses a dynamic ZSTD library from the `dll` directory.
`fullbench-lib` uses a static ZSTD library from the `lib` directory.

## Using ZSTD DLL with gcc/MinGW

The header files from `include\` and the dynamic library `dll\libzstd.dll`
are required to compile a project using gcc/MinGW.
The dynamic library has to be added to linking options.
It means that if a project that uses ZSTD consists of a single `test-dll.c`
file it should be linked with `dll\libzstd.dll`. For example:

    gcc $(CFLAGS) -Iinclude\ test-dll.c -o test-dll dll\libzstd.dll

The compiled executable will require ZSTD DLL which is available at `dll\libzstd.dll`.

## The example of usage of static and dynamic ZSTD libraries with Visual C++

Open `example\fullbench-dll.sln` to compile `fullbench-dll` that uses a
dynamic ZSTD library from the `dll` directory. The solution works with Visual C++
2010 or newer. When one will open the solution with Visual C++ newer than 2010
then the solution will be upgraded to the current version.

## Using ZSTD DLL with Visual C++

The header files from `include\` and the import library `dll\libzstd.lib`
are required to compile a project using Visual C++.

1. The path to header files should be added to `Additional Include Directories` that can
   be found in project properties `C/C++` then `General`.
2. The import library has to be added to `Additional Dependencies` that can
   be found in project properties `Linker` then `Input`.
   If one will provide only the name `libzstd.lib` without a full path to the library
   the directory has to be added to `Linker\General\Additional Library Directories`.

The compiled executable will require ZSTD DLL which is available at `dll\libzstd.dll`.
