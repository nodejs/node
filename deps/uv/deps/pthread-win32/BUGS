----------
Known bugs
----------

1. Not strictly a bug, more of a gotcha.

   Under MS VC++ (only tested with version 6.0), a term_func
   set via the standard C++ set_terminate() function causes the
   application to abort.

   Notes from the MSVC++ manual:
         1) A term_func() should call exit(), otherwise
            abort() will be called on return to the caller.
            A call to abort() raises SIGABRT and the default signal handler
            for all signals terminates the calling program with
            exit code 3.
         2) A term_func() must not throw an exception. Therefore
            term_func() should not call pthread_exit(), which
            works by throwing an exception (pthreadVCE or pthreadVSE)
            or by calling longjmp (pthreadVC).

   Workaround: avoid using pthread_exit() in C++ applications. Exit
   threads by dropping through the end of the thread routine.

2. Cancellation problems in C++ builds
   - Milan Gardian

   [Note: It's not clear if this problem isn't simply due to the context
   switch in pthread_cancel() which occurs unless the QueueUserAPCEx
   library and driver are installed and used. Just like setjmp/longjmp,
   this is probably not going to work well in C++. In any case, unless for
   some very unusual reason you really must use the C++ build then please
   use the C build pthreadVC2.dll or pthreadGC2.dll, i.e. for C++
   applications.]

   This is suspected to be a compiler bug in VC6.0, and also seen in
   VC7.0 and VS .NET 2003. The GNU C++ compiler does not have a problem
   with this, and it has been reported that the Intel C++ 8.1 compiler
   and Visual C++ 2005 Express Edition Beta2 pass tests\semaphore4.c
   (which exposes the bug).

   Workaround [rpj - 2 Feb 2002]
   -----------------------------
   [Please note: this workaround did not solve a similar problem in
   snapshot-2004-11-03 or later, even though similar symptoms were seen.
   tests\semaphore4.c fails in that snapshot for the VCE version of the
   DLL.]

   The problem disappears when /Ob0 is used, i.e. /O2 /Ob0 works OK,
   but if you want to use inlining optimisation you can be much more
   specific about where it's switched off and on by using a pragma.

   So the inlining optimisation is interfering with the way that cleanup
   handlers are run. It appears to relate to auto-inlining of class methods
   since this is the only auto inlining that is performed at /O1 optimisation
   (functions with the "inline" qualifier are also inlined, but the problem
   doesn't appear to involve any such functions in the library or testsuite).

   In order to confirm the inlining culprit, the following use of pragmas
   eliminate the problem but I don't know how to make it transparent, putting
   it in, say, pthread.h where pthread_cleanup_push defined as a macro.

   #pragma inline_depth(0)
     pthread_cleanup_push(handlerFunc, (void *) &arg);

        /* ... */

     pthread_cleanup_pop(0);
   #pragma inline_depth()

   Note the empty () pragma value after the pop macro. This resets depth to the
   default. Or you can specify a non-zero depth here.

   The pragma is also needed (and now used) within the library itself wherever
   cleanup handlers are used (condvar.c and rwlock.c).

   Use of these pragmas allows compiler optimisations /O1 and /O2 to be
   used for either or both the library and applications.

   Experimenting further, I found that wrapping the actual cleanup handler
   function with #pragma auto_inline(off|on) does NOT work.

   MSVC6.0 doesn't appear to support the C99 standard's _Pragma directive,
   however, later versions may. This form is embeddable inside #define
   macros, which would be ideal because it would mean that it could be added
   to the push/pop macro definitions in pthread.h and hidden from the
   application programmer.

   [/rpj]

   Original problem description
   ----------------------------

   The cancellation (actually, cleanup-after-cancel) tests fail when using VC
   (professional) optimisation switches (/O1 or /O2) in pthreads library. I
   have not investigated which concrete optimisation technique causes this
   problem (/Og, /Oi, /Ot, /Oy, /Ob1, /Gs, /Gf, /Gy, etc.), but here is a
   summary of builds and corresponding failures:

     * pthreads VSE (optimised tests): OK
     * pthreads VCE (optimised tests): Failed "cleanup1" test (runtime)

     * pthreads VSE (DLL in CRT, optimised tests): OK
     * pthreads VCE (DLL in CRT, optimised tests): Failed "cleanup1" test
   (runtime)

   Please note that while in VSE version of the pthreads library the
   optimisation does not really have any impact on the tests (they pass OK), in
   VCE version addition of optimisation (/O2 in this case) causes the tests to
   fail uniformly - either in "cleanup0" or "cleanup1" test cases.

   Please note that all the tests above use default pthreads DLL (no
   optimisations, linked with either static or DLL CRT, based on test type).
   Therefore the problem lies not within the pthreads DLL but within the
   compiled client code (the application using pthreads -> involvement of
   "pthread.h").

   I think the message of this section is that usage of VCE version of pthreads
   in applications relying on cancellation/cleanup AND using optimisations for
   creation of production code is highly unreliable for the current version of
   the pthreads library.

3. The Borland Builder 5.5 version of the library produces memory read exceptions
in some tests.

4. pthread_barrier_wait() can deadlock if the number of potential calling
threads for a particular barrier is greater than the barrier count parameter
given to pthread_barrier_init() for that barrier.

This is due to the very lightweight implementation of pthread-win32 barriers.
To cope with more than "count" possible waiters, barriers must effectively
implement all the same safeguards as condition variables, making them much
"heavier" than at present.

The workaround is to ensure that no more than "count" threads attempt to wait
at the barrier.

5. Canceling a thread blocked on pthread_once appears not to work in the MSVC++
version of the library "pthreadVCE.dll". The test case "once3.c" hangs. I have no
clues on this at present. All other versions pass this test ok - pthreadsVC.dll,
pthreadsVSE.dll, pthreadsGC.dll and pthreadsGCE.dll. 
