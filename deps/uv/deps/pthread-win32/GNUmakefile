#
# --------------------------------------------------------------------------
#
#      Pthreads-win32 - POSIX Threads Library for Win32
#      Copyright(C) 1998 John E. Bossom
#      Copyright(C) 1999,2005 Pthreads-win32 contributors
# 
#      Contact Email: rpj@callisto.canberra.edu.au
# 
#      The current list of contributors is contained
#      in the file CONTRIBUTORS included with the source
#      code distribution. The list can also be seen at the
#      following World Wide Web location:
#      http://sources.redhat.com/pthreads-win32/contributors.html
# 
#      This library is free software; you can redistribute it and/or
#      modify it under the terms of the GNU Lesser General Public
#      License as published by the Free Software Foundation; either
#      version 2 of the License, or (at your option) any later version.
# 
#      This library is distributed in the hope that it will be useful,
#      but WITHOUT ANY WARRANTY; without even the implied warranty of
#      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#      Lesser General Public License for more details.
# 
#      You should have received a copy of the GNU Lesser General Public
#      License along with this library in the file COPYING.LIB;
#      if not, write to the Free Software Foundation, Inc.,
#      59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
#

DLL_VER	= 2
DLL_VERD= $(DLL_VER)d

DEVROOT	= C:\PTHREADS

DLLDEST	= $(DEVROOT)\DLL
LIBDEST	= $(DEVROOT)\DLL

# If Running MsysDTK
RM	= rm -f
MV	= mv -f
CP	= cp -f

# If not.
#RM	= erase
#MV	= rename
#CP	= copy

# For cross compiling use e.g.
# make CROSS=x86_64-w64-mingw32- clean GC-inlined
CROSS	= 

AR	= $(CROSS)ar
DLLTOOL = $(CROSS)dlltool
CC      = $(CROSS)gcc
CXX     = $(CROSS)g++
RANLIB  = $(CROSS)ranlib
RC	= $(CROSS)windres

OPT	= $(CLEANUP) -O3 # -finline-functions -findirect-inlining
XOPT	=

RCFLAGS		= --include-dir=.
# Uncomment this if config.h defines RETAIN_WSALASTERROR
#LFLAGS		= -lws2_32

# ----------------------------------------------------------------------
# The library can be built with some alternative behaviour to
# facilitate development of applications on Win32 that will be ported
# to other POSIX systems. Nothing definable here will make the library
# non-compliant, but applications that make assumptions that POSIX
# does not garrantee may fail or misbehave under some settings.
#
# PTW32_THREAD_ID_REUSE_INCREMENT
# Purpose:
# POSIX says that applications should assume that thread IDs can be
# recycled. However, Solaris and some other systems use a [very large]
# sequence number as the thread ID, which provides virtual uniqueness.
# Pthreads-win32 provides pseudo-unique IDs when the default increment
# (1) is used, but pthread_t is not a scalar type like Solaris's.
#
# Usage:
# Set to any value in the range: 0 <= value <= 2^wordsize
#
# Examples:
# Set to 0 to emulate non recycle-unique behaviour like Linux or *BSD.
# Set to 1 for recycle-unique thread IDs (this is the default).
# Set to some other +ve value to emulate smaller word size types
# (i.e. will wrap sooner).
#
#PTW32_FLAGS	= "-DPTW32_THREAD_ID_REUSE_INCREMENT=0"
#
# ----------------------------------------------------------------------

GC_CFLAGS	= $(PTW32_FLAGS) 
GCE_CFLAGS	= $(PTW32_FLAGS) -mthreads

## Mingw32
MAKE		?= make
CFLAGS	= $(OPT) $(XOPT) -I. -DHAVE_PTW32_CONFIG_H -Wall

DLL_INLINED_OBJS	= \
		pthread.o \
		version.o

# Agregate modules for inlinability
DLL_OBJS	= \
		attr.o \
		barrier.o \
		cancel.o \
		cleanup.o \
		condvar.o \
		create.o \
		dll.o \
		errno.o \
		exit.o \
		fork.o \
		global.o \
		misc.o \
		mutex.o \
		nonportable.o \
		private.o \
		rwlock.o \
		sched.o \
		semaphore.o \
		signal.o \
		spin.o \
		sync.o \
		tsd.o \
		version.o

# Separate modules for minimum size statically linked images
SMALL_STATIC_OBJS	= \
		pthread_attr_init.o \
		pthread_attr_destroy.o \
		pthread_attr_getdetachstate.o \
		pthread_attr_setdetachstate.o \
		pthread_attr_getstackaddr.o \
		pthread_attr_setstackaddr.o \
		pthread_attr_getstacksize.o \
		pthread_attr_setstacksize.o \
		pthread_attr_getscope.o \
		pthread_attr_setscope.o \
		pthread_attr_setschedpolicy.o \
		pthread_attr_getschedpolicy.o \
		pthread_attr_setschedparam.o \
		pthread_attr_getschedparam.o \
		pthread_attr_setinheritsched.o \
		pthread_attr_getinheritsched.o \
		pthread_barrier_init.o \
		pthread_barrier_destroy.o \
		pthread_barrier_wait.o \
		pthread_barrierattr_init.o \
		pthread_barrierattr_destroy.o \
		pthread_barrierattr_setpshared.o \
		pthread_barrierattr_getpshared.o \
		pthread_setcancelstate.o \
		pthread_setcanceltype.o \
		pthread_testcancel.o \
		pthread_cancel.o \
		cleanup.o \
		pthread_condattr_destroy.o \
		pthread_condattr_getpshared.o \
		pthread_condattr_init.o \
		pthread_condattr_setpshared.o \
		pthread_cond_destroy.o \
		pthread_cond_init.o \
		pthread_cond_signal.o \
		pthread_cond_wait.o \
		create.o \
		dll.o \
		autostatic.o \
		errno.o \
		pthread_exit.o \
		fork.o \
		global.o \
		pthread_mutex_init.o \
		pthread_mutex_destroy.o \
		pthread_mutexattr_init.o \
		pthread_mutexattr_destroy.o \
		pthread_mutexattr_getpshared.o \
		pthread_mutexattr_setpshared.o \
		pthread_mutexattr_settype.o \
		pthread_mutexattr_gettype.o \
		pthread_mutexattr_setrobust.o \
		pthread_mutexattr_getrobust.o \
		pthread_mutex_lock.o \
		pthread_mutex_timedlock.o \
		pthread_mutex_unlock.o \
		pthread_mutex_trylock.o \
		pthread_mutex_consistent.o \
		pthread_mutexattr_setkind_np.o \
		pthread_mutexattr_getkind_np.o \
		pthread_getw32threadhandle_np.o \
		pthread_getunique_np.o \
		pthread_delay_np.o \
		pthread_num_processors_np.o \
		pthread_win32_attach_detach_np.o \
		pthread_equal.o \
		pthread_getconcurrency.o \
		pthread_once.o \
		pthread_self.o \
		pthread_setconcurrency.o \
		pthread_rwlock_init.o \
		pthread_rwlock_destroy.o \
		pthread_rwlockattr_init.o \
		pthread_rwlockattr_destroy.o \
		pthread_rwlockattr_getpshared.o \
		pthread_rwlockattr_setpshared.o \
		pthread_rwlock_rdlock.o \
		pthread_rwlock_wrlock.o \
		pthread_rwlock_unlock.o \
		pthread_rwlock_tryrdlock.o \
		pthread_rwlock_trywrlock.o \
		pthread_setschedparam.o \
		pthread_getschedparam.o \
		pthread_timechange_handler_np.o \
		ptw32_is_attr.o \
		ptw32_cond_check_need_init.o \
		ptw32_MCS_lock.o \
		ptw32_mutex_check_need_init.o \
		ptw32_processInitialize.o \
		ptw32_processTerminate.o \
		ptw32_threadStart.o \
		ptw32_threadDestroy.o \
		ptw32_tkAssocCreate.o \
		ptw32_tkAssocDestroy.o \
		ptw32_callUserDestroyRoutines.o \
		ptw32_timespec.o \
		ptw32_throw.o \
		ptw32_getprocessors.o \
		ptw32_calloc.o \
		ptw32_new.o \
		ptw32_reuse.o \
		ptw32_semwait.o \
		ptw32_relmillisecs.o \
		ptw32_rwlock_check_need_init.o \
		sched_get_priority_max.o \
		sched_get_priority_min.o \
		sched_setscheduler.o \
		sched_getscheduler.o \
		sched_yield.o \
		sem_init.o \
		sem_destroy.o \
		sem_trywait.o \
		sem_timedwait.o \
		sem_wait.o \
		sem_post.o \
		sem_post_multiple.o \
		sem_getvalue.o \
		sem_open.o \
		sem_close.o \
		sem_unlink.o \
		signal.o \
		pthread_kill.o \
		ptw32_spinlock_check_need_init.o \
		pthread_spin_init.o \
		pthread_spin_destroy.o \
		pthread_spin_lock.o \
		pthread_spin_unlock.o \
		pthread_spin_trylock.o \
		pthread_detach.o \
		pthread_join.o \
		pthread_key_create.o \
		pthread_key_delete.o \
		pthread_setspecific.o \
		pthread_getspecific.o \
		w32_CancelableWait.o \
		version.o

INCL	= \
		config.h \
		implement.h \
		semaphore.h \
		pthread.h \
		need_errno.h

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
		ptw32_relmillisecs.c \
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


GCE_DLL	= pthreadGCE$(DLL_VER).dll
GCED_DLL= pthreadGCE$(DLL_VERD).dll
GCE_LIB	= libpthreadGCE$(DLL_VER).a
GCED_LIB= libpthreadGCE$(DLL_VERD).a
GCE_INLINED_STAMP = pthreadGCE$(DLL_VER).stamp
GCED_INLINED_STAMP = pthreadGCE$(DLL_VERD).stamp
GCE_STATIC_STAMP = libpthreadGCE$(DLL_VER).stamp
GCED_STATIC_STAMP = libpthreadGCE$(DLL_VERD).stamp

GC_DLL 	= pthreadGC$(DLL_VER).dll
GCD_DLL	= pthreadGC$(DLL_VERD).dll
GC_LIB	= libpthreadGC$(DLL_VER).a
GCD_LIB	= libpthreadGC$(DLL_VERD).a
GC_INLINED_STAMP = pthreadGC$(DLL_VER).stamp
GCD_INLINED_STAMP = pthreadGC$(DLL_VERD).stamp
GC_STATIC_STAMP = libpthreadGC$(DLL_VER).stamp
GCD_STATIC_STAMP = libpthreadGC$(DLL_VERD).stamp

PTHREAD_DEF	= pthread.def

help:
	@ echo "Run one of the following command lines:"
	@ echo "make clean GC            (to build the GNU C dll with C cleanup code)"
	@ echo "make clean GCE           (to build the GNU C dll with C++ exception handling)"
	@ echo "make clean GC-inlined    (to build the GNU C inlined dll with C cleanup code)"
	@ echo "make clean GCE-inlined   (to build the GNU C inlined dll with C++ exception handling)"
	@ echo "make clean GC-static     (to build the GNU C inlined static lib with C cleanup code)"
	@ echo "make clean GC-debug      (to build the GNU C debug dll with C cleanup code)"
	@ echo "make clean GCE-debug     (to build the GNU C debug dll with C++ exception handling)"
	@ echo "make clean GC-inlined-debug    (to build the GNU C inlined debug dll with C cleanup code)"
	@ echo "make clean GCE-inlined-debug   (to build the GNU C inlined debug dll with C++ exception handling)"
	@ echo "make clean GC-static-debug     (to build the GNU C inlined static debug lib with C cleanup code)"

all:
	@ $(MAKE) clean GCE
	@ $(MAKE) clean GC

GC:
		$(MAKE) CLEANUP=-D__CLEANUP_C XC_FLAGS="$(GC_CFLAGS)" OBJ="$(DLL_OBJS)" $(GC_DLL)

GC-debug:
		$(MAKE) CLEANUP=-D__CLEANUP_C XC_FLAGS="$(GC_CFLAGS)" OBJ="$(DLL_OBJS)" DLL_VER=$(DLL_VERD) OPT="-D__CLEANUP_C -g -O0" $(GCD_DLL)

GCE:
		$(MAKE) CC=$(CXX) CLEANUP=-D__CLEANUP_CXX XC_FLAGS="$(GCE_CFLAGS)" OBJ="$(DLL_OBJS)" $(GCE_DLL)

GCE-debug:
		$(MAKE) CC=$(CXX) CLEANUP=-D__CLEANUP_CXX XC_FLAGS="$(GCE_CFLAGS)" OBJ="$(DLL_OBJS)" DLL_VER=$(DLL_VERD) OPT="-D__CLEANUP_CXX -g -O0" $(GCED_DLL)

GC-inlined:
		$(MAKE) XOPT="-DPTW32_BUILD_INLINED" CLEANUP=-D__CLEANUP_C XC_FLAGS="$(GC_CFLAGS)" OBJ="$(DLL_INLINED_OBJS)" $(GC_INLINED_STAMP)

GC-inlined-debug:
		$(MAKE) XOPT="-DPTW32_BUILD_INLINED" CLEANUP=-D__CLEANUP_C XC_FLAGS="$(GC_CFLAGS)" OBJ="$(DLL_INLINED_OBJS)" DLL_VER=$(DLL_VERD) OPT="-D__CLEANUP_C -g -O0" $(GCD_INLINED_STAMP)

GCE-inlined:
		$(MAKE) CC=$(CXX) XOPT="-DPTW32_BUILD_INLINED" CLEANUP=-D__CLEANUP_CXX XC_FLAGS="$(GCE_CFLAGS)" OBJ="$(DLL_INLINED_OBJS)" $(GCE_INLINED_STAMP)

GCE-inlined-debug:
		$(MAKE) CC=$(CXX) XOPT="-DPTW32_BUILD_INLINED" CLEANUP=-D__CLEANUP_CXX XC_FLAGS="$(GCE_CFLAGS)" OBJ="$(DLL_INLINED_OBJS)" DLL_VER=$(DLL_VERD) OPT="-D__CLEANUP_CXX -g -O0" $(GCED_INLINED_STAMP)

GC-static:
		$(MAKE) XOPT="-DPTW32_BUILD_INLINED -DPTW32_STATIC_LIB" CLEANUP=-D__CLEANUP_C XC_FLAGS="$(GC_CFLAGS)" OBJ="$(DLL_INLINED_OBJS)" $(GC_STATIC_STAMP)

GC-static-debug:
		$(MAKE) XOPT="-DPTW32_BUILD_INLINED -DPTW32_STATIC_LIB" CLEANUP=-D__CLEANUP_C XC_FLAGS="$(GC_CFLAGS)" OBJ="$(DLL_INLINED_OBJS)" DLL_VER=$(DLL_VERD) OPT="-D__CLEANUP_C -g -O0" $(GCD_STATIC_STAMP)

tests:
	@ cd tests
	@ $(MAKE) auto

%.pre: %.c
	$(CC) -E -o $@ $(CFLAGS) $^

%.s: %.c
	$(CC) -c $(CFLAGS) -DPTW32_BUILD_INLINED -Wa,-ahl $^ > $@

%.o: %.rc
	$(RC) $(RCFLAGS) $(CLEANUP) -o $@ $<

.SUFFIXES: .dll .rc .c .o

.c.o:;		 $(CC) -c -o $@ $(CFLAGS) $(XC_FLAGS) $<


$(GC_DLL) $(GCD_DLL): $(DLL_OBJS)
	$(CC) $(OPT) -shared -o $(GC_DLL) $(DLL_OBJS) $(LFLAGS)
	$(DLLTOOL) -z pthread.def $(DLL_OBJS)
	$(DLLTOOL) -k --dllname $@ --output-lib $(GC_LIB) --def $(PTHREAD_DEF)

$(GCE_DLL): $(DLL_OBJS)
	$(CC) $(OPT) -mthreads -shared -o $(GCE_DLL) $(DLL_OBJS) $(LFLAGS)
	$(DLLTOOL) -z pthread.def $(DLL_OBJS)
	$(DLLTOOL) -k --dllname $@ --output-lib $(GCE_LIB) --def $(PTHREAD_DEF)

$(GC_INLINED_STAMP) $(GCD_INLINED_STAMP): $(DLL_INLINED_OBJS)
	$(CC) $(OPT) $(XOPT) -shared -o $(GC_DLL) $(DLL_INLINED_OBJS) $(LFLAGS)
	$(DLLTOOL) -z pthread.def $(DLL_INLINED_OBJS)
	$(DLLTOOL) -k --dllname $(GC_DLL) --output-lib $(GC_LIB) --def $(PTHREAD_DEF)
	echo touched > $(GC_INLINED_STAMP)

$(GCE_INLINED_STAMP) $(GCED_INLINED_STAMP): $(DLL_INLINED_OBJS)
	$(CC) $(OPT) $(XOPT) -mthreads -shared -o $(GCE_DLL) $(DLL_INLINED_OBJS)  $(LFLAGS)
	$(DLLTOOL) -z pthread.def $(DLL_INLINED_OBJS)
	$(DLLTOOL) -k --dllname $(GCE_DLL) --output-lib $(GCE_LIB) --def $(PTHREAD_DEF)
	echo touched > $(GCE_INLINED_STAMP)

$(GC_STATIC_STAMP) $(GCD_STATIC_STAMP): $(DLL_INLINED_OBJS)
	$(RM) $(GC_LIB)
	$(AR) -rv $(GC_LIB) $(DLL_INLINED_OBJS)
	$(RANLIB) $(GC_LIB)
	echo touched > $(GC_STATIC_STAMP)

clean:
	-$(RM) *~
	-$(RM) *.i
	-$(RM) *.s
	-$(RM) *.o
	-$(RM) *.obj
	-$(RM) *.exe
	-$(RM) $(PTHREAD_DEF)

realclean: clean
	-$(RM) $(GC_LIB)
	-$(RM) $(GCE_LIB)
	-$(RM) $(GC_DLL)
	-$(RM) $(GCE_DLL)
	-$(RM) $(GC_INLINED_STAMP)
	-$(RM) $(GCE_INLINED_STAMP)
	-$(RM) $(GC_STATIC_STAMP)
	-$(RM) $(GCD_LIB)
	-$(RM) $(GCED_LIB)
	-$(RM) $(GCD_DLL)
	-$(RM) $(GCED_DLL)
	-$(RM) $(GCD_INLINED_STAMP)
	-$(RM) $(GCED_INLINED_STAMP)
	-$(RM) $(GCD_STATIC_STAMP)

attr.o:		attr.c $(ATTR_SRCS) $(INCL)
barrier.o:	barrier.c $(BARRIER_SRCS) $(INCL)
cancel.o:	cancel.c $(CANCEL_SRCS) $(INCL)
condvar.o:	condvar.c $(CONDVAR_SRCS) $(INCL)
exit.o:		exit.c $(EXIT_SRCS) $(INCL)
misc.o:		misc.c $(MISC_SRCS) $(INCL)
mutex.o:	mutex.c $(MUTEX_SRCS) $(INCL)
nonportable.o:	nonportable.c $(NONPORTABLE_SRCS) $(INCL)
private.o:	private.c $(PRIVATE_SRCS) $(INCL)
rwlock.o:	rwlock.c $(RWLOCK_SRCS) $(INCL)
sched.o:	sched.c $(SCHED_SRCS) $(INCL)
semaphore.o:	semaphore.c $(SEMAPHORE_SRCS) $(INCL)
spin.o:		spin.c $(SPIN_SRCS) $(INCL)
sync.o:		sync.c $(SYNC_SRCS) $(INCL)
tsd.o:		tsd.c $(TSD_SRCS) $(INCL)
version.o:	version.rc $(INCL)
