Notes on Sanitizers
===================

Compiler sanitizers are tools that can be enabled during compilation to detect
various types of bugs at runtime. OpenSSL supports three sanitizers:

- **AddressSanitizer (ASan)**: Detects memory errors such as use-after-free,
  buffer overflows, and memory leaks.
- **UndefinedBehaviorSanitizer (UBSan)**: Detects undefined behavior such as
  integer overflow, null pointer dereference, and type mismatches.
- **MemorySanitizer (MSan)**: Detects use of uninitialized memory.

Sanitizers are generally faster than Valgrind and can detect certain issues
that Valgrind cannot, making them a useful complement to Valgrind-based testing.

Requirements
------------

1. GCC or Clang compiler with sanitizer support
   - GCC 4.8+ or Clang 3.1+ for ASan and UBSan
   - Clang only for MSan (GCC does not implement MemorySanitizer)
2. Linux, macOS, or other supported platform
   - Note: MSan is only supported on Linux
   - Note: Leak detection (LSan) is not yet supported on macOS

Building with Sanitizers
------------------------

OpenSSL provides configuration options to enable sanitizers:

### AddressSanitizer (ASan)

    $ ./config enable-asan
    $ make

### UndefinedBehaviorSanitizer (UBSan)

    $ ./config enable-ubsan
    $ make

### MemorySanitizer (MSan)

MSan is only implemented by Clang, so the compiler must be set to clang:

    $ CC=clang ./config enable-msan
    $ make

Note: MSan requires that all code, including libraries, be compiled with MSan.
This makes it more difficult to use than ASan or UBSan.

### Combining Sanitizers

ASan and UBSan can be used together:

    $ ./config enable-asan enable-ubsan
    $ make

Note: ASan and MSan cannot be used together as they are mutually exclusive.

Running Tests
-------------

After building with sanitizers enabled, run the tests normally:

    $ make test

The sanitizers will automatically detect issues during test execution and
report them to stderr. If a sanitizer detects an error, the test will fail.

### Running Specific Tests

To run a specific test with verbose output:

    $ make test TESTS=test_name VERBOSE=1

### Sanitizer Environment Variables

Sanitizer behavior can be controlled via environment variables:

#### ASAN_OPTIONS

Controls AddressSanitizer behavior. Common options:

    # Allow malloc to return NULL instead of aborting
    ASAN_OPTIONS=allocator_may_return_null=1

    # Disable leak detection (LSan runs as part of ASan by default)
    ASAN_OPTIONS=detect_leaks=0

    # Get more detailed stack traces
    ASAN_OPTIONS=fast_unwind_on_malloc=0

#### UBSAN_OPTIONS

Controls UndefinedBehaviorSanitizer behavior:

    # Print stack traces for UBSan errors
    UBSAN_OPTIONS=print_stacktrace=1

#### MSAN_OPTIONS

Controls MemorySanitizer behavior:

    # Allow malloc to return NULL instead of aborting
    MSAN_OPTIONS=allocator_may_return_null=1

Example with environment variables:

    $ ASAN_OPTIONS=detect_leaks=1 make test TESTS=test_name

Interpreting Results
--------------------

When a sanitizer detects an issue, it will print a detailed error report
including:

- The type of error (e.g., "heap-buffer-overflow", "use-after-free")
- Stack trace showing where the error occurred
- Stack trace showing where the memory was allocated (for memory errors)
- Information about the memory region involved

Example ASan output:

    ==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
        #0 0x... in function_name file.c:123
        #1 0x... in caller_function file.c:456
        ...

Comparison with Valgrind
------------------------

| Feature                    | Sanitizers        | Valgrind          |
|----------------------------|-------------------|-------------------|
| Performance                | ~2x slowdown      | ~10-50x slowdown  |
| Requires recompilation     | Yes               | No                |
| Memory leak detection      | ASan (with LSan)  | Yes               |
| Uninitialized memory       | MSan              | Yes               |
| Buffer overflow detection  | ASan              | Yes               |
| Undefined behavior         | UBSan             | Limited           |
| Platform support           | Linux, macOS      | Linux, macOS, etc |

See Also
--------

- [NOTES-VALGRIND.md](NOTES-VALGRIND.md) - Running tests with Valgrind
- [test/README.md](test/README.md) - General test documentation
- [AddressSanitizer documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer documentation](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [MemorySanitizer documentation](https://clang.llvm.org/docs/MemorySanitizer.html)
