Zstandard library files
================================

The __lib__ directory is split into several sub-directories,
in order to make it easier to select or exclude features.


#### Building

`Makefile` script is provided, supporting [Makefile conventions](https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html#Makefile-Conventions),
including commands variables, staged install, directory variables and standard targets.
- `make` : generates both static and dynamic libraries
- `make install` : install libraries and headers in target system directories

`libzstd` default scope is pretty large, including compression, decompression, dictionary builder,
and support for decoding legacy formats >= v0.5.0.
The scope can be reduced on demand (see paragraph _modular build_).


#### Multithreading support

When building with `make`, by default the dynamic library is multithreaded and static library is single-threaded (for compatibility reasons).

Enabling multithreading requires 2 conditions :
- set build macro `ZSTD_MULTITHREAD` (`-DZSTD_MULTITHREAD` for `gcc`)
- for POSIX systems : compile with pthread (`-pthread` compilation flag for `gcc`)

For convenience, we provide a build target to generate multi and single threaded libraries:
- Force enable multithreading on both dynamic and static libraries by appending `-mt` to the target, e.g. `make lib-mt`.
- Force disable multithreading on both dynamic and static libraries by appending `-nomt` to the target, e.g. `make lib-nomt`.
- By default, as mentioned before, dynamic library is multithreaded, and static library is single-threaded, e.g. `make lib`.

When linking a POSIX program with a multithreaded version of `libzstd`,
note that it's necessary to invoke the `-pthread` flag during link stage.

Multithreading capabilities are exposed
via the [advanced API defined in `lib/zstd.h`](https://github.com/facebook/zstd/blob/v1.4.3/lib/zstd.h#L351).


#### API

Zstandard's stable API is exposed within [lib/zstd.h](zstd.h).


#### Advanced API

Optional advanced features are exposed via :

- `lib/zstd_errors.h` : translates `size_t` function results
                        into a `ZSTD_ErrorCode`, for accurate error handling.

- `ZSTD_STATIC_LINKING_ONLY` : if this macro is defined _before_ including `zstd.h`,
                          it unlocks access to the experimental API,
                          exposed in the second part of `zstd.h`.
                          All definitions in the experimental APIs are unstable,
                          they may still change in the future, or even be removed.
                          As a consequence, experimental definitions shall ___never be used with dynamic library___ !
                          Only static linking is allowed.


#### Modular build

It's possible to compile only a limited set of features within `libzstd`.
The file structure is designed to make this selection manually achievable for any build system :

- Directory `lib/common` is always required, for all variants.

- Compression source code lies in `lib/compress`

- Decompression source code lies in `lib/decompress`

- It's possible to include only `compress` or only `decompress`, they don't depend on each other.

- `lib/dictBuilder` : makes it possible to generate dictionaries from a set of samples.
        The API is exposed in `lib/dictBuilder/zdict.h`.
        This module depends on both `lib/common` and `lib/compress` .

- `lib/legacy` : makes it possible to decompress legacy zstd formats, starting from `v0.1.0`.
        This module depends on `lib/common` and `lib/decompress`.
        To enable this feature, define `ZSTD_LEGACY_SUPPORT` during compilation.
        Specifying a number limits versions supported to that version onward.
        For example, `ZSTD_LEGACY_SUPPORT=2` means : "support legacy formats >= v0.2.0".
        Conversely, `ZSTD_LEGACY_SUPPORT=0` means "do __not__ support legacy formats".
        By default, this build macro is set as `ZSTD_LEGACY_SUPPORT=5`.
        Decoding supported legacy format is a transparent capability triggered within decompression functions.
        It's also allowed to invoke legacy API directly, exposed in `lib/legacy/zstd_legacy.h`.
        Each version does also provide its own set of advanced API.
        For example, advanced API for version `v0.4` is exposed in `lib/legacy/zstd_v04.h` .

- While invoking `make libzstd`, it's possible to define build macros
        `ZSTD_LIB_COMPRESSION`, `ZSTD_LIB_DECOMPRESSION`, `ZSTD_LIB_DICTBUILDER`,
        and `ZSTD_LIB_DEPRECATED` as `0` to forgo compilation of the
        corresponding features. This will also disable compilation of all
        dependencies (e.g. `ZSTD_LIB_COMPRESSION=0` will also disable
        dictBuilder).

- There are a number of options that can help minimize the binary size of
  `libzstd`.

  The first step is to select the components needed (using the above-described
  `ZSTD_LIB_COMPRESSION` etc.).

  The next step is to set `ZSTD_LIB_MINIFY` to `1` when invoking `make`. This
  disables various optional components and changes the compilation flags to
  prioritize space-saving.

  Detailed options: Zstandard's code and build environment is set up by default
  to optimize above all else for performance. In pursuit of this goal, Zstandard
  makes significant trade-offs in code size. For example, Zstandard often has
  more than one implementation of a particular component, with each
  implementation optimized for different scenarios. For example, the Huffman
  decoder has complementary implementations that decode the stream one symbol at
  a time or two symbols at a time. Zstd normally includes both (and dispatches
  between them at runtime), but by defining `HUF_FORCE_DECOMPRESS_X1` or
  `HUF_FORCE_DECOMPRESS_X2`, you can force the use of one or the other, avoiding
  compilation of the other. Similarly, `ZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT`
  and `ZSTD_FORCE_DECOMPRESS_SEQUENCES_LONG` force the compilation and use of
  only one or the other of two decompression implementations. The smallest
  binary is achieved by using `HUF_FORCE_DECOMPRESS_X1` and
  `ZSTD_FORCE_DECOMPRESS_SEQUENCES_SHORT` (implied by `ZSTD_LIB_MINIFY`).

  On the compressor side, Zstd's compression levels map to several internal
  strategies. In environments where the higher compression levels aren't used,
  it is possible to exclude all but the fastest strategy with
  `ZSTD_LIB_EXCLUDE_COMPRESSORS_DFAST_AND_UP=1`. (Note that this will change
  the behavior of the default compression level.) Or if you want to retain the
  default compressor as well, you can set
  `ZSTD_LIB_EXCLUDE_COMPRESSORS_GREEDY_AND_UP=1`, at the cost of an additional
  ~20KB or so.

  For squeezing the last ounce of size out, you can also define
  `ZSTD_NO_INLINE`, which disables inlining, and `ZSTD_STRIP_ERROR_STRINGS`,
  which removes the error messages that are otherwise returned by
  `ZSTD_getErrorName` (implied by `ZSTD_LIB_MINIFY`).

  Finally, when integrating into your application, make sure you're doing link-
  time optimization and unused symbol garbage collection (via some combination of,
  e.g., `-flto`, `-ffat-lto-objects`, `-fuse-linker-plugin`,
  `-ffunction-sections`, `-fdata-sections`, `-fmerge-all-constants`,
  `-Wl,--gc-sections`, `-Wl,-z,norelro`, and an archiver that understands
  the compiler's intermediate representation, e.g., `AR=gcc-ar`). Consult your
  compiler's documentation.

- While invoking `make libzstd`, the build macro `ZSTD_LEGACY_MULTITHREADED_API=1`
  will expose the deprecated `ZSTDMT` API exposed by `zstdmt_compress.h` in
  the shared library, which is now hidden by default.

- The build macro `DYNAMIC_BMI2` can be set to 1 or 0 in order to generate binaries
  which can detect at runtime the presence of BMI2 instructions, and use them only if present.
  These instructions contribute to better performance, notably on the decoder side.
  By default, this feature is automatically enabled on detecting
  the right instruction set (x64) and compiler (clang or gcc >= 5).
  It's obviously disabled for different cpus,
  or when BMI2 instruction set is _required_ by the compiler command line
  (in this case, only the BMI2 code path is generated).
  Setting this macro will either force to generate the BMI2 dispatcher (1)
  or prevent it (0). It overrides automatic detection.

- The build macro `ZSTD_NO_UNUSED_FUNCTIONS` can be defined to hide the definitions of functions
  that zstd does not use. Not all unused functions are hidden, but they can be if needed.
  Currently, this macro will hide function definitions in FSE and HUF that use an excessive
  amount of stack space.

- The build macro `ZSTD_NO_INTRINSICS` can be defined to disable all explicit intrinsics.
  Compiler builtins are still used.

- The build macro `ZSTD_DECODER_INTERNAL_BUFFER` can be set to control
  the amount of extra memory used during decompression to store literals.
  This defaults to 64kB.  Reducing this value reduces the memory footprint of
  `ZSTD_DCtx` decompression contexts,
  but might also result in a small decompression speed cost.

- The C compiler macros `ZSTDLIB_VISIBLE`, `ZSTDERRORLIB_VISIBLE` and `ZDICTLIB_VISIBLE`
  can be overridden to control the visibility of zstd's API. Additionally,
  `ZSTDLIB_STATIC_API` and `ZDICTLIB_STATIC_API` can be overridden to control the visibility
  of zstd's static API. Specifically, it can be set to `ZSTDLIB_HIDDEN` to hide the symbols
  from the shared library. These macros default to `ZSTDLIB_VISIBILITY`,
  `ZSTDERRORLIB_VSIBILITY`, and `ZDICTLIB_VISIBILITY` if unset, for backwards compatibility
  with the old macro names.

- The C compiler macro `HUF_DISABLE_FAST_DECODE` disables the newer Huffman fast C
  and assembly decoding loops. You may want to use this macro if these loops are
  slower on your platform.

#### Windows : using MinGW+MSYS to create DLL

DLL can be created using MinGW+MSYS with the `make libzstd` command.
This command creates `dll\libzstd.dll` and the import library `dll\libzstd.lib`.
The import library is only required with Visual C++.
The header file `zstd.h` and the dynamic library `dll\libzstd.dll` are required to
compile a project using gcc/MinGW.
The dynamic library has to be added to linking options.
It means that if a project that uses ZSTD consists of a single `test-dll.c`
file it should be linked with `dll\libzstd.dll`. For example:
```
    gcc $(CFLAGS) -Iinclude/ test-dll.c -o test-dll dll\libzstd.dll
```
The compiled executable will require ZSTD DLL which is available at `dll\libzstd.dll`.


#### Advanced Build options

The build system requires a hash function in order to
separate object files created with different compilation flags.
By default, it tries to use `md5sum` or equivalent.
The hash function can be manually switched by setting the `HASH` variable.
For example : `make HASH=xxhsum`
The hash function needs to generate at least 64-bit using hexadecimal format.
When no hash function is found,
the Makefile just generates all object files into the same default directory,
irrespective of compilation flags.
This functionality only matters if `libzstd` is compiled multiple times
with different build flags.

The build directory, where object files are stored
can also be manually controlled using variable `BUILD_DIR`,
for example `make BUILD_DIR=objectDir/v1`.
In which case, the hash function doesn't matter.


#### Deprecated API

Obsolete API on their way out are stored in directory `lib/deprecated`.
At this stage, it contains older streaming prototypes, in `lib/deprecated/zbuff.h`.
These prototypes will be removed in some future version.
Consider migrating code towards supported streaming API exposed in `zstd.h`.


#### Miscellaneous

The other files are not source code. There are :

 - `BUCK` : support for `buck` build system (https://buckbuild.com/)
 - `Makefile` : `make` script to build and install zstd library (static and dynamic)
 - `README.md` : this file
 - `dll/` : resources directory for Windows compilation
 - `libzstd.pc.in` : script for `pkg-config` (used in `make install`)
