Watcom compiler notes
=====================

Status
------
Not yet usable. Although the library builds under Watcom it
substantially fails the test suite.

There is a working Wmakefile for wmake for the library build.

invoke as any of:
wmake -f Wmakefile clean WC
wmake -f Wmakefile clean WC-inlined
wmake -f Wmakefile clean WCE
wmake -f Wmakefile clean WCE-inlined

These build pthreadWC.dll and pthreadWCE.dll.

There is a working Wmakefile for wmake for the test suite.

invoke as any of:
wmake -f Wmakefile clean WC
wmake -f Wmakefile clean WCX
wmake -f Wmakefile clean WCE
wmake -f Wmakefile clean WC-bench
wmake -f Wmakefile clean WCX-bench
wmake -f Wmakefile clean WCE-bench


Current known problems
----------------------

Library build:
The Watcom compiler uses a different default call convention to MS C or GNU C and so
applications are not compatible with pthreadVC.dll etc using pre 2003-10-14 versions
of pthread.h, sched.h, or semaphore.h. The cdecl attribute can be used on exposed
function prototypes to force compatibility with MS C built DLLs.

However, there appear to be other incompatibilities. Errno.h, for example, defines
different values for the standard C and POSIX errors to those defined by the MS C
errno.h. It may be that references to Watcom's threads compatible 'errno' do set
and return translated numbers consistently, but I have not verified this.

Watcom defines errno as a dereferenced pointer returned by the function
_get_errno_ptr(). This is similar to both the MS and GNU C environments for
multithreaded use. However, the Watcom version appears to have a number of problems:

- different threads return the same pointer value. Compare with the MS and GNU C
versions which correctly return different values (since each thread must maintain
a thread specific errno value).

- an errno value set within the DLL appears as zero in the application even though
both share the same thread.

Therefore applications built using the Watcom compiler may need to use
a Watcom built version of the library (pthreadWC.dll). If this is the case, then
the cdecl function attribute should not be required.

Application builds:
The test suite fails with the Watcom compiler.

Test semaphore1.c fails for pthreadWC.dll because errno returns 0 instead of EAGAIN.
