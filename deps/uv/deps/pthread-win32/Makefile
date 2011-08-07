# This makefile is compatible with MS nmake and can be used as a
# replacement for buildlib.bat. I've changed the target from an ordinary dll
# (/LD) to a debugging dll (/LDd).
# 
# The variables $DLLDEST and $LIBDEST hold the destination directories for the
# dll and the lib, respectively. Probably all that needs to change is $DEVROOT.


# DLL_VER:
# See pthread.h and README - This number is computed as 'current - age'
DLL_VER	= 2
DLL_VERD= $(DLL_VER)d

DEVROOT	= C:\pthreads

DLLDEST	= $(DEVROOT)\dll
LIBDEST	= $(DEVROOT)\lib
HDRDEST	= $(DEVROOT)\include

DLLS	= pthreadVCE$(DLL_VER).dll pthreadVSE$(DLL_VER).dll pthreadVC$(DLL_VER).dll \
		  pthreadVCE$(DLL_VERD).dll pthreadVSE$(DLL_VERD).dll pthreadVC$(DLL_VERD).dll
INLINED_STAMPS	= pthreadVCE$(DLL_VER).stamp pthreadVSE$(DLL_VER).stamp pthreadVC$(DLL_VER).stamp \
				  pthreadVCE$(DLL_VERD).stamp pthreadVSE$(DLL_VERD).stamp pthreadVC$(DLL_VERD).stamp
STATIC_STAMPS	= pthreadVCE$(DLL_VER).static pthreadVSE$(DLL_VER).static pthreadVC$(DLL_VER).static \
				  pthreadVCE$(DLL_VERD).static pthreadVSE$(DLL_VERD).static pthreadVC$(DLL_VERD).static

CC	= cl
CPPFLAGS = /I. /DHAVE_PTW32_CONFIG_H
STATICCRT  = /MT
STATICCRTD = /MTd
DLLCRT     = /MD
DLLCRTD    = /MDd
XCFLAGS = /W3 /nologo
CFLAGS	= /O2 /Ob2 $(XCFLAGS)
CFLAGSD	= /Z7 $(XCFLAGS)

# Uncomment this if config.h defines RETAIN_WSALASTERROR
#XLIBS = wsock32.lib

# Default cleanup style
CLEANUP	= __CLEANUP_C

# C++ Exceptions
VCEFLAGS	= /EHsc /TP $(CPPFLAGS) $(CFLAGS)
VCEFLAGSD	= /EHsc /TP $(CPPFLAGS) $(CFLAGSD)
#Structured Exceptions
VSEFLAGS	= $(CPPFLAGS) $(CFLAGS)
VSEFLAGSD	= $(CPPFLAGS) $(CFLAGSD)
#C cleanup code
VCFLAGS		= $(CPPFLAGS) $(CFLAGS)
VCFLAGSD	= $(CPPFLAGS) $(CFLAGSD)

DLL_INLINED_OBJS = \
		pthread.obj \
		version.res

# Aggregate modules for inlinability
DLL_OBJS	= \
		attr.obj \
		barrier.obj \
		cancel.obj \
		cleanup.obj \
		condvar.obj \
		create.obj \
		dll.obj \
		autostatic.obj \
		errno.obj \
		exit.obj \
		fork.obj \
		global.obj \
		misc.obj \
		mutex.obj \
		nonportable.obj \
		private.obj \
		rwlock.obj \
		sched.obj \
		semaphore.obj \
		signal.obj \
		spin.obj \
		sync.obj \
		tsd.obj \
		version.res

# Separate modules for minimising the size of statically linked images
SMALL_STATIC_OBJS	= \
		pthread_attr_init.obj \
		pthread_attr_destroy.obj \
		pthread_attr_getdetachstate.obj \
		pthread_attr_setdetachstate.obj \
		pthread_attr_getstackaddr.obj \
		pthread_attr_setstackaddr.obj \
		pthread_attr_getstacksize.obj \
		pthread_attr_setstacksize.obj \
		pthread_attr_getscope.obj \
		pthread_attr_setscope.obj \
		pthread_attr_setschedpolicy.obj \
		pthread_attr_getschedpolicy.obj \
		pthread_attr_setschedparam.obj \
		pthread_attr_getschedparam.obj \
		pthread_attr_setinheritsched.obj \
		pthread_attr_getinheritsched.obj \
		pthread_barrier_init.obj \
		pthread_barrier_destroy.obj \
		pthread_barrier_wait.obj \
		pthread_barrierattr_init.obj \
		pthread_barrierattr_destroy.obj \
		pthread_barrierattr_setpshared.obj \
		pthread_barrierattr_getpshared.obj \
		pthread_setcancelstate.obj \
		pthread_setcanceltype.obj \
		pthread_testcancel.obj \
		pthread_cancel.obj \
		cleanup.obj \
		pthread_condattr_destroy.obj \
		pthread_condattr_getpshared.obj \
		pthread_condattr_init.obj \
		pthread_condattr_setpshared.obj \
		pthread_cond_destroy.obj \
		pthread_cond_init.obj \
		pthread_cond_signal.obj \
		pthread_cond_wait.obj \
		create.obj \
		dll.obj \
		autostatic.obj \
		errno.obj \
		pthread_exit.obj \
		fork.obj \
		global.obj \
		pthread_mutex_init.obj \
		pthread_mutex_destroy.obj \
		pthread_mutexattr_init.obj \
		pthread_mutexattr_destroy.obj \
		pthread_mutexattr_getpshared.obj \
		pthread_mutexattr_setpshared.obj \
		pthread_mutexattr_settype.obj \
		pthread_mutexattr_gettype.obj \
		pthread_mutexattr_setrobust.obj \
		pthread_mutexattr_getrobust.obj \
		pthread_mutex_lock.obj \
		pthread_mutex_timedlock.obj \
		pthread_mutex_unlock.obj \
		pthread_mutex_trylock.obj \
		pthread_mutex_consistent.obj \
		pthread_mutexattr_setkind_np.obj \
		pthread_mutexattr_getkind_np.obj \
		pthread_getw32threadhandle_np.obj \
		pthread_getunique_np.obj \
		pthread_delay_np.obj \
		pthread_num_processors_np.obj \
		pthread_win32_attach_detach_np.obj \
		pthread_equal.obj \
		pthread_getconcurrency.obj \
		pthread_once.obj \
		pthread_self.obj \
		pthread_setconcurrency.obj \
		pthread_rwlock_init.obj \
		pthread_rwlock_destroy.obj \
		pthread_rwlockattr_init.obj \
		pthread_rwlockattr_destroy.obj \
		pthread_rwlockattr_getpshared.obj \
		pthread_rwlockattr_setpshared.obj \
		pthread_rwlock_rdlock.obj \
		pthread_rwlock_wrlock.obj \
		pthread_rwlock_unlock.obj \
		pthread_rwlock_tryrdlock.obj \
		pthread_rwlock_trywrlock.obj \
		pthread_setschedparam.obj \
		pthread_getschedparam.obj \
		pthread_timechange_handler_np.obj \
		ptw32_is_attr.obj \
		ptw32_processInitialize.obj \
		ptw32_processTerminate.obj \
		ptw32_threadStart.obj \
		ptw32_threadDestroy.obj \
		ptw32_tkAssocCreate.obj \
		ptw32_tkAssocDestroy.obj \
		ptw32_callUserDestroyRoutines.obj \
		ptw32_timespec.obj \
		ptw32_throw.obj \
		ptw32_getprocessors.obj \
		ptw32_calloc.obj \
		ptw32_new.obj \
		ptw32_reuse.obj \
		ptw32_rwlock_check_need_init.obj \
		ptw32_cond_check_need_init.obj \
		ptw32_mutex_check_need_init.obj \
		ptw32_semwait.obj \
		ptw32_relmillisecs.obj \
		ptw32_MCS_lock.obj \
		sched_get_priority_max.obj \
		sched_get_priority_min.obj \
		sched_setscheduler.obj \
		sched_getscheduler.obj \
		sched_yield.obj \
		sem_init.obj \
		sem_destroy.obj \
		sem_trywait.obj \
		sem_timedwait.obj \
		sem_wait.obj \
		sem_post.obj \
		sem_post_multiple.obj \
		sem_getvalue.obj \
		sem_open.obj \
		sem_close.obj \
		sem_unlink.obj \
		signal.obj \
		pthread_kill.obj \
		ptw32_spinlock_check_need_init.obj \
		pthread_spin_init.obj \
		pthread_spin_destroy.obj \
		pthread_spin_lock.obj \
		pthread_spin_unlock.obj \
		pthread_spin_trylock.obj \
		pthread_detach.obj \
		pthread_join.obj \
		pthread_key_create.obj \
		pthread_key_delete.obj \
		pthread_setspecific.obj \
		pthread_getspecific.obj \
		w32_CancelableWait.obj \
		version.res

INCL	= config.h implement.h semaphore.h pthread.h need_errno.h

ATTR_SRCS	= \
		pthread_attr_init.c \
		pthread_attr_destroy.c \
		pthread_attr_getdetachstate.c \
		pthread_attr_setdetachstate.c \
		pthread_attr_getstackaddr.c \
		pthread_attr_setstackaddr.c \
		pthread_attr_getstacksize.c \
		pthread_attr_setstacksize.c \
		pthread_attr_getscope.c \
		pthread_attr_setscope.c

BARRIER_SRCS = \
		pthread_barrier_init.c \
		pthread_barrier_destroy.c \
		pthread_barrier_wait.c \
		pthread_barrierattr_init.c \
		pthread_barrierattr_destroy.c \
		pthread_barrierattr_setpshared.c \
		pthread_barrierattr_getpshared.c

CANCEL_SRCS	= \
		pthread_setcancelstate.c \
		pthread_setcanceltype.c \
		pthread_testcancel.c \
		pthread_cancel.c 

CONDVAR_SRCS	= \
		ptw32_cond_check_need_init.c \
		pthread_condattr_destroy.c \
		pthread_condattr_getpshared.c \
		pthread_condattr_init.c \
		pthread_condattr_setpshared.c \
		pthread_cond_destroy.c \
		pthread_cond_init.c \
		pthread_cond_signal.c \
		pthread_cond_wait.c

EXIT_SRCS	= \
		pthread_exit.c

MISC_SRCS	= \
		pthread_equal.c \
		pthread_getconcurrency.c \
		pthread_kill.c \
		pthread_once.c \
		pthread_self.c \
		pthread_setconcurrency.c \
		ptw32_calloc.c \
		ptw32_MCS_lock.c \
		ptw32_new.c \
		ptw32_reuse.c \
		ptw32_relmillisecs.c \
		w32_CancelableWait.c

MUTEX_SRCS	= \
		ptw32_mutex_check_need_init.c \
		pthread_mutex_init.c \
		pthread_mutex_destroy.c \
		pthread_mutexattr_init.c \
		pthread_mutexattr_destroy.c \
		pthread_mutexattr_getpshared.c \
		pthread_mutexattr_setpshared.c \
		pthread_mutexattr_settype.c \
		pthread_mutexattr_gettype.c \
		pthread_mutexattr_setrobust.c \
		pthread_mutexattr_getrobust.c \
		pthread_mutex_lock.c \
		pthread_mutex_timedlock.c \
		pthread_mutex_unlock.c \
		pthread_mutex_trylock.c \
		pthread_mutex_consistent.c

NONPORTABLE_SRCS = \
		pthread_mutexattr_setkind_np.c \
		pthread_mutexattr_getkind_np.c \
		pthread_getw32threadhandle_np.c \
		pthread_getunique_np.c \
		pthread_delay_np.c \
		pthread_num_processors_np.c \
		pthread_win32_attach_detach_np.c \
		pthread_timechange_handler_np.c 

PRIVATE_SRCS	= \
		ptw32_is_attr.c \
		ptw32_processInitialize.c \
		ptw32_processTerminate.c \
		ptw32_threadStart.c \
		ptw32_threadDestroy.c \
		ptw32_tkAssocCreate.c \
		ptw32_tkAssocDestroy.c \
		ptw32_callUserDestroyRoutines.c \
		ptw32_semwait.c \
		ptw32_timespec.c \
		ptw32_throw.c \
		ptw32_getprocessors.c

RWLOCK_SRCS	= \
		ptw32_rwlock_check_need_init.c \
		ptw32_rwlock_cancelwrwait.c \
		pthread_rwlock_init.c \
		pthread_rwlock_destroy.c \
		pthread_rwlockattr_init.c \
		pthread_rwlockattr_destroy.c \
		pthread_rwlockattr_getpshared.c \
		pthread_rwlockattr_setpshared.c \
		pthread_rwlock_rdlock.c \
		pthread_rwlock_timedrdlock.c \
		pthread_rwlock_wrlock.c \
		pthread_rwlock_timedwrlock.c \
		pthread_rwlock_unlock.c \
		pthread_rwlock_tryrdlock.c \
		pthread_rwlock_trywrlock.c

SCHED_SRCS	= \
		pthread_attr_setschedpolicy.c \
		pthread_attr_getschedpolicy.c \
		pthread_attr_setschedparam.c \
		pthread_attr_getschedparam.c \
		pthread_attr_setinheritsched.c \
		pthread_attr_getinheritsched.c \
		pthread_setschedparam.c \
		pthread_getschedparam.c \
		sched_get_priority_max.c \
		sched_get_priority_min.c \
		sched_setscheduler.c \
		sched_getscheduler.c \
		sched_yield.c

SEMAPHORE_SRCS = \
		sem_init.c \
		sem_destroy.c \
		sem_trywait.c \
		sem_timedwait.c \
		sem_wait.c \
		sem_post.c \
		sem_post_multiple.c \
		sem_getvalue.c \
		sem_open.c \
		sem_close.c \
		sem_unlink.c

SPIN_SRCS	= \
		ptw32_spinlock_check_need_init.c \
		pthread_spin_init.c \
		pthread_spin_destroy.c \
		pthread_spin_lock.c \
		pthread_spin_unlock.c \
		pthread_spin_trylock.c

SYNC_SRCS	= \
		pthread_detach.c \
		pthread_join.c

TSD_SRCS	= \
		pthread_key_create.c \
		pthread_key_delete.c \
		pthread_setspecific.c \
		pthread_getspecific.c


help:
	@ echo Run one of the following command lines:
	@ echo nmake clean VCE   (to build the MSVC dll with C++ exception handling)
	@ echo nmake clean VSE   (to build the MSVC dll with structured exception handling)
	@ echo nmake clean VC    (to build the MSVC dll with C cleanup code)
	@ echo nmake clean VCE-inlined   (to build the MSVC inlined dll with C++ exception handling)
	@ echo nmake clean VSE-inlined   (to build the MSVC inlined dll with structured exception handling)
	@ echo nmake clean VC-inlined    (to build the MSVC inlined dll with C cleanup code)
	@ echo nmake clean VC-static     (to build the MSVC static lib with C cleanup code)
	@ echo nmake clean VCE-debug   (to build the debug MSVC dll with C++ exception handling)
	@ echo nmake clean VSE-debug   (to build the debug MSVC dll with structured exception handling)
	@ echo nmake clean VC-debug    (to build the debug MSVC dll with C cleanup code)
	@ echo nmake clean VCE-inlined-debug   (to build the debug MSVC inlined dll with C++ exception handling)
	@ echo nmake clean VSE-inlined-debug   (to build the debug MSVC inlined dll with structured exception handling)
	@ echo nmake clean VC-inlined-debug    (to build the debug MSVC inlined dll with C cleanup code)
	@ echo nmake clean VC-static-debug     (to build the debug MSVC static lib with C cleanup code)

all:
	@ $(MAKE) /E clean VCE-inlined
	@ $(MAKE) /E clean VSE-inlined
	@ $(MAKE) /E clean VC-inlined
	@ $(MAKE) /E clean VCE-inlined-debug
	@ $(MAKE) /E clean VSE-inlined-debug
	@ $(MAKE) /E clean VC-inlined-debug

VCE:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCEFLAGS) $(DLLCRT)" CLEANUP=__CLEANUP_CXX pthreadVCE$(DLL_VER).dll

VCE-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCEFLAGSD) $(DLLCRTD)" CLEANUP=__CLEANUP_CXX pthreadVCE$(DLL_VERD).dll

VSE:
	@ $(MAKE) /E /nologo EHFLAGS="$(VSEFLAGS) $(DLLCRT)" CLEANUP=__CLEANUP_SEH pthreadVSE$(DLL_VER).dll

VSE-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VSEFLAGSD) $(DLLCRTD)" CLEANUP=__CLEANUP_SEH pthreadVSE$(DLL_VERD).dll

VC:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCFLAGS) $(DLLCRT)" CLEANUP=__CLEANUP_C pthreadVC$(DLL_VER).dll

VC-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCFLAGSD) $(DLLCRTD)" CLEANUP=__CLEANUP_C pthreadVC$(DLL_VERD).dll

#
# The so-called inlined DLL is just a single translation unit with
# inlining optimisation turned on.
#
VCE-inlined:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCEFLAGS) $(DLLCRT) /DPTW32_BUILD_INLINED" CLEANUP=__CLEANUP_CXX pthreadVCE$(DLL_VER).stamp

VCE-inlined-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCEFLAGSD) $(DLLCRTD) /DPTW32_BUILD_INLINED" CLEANUP=__CLEANUP_CXX pthreadVCE$(DLL_VERD).stamp

VSE-inlined:
	@ $(MAKE) /E /nologo EHFLAGS="$(VSEFLAGS) $(DLLCRT) /DPTW32_BUILD_INLINED" CLEANUP=__CLEANUP_SEH pthreadVSE$(DLL_VER).stamp

VSE-inlined-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VSEFLAGSD) $(DLLCRTD) /DPTW32_BUILD_INLINED" CLEANUP=__CLEANUP_SEH pthreadVSE$(DLL_VERD).stamp

VC-inlined:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCFLAGS) $(DLLCRT) /DPTW32_BUILD_INLINED" CLEANUP=__CLEANUP_C pthreadVC$(DLL_VER).stamp

VC-inlined-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCFLAGSD) $(DLLCRTD) /DPTW32_BUILD_INLINED" CLEANUP=__CLEANUP_C pthreadVC$(DLL_VERD).stamp

VC-static:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCFLAGS) $(STATICCRT) /DPTW32_BUILD_INLINED /DPTW32_STATIC_LIB" CLEANUP=__CLEANUP_C pthreadVC$(DLL_VER).static

VC-static-debug:
	@ $(MAKE) /E /nologo EHFLAGS="$(VCFLAGSD) $(STATICCRTD) /DPTW32_BUILD_INLINED /DPTW32_STATIC_LIB" CLEANUP=__CLEANUP_C pthreadVC$(DLL_VERD).static

realclean: clean
	if exist pthread*.dll del pthread*.dll
	if exist pthread*.lib del pthread*.lib
	if exist *.manifest del *.manifest
	if exist *.stamp del *.stamp

clean:
	if exist *.obj del *.obj
	if exist *.def del *.def
	if exist *.ilk del *.ilk
	if exist *.pdb del *.pdb
	if exist *.exp del *.exp
	if exist *.map del *.map
	if exist *.o del *.o
	if exist *.i del *.i
	if exist *.res del *.res


install:
	copy pthread*.dll $(DLLDEST)
	copy pthread*.lib $(LIBDEST)
	copy pthread.h $(HDRDEST)
	copy sched.h $(HDRDEST)
	copy semaphore.h $(HDRDEST)

$(DLLS): $(DLL_OBJS)
	$(CC) /LDd /Zi /nologo $(DLL_OBJS) /link /implib:$*.lib $(XLIBS) /out:$@

$(INLINED_STAMPS): $(DLL_INLINED_OBJS)
	$(CC) /LDd /Zi /nologo $(DLL_INLINED_OBJS) /link /implib:$*.lib $(XLIBS) /out:$*.dll

$(STATIC_STAMPS): $(DLL_INLINED_OBJS)
	if exist $*.lib del $*.lib
	lib $(DLL_INLINED_OBJS) /out:$*.lib

.c.obj:
	$(CC) $(EHFLAGS) /D$(CLEANUP) -c $<

.rc.res:
	rc /dPTW32_RC_MSC /d$(CLEANUP) $<

.c.i:
	$(CC) /P /O2 /Ob1 $(VCFLAGS) $<

attr.obj:	attr.c $(ATTR_SRCS) $(INCL)
barrier.obj:	barrier.c $(BARRIER_SRCS) $(INCL)
cancel.obj:	cancel.c $(CANCEL_SRCS) $(INCL)
condvar.obj:	condvar.c $(CONDVAR_SRCS) $(INCL)
exit.obj:	exit.c $(EXIT_SRCS) $(INCL)
misc.obj:	misc.c $(MISC_SRCS) $(INCL)
mutex.obj:	mutex.c $(MUTEX_SRCS) $(INCL)
nonportable.obj:	nonportable.c $(NONPORTABLE_SRCS) $(INCL)
private.obj:	private.c $(PRIVATE_SRCS) $(INCL)
rwlock.obj:	rwlock.c $(RWLOCK_SRCS) $(INCL)
sched.obj:	sched.c $(SCHED_SRCS) $(INCL)
semaphore.obj:	semaphore.c $(SEMAPHORE_SRCS) $(INCL)
spin.obj:	spin.c $(SPIN_SRCS) $(INCL)
sync.obj:	sync.c $(SYNC_SRCS) $(INCL)
tsd.obj:	tsd.c $(TSD_SRCS) $(INCL)
version.res:	version.rc $(INCL)
