In ptw32_InterlockedCompareExchange.c, I've added a section for
Borland's compiler; it's identical to that for the MS compiler except
that it uses /* ... */ comments instead of ; comments.

[RPJ: need to define HAVE_TASM32 in config.h to use the above.]


The other file is a makefile suitable for use with Borland's compiler
(run "make -fBmakefile" in the directory).  It builds a single version
of the library, pthreadBC.dll and the corresponding pthreadBC.lib
import library, which is comparable to the pthreadVC version; I can't
personally see any demand for the versions that include structured or
C++ exception cancellation handling so I haven't attempted to build
those versions of the library.  (I imagine a static version might be
of use to some, but we can't legally use that on my commercial
projects so I can't try that out, unfortunately.)

[RPJ: Added tests\Bmakefile as well.]

Borland C++ doesn't define the ENOSYS constant used by pthreads-win32;
rather than make more extensive patches to the pthreads-win32 source I
have a mostly-arbitrary constant for it in the makefile.  However this
doesn't make it visible to the application using the library, so if
anyone actually wants to use this constant in their apps (why?)
someone might like to make a seperate NEED_BCC_something define to add
this stuff.

The makefile also #defines EDEADLK as EDEADLOCK, _timeb as timeb, and
_ftime as ftime, to deal with the minor differences between the two
RTLs' naming conventions, and sets the compiler flags as required to
get a normal compile of the library.

[RPJ: Moved errno values and _timeb etc to pthread.h, so apps will also
use them.]

(While I'm on the subject, the reason Borland users should recompile
the library, rather than using the impdef/implib technique suggested
previously on the mailing list, is that a) the errno constants are
different, so the results returned by the pthread_* functions can be
meaningless, and b) the errno variable/pseudo-variable itself is
different in the MS & BCC runtimes, so you can't access the
pthreadVC's errno from a Borland C++-compiled host application
correctly - I imagine there are other potential problems from the RTL
mismatch too.)

[RPJ: Make sure you use the same RTL in both dll and application builds.
The dll and tests Bmakefiles use cw32mti.lib. Having some trouble with
memory read exceptions running the test suite using BCC55.]

Best regards,
Will

-- 
Will Bryant
Systems Architect, eCOSM Limited
Cell +64 21 655 443, office +64 3 365 4176
http://www.ecosm.com/
