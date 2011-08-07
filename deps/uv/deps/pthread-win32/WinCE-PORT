NOTE: The comments in this file relate to the original WinCE port
done by Tristan Savatier. The semaphore routines have been 
completely rewritten since (2005-04-25), having been progressively
broken more and more by changes to the library. All of the semaphore
routines implemented for W9x/WNT/2000 and up should now also work for
WinCE. Also, pthread_mutex_timedlock should now work.

Additional WinCE updates have been applied since this as well. Check the
ChangeLog file and search for WINCE for example. (2007-01-07)

[RPJ]

----

Some interesting news:

I have been able to port pthread-win32 to Windows-CE,
which uses a subset of the WIN32 API.

Since we intend to keep using pthread-win32 for our
Commercial WinCE developments, I would be very interested
if WinCE support could be added to the main source tree
of pthread-win32.  Also, I would like to be credited
for this port :-)

Now, here is the story...

The port was performed and tested on a Casio "Cassiopeia"
PalmSize PC, which runs a MIP processor.  The OS in the
Casio is WinCE version 2.11, but I used VC++ 6.0 with
the WinCE SDK for version 2.01.

I used pthread-win32 to port a heavily multithreaded
commercial application (real-time MPEG video player)
from Linux to WinCE.  I consider the changes that
I have done to be quite well tested.

Overall the modifications that we had to do are minor.

The WinCE port were based on pthread-win32-snap-1999-05-30,
but I am certain that they can be integrated very easiely
to more recent versions of the source.

I have attached the modified source code:
pthread-win32-snap-1999-05-30-WinCE.

All the changes do not affect the code compiled on non-WinCE
environment, provided that the macros used for WinCE compilation
are not used, of course!

Overall description of the WinCE port:
-------------------------------------

Most of the changes had to be made in areas where
pthread-win32 was relying on some standard-C librairies
(e.g. _ftime, calloc, errno), which are not available
on WinCE. We have changed the code to use native Win32
API instead (or in some cases we made wrappers).

The Win32 Semaphores are not available,
so we had to re-implement Semaphores using mutexes
and events.

Limitations / known problems of the WinCE port:
----------------------------------------------

Not all the semaphore routines have been ported
(semaphores are defined by Posix but are not part
pf pthread).  I have just done enough to make
pthread routines (that rely internally on semaphores)
work, like signal conditions.

I noticed that the Win32 threads work slightly
differently on WinCE.  This may have some impact
on some tricky parts of pthread-win32, but I have
not really investigated.  For example, on WinCE,
the process is killed if the main thread falls off
the bottom (or calls pthread_exit), regardless
of the existence of any other detached thread.
Microsoft manual indicates that this behavior is
deffirent from that of Windows Threads for other
Win32 platforms.


Detailed descriptions of the changes and rationals:

------------------------------------
- use a new macro NEED_ERRNO.

If defined, the code in errno.c that defines a reentrant errno
is compiled, regardless of _MT and _REENTRANT.

Rational: On WinCE, there is no support for <stdio.h>, <errno.h> or
any other standard C library, i.e. even if _MT or _REENTRANT
is defined, errno is not provided by any library.  NEED_ERRNO
must be set to compile for WinCE.

------------------------------------
- In implement.h, change #include <semaphore.h> to #include "semaphore.h".

Rational: semaphore.h is provided in pthread-win32 and should not
be searched in the systems standard include.  would not compile.
This change does not seem to create problems on "classic" win32
(e.g. win95).

------------------------------------
- use a new macro NEED_CALLOC.

If defined, some code in misc.c will provide a replacement
for calloc, which is not available on Win32.


------------------------------------
- use a new macro NEED_CREATETHREAD.

If defined, implement.h defines the macro _beginthreadex
and _endthreadex.

Rational: On WinCE, the wrappers _beginthreadex and _endthreadex
do not exist. The native Win32 routines must be used.

------------------------------------
- in misc.c:

#ifdef NEED_DUPLICATEHANDLE
	  /* DuplicateHandle does not exist on WinCE */
	  self->threadH = GetCurrentThread();
#else
	  if( !DuplicateHandle(
			       GetCurrentProcess(),
			       GetCurrentThread(),
			       GetCurrentProcess(),
			       &self->threadH,
			       0,
			       FALSE,
			       DUPLICATE_SAME_ACCESS ) )
	    {
	      free( self );
	      return (NULL);
	    }
#endif

Rational: On WinCE, DuplicateHandle does not exist.  I could not understand
why DuplicateHandle must be used.  It seems to me that getting the current
thread handle with GetCurrentThread() is sufficient, and it seems to work
perfectly fine, so maybe DuplicateHandle was just plain useless to begin with ?

------------------------------------
- In private.c, added some code at the beginning of ptw32_processInitialize
to detect the case of multiple calls to ptw32_processInitialize.

Rational: In order to debug pthread-win32, it is easier to compile
it as a regular library (it is not possible to debug DLL's on winCE).
In that case, the application must call ptw32_rocessInitialize()
explicitely, to initialize pthread-win32.  It is safer in this circumstance
to handle the case where ptw32_processInitialize() is called on
an already initialized library:

int
ptw32_processInitialize (void)
{
	if (ptw32_processInitialized) {
		/* 
		 * ignore if already initialized. this is useful for 
		 * programs that uses a non-dll pthread
		 * library. such programs must call ptw32_processInitialize() explicitely,
		 * since this initialization routine is automatically called only when
		 * the dll is loaded.
		 */
		return TRUE;
	}
    ptw32_processInitialized = TRUE;
  	[...]
}

------------------------------------
- in private.c, if macro NEED_FTIME is defined, add routines to
convert timespec_to_filetime and filetime_to_timespec, and modified
code that was using _ftime() to use Win32 API instead.

Rational: _ftime is not available on WinCE.  It is necessary to use
the native Win32 time API instead.

Note: the routine timespec_to_filetime is provided as a convenience and a mean
to test that filetime_to_timespec works, but it is not used by the library.

------------------------------------
- in semaphore.c, if macro NEED_SEM is defined, add code for the routines
_increase_semaphore and _decrease_semaphore, and modify significantly
the implementation of the semaphores so that it does not use CreateSemaphore.

Rational: CreateSemaphore is not available on WinCE.  I had to re-implement
semaphores using mutexes and Events.

Note: Only the semaphore routines that are used by pthread are implemented
(i.e. signal conditions rely on a subset of the semaphores routines, and
this subset works). Some other semaphore routines (e.g. sem_trywait) are
not yet supported on my WinCE port (and since I don't need them, I am not
planning to do anything about them).

------------------------------------
- in tsd.c, changed the code that defines TLS_OUT_OF_INDEXES

/* TLS_OUT_OF_INDEXES not defined on WinCE */
#ifndef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES 0xffffffff
#endif

Rational: TLS_OUT_OF_INDEXES is not defined in any standard include file
on WinCE.

------------------------------------
- added file need_errno.h

Rational: On WinCE, there is no errno.h file. need_errno.h is just a
copy of windows version of errno.h, with minor modifications due to the fact
that some of the error codes are defined by the WinCE socket library.
In pthread.h, if NEED_ERRNO is defined, the file need_errno.h is
included (instead of <errno.h>).


-- eof
