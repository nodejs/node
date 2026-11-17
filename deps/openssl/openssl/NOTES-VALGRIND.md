Notes on Valgrind
=================

[Valgrind](https://valgrind.org/) is a test harness that includes many tools such as memcheck,
which is commonly used to check for memory leaks, etc. The default tool
run by Valgrind is memcheck. There are [other tools available](https://valgrind.org/info/tools.html), but this
will focus on memcheck.

Valgrind runs programs in a virtual machine, this means OpenSSL unit
tests run under Valgrind will take longer than normal.

Requirements
------------

1. Platform supported by Valgrind
   - See [Valgrind Supported Platforms](http://valgrind.org/info/platforms.html)
2. Valgrind installed on the platform
   - See [Valgrind Current Releases](http://valgrind.org/downloads/current.html)
3. OpenSSL compiled
   - See [INSTALL.md](INSTALL.md)

Running Tests
-------------

Test behavior can be modified by adjusting environment variables.

`EXE_SHELL`

This variable is used to specify the shell used to execute OpenSSL test
programs. The default wrapper (`util/wrap.pl`) initializes the environment
to allow programs to find shared libraries. The variable can be modified
to specify a different executable environment.

    EXE_SHELL=\
    "$(/bin/pwd)/util/wrap.pl valgrind --error-exitcode=1 --leak-check=full -q"

This will start up Valgrind with the default checker (`memcheck`).
The `--error-exitcode=1` option specifies that Valgrind should exit with an
error code of 1 when memory leaks occur.
The `--leak-check=full` option specifies extensive memory checking.
The `-q` option prints only error messages.
Additional Valgrind options may be added to the `EXE_SHELL` variable.

`OPENSSL_ia32cap`

This variable controls the processor-specific code on Intel processors.
By default, OpenSSL will attempt to figure out the capabilities of a
processor, and use it to its fullest capability. This variable can be
used to control what capabilities OpenSSL uses.

As of valgrind-3.15.0 on Linux/x86_64, instructions up to AVX2 are
supported. Setting the following disables instructions beyond AVX2:

`OPENSSL_ia32cap=":0"`

This variable may need to be set to something different based on the
processor and Valgrind version you are running tests on. More information
may be found in [doc/man3/OPENSSL_ia32cap.pod](doc/man3/OPENSSL_ia32cap.pod).

Additional variables (such as `VERBOSE` and `TESTS`) are described in the
file [test/README.md](test/README.md).

Example command line:

    $ make test EXE_SHELL="$(/bin/pwd)/util/wrap.pl valgrind --error-exitcode=1 \
        --leak-check=full -q" OPENSSL_ia32cap=":0"

If an error occurs, you can then run the specific test via the `TESTS` variable
with the `VERBOSE` or `VF` or `VFP` options to gather additional information.

    $ make test VERBOSE=1 TESTS=test_test EXE_SHELL="$(/bin/pwd)/util/wrap.pl \
       valgrind --error-exitcode=1 --leak-check=full -q" OPENSSL_ia32cap=":0"

Still reachable memory
======================

OpenSSL 4.0 no longer arms `OPENSSL_cleanup()` function as an `atexit(3)`
handler. So, unless the application explicitly calls `OPENSSL_cleanup()`, valgrind and
similar memory leak detectors may report `still reachable` memory blocks
as memory leaks. An example of a valgrind report reads as follows:

    # valgrind ./pkeyread -f pem -k dh 8
    ==280439== Memcheck, a memory error detector
    ==280439== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
    ==280439== Using Valgrind-3.24.0 and LibVEX; rerun with -h for copyright info
    ==280439== Command: ./pkeyread -f pem -k dh 8
    ==280439==
    Average time per pem(dh) call: 506329.113924us
    ==280439==
    ==280439== HEAP SUMMARY:
    ==280439==     in use at exit: 239,521 bytes in 4,137 blocks
    ==280439==   total heap usage: 21,841 allocs, 17,704 frees, 4,089,104 bytes allocated
    ==280439==
    ==280439== LEAK SUMMARY:
    ==280439==    definitely lost: 0 bytes in 0 blocks
    ==280439==    indirectly lost: 0 bytes in 0 blocks
    ==280439==      possibly lost: 0 bytes in 0 blocks
    ==280439==    still reachable: 239,521 bytes in 4,137 blocks
    ==280439==         suppressed: 0 bytes in 0 blocks
    ==280439== Rerun with --leak-check=full to see details of leaked memory
    ==280439==
    ==280439== For lists of detected and suppressed errors, rerun with: -s
    ==280439== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

The valgrind output above reports there are 239,521 of reachable memory
when process exits. That memory is not regarded as a true memory leak
as the OS will reclaim that memory on process exit, rendering calls to libc
`free()` within `OPENSSL_cleanup()` useless. Also calling `OPENSSL_cleanup()`
is discouraged when libcrypto is being linked with process to satisfy more
than one dependency paths. If it is the case then calling `OPENSSL_cleanup()`
may lead to spurious application crashes during exit.

If memory leaks caused by _still reachable memory_ are still an issue,
then preferred way is to suppress those reports using the suppression
file [1] instead of changing exiting code by adding a call to `OPENSSL_cleanup()`.
The suppression file for OpenSSL is shipped within the OpenSSL sources and
can be found at`$OPENSSL_SRCS/util/valgrind.suppressions` where `OPENSSL_SRCS`
is an environment variable containing path to the OpenSSL source
tree. To use it, just add `--suppressions` option to the valgrind command:
`valgrind --suppressions="$OPENSSL_SRCS/util/valgrind.suppression" ...`
For `pkeyread` the command and output reads as follows:

    # valgrind --suppressions=$OPENSSL_SRCS/util/valgrind.suppression  ./pkeyread -f pem -k dh 8
    ==280896== Memcheck, a memory error detector
    ==280896== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
    ==280896== Using Valgrind-3.24.0 and LibVEX; rerun with -h for copyright info
    ==280896== Command: ./pkeyread -f pem -k dh 8
    ==280896==
    Average time per pem(dh) call: 476190.476190us
    ==280896==
    ==280896== HEAP SUMMARY:
    ==280896==     in use at exit: 239,521 bytes in 4,137 blocks
    ==280896==   total heap usage: 22,816 allocs, 18,679 frees, 4,325,714 bytes allocated
    ==280896==
    ==280896== LEAK SUMMARY:
    ==280896==    definitely lost: 0 bytes in 0 blocks
    ==280896==    indirectly lost: 0 bytes in 0 blocks
    ==280896==      possibly lost: 0 bytes in 0 blocks
    ==280896==    still reachable: 0 bytes in 0 blocks
    ==280896==         suppressed: 239,521 bytes in 4,137 blocks
    ==280896==
    ==280896== For lists of detected and suppressed errors, rerun with: -s
    ==280896== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)

[1] <https://valgrind.org/docs/manual/manual-core.html#manual-core.suppress>
