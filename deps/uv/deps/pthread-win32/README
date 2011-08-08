PTHREADS-WIN32
==============

Pthreads-win32 is free software, distributed under the GNU Lesser
General Public License (LGPL). See the file 'COPYING.LIB' for terms
and conditions. Also see the file 'COPYING' for information
specific to pthreads-win32, copyrights and the LGPL.


What is it?
-----------

Pthreads-win32 is an Open Source Software implementation of the
Threads component of the POSIX 1003.1c 1995 Standard (or later)
for Microsoft's Win32 environment. Some functions from POSIX
1003.1b are also supported including semaphores. Other related
functions include the set of read-write lock functions. The
library also supports some of the functionality of the Open
Group's Single Unix specification, version 2, namely mutex types,
plus some common and pthreads-win32 specific non-portable
routines (see README.NONPORTABLE).

See the file "ANNOUNCE" for more information including standards
conformance details and the list of supported and unsupported
routines.


Prerequisites
-------------
MSVC or GNU C (MinGW32 MSys development kit)
	To build from source.

QueueUserAPCEx by Panagiotis E. Hadjidoukas
	To support any thread cancelation in C++ library builds or
	to support cancelation of blocked threads in any build.
	This library is not required otherwise.

	For true async cancelation of threads (including blocked threads).
	This is a DLL and Windows driver that provides pre-emptive APC
	by forcing threads into an alertable state when the APC is queued.
	Both the DLL and driver are provided with the pthreads-win32.exe
	self-unpacking ZIP, and on the pthreads-win32 FTP site  (in source
	and pre-built forms). Currently this is a separate LGPL package to
	pthreads-win32. See the README in the QueueUserAPCEx folder for
	installation instructions.

	Pthreads-win32 will automatically detect if the QueueUserAPCEx DLL
	QuserEx.DLL is available and whether the driver AlertDrv.sys is
	loaded. If it is not available, pthreads-win32 will simulate async
	cancelation, which means that it can async cancel only threads that
	are runnable. The simulated async cancellation cannot cancel blocked
	threads.

        [FOR SECURITY] To be found Quserex.dll MUST be installed in the
	Windows System Folder. This is not an unreasonable constraint given a
	driver must also be installed and loaded at system startup.


Library naming
--------------

Because the library is being built using various exception
handling schemes and compilers - and because the library
may not work reliably if these are mixed in an application,
each different version of the library has it's own name.

Note 1: the incompatibility is really between EH implementations
of the different compilers. It should be possible to use the
standard C version from either compiler with C++ applications
built with a different compiler. If you use an EH version of
the library, then you must use the same compiler for the
application. This is another complication and dependency that
can be avoided by using only the standard C library version.

Note 2: if you use a standard C pthread*.dll with a C++
application, then any functions that you define that are
intended to be called via pthread_cleanup_push() must be
__cdecl.

Note 3: the intention was to also name either the VC or GC
version (it should be arbitrary) as pthread.dll, including
pthread.lib and libpthread.a as appropriate. This is no longer
likely to happen.

Note 4: the compatibility number was added so that applications
can differentiate between binary incompatible versions of the
libs and dlls.

In general:
	pthread[VG]{SE,CE,C}[c].dll
	pthread[VG]{SE,CE,C}[c].lib

where:
	[VG] indicates the compiler
	V	- MS VC, or
	G	- GNU C

	{SE,CE,C} indicates the exception handling scheme
	SE	- Structured EH, or
	CE	- C++ EH, or
	C	- no exceptions - uses setjmp/longjmp

	c	- DLL compatibility number indicating ABI and API
		  compatibility with applications built against
		  a snapshot with the same compatibility number.
		  See 'Version numbering' below.

The name may also be suffixed by a 'd' to indicate a debugging version
of the library. E.g. pthreadVC2d.lib. Debugging versions contain
additional information for debugging (symbols etc) and are often not
optimised in any way (compiled with optimisation turned off).

Examples:
	pthreadVSE.dll	(MSVC/SEH)
	pthreadGCE.dll	(GNUC/C++ EH)
	pthreadGC.dll	(GNUC/not dependent on exceptions)
	pthreadVC1.dll	(MSVC/not dependent on exceptions - not binary
			compatible with pthreadVC.dll)
	pthreadVC2.dll	(MSVC/not dependent on exceptions - not binary
			compatible with pthreadVC1.dll or pthreadVC.dll)

The GNU library archive file names have correspondingly changed to:

	libpthreadGCEc.a
	libpthreadGCc.a


Versioning numbering
--------------------

Version numbering is separate from the snapshot dating system, and
is the canonical version identification system embedded within the
DLL using the Microsoft version resource system. The versioning
system chosen follows the GNU Libtool system. See
http://www.gnu.org/software/libtool/manual.html section 6.2.

See the resource file 'version.rc'.

Microsoft version numbers use 4 integers:

	0.0.0.0

Pthreads-win32 uses the first 3 following the Libtool convention.
The fourth is commonly used for the build number, but will be reserved
for future use.

	current.revision.age.0

The numbers are changed as follows:

1. If the library source code has changed at all since the last update,
   then increment revision (`c:r:a' becomes `c:r+1:a').
2. If any interfaces have been added, removed, or changed since the last
   update, increment current, and set revision to 0.
3. If any interfaces have been added since the last public release, then
   increment age.
4. If any interfaces have been removed or changed since the last public
   release, then set age to 0.


DLL compatibility numbering is an attempt to ensure that applications
always load a compatible pthreads-win32 DLL by using a DLL naming system
that is consistent with the version numbering system. It also allows
older and newer DLLs to coexist in the same filesystem so that older
applications can continue to be used. For pre .NET Windows systems,
this inevitably requires incompatible versions of the same DLLs to have
different names.

Pthreads-win32 has adopted the Cygwin convention of appending a single
integer number to the DLL name. The number used is based on the library
version number and is computed as 'current' - 'age'.

(See http://home.att.net/~perlspinr/libversioning.html for a nicely
detailed explanation.)

Using this method, DLL name/s will only change when the DLL's
backwards compatibility changes. Note that the addition of new
'interfaces' will not of itself change the DLL's compatibility for older
applications.


Which of the several dll versions to use?
-----------------------------------------
or,
---
What are all these pthread*.dll and pthread*.lib files?
-------------------------------------------------------

Simple, use either pthreadGCv.* if you use GCC, or pthreadVCv.* if you
use MSVC - where 'v' is the DLL versioning (compatibility) number.

Otherwise, you need to choose carefully and know WHY.

The most important choice you need to make is whether to use a
version that uses exceptions internally, or not. There are versions
of the library that use exceptions as part of the thread
cancelation and exit implementation. The default version uses
setjmp/longjmp.

There is some contension amongst POSIX threads experts as
to how POSIX threads cancelation and exit should work
with languages that use exceptions, e.g. C++ and even C
(Microsoft's Structured Exceptions).

The issue is: should cancelation of a thread in, say,
a C++ application cause object destructors and C++ exception
handlers to be invoked as the stack unwinds during thread
exit, or not?

There seems to be more opinion in favour of using the
standard C version of the library (no EH) with C++ applications
for the reason that this appears to be the assumption commercial
pthreads implementations make. Therefore, if you use an EH version
of pthreads-win32 then you may be under the illusion that
your application will be portable, when in fact it is likely to
behave differently when linked with other pthreads libraries.

Now you may be asking: then why have you kept the EH versions of
the library?

There are a couple of reasons:
- there is division amongst the experts and so the code may
  be needed in the future. Yes, it's in the repository and we
  can get it out anytime in the future, but it would be difficult
  to find.
- pthreads-win32 is one of the few implementations, and possibly
  the only freely available one, that has EH versions. It may be
  useful to people who want to play with or study application
  behaviour under these conditions.

Notes:

[If you use either pthreadVCE or pthreadGCE]

1. [See also the discussion in the FAQ file - Q2, Q4, and Q5]

If your application contains catch(...) blocks in your POSIX
threads then you will need to replace the "catch(...)" with the macro
"PtW32Catch", eg.

	#ifdef PtW32Catch
		PtW32Catch {
			...
		}
	#else
		catch(...) {
			...
		}
	#endif

Otherwise neither pthreads cancelation nor pthread_exit() will work
reliably when using versions of the library that use C++ exceptions
for cancelation and thread exit.

This is due to what is believed to be a C++ compliance error in VC++
whereby you may not have multiple handlers for the same exception in
the same try/catch block. GNU G++ doesn't have this restriction.


Other name changes
------------------

All snapshots prior to and including snapshot 2000-08-13
used "_pthread_" as the prefix to library internal
functions, and "_PTHREAD_" to many library internal
macros. These have now been changed to "ptw32_" and "PTW32_"
respectively so as to not conflict with the ANSI standard's
reservation of identifiers beginning with "_" and "__" for
use by compiler implementations only.

If you have written any applications and you are linking
statically with the pthreads-win32 library then you may have
included a call to _pthread_processInitialize. You will
now have to change that to ptw32_processInitialize.


Cleanup code default style
--------------------------

Previously, if not defined, the cleanup style was determined automatically
from the compiler used, and one of the following was defined accordingly:

	__CLEANUP_SEH	MSVC only
	__CLEANUP_CXX	C++, including MSVC++, GNU G++
	__CLEANUP_C	C, including GNU GCC, not MSVC

These defines determine the style of cleanup (see pthread.h) and,
most importantly, the way that cancelation and thread exit (via
pthread_exit) is performed (see the routine ptw32_throw()).

In short, the exceptions versions of the library throw an exception
when a thread is canceled, or exits via pthread_exit(). This exception is
caught by a handler in the thread startup routine, so that the
the correct stack unwinding occurs regardless of where the thread
is when it's canceled or exits via pthread_exit().

In this snapshot, unless the build explicitly defines (e.g. via a
compiler option) __CLEANUP_SEH, __CLEANUP_CXX, or __CLEANUP_C, then
the build NOW always defaults to __CLEANUP_C style cleanup. This style
uses setjmp/longjmp in the cancelation and pthread_exit implementations,
and therefore won't do stack unwinding even when linked to applications
that have it (e.g. C++ apps). This is for consistency with most/all
commercial Unix POSIX threads implementations.

Although it was not clearly documented before, it is still necessary to
build your application using the same __CLEANUP_* define as was
used for the version of the library that you link with, so that the
correct parts of pthread.h are included. That is, the possible
defines require the following library versions:

	__CLEANUP_SEH	pthreadVSE.dll
	__CLEANUP_CXX	pthreadVCE.dll or pthreadGCE.dll
	__CLEANUP_C	pthreadVC.dll or pthreadGC.dll

It is recommended that you let pthread.h use it's default __CLEANUP_C
for both library and application builds. That is, don't define any of
the above, and then link with pthreadVC.lib (MSVC or MSVC++) and
libpthreadGC.a (MinGW GCC or G++). The reason is explained below, but
another reason is that the prebuilt pthreadVCE.dll is currently broken.
Versions built with MSVC++ later than version 6 may not be broken, but I
can't verify this yet.

WHY ARE WE MAKING THE DEFAULT STYLE LESS EXCEPTION-FRIENDLY?
Because no commercial Unix POSIX threads implementation allows you to
choose to have stack unwinding. Therefore, providing it in pthread-win32
as a default is dangerous. We still provide the choice but unless
you consciously choose to do otherwise, your pthreads applications will
now run or crash in similar ways irrespective of the pthreads platform
you use. Or at least this is the hope.


Building under VC++ using C++ EH, Structured EH, or just C
----------------------------------------------------------

From the source directory run nmake without any arguments to list
help information. E.g.

$ nmake

Microsoft (R) Program Maintenance Utility   Version 6.00.8168.0
Copyright (C) Microsoft Corp 1988-1998. All rights reserved.

Run one of the following command lines:
nmake clean VCE (to build the MSVC dll with C++ exception handling)
nmake clean VSE (to build the MSVC dll with structured exception handling)
nmake clean VC (to build the MSVC dll with C cleanup code)
nmake clean VCE-inlined (to build the MSVC inlined dll with C++ exception handling)
nmake clean VSE-inlined (to build the MSVC inlined dll with structured exception handling)
nmake clean VC-inlined (to build the MSVC inlined dll with C cleanup code)
nmake clean VC-static (to build the MSVC static lib with C cleanup code)
nmake clean VCE-debug (to build the debug MSVC dll with C++ exception handling)
nmake clean VSE-debug (to build the debug MSVC dll with structured exception handling)
nmake clean VC-debug (to build the debug MSVC dll with C cleanup code)
nmake clean VCE-inlined-debug (to build the debug MSVC inlined dll with C++ exception handling)
nmake clean VSE-inlined-debug (to build the debug MSVC inlined dll with structured exception handling)
nmake clean VC-inlined-debug (to build the debug MSVC inlined dll with C cleanup code)
nmake clean VC-static-debug (to build the debug MSVC static lib with C cleanup code)


The pre-built dlls are normally built using the *-inlined targets.

You can run the testsuite by changing to the "tests" directory and
running nmake. E.g.:

$ cd tests
$ nmake

Microsoft (R) Program Maintenance Utility   Version 6.00.8168.0
Copyright (C) Microsoft Corp 1988-1998. All rights reserved.

Run one of the following command lines:
nmake clean VC (to test using VC dll with VC (no EH) applications)
nmake clean VCX (to test using VC dll with VC++ (EH) applications)
nmake clean VCE (to test using the VCE dll with VC++ EH applications)
nmake clean VSE (to test using VSE dll with VC (SEH) applications)
nmake clean VC-bench (to benchtest using VC dll with C bench app)
nmake clean VCX-bench (to benchtest using VC dll with C++ bench app)
nmake clean VCE-bench (to benchtest using VCE dll with C++ bench app)
nmake clean VSE-bench (to benchtest using VSE dll with SEH bench app)
nmake clean VC-static (to test using VC static lib with VC (no EH) applications)


Building under Mingw32
----------------------

The dll can be built easily with recent versions of Mingw32.
(The distributed versions are built using Mingw32 and MsysDTK
from www.mingw32.org.)

From the source directory, run make for help information. E.g.:

$ make
Run one of the following command lines:
make clean GC            (to build the GNU C dll with C cleanup code)
make clean GCE           (to build the GNU C dll with C++ exception handling)
make clean GC-inlined    (to build the GNU C inlined dll with C cleanup code)
make clean GCE-inlined   (to build the GNU C inlined dll with C++ exception handling)
make clean GC-static     (to build the GNU C inlined static lib with C cleanup code)
make clean GC-debug      (to build the GNU C debug dll with C cleanup code)
make clean GCE-debug     (to build the GNU C debug dll with C++ exception handling)
make clean GC-inlined-debug    (to build the GNU C inlined debug dll with C cleanup code)
make clean GCE-inlined-debug   (to build the GNU C inlined debug dll with C++ exception handling)
make clean GC-static-debug     (to build the GNU C inlined static debug lib with C cleanup code)


The pre-built dlls are normally built using the *-inlined targets.

You can run the testsuite by changing to the "tests" directory and
running make for help information. E.g.:

$ cd tests
$ make
Run one of the following command lines:
make clean GC    (to test using GC dll with C (no EH) applications)
make clean GCX   (to test using GC dll with C++ (EH) applications)
make clean GCE   (to test using GCE dll with C++ (EH) applications)
make clean GC-bench       (to benchtest using GNU C dll with C cleanup code)
make clean GCE-bench   (to benchtest using GNU C dll with C++ exception handling)
make clean GC-static   (to test using GC static lib with C (no EH) applications)


Building under Linux using the Mingw32 cross development tools
--------------------------------------------------------------

You can build the library without leaving Linux by using the Mingw32 cross
development toolchain. See http://www.libsdl.org/extras/win32/cross/ for
tools and info. The GNUmakefile contains some support for this, for example:

make CROSS=i386-mingw32msvc- clean GC-inlined

will build pthreadGCn.dll and libpthreadGCn.a (n=version#), provided your
cross-tools/bin directory is in your PATH (or use the cross-make.sh script
at the URL above).


Building the library as a statically linkable library
-----------------------------------------------------

General: PTW32_STATIC_LIB must be defined for both the library build and the
application build. The makefiles supplied and used by the following 'make'
command lines will define this for you.

MSVC (creates pthreadVCn.lib as a static link lib):

nmake clean VC-static


MinGW32 (creates libpthreadGCn.a as a static link lib):

make clean GC-static


Define PTW32_STATIC_LIB when building your application. Also, your
application must call a two non-portable routines to initialise the
some state on startup and cleanup before exit. One other routine needs
to be called to cleanup after any Win32 threads have called POSIX API
routines. See README.NONPORTABLE or the html reference manual pages for
details on these routines:

BOOL pthread_win32_process_attach_np (void);
BOOL pthread_win32_process_detach_np (void);
BOOL pthread_win32_thread_attach_np (void); // Currently a no-op
BOOL pthread_win32_thread_detach_np (void);


The tests makefiles have the same targets but only check that the
static library is statically linkable. They don't run the full
testsuite. To run the full testsuite, build the dlls and run the
dll test targets.


Building the library under Cygwin
---------------------------------

Cygwin is implementing it's own POSIX threads routines and these
will be the ones to use if you develop using Cygwin.


Ready to run binaries
---------------------

For convenience, the following ready-to-run files can be downloaded
from the FTP site (see under "Availability" below):

	pthread.h
	semaphore.h
	sched.h
	pthreadVC.dll	- built with MSVC compiler using C setjmp/longjmp
	pthreadVC.lib
	pthreadVCE.dll	- built with MSVC++ compiler using C++ EH
	pthreadVCE.lib
	pthreadVSE.dll	- built with MSVC compiler using SEH
	pthreadVSE.lib
	pthreadGC.dll	- built with Mingw32 GCC
	libpthreadGC.a	- derived from pthreadGC.dll
	pthreadGCE.dll	- built with Mingw32 G++
	libpthreadGCE.a	- derived from pthreadGCE.dll

As of August 2003 pthreads-win32 pthreadG* versions are built and tested
using the MinGW + MsysDTK environment current as of that date or later.
The following file MAY be needed for older MinGW environments.

	gcc.dll 	- needed to build and run applications that use
			  pthreadGCE.dll.


Building applications with GNU compilers
----------------------------------------

If you're using pthreadGC.dll:

With the three header files, pthreadGC.dll and libpthreadGC.a in the
same directory as your application myapp.c, you could compile, link
and run myapp.c under Mingw32 as follows:

	gcc -o myapp.exe myapp.c -I. -L. -lpthreadGC
	myapp

Or put pthreadGC.dll in an appropriate directory in your PATH,
put libpthreadGC.a in your system lib directory, and
put the three header files in your system include directory,
then use:

	gcc -o myapp.exe myapp.c -lpthreadGC
	myapp


If you're using pthreadGCE.dll:

With the three header files, pthreadGCE.dll, gcc.dll and libpthreadGCE.a
in the same directory as your application myapp.c, you could compile,
link and run myapp.c under Mingw32 as follows:

	gcc -x c++ -o myapp.exe myapp.c -I. -L. -lpthreadGCE
	myapp

Or put pthreadGCE.dll and gcc.dll in an appropriate directory in
your PATH, put libpthreadGCE.a in your system lib directory, and
put the three header files in your system include directory,
then use:

	gcc -x c++ -o myapp.exe myapp.c -lpthreadGCE
	myapp


Availability
------------

The complete source code in either unbundled, self-extracting
Zip file, or tar/gzipped format can be found at:

	ftp://sources.redhat.com/pub/pthreads-win32

The pre-built DLL, export libraries and matching pthread.h can
be found at:

	ftp://sources.redhat.com/pub/pthreads-win32/dll-latest

Home page:

	http://sources.redhat.com/pthreads-win32/


Mailing list
------------

There is a mailing list for discussing pthreads on Win32.
To join, send email to:

	pthreads-win32-subscribe@sources.redhat.com

Unsubscribe by sending mail to:

	pthreads-win32-unsubscribe@sources.redhat.com


Acknowledgements
----------------

See the ANNOUNCE file for acknowledgements.
See the 'CONTRIBUTORS' file for the list of contributors.

As much as possible, the ChangeLog file attributes
contributions and patches that have been incorporated
in the library to the individuals responsible.

Finally, thanks to all those who work on and contribute to the
POSIX and Single Unix Specification standards. The maturity of an
industry can be measured by it's open standards.

----
Ross Johnson
<rpj@callisto.canberra.edu.au>








