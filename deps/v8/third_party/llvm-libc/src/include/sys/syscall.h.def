//===-- Linux like sys/syscall.h ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SYS_SYSCALL_H
#define LLVM_LIBC_SYS_SYSCALL_H

#if defined(__linux__)

#include <asm/unistd.h>

#ifdef __NR_FAST_atomic_update
#define SYS_FAST_atomic_update __NR_FAST_atomic_update
#endif

#ifdef __NR_FAST_cmpxchg
#define SYS_FAST_cmpxchg __NR_FAST_cmpxchg
#endif

#ifdef __NR_FAST_cmpxchg64
#define SYS_FAST_cmpxchg64 __NR_FAST_cmpxchg64
#endif

#ifdef __NR__llseek
#define SYS__llseek __NR__llseek
#endif

#ifdef __NR__newselect
#define SYS__newselect __NR__newselect
#endif

#ifdef __NR__sysctl
#define SYS__sysctl __NR__sysctl
#endif

#ifdef __NR_accept
#define SYS_accept __NR_accept
#endif

#ifdef __NR_accept4
#define SYS_accept4 __NR_accept4
#endif

#ifdef __NR_access
#define SYS_access __NR_access
#endif

#ifdef __NR_acct
#define SYS_acct __NR_acct
#endif

#ifdef __NR_acl_get
#define SYS_acl_get __NR_acl_get
#endif

#ifdef __NR_acl_set
#define SYS_acl_set __NR_acl_set
#endif

#ifdef __NR_add_key
#define SYS_add_key __NR_add_key
#endif

#ifdef __NR_adjtimex
#define SYS_adjtimex __NR_adjtimex
#endif

#ifdef __NR_afs_syscall
#define SYS_afs_syscall __NR_afs_syscall
#endif

#ifdef __NR_alarm
#define SYS_alarm __NR_alarm
#endif

#ifdef __NR_alloc_hugepages
#define SYS_alloc_hugepages __NR_alloc_hugepages
#endif

#ifdef __NR_arch_prctl
#define SYS_arch_prctl __NR_arch_prctl
#endif

#ifdef __NR_arm_fadvise64_64
#define SYS_arm_fadvise64_64 __NR_arm_fadvise64_64
#endif

#ifdef __NR_arm_sync_file_range
#define SYS_arm_sync_file_range __NR_arm_sync_file_range
#endif

#ifdef __NR_atomic_barrier
#define SYS_atomic_barrier __NR_atomic_barrier
#endif

#ifdef __NR_atomic_cmpxchg_32
#define SYS_atomic_cmpxchg_32 __NR_atomic_cmpxchg_32
#endif

#ifdef __NR_attrctl
#define SYS_attrctl __NR_attrctl
#endif

#ifdef __NR_bdflush
#define SYS_bdflush __NR_bdflush
#endif

#ifdef __NR_bind
#define SYS_bind __NR_bind
#endif

#ifdef __NR_bpf
#define SYS_bpf __NR_bpf
#endif

#ifdef __NR_break
#define SYS_break __NR_break
#endif

#ifdef __NR_brk
#define SYS_brk __NR_brk
#endif

#ifdef __NR_cachectl
#define SYS_cachectl __NR_cachectl
#endif

#ifdef __NR_cacheflush
#define SYS_cacheflush __NR_cacheflush
#endif

#ifdef __NR_capget
#define SYS_capget __NR_capget
#endif

#ifdef __NR_capset
#define SYS_capset __NR_capset
#endif

#ifdef __NR_chdir
#define SYS_chdir __NR_chdir
#endif

#ifdef __NR_chmod
#define SYS_chmod __NR_chmod
#endif

#ifdef __NR_chown
#define SYS_chown __NR_chown
#endif

#ifdef __NR_chown32
#define SYS_chown32 __NR_chown32
#endif

#ifdef __NR_chroot
#define SYS_chroot __NR_chroot
#endif

#ifdef __NR_clock_adjtime
#define SYS_clock_adjtime __NR_clock_adjtime
#endif

#ifdef __NR_clock_getres
#define SYS_clock_getres __NR_clock_getres
#endif

#ifdef __NR_clock_gettime
#define SYS_clock_gettime __NR_clock_gettime
#endif

#ifdef __NR_clock_gettime64
#define SYS_clock_gettime64 __NR_clock_gettime64
#endif

#ifdef __NR_clock_nanosleep
#define SYS_clock_nanosleep __NR_clock_nanosleep
#endif

#ifdef __NR_clock_nanosleep_time64
#define SYS_clock_nanosleep_time64 __NR_clock_nanosleep_time64
#endif

#ifdef __NR_clock_settime
#define SYS_clock_settime __NR_clock_settime
#endif

#ifdef __NR_clone
#define SYS_clone __NR_clone
#endif

#ifdef __NR_clone2
#define SYS_clone2 __NR_clone2
#endif

#ifdef __NR_close
#define SYS_close __NR_close
#endif

#ifdef __NR_cmpxchg_badaddr
#define SYS_cmpxchg_badaddr __NR_cmpxchg_badaddr
#endif

#ifdef __NR_connect
#define SYS_connect __NR_connect
#endif

#ifdef __NR_copy_file_range
#define SYS_copy_file_range __NR_copy_file_range
#endif

#ifdef __NR_creat
#define SYS_creat __NR_creat
#endif

#ifdef __NR_create_module
#define SYS_create_module __NR_create_module
#endif

#ifdef __NR_delete_module
#define SYS_delete_module __NR_delete_module
#endif

#ifdef __NR_dipc
#define SYS_dipc __NR_dipc
#endif

#ifdef __NR_dup
#define SYS_dup __NR_dup
#endif

#ifdef __NR_dup2
#define SYS_dup2 __NR_dup2
#endif

#ifdef __NR_dup3
#define SYS_dup3 __NR_dup3
#endif

#ifdef __NR_epoll_create
#define SYS_epoll_create __NR_epoll_create
#endif

#ifdef __NR_epoll_create1
#define SYS_epoll_create1 __NR_epoll_create1
#endif

#ifdef __NR_epoll_ctl
#define SYS_epoll_ctl __NR_epoll_ctl
#endif

#ifdef __NR_epoll_ctl_old
#define SYS_epoll_ctl_old __NR_epoll_ctl_old
#endif

#ifdef __NR_epoll_pwait
#define SYS_epoll_pwait __NR_epoll_pwait
#endif

#ifdef __NR_epoll_pwait2
#define SYS_epoll_pwait2 __NR_epoll_pwait2
#endif

#ifdef __NR_epoll_wait
#define SYS_epoll_wait __NR_epoll_wait
#endif

#ifdef __NR_epoll_wait_old
#define SYS_epoll_wait_old __NR_epoll_wait_old
#endif

#ifdef __NR_eventfd
#define SYS_eventfd __NR_eventfd
#endif

#ifdef __NR_eventfd2
#define SYS_eventfd2 __NR_eventfd2
#endif

#ifdef __NR_exec_with_loader
#define SYS_exec_with_loader __NR_exec_with_loader
#endif

#ifdef __NR_execv
#define SYS_execv __NR_execv
#endif

#ifdef __NR_execve
#define SYS_execve __NR_execve
#endif

#ifdef __NR_execveat
#define SYS_execveat __NR_execveat
#endif

#ifdef __NR_exit
#define SYS_exit __NR_exit
#endif

#ifdef __NR_exit_group
#define SYS_exit_group __NR_exit_group
#endif

#ifdef __NR_faccessat
#define SYS_faccessat __NR_faccessat
#endif

#ifdef __NR_faccessat2
#define SYS_faccessat2 __NR_faccessat2
#endif

#ifdef __NR_fadvise64
#define SYS_fadvise64 __NR_fadvise64
#endif

#ifdef __NR_fadvise64_64
#define SYS_fadvise64_64 __NR_fadvise64_64
#endif

#ifdef __NR_fallocate
#define SYS_fallocate __NR_fallocate
#endif

#ifdef __NR_fanotify_init
#define SYS_fanotify_init __NR_fanotify_init
#endif

#ifdef __NR_fanotify_mark
#define SYS_fanotify_mark __NR_fanotify_mark
#endif

#ifdef __NR_fchdir
#define SYS_fchdir __NR_fchdir
#endif

#ifdef __NR_fchmod
#define SYS_fchmod __NR_fchmod
#endif

#ifdef __NR_fchmodat
#define SYS_fchmodat __NR_fchmodat
#endif

#ifdef __NR_fchmodat2
#define SYS_fchmodat2 __NR_fchmodat2
#endif

#ifdef __NR_fchown
#define SYS_fchown __NR_fchown
#endif

#ifdef __NR_fchown32
#define SYS_fchown32 __NR_fchown32
#endif

#ifdef __NR_fchownat
#define SYS_fchownat __NR_fchownat
#endif

#ifdef __NR_fcntl
#define SYS_fcntl __NR_fcntl
#endif

#ifdef __NR_fcntl64
#define SYS_fcntl64 __NR_fcntl64
#endif

#ifdef __NR_fdatasync
#define SYS_fdatasync __NR_fdatasync
#endif

#ifdef __NR_fgetxattr
#define SYS_fgetxattr __NR_fgetxattr
#endif

#ifdef __NR_finit_module
#define SYS_finit_module __NR_finit_module
#endif

#ifdef __NR_flistxattr
#define SYS_flistxattr __NR_flistxattr
#endif

#ifdef __NR_flock
#define SYS_flock __NR_flock
#endif

#ifdef __NR_fork
#define SYS_fork __NR_fork
#endif

#ifdef __NR_free_hugepages
#define SYS_free_hugepages __NR_free_hugepages
#endif

#ifdef __NR_fremovexattr
#define SYS_fremovexattr __NR_fremovexattr
#endif

#ifdef __NR_fsetxattr
#define SYS_fsetxattr __NR_fsetxattr
#endif

#ifdef __NR_fstat
#define SYS_fstat __NR_fstat
#endif

#ifdef __NR_fstat64
#define SYS_fstat64 __NR_fstat64
#endif

#ifdef __NR_fstatat64
#define SYS_fstatat64 __NR_fstatat64
#endif

#ifdef __NR_fstatfs
#define SYS_fstatfs __NR_fstatfs
#endif

#ifdef __NR_fstatfs64
#define SYS_fstatfs64 __NR_fstatfs64
#endif

#ifdef __NR_fsync
#define SYS_fsync __NR_fsync
#endif

#ifdef __NR_ftime
#define SYS_ftime __NR_ftime
#endif

#ifdef __NR_ftruncate
#define SYS_ftruncate __NR_ftruncate
#endif

#ifdef __NR_ftruncate64
#define SYS_ftruncate64 __NR_ftruncate64
#endif

#ifdef __NR_futex
#define SYS_futex __NR_futex
#endif

#ifdef __NR_futex_time64
#define SYS_futex_time64 __NR_futex_time64
#endif

#ifdef __NR_futimesat
#define SYS_futimesat __NR_futimesat
#endif

#ifdef __NR_get_kernel_syms
#define SYS_get_kernel_syms __NR_get_kernel_syms
#endif

#ifdef __NR_get_mempolicy
#define SYS_get_mempolicy __NR_get_mempolicy
#endif

#ifdef __NR_get_robust_list
#define SYS_get_robust_list __NR_get_robust_list
#endif

#ifdef __NR_get_thread_area
#define SYS_get_thread_area __NR_get_thread_area
#endif

#ifdef __NR_getcpu
#define SYS_getcpu __NR_getcpu
#endif

#ifdef __NR_getcwd
#define SYS_getcwd __NR_getcwd
#endif

#ifdef __NR_getdents
#define SYS_getdents __NR_getdents
#endif

#ifdef __NR_getdents64
#define SYS_getdents64 __NR_getdents64
#endif

#ifdef __NR_getdomainname
#define SYS_getdomainname __NR_getdomainname
#endif

#ifdef __NR_getdtablesize
#define SYS_getdtablesize __NR_getdtablesize
#endif

#ifdef __NR_getegid
#define SYS_getegid __NR_getegid
#endif

#ifdef __NR_getegid32
#define SYS_getegid32 __NR_getegid32
#endif

#ifdef __NR_geteuid
#define SYS_geteuid __NR_geteuid
#endif

#ifdef __NR_geteuid32
#define SYS_geteuid32 __NR_geteuid32
#endif

#ifdef __NR_getgid
#define SYS_getgid __NR_getgid
#endif

#ifdef __NR_getgid32
#define SYS_getgid32 __NR_getgid32
#endif

#ifdef __NR_getgroups
#define SYS_getgroups __NR_getgroups
#endif

#ifdef __NR_getgroups32
#define SYS_getgroups32 __NR_getgroups32
#endif

#ifdef __NR_gethostname
#define SYS_gethostname __NR_gethostname
#endif

#ifdef __NR_getitimer
#define SYS_getitimer __NR_getitimer
#endif

#ifdef __NR_getpagesize
#define SYS_getpagesize __NR_getpagesize
#endif

#ifdef __NR_getpeername
#define SYS_getpeername __NR_getpeername
#endif

#ifdef __NR_getpgid
#define SYS_getpgid __NR_getpgid
#endif

#ifdef __NR_getpgrp
#define SYS_getpgrp __NR_getpgrp
#endif

#ifdef __NR_getpid
#define SYS_getpid __NR_getpid
#endif

#ifdef __NR_getpmsg
#define SYS_getpmsg __NR_getpmsg
#endif

#ifdef __NR_getppid
#define SYS_getppid __NR_getppid
#endif

#ifdef __NR_getpriority
#define SYS_getpriority __NR_getpriority
#endif

#ifdef __NR_getrandom
#define SYS_getrandom __NR_getrandom
#endif

#ifdef __NR_getresgid
#define SYS_getresgid __NR_getresgid
#endif

#ifdef __NR_getresgid32
#define SYS_getresgid32 __NR_getresgid32
#endif

#ifdef __NR_getresuid
#define SYS_getresuid __NR_getresuid
#endif

#ifdef __NR_getresuid32
#define SYS_getresuid32 __NR_getresuid32
#endif

#ifdef __NR_getrlimit
#define SYS_getrlimit __NR_getrlimit
#endif

#ifdef __NR_getrusage
#define SYS_getrusage __NR_getrusage
#endif

#ifdef __NR_getsid
#define SYS_getsid __NR_getsid
#endif

#ifdef __NR_getsockname
#define SYS_getsockname __NR_getsockname
#endif

#ifdef __NR_getsockopt
#define SYS_getsockopt __NR_getsockopt
#endif

#ifdef __NR_gettid
#define SYS_gettid __NR_gettid
#endif

#ifdef __NR_gettimeofday
#define SYS_gettimeofday __NR_gettimeofday
#endif

#ifdef __NR_getuid
#define SYS_getuid __NR_getuid
#endif

#ifdef __NR_getuid32
#define SYS_getuid32 __NR_getuid32
#endif

#ifdef __NR_getunwind
#define SYS_getunwind __NR_getunwind
#endif

#ifdef __NR_getxattr
#define SYS_getxattr __NR_getxattr
#endif

#ifdef __NR_getxgid
#define SYS_getxgid __NR_getxgid
#endif

#ifdef __NR_getxpid
#define SYS_getxpid __NR_getxpid
#endif

#ifdef __NR_getxuid
#define SYS_getxuid __NR_getxuid
#endif

#ifdef __NR_gtty
#define SYS_gtty __NR_gtty
#endif

#ifdef __NR_idle
#define SYS_idle __NR_idle
#endif

#ifdef __NR_init_module
#define SYS_init_module __NR_init_module
#endif

#ifdef __NR_inotify_add_watch
#define SYS_inotify_add_watch __NR_inotify_add_watch
#endif

#ifdef __NR_inotify_init
#define SYS_inotify_init __NR_inotify_init
#endif

#ifdef __NR_inotify_init1
#define SYS_inotify_init1 __NR_inotify_init1
#endif

#ifdef __NR_inotify_rm_watch
#define SYS_inotify_rm_watch __NR_inotify_rm_watch
#endif

#ifdef __NR_io_cancel
#define SYS_io_cancel __NR_io_cancel
#endif

#ifdef __NR_io_destroy
#define SYS_io_destroy __NR_io_destroy
#endif

#ifdef __NR_io_getevents
#define SYS_io_getevents __NR_io_getevents
#endif

#ifdef __NR_io_pgetevents
#define SYS_io_pgetevents __NR_io_pgetevents
#endif

#ifdef __NR_io_setup
#define SYS_io_setup __NR_io_setup
#endif

#ifdef __NR_io_submit
#define SYS_io_submit __NR_io_submit
#endif

#ifdef __NR_ioctl
#define SYS_ioctl __NR_ioctl
#endif

#ifdef __NR_ioperm
#define SYS_ioperm __NR_ioperm
#endif

#ifdef __NR_iopl
#define SYS_iopl __NR_iopl
#endif

#ifdef __NR_ioprio_get
#define SYS_ioprio_get __NR_ioprio_get
#endif

#ifdef __NR_ioprio_set
#define SYS_ioprio_set __NR_ioprio_set
#endif

#ifdef __NR_ipc
#define SYS_ipc __NR_ipc
#endif

#ifdef __NR_kcmp
#define SYS_kcmp __NR_kcmp
#endif

#ifdef __NR_kern_features
#define SYS_kern_features __NR_kern_features
#endif

#ifdef __NR_kexec_file_load
#define SYS_kexec_file_load __NR_kexec_file_load
#endif

#ifdef __NR_kexec_load
#define SYS_kexec_load __NR_kexec_load
#endif

#ifdef __NR_keyctl
#define SYS_keyctl __NR_keyctl
#endif

#ifdef __NR_kill
#define SYS_kill __NR_kill
#endif

#ifdef __NR_lchown
#define SYS_lchown __NR_lchown
#endif

#ifdef __NR_lchown32
#define SYS_lchown32 __NR_lchown32
#endif

#ifdef __NR_lgetxattr
#define SYS_lgetxattr __NR_lgetxattr
#endif

#ifdef __NR_link
#define SYS_link __NR_link
#endif

#ifdef __NR_linkat
#define SYS_linkat __NR_linkat
#endif

#ifdef __NR_listen
#define SYS_listen __NR_listen
#endif

#ifdef __NR_listxattr
#define SYS_listxattr __NR_listxattr
#endif

#ifdef __NR_llistxattr
#define SYS_llistxattr __NR_llistxattr
#endif

#ifdef __NR_llseek
#define SYS_llseek __NR_llseek
#endif

#ifdef __NR_lock
#define SYS_lock __NR_lock
#endif

#ifdef __NR_lookup_dcookie
#define SYS_lookup_dcookie __NR_lookup_dcookie
#endif

#ifdef __NR_lremovexattr
#define SYS_lremovexattr __NR_lremovexattr
#endif

#ifdef __NR_lseek
#define SYS_lseek __NR_lseek
#endif

#ifdef __NR_lsetxattr
#define SYS_lsetxattr __NR_lsetxattr
#endif

#ifdef __NR_lstat
#define SYS_lstat __NR_lstat
#endif

#ifdef __NR_lstat64
#define SYS_lstat64 __NR_lstat64
#endif

#ifdef __NR_madvise
#define SYS_madvise __NR_madvise
#endif

#ifdef __NR_mbind
#define SYS_mbind __NR_mbind
#endif

#ifdef __NR_membarrier
#define SYS_membarrier __NR_membarrier
#endif

#ifdef __NR_memfd_create
#define SYS_memfd_create __NR_memfd_create
#endif

#ifdef __NR_memory_ordering
#define SYS_memory_ordering __NR_memory_ordering
#endif

#ifdef __NR_migrate_pages
#define SYS_migrate_pages __NR_migrate_pages
#endif

#ifdef __NR_mincore
#define SYS_mincore __NR_mincore
#endif

#ifdef __NR_mkdir
#define SYS_mkdir __NR_mkdir
#endif

#ifdef __NR_mkdirat
#define SYS_mkdirat __NR_mkdirat
#endif

#ifdef __NR_mknod
#define SYS_mknod __NR_mknod
#endif

#ifdef __NR_mknodat
#define SYS_mknodat __NR_mknodat
#endif

#ifdef __NR_mlock
#define SYS_mlock __NR_mlock
#endif

#ifdef __NR_mlock2
#define SYS_mlock2 __NR_mlock2
#endif

#ifdef __NR_mlockall
#define SYS_mlockall __NR_mlockall
#endif

#ifdef __NR_mmap
#define SYS_mmap __NR_mmap
#endif

#ifdef __NR_mmap2
#define SYS_mmap2 __NR_mmap2
#endif

#ifdef __NR_modify_ldt
#define SYS_modify_ldt __NR_modify_ldt
#endif

#ifdef __NR_mount
#define SYS_mount __NR_mount
#endif

#ifdef __NR_move_pages
#define SYS_move_pages __NR_move_pages
#endif

#ifdef __NR_mprotect
#define SYS_mprotect __NR_mprotect
#endif

#ifdef __NR_mpx
#define SYS_mpx __NR_mpx
#endif

#ifdef __NR_mq_getsetattr
#define SYS_mq_getsetattr __NR_mq_getsetattr
#endif

#ifdef __NR_mq_notify
#define SYS_mq_notify __NR_mq_notify
#endif

#ifdef __NR_mq_open
#define SYS_mq_open __NR_mq_open
#endif

#ifdef __NR_mq_timedreceive
#define SYS_mq_timedreceive __NR_mq_timedreceive
#endif

#ifdef __NR_mq_timedsend
#define SYS_mq_timedsend __NR_mq_timedsend
#endif

#ifdef __NR_mq_unlink
#define SYS_mq_unlink __NR_mq_unlink
#endif

#ifdef __NR_mremap
#define SYS_mremap __NR_mremap
#endif

#ifdef __NR_msgctl
#define SYS_msgctl __NR_msgctl
#endif

#ifdef __NR_msgget
#define SYS_msgget __NR_msgget
#endif

#ifdef __NR_msgrcv
#define SYS_msgrcv __NR_msgrcv
#endif

#ifdef __NR_msgsnd
#define SYS_msgsnd __NR_msgsnd
#endif

#ifdef __NR_msync
#define SYS_msync __NR_msync
#endif

#ifdef __NR_multiplexer
#define SYS_multiplexer __NR_multiplexer
#endif

#ifdef __NR_munlock
#define SYS_munlock __NR_munlock
#endif

#ifdef __NR_munlockall
#define SYS_munlockall __NR_munlockall
#endif

#ifdef __NR_munmap
#define SYS_munmap __NR_munmap
#endif

#ifdef __NR_name_to_handle_at
#define SYS_name_to_handle_at __NR_name_to_handle_at
#endif

#ifdef __NR_nanosleep
#define SYS_nanosleep __NR_nanosleep
#endif

#ifdef __NR_newfstatat
#define SYS_newfstatat __NR_newfstatat
#endif

#ifdef __NR_nfsservctl
#define SYS_nfsservctl __NR_nfsservctl
#endif

#ifdef __NR_ni_syscall
#define SYS_ni_syscall __NR_ni_syscall
#endif

#ifdef __NR_nice
#define SYS_nice __NR_nice
#endif

#ifdef __NR_old_adjtimex
#define SYS_old_adjtimex __NR_old_adjtimex
#endif

#ifdef __NR_oldfstat
#define SYS_oldfstat __NR_oldfstat
#endif

#ifdef __NR_oldlstat
#define SYS_oldlstat __NR_oldlstat
#endif

#ifdef __NR_oldolduname
#define SYS_oldolduname __NR_oldolduname
#endif

#ifdef __NR_oldstat
#define SYS_oldstat __NR_oldstat
#endif

#ifdef __NR_oldumount
#define SYS_oldumount __NR_oldumount
#endif

#ifdef __NR_olduname
#define SYS_olduname __NR_olduname
#endif

#ifdef __NR_open
#define SYS_open __NR_open
#endif

#ifdef __NR_open_by_handle_at
#define SYS_open_by_handle_at __NR_open_by_handle_at
#endif

#ifdef __NR_openat
#define SYS_openat __NR_openat
#endif

#ifdef __NR_osf_adjtime
#define SYS_osf_adjtime __NR_osf_adjtime
#endif

#ifdef __NR_osf_afs_syscall
#define SYS_osf_afs_syscall __NR_osf_afs_syscall
#endif

#ifdef __NR_osf_alt_plock
#define SYS_osf_alt_plock __NR_osf_alt_plock
#endif

#ifdef __NR_osf_alt_setsid
#define SYS_osf_alt_setsid __NR_osf_alt_setsid
#endif

#ifdef __NR_osf_alt_sigpending
#define SYS_osf_alt_sigpending __NR_osf_alt_sigpending
#endif

#ifdef __NR_osf_asynch_daemon
#define SYS_osf_asynch_daemon __NR_osf_asynch_daemon
#endif

#ifdef __NR_osf_audcntl
#define SYS_osf_audcntl __NR_osf_audcntl
#endif

#ifdef __NR_osf_audgen
#define SYS_osf_audgen __NR_osf_audgen
#endif

#ifdef __NR_osf_chflags
#define SYS_osf_chflags __NR_osf_chflags
#endif

#ifdef __NR_osf_execve
#define SYS_osf_execve __NR_osf_execve
#endif

#ifdef __NR_osf_exportfs
#define SYS_osf_exportfs __NR_osf_exportfs
#endif

#ifdef __NR_osf_fchflags
#define SYS_osf_fchflags __NR_osf_fchflags
#endif

#ifdef __NR_osf_fdatasync
#define SYS_osf_fdatasync __NR_osf_fdatasync
#endif

#ifdef __NR_osf_fpathconf
#define SYS_osf_fpathconf __NR_osf_fpathconf
#endif

#ifdef __NR_osf_fstat
#define SYS_osf_fstat __NR_osf_fstat
#endif

#ifdef __NR_osf_fstatfs
#define SYS_osf_fstatfs __NR_osf_fstatfs
#endif

#ifdef __NR_osf_fstatfs64
#define SYS_osf_fstatfs64 __NR_osf_fstatfs64
#endif

#ifdef __NR_osf_fuser
#define SYS_osf_fuser __NR_osf_fuser
#endif

#ifdef __NR_osf_getaddressconf
#define SYS_osf_getaddressconf __NR_osf_getaddressconf
#endif

#ifdef __NR_osf_getdirentries
#define SYS_osf_getdirentries __NR_osf_getdirentries
#endif

#ifdef __NR_osf_getdomainname
#define SYS_osf_getdomainname __NR_osf_getdomainname
#endif

#ifdef __NR_osf_getfh
#define SYS_osf_getfh __NR_osf_getfh
#endif

#ifdef __NR_osf_getfsstat
#define SYS_osf_getfsstat __NR_osf_getfsstat
#endif

#ifdef __NR_osf_gethostid
#define SYS_osf_gethostid __NR_osf_gethostid
#endif

#ifdef __NR_osf_getitimer
#define SYS_osf_getitimer __NR_osf_getitimer
#endif

#ifdef __NR_osf_getlogin
#define SYS_osf_getlogin __NR_osf_getlogin
#endif

#ifdef __NR_osf_getmnt
#define SYS_osf_getmnt __NR_osf_getmnt
#endif

#ifdef __NR_osf_getrusage
#define SYS_osf_getrusage __NR_osf_getrusage
#endif

#ifdef __NR_osf_getsysinfo
#define SYS_osf_getsysinfo __NR_osf_getsysinfo
#endif

#ifdef __NR_osf_gettimeofday
#define SYS_osf_gettimeofday __NR_osf_gettimeofday
#endif

#ifdef __NR_osf_kloadcall
#define SYS_osf_kloadcall __NR_osf_kloadcall
#endif

#ifdef __NR_osf_kmodcall
#define SYS_osf_kmodcall __NR_osf_kmodcall
#endif

#ifdef __NR_osf_lstat
#define SYS_osf_lstat __NR_osf_lstat
#endif

#ifdef __NR_osf_memcntl
#define SYS_osf_memcntl __NR_osf_memcntl
#endif

#ifdef __NR_osf_mincore
#define SYS_osf_mincore __NR_osf_mincore
#endif

#ifdef __NR_osf_mount
#define SYS_osf_mount __NR_osf_mount
#endif

#ifdef __NR_osf_mremap
#define SYS_osf_mremap __NR_osf_mremap
#endif

#ifdef __NR_osf_msfs_syscall
#define SYS_osf_msfs_syscall __NR_osf_msfs_syscall
#endif

#ifdef __NR_osf_msleep
#define SYS_osf_msleep __NR_osf_msleep
#endif

#ifdef __NR_osf_mvalid
#define SYS_osf_mvalid __NR_osf_mvalid
#endif

#ifdef __NR_osf_mwakeup
#define SYS_osf_mwakeup __NR_osf_mwakeup
#endif

#ifdef __NR_osf_naccept
#define SYS_osf_naccept __NR_osf_naccept
#endif

#ifdef __NR_osf_nfssvc
#define SYS_osf_nfssvc __NR_osf_nfssvc
#endif

#ifdef __NR_osf_ngetpeername
#define SYS_osf_ngetpeername __NR_osf_ngetpeername
#endif

#ifdef __NR_osf_ngetsockname
#define SYS_osf_ngetsockname __NR_osf_ngetsockname
#endif

#ifdef __NR_osf_nrecvfrom
#define SYS_osf_nrecvfrom __NR_osf_nrecvfrom
#endif

#ifdef __NR_osf_nrecvmsg
#define SYS_osf_nrecvmsg __NR_osf_nrecvmsg
#endif

#ifdef __NR_osf_nsendmsg
#define SYS_osf_nsendmsg __NR_osf_nsendmsg
#endif

#ifdef __NR_osf_ntp_adjtime
#define SYS_osf_ntp_adjtime __NR_osf_ntp_adjtime
#endif

#ifdef __NR_osf_ntp_gettime
#define SYS_osf_ntp_gettime __NR_osf_ntp_gettime
#endif

#ifdef __NR_osf_old_creat
#define SYS_osf_old_creat __NR_osf_old_creat
#endif

#ifdef __NR_osf_old_fstat
#define SYS_osf_old_fstat __NR_osf_old_fstat
#endif

#ifdef __NR_osf_old_getpgrp
#define SYS_osf_old_getpgrp __NR_osf_old_getpgrp
#endif

#ifdef __NR_osf_old_killpg
#define SYS_osf_old_killpg __NR_osf_old_killpg
#endif

#ifdef __NR_osf_old_lstat
#define SYS_osf_old_lstat __NR_osf_old_lstat
#endif

#ifdef __NR_osf_old_open
#define SYS_osf_old_open __NR_osf_old_open
#endif

#ifdef __NR_osf_old_sigaction
#define SYS_osf_old_sigaction __NR_osf_old_sigaction
#endif

#ifdef __NR_osf_old_sigblock
#define SYS_osf_old_sigblock __NR_osf_old_sigblock
#endif

#ifdef __NR_osf_old_sigreturn
#define SYS_osf_old_sigreturn __NR_osf_old_sigreturn
#endif

#ifdef __NR_osf_old_sigsetmask
#define SYS_osf_old_sigsetmask __NR_osf_old_sigsetmask
#endif

#ifdef __NR_osf_old_sigvec
#define SYS_osf_old_sigvec __NR_osf_old_sigvec
#endif

#ifdef __NR_osf_old_stat
#define SYS_osf_old_stat __NR_osf_old_stat
#endif

#ifdef __NR_osf_old_vadvise
#define SYS_osf_old_vadvise __NR_osf_old_vadvise
#endif

#ifdef __NR_osf_old_vtrace
#define SYS_osf_old_vtrace __NR_osf_old_vtrace
#endif

#ifdef __NR_osf_old_wait
#define SYS_osf_old_wait __NR_osf_old_wait
#endif

#ifdef __NR_osf_oldquota
#define SYS_osf_oldquota __NR_osf_oldquota
#endif

#ifdef __NR_osf_pathconf
#define SYS_osf_pathconf __NR_osf_pathconf
#endif

#ifdef __NR_osf_pid_block
#define SYS_osf_pid_block __NR_osf_pid_block
#endif

#ifdef __NR_osf_pid_unblock
#define SYS_osf_pid_unblock __NR_osf_pid_unblock
#endif

#ifdef __NR_osf_plock
#define SYS_osf_plock __NR_osf_plock
#endif

#ifdef __NR_osf_priocntlset
#define SYS_osf_priocntlset __NR_osf_priocntlset
#endif

#ifdef __NR_osf_profil
#define SYS_osf_profil __NR_osf_profil
#endif

#ifdef __NR_osf_proplist_syscall
#define SYS_osf_proplist_syscall __NR_osf_proplist_syscall
#endif

#ifdef __NR_osf_reboot
#define SYS_osf_reboot __NR_osf_reboot
#endif

#ifdef __NR_osf_revoke
#define SYS_osf_revoke __NR_osf_revoke
#endif

#ifdef __NR_osf_sbrk
#define SYS_osf_sbrk __NR_osf_sbrk
#endif

#ifdef __NR_osf_security
#define SYS_osf_security __NR_osf_security
#endif

#ifdef __NR_osf_select
#define SYS_osf_select __NR_osf_select
#endif

#ifdef __NR_osf_set_program_attributes
#define SYS_osf_set_program_attributes __NR_osf_set_program_attributes
#endif

#ifdef __NR_osf_set_speculative
#define SYS_osf_set_speculative __NR_osf_set_speculative
#endif

#ifdef __NR_osf_sethostid
#define SYS_osf_sethostid __NR_osf_sethostid
#endif

#ifdef __NR_osf_setitimer
#define SYS_osf_setitimer __NR_osf_setitimer
#endif

#ifdef __NR_osf_setlogin
#define SYS_osf_setlogin __NR_osf_setlogin
#endif

#ifdef __NR_osf_setsysinfo
#define SYS_osf_setsysinfo __NR_osf_setsysinfo
#endif

#ifdef __NR_osf_settimeofday
#define SYS_osf_settimeofday __NR_osf_settimeofday
#endif

#ifdef __NR_osf_shmat
#define SYS_osf_shmat __NR_osf_shmat
#endif

#ifdef __NR_osf_signal
#define SYS_osf_signal __NR_osf_signal
#endif

#ifdef __NR_osf_sigprocmask
#define SYS_osf_sigprocmask __NR_osf_sigprocmask
#endif

#ifdef __NR_osf_sigsendset
#define SYS_osf_sigsendset __NR_osf_sigsendset
#endif

#ifdef __NR_osf_sigstack
#define SYS_osf_sigstack __NR_osf_sigstack
#endif

#ifdef __NR_osf_sigwaitprim
#define SYS_osf_sigwaitprim __NR_osf_sigwaitprim
#endif

#ifdef __NR_osf_sstk
#define SYS_osf_sstk __NR_osf_sstk
#endif

#ifdef __NR_osf_stat
#define SYS_osf_stat __NR_osf_stat
#endif

#ifdef __NR_osf_statfs
#define SYS_osf_statfs __NR_osf_statfs
#endif

#ifdef __NR_osf_statfs64
#define SYS_osf_statfs64 __NR_osf_statfs64
#endif

#ifdef __NR_osf_subsys_info
#define SYS_osf_subsys_info __NR_osf_subsys_info
#endif

#ifdef __NR_osf_swapctl
#define SYS_osf_swapctl __NR_osf_swapctl
#endif

#ifdef __NR_osf_swapon
#define SYS_osf_swapon __NR_osf_swapon
#endif

#ifdef __NR_osf_syscall
#define SYS_osf_syscall __NR_osf_syscall
#endif

#ifdef __NR_osf_sysinfo
#define SYS_osf_sysinfo __NR_osf_sysinfo
#endif

#ifdef __NR_osf_table
#define SYS_osf_table __NR_osf_table
#endif

#ifdef __NR_osf_uadmin
#define SYS_osf_uadmin __NR_osf_uadmin
#endif

#ifdef __NR_osf_usleep_thread
#define SYS_osf_usleep_thread __NR_osf_usleep_thread
#endif

#ifdef __NR_osf_uswitch
#define SYS_osf_uswitch __NR_osf_uswitch
#endif

#ifdef __NR_osf_utc_adjtime
#define SYS_osf_utc_adjtime __NR_osf_utc_adjtime
#endif

#ifdef __NR_osf_utc_gettime
#define SYS_osf_utc_gettime __NR_osf_utc_gettime
#endif

#ifdef __NR_osf_utimes
#define SYS_osf_utimes __NR_osf_utimes
#endif

#ifdef __NR_osf_utsname
#define SYS_osf_utsname __NR_osf_utsname
#endif

#ifdef __NR_osf_wait4
#define SYS_osf_wait4 __NR_osf_wait4
#endif

#ifdef __NR_osf_waitid
#define SYS_osf_waitid __NR_osf_waitid
#endif

#ifdef __NR_pause
#define SYS_pause __NR_pause
#endif

#ifdef __NR_pciconfig_iobase
#define SYS_pciconfig_iobase __NR_pciconfig_iobase
#endif

#ifdef __NR_pciconfig_read
#define SYS_pciconfig_read __NR_pciconfig_read
#endif

#ifdef __NR_pciconfig_write
#define SYS_pciconfig_write __NR_pciconfig_write
#endif

#ifdef __NR_perf_event_open
#define SYS_perf_event_open __NR_perf_event_open
#endif

#ifdef __NR_perfctr
#define SYS_perfctr __NR_perfctr
#endif

#ifdef __NR_perfmonctl
#define SYS_perfmonctl __NR_perfmonctl
#endif

#ifdef __NR_personality
#define SYS_personality __NR_personality
#endif

#ifdef __NR_pipe
#define SYS_pipe __NR_pipe
#endif

#ifdef __NR_pipe2
#define SYS_pipe2 __NR_pipe2
#endif

#ifdef __NR_pivot_root
#define SYS_pivot_root __NR_pivot_root
#endif

#ifdef __NR_pkey_alloc
#define SYS_pkey_alloc __NR_pkey_alloc
#endif

#ifdef __NR_pkey_free
#define SYS_pkey_free __NR_pkey_free
#endif

#ifdef __NR_pkey_mprotect
#define SYS_pkey_mprotect __NR_pkey_mprotect
#endif

#ifdef __NR_poll
#define SYS_poll __NR_poll
#endif

#ifdef __NR_ppoll
#define SYS_ppoll __NR_ppoll
#endif

#ifdef __NR_ppoll_time64
#define SYS_ppoll_time64 __NR_ppoll_time64
#endif

#ifdef __NR_prctl
#define SYS_prctl __NR_prctl
#endif

#ifdef __NR_pread64
#define SYS_pread64 __NR_pread64
#endif

#ifdef __NR_preadv
#define SYS_preadv __NR_preadv
#endif

#ifdef __NR_preadv2
#define SYS_preadv2 __NR_preadv2
#endif

#ifdef __NR_prlimit64
#define SYS_prlimit64 __NR_prlimit64
#endif

#ifdef __NR_process_vm_readv
#define SYS_process_vm_readv __NR_process_vm_readv
#endif

#ifdef __NR_process_vm_writev
#define SYS_process_vm_writev __NR_process_vm_writev
#endif

#ifdef __NR_prof
#define SYS_prof __NR_prof
#endif

#ifdef __NR_profil
#define SYS_profil __NR_profil
#endif

#ifdef __NR_pselect6
#define SYS_pselect6 __NR_pselect6
#endif

#ifdef __NR_pselect6_time64
#define SYS_pselect6_time64 __NR_pselect6_time64
#endif

#ifdef __NR_ptrace
#define SYS_ptrace __NR_ptrace
#endif

#ifdef __NR_putpmsg
#define SYS_putpmsg __NR_putpmsg
#endif

#ifdef __NR_pwrite64
#define SYS_pwrite64 __NR_pwrite64
#endif

#ifdef __NR_pwritev
#define SYS_pwritev __NR_pwritev
#endif

#ifdef __NR_pwritev2
#define SYS_pwritev2 __NR_pwritev2
#endif

#ifdef __NR_query_module
#define SYS_query_module __NR_query_module
#endif

#ifdef __NR_quotactl
#define SYS_quotactl __NR_quotactl
#endif

#ifdef __NR_read
#define SYS_read __NR_read
#endif

#ifdef __NR_readahead
#define SYS_readahead __NR_readahead
#endif

#ifdef __NR_readdir
#define SYS_readdir __NR_readdir
#endif

#ifdef __NR_readlink
#define SYS_readlink __NR_readlink
#endif

#ifdef __NR_readlinkat
#define SYS_readlinkat __NR_readlinkat
#endif

#ifdef __NR_readv
#define SYS_readv __NR_readv
#endif

#ifdef __NR_reboot
#define SYS_reboot __NR_reboot
#endif

#ifdef __NR_recv
#define SYS_recv __NR_recv
#endif

#ifdef __NR_recvfrom
#define SYS_recvfrom __NR_recvfrom
#endif

#ifdef __NR_recvmmsg
#define SYS_recvmmsg __NR_recvmmsg
#endif

#ifdef __NR_recvmsg
#define SYS_recvmsg __NR_recvmsg
#endif

#ifdef __NR_remap_file_pages
#define SYS_remap_file_pages __NR_remap_file_pages
#endif

#ifdef __NR_removexattr
#define SYS_removexattr __NR_removexattr
#endif

#ifdef __NR_rename
#define SYS_rename __NR_rename
#endif

#ifdef __NR_renameat
#define SYS_renameat __NR_renameat
#endif

#ifdef __NR_renameat2
#define SYS_renameat2 __NR_renameat2
#endif

#ifdef __NR_request_key
#define SYS_request_key __NR_request_key
#endif

#ifdef __NR_restart_syscall
#define SYS_restart_syscall __NR_restart_syscall
#endif

#ifdef __NR_rmdir
#define SYS_rmdir __NR_rmdir
#endif

#ifdef __NR_rseq
#define SYS_rseq __NR_rseq
#endif

#ifdef __NR_rt_sigaction
#define SYS_rt_sigaction __NR_rt_sigaction
#endif

#ifdef __NR_rt_sigpending
#define SYS_rt_sigpending __NR_rt_sigpending
#endif

#ifdef __NR_rt_sigprocmask
#define SYS_rt_sigprocmask __NR_rt_sigprocmask
#endif

#ifdef __NR_rt_sigqueueinfo
#define SYS_rt_sigqueueinfo __NR_rt_sigqueueinfo
#endif

#ifdef __NR_rt_sigreturn
#define SYS_rt_sigreturn __NR_rt_sigreturn
#endif

#ifdef __NR_rt_sigsuspend
#define SYS_rt_sigsuspend __NR_rt_sigsuspend
#endif

#ifdef __NR_rt_sigtimedwait
#define SYS_rt_sigtimedwait __NR_rt_sigtimedwait
#endif

#ifdef __NR_rt_tgsigqueueinfo
#define SYS_rt_tgsigqueueinfo __NR_rt_tgsigqueueinfo
#endif

#ifdef __NR_rtas
#define SYS_rtas __NR_rtas
#endif

#ifdef __NR_s390_guarded_storage
#define SYS_s390_guarded_storage __NR_s390_guarded_storage
#endif

#ifdef __NR_s390_pci_mmio_read
#define SYS_s390_pci_mmio_read __NR_s390_pci_mmio_read
#endif

#ifdef __NR_s390_pci_mmio_write
#define SYS_s390_pci_mmio_write __NR_s390_pci_mmio_write
#endif

#ifdef __NR_s390_runtime_instr
#define SYS_s390_runtime_instr __NR_s390_runtime_instr
#endif

#ifdef __NR_s390_sthyi
#define SYS_s390_sthyi __NR_s390_sthyi
#endif

#ifdef __NR_sched_get_affinity
#define SYS_sched_get_affinity __NR_sched_get_affinity
#endif

#ifdef __NR_sched_get_priority_max
#define SYS_sched_get_priority_max __NR_sched_get_priority_max
#endif

#ifdef __NR_sched_get_priority_min
#define SYS_sched_get_priority_min __NR_sched_get_priority_min
#endif

#ifdef __NR_sched_getaffinity
#define SYS_sched_getaffinity __NR_sched_getaffinity
#endif

#ifdef __NR_sched_getattr
#define SYS_sched_getattr __NR_sched_getattr
#endif

#ifdef __NR_sched_getparam
#define SYS_sched_getparam __NR_sched_getparam
#endif

#ifdef __NR_sched_getscheduler
#define SYS_sched_getscheduler __NR_sched_getscheduler
#endif

#ifdef __NR_sched_rr_get_interval
#define SYS_sched_rr_get_interval __NR_sched_rr_get_interval
#endif

#ifdef __NR_sched_rr_get_interval_time64
#define SYS_sched_rr_get_interval_time64 __NR_sched_rr_get_interval_time64
#endif

#ifdef __NR_sched_set_affinity
#define SYS_sched_set_affinity __NR_sched_set_affinity
#endif

#ifdef __NR_sched_setaffinity
#define SYS_sched_setaffinity __NR_sched_setaffinity
#endif

#ifdef __NR_sched_setattr
#define SYS_sched_setattr __NR_sched_setattr
#endif

#ifdef __NR_sched_setparam
#define SYS_sched_setparam __NR_sched_setparam
#endif

#ifdef __NR_sched_setscheduler
#define SYS_sched_setscheduler __NR_sched_setscheduler
#endif

#ifdef __NR_sched_yield
#define SYS_sched_yield __NR_sched_yield
#endif

#ifdef __NR_seccomp
#define SYS_seccomp __NR_seccomp
#endif

#ifdef __NR_security
#define SYS_security __NR_security
#endif

#ifdef __NR_select
#define SYS_select __NR_select
#endif

#ifdef __NR_semctl
#define SYS_semctl __NR_semctl
#endif

#ifdef __NR_semget
#define SYS_semget __NR_semget
#endif

#ifdef __NR_semop
#define SYS_semop __NR_semop
#endif

#ifdef __NR_semtimedop
#define SYS_semtimedop __NR_semtimedop
#endif

#ifdef __NR_send
#define SYS_send __NR_send
#endif

#ifdef __NR_sendfile
#define SYS_sendfile __NR_sendfile
#endif

#ifdef __NR_sendfile64
#define SYS_sendfile64 __NR_sendfile64
#endif

#ifdef __NR_sendmmsg
#define SYS_sendmmsg __NR_sendmmsg
#endif

#ifdef __NR_sendmsg
#define SYS_sendmsg __NR_sendmsg
#endif

#ifdef __NR_sendto
#define SYS_sendto __NR_sendto
#endif

#ifdef __NR_set_mempolicy
#define SYS_set_mempolicy __NR_set_mempolicy
#endif

#ifdef __NR_set_robust_list
#define SYS_set_robust_list __NR_set_robust_list
#endif

#ifdef __NR_set_thread_area
#define SYS_set_thread_area __NR_set_thread_area
#endif

#ifdef __NR_set_tid_address
#define SYS_set_tid_address __NR_set_tid_address
#endif

#ifdef __NR_setdomainname
#define SYS_setdomainname __NR_setdomainname
#endif

#ifdef __NR_setfsgid
#define SYS_setfsgid __NR_setfsgid
#endif

#ifdef __NR_setfsgid32
#define SYS_setfsgid32 __NR_setfsgid32
#endif

#ifdef __NR_setfsuid
#define SYS_setfsuid __NR_setfsuid
#endif

#ifdef __NR_setfsuid32
#define SYS_setfsuid32 __NR_setfsuid32
#endif

#ifdef __NR_setgid
#define SYS_setgid __NR_setgid
#endif

#ifdef __NR_setgid32
#define SYS_setgid32 __NR_setgid32
#endif

#ifdef __NR_setgroups
#define SYS_setgroups __NR_setgroups
#endif

#ifdef __NR_setgroups32
#define SYS_setgroups32 __NR_setgroups32
#endif

#ifdef __NR_sethae
#define SYS_sethae __NR_sethae
#endif

#ifdef __NR_sethostname
#define SYS_sethostname __NR_sethostname
#endif

#ifdef __NR_setitimer
#define SYS_setitimer __NR_setitimer
#endif

#ifdef __NR_setns
#define SYS_setns __NR_setns
#endif

#ifdef __NR_setpgid
#define SYS_setpgid __NR_setpgid
#endif

#ifdef __NR_setpgrp
#define SYS_setpgrp __NR_setpgrp
#endif

#ifdef __NR_setpriority
#define SYS_setpriority __NR_setpriority
#endif

#ifdef __NR_setregid
#define SYS_setregid __NR_setregid
#endif

#ifdef __NR_setregid32
#define SYS_setregid32 __NR_setregid32
#endif

#ifdef __NR_setresgid
#define SYS_setresgid __NR_setresgid
#endif

#ifdef __NR_setresgid32
#define SYS_setresgid32 __NR_setresgid32
#endif

#ifdef __NR_setresuid
#define SYS_setresuid __NR_setresuid
#endif

#ifdef __NR_setresuid32
#define SYS_setresuid32 __NR_setresuid32
#endif

#ifdef __NR_setreuid
#define SYS_setreuid __NR_setreuid
#endif

#ifdef __NR_setreuid32
#define SYS_setreuid32 __NR_setreuid32
#endif

#ifdef __NR_setrlimit
#define SYS_setrlimit __NR_setrlimit
#endif

#ifdef __NR_setsid
#define SYS_setsid __NR_setsid
#endif

#ifdef __NR_setsockopt
#define SYS_setsockopt __NR_setsockopt
#endif

#ifdef __NR_settimeofday
#define SYS_settimeofday __NR_settimeofday
#endif

#ifdef __NR_setuid
#define SYS_setuid __NR_setuid
#endif

#ifdef __NR_setuid32
#define SYS_setuid32 __NR_setuid32
#endif

#ifdef __NR_setxattr
#define SYS_setxattr __NR_setxattr
#endif

#ifdef __NR_sgetmask
#define SYS_sgetmask __NR_sgetmask
#endif

#ifdef __NR_shmat
#define SYS_shmat __NR_shmat
#endif

#ifdef __NR_shmctl
#define SYS_shmctl __NR_shmctl
#endif

#ifdef __NR_shmdt
#define SYS_shmdt __NR_shmdt
#endif

#ifdef __NR_shmget
#define SYS_shmget __NR_shmget
#endif

#ifdef __NR_shutdown
#define SYS_shutdown __NR_shutdown
#endif

#ifdef __NR_sigaction
#define SYS_sigaction __NR_sigaction
#endif

#ifdef __NR_sigaltstack
#define SYS_sigaltstack __NR_sigaltstack
#endif

#ifdef __NR_signal
#define SYS_signal __NR_signal
#endif

#ifdef __NR_signalfd
#define SYS_signalfd __NR_signalfd
#endif

#ifdef __NR_signalfd4
#define SYS_signalfd4 __NR_signalfd4
#endif

#ifdef __NR_sigpending
#define SYS_sigpending __NR_sigpending
#endif

#ifdef __NR_sigprocmask
#define SYS_sigprocmask __NR_sigprocmask
#endif

#ifdef __NR_sigreturn
#define SYS_sigreturn __NR_sigreturn
#endif

#ifdef __NR_sigsuspend
#define SYS_sigsuspend __NR_sigsuspend
#endif

#ifdef __NR_socket
#define SYS_socket __NR_socket
#endif

#ifdef __NR_socketcall
#define SYS_socketcall __NR_socketcall
#endif

#ifdef __NR_socketpair
#define SYS_socketpair __NR_socketpair
#endif

#ifdef __NR_splice
#define SYS_splice __NR_splice
#endif

#ifdef __NR_spu_create
#define SYS_spu_create __NR_spu_create
#endif

#ifdef __NR_spu_run
#define SYS_spu_run __NR_spu_run
#endif

#ifdef __NR_ssetmask
#define SYS_ssetmask __NR_ssetmask
#endif

#ifdef __NR_stat
#define SYS_stat __NR_stat
#endif

#ifdef __NR_stat64
#define SYS_stat64 __NR_stat64
#endif

#ifdef __NR_statfs
#define SYS_statfs __NR_statfs
#endif

#ifdef __NR_statfs64
#define SYS_statfs64 __NR_statfs64
#endif

#ifdef __NR_statx
#define SYS_statx __NR_statx
#endif

#ifdef __NR_stime
#define SYS_stime __NR_stime
#endif

#ifdef __NR_stty
#define SYS_stty __NR_stty
#endif

#ifdef __NR_subpage_prot
#define SYS_subpage_prot __NR_subpage_prot
#endif

#ifdef __NR_swapcontext
#define SYS_swapcontext __NR_swapcontext
#endif

#ifdef __NR_swapoff
#define SYS_swapoff __NR_swapoff
#endif

#ifdef __NR_swapon
#define SYS_swapon __NR_swapon
#endif

#ifdef __NR_switch_endian
#define SYS_switch_endian __NR_switch_endian
#endif

#ifdef __NR_symlink
#define SYS_symlink __NR_symlink
#endif

#ifdef __NR_symlinkat
#define SYS_symlinkat __NR_symlinkat
#endif

#ifdef __NR_sync
#define SYS_sync __NR_sync
#endif

#ifdef __NR_sync_file_range
#define SYS_sync_file_range __NR_sync_file_range
#endif

#ifdef __NR_sync_file_range2
#define SYS_sync_file_range2 __NR_sync_file_range2
#endif

#ifdef __NR_syncfs
#define SYS_syncfs __NR_syncfs
#endif

#ifdef __NR_sys_debug_setcontext
#define SYS_sys_debug_setcontext __NR_sys_debug_setcontext
#endif

#ifdef __NR_sys_epoll_create
#define SYS_sys_epoll_create __NR_sys_epoll_create
#endif

#ifdef __NR_sys_epoll_ctl
#define SYS_sys_epoll_ctl __NR_sys_epoll_ctl
#endif

#ifdef __NR_sys_epoll_wait
#define SYS_sys_epoll_wait __NR_sys_epoll_wait
#endif

#ifdef __NR_syscall
#define SYS_syscall __NR_syscall
#endif

#ifdef __NR_sysfs
#define SYS_sysfs __NR_sysfs
#endif

#ifdef __NR_sysinfo
#define SYS_sysinfo __NR_sysinfo
#endif

#ifdef __NR_syslog
#define SYS_syslog __NR_syslog
#endif

#ifdef __NR_sysmips
#define SYS_sysmips __NR_sysmips
#endif

#ifdef __NR_tee
#define SYS_tee __NR_tee
#endif

#ifdef __NR_tgkill
#define SYS_tgkill __NR_tgkill
#endif

#ifdef __NR_time
#define SYS_time __NR_time
#endif

#ifdef __NR_timer_create
#define SYS_timer_create __NR_timer_create
#endif

#ifdef __NR_timer_delete
#define SYS_timer_delete __NR_timer_delete
#endif

#ifdef __NR_timer_getoverrun
#define SYS_timer_getoverrun __NR_timer_getoverrun
#endif

#ifdef __NR_timer_gettime
#define SYS_timer_gettime __NR_timer_gettime
#endif

#ifdef __NR_timer_settime
#define SYS_timer_settime __NR_timer_settime
#endif

#ifdef __NR_timerfd
#define SYS_timerfd __NR_timerfd
#endif

#ifdef __NR_timerfd_create
#define SYS_timerfd_create __NR_timerfd_create
#endif

#ifdef __NR_timerfd_gettime
#define SYS_timerfd_gettime __NR_timerfd_gettime
#endif

#ifdef __NR_timerfd_settime
#define SYS_timerfd_settime __NR_timerfd_settime
#endif

#ifdef __NR_times
#define SYS_times __NR_times
#endif

#ifdef __NR_tkill
#define SYS_tkill __NR_tkill
#endif

#ifdef __NR_truncate
#define SYS_truncate __NR_truncate
#endif

#ifdef __NR_truncate64
#define SYS_truncate64 __NR_truncate64
#endif

#ifdef __NR_tuxcall
#define SYS_tuxcall __NR_tuxcall
#endif

#ifdef __NR_ugetrlimit
#define SYS_ugetrlimit __NR_ugetrlimit
#endif

#ifdef __NR_ulimit
#define SYS_ulimit __NR_ulimit
#endif

#ifdef __NR_umask
#define SYS_umask __NR_umask
#endif

#ifdef __NR_umount
#define SYS_umount __NR_umount
#endif

#ifdef __NR_umount2
#define SYS_umount2 __NR_umount2
#endif

#ifdef __NR_uname
#define SYS_uname __NR_uname
#endif

#ifdef __NR_unlink
#define SYS_unlink __NR_unlink
#endif

#ifdef __NR_unlinkat
#define SYS_unlinkat __NR_unlinkat
#endif

#ifdef __NR_unshare
#define SYS_unshare __NR_unshare
#endif

#ifdef __NR_uselib
#define SYS_uselib __NR_uselib
#endif

#ifdef __NR_userfaultfd
#define SYS_userfaultfd __NR_userfaultfd
#endif

#ifdef __NR_ustat
#define SYS_ustat __NR_ustat
#endif

#ifdef __NR_utime
#define SYS_utime __NR_utime
#endif

#ifdef __NR_utimensat
#define SYS_utimensat __NR_utimensat
#endif

#ifdef __NR_utimes
#define SYS_utimes __NR_utimes
#endif

#ifdef __NR_utimensat_time64
#define SYS_utimensat_time64 __NR_utimensat_time64
#endif

#ifdef __NR_utrap_install
#define SYS_utrap_install __NR_utrap_install
#endif

#ifdef __NR_vfork
#define SYS_vfork __NR_vfork
#endif

#ifdef __NR_vhangup
#define SYS_vhangup __NR_vhangup
#endif

#ifdef __NR_vm86
#define SYS_vm86 __NR_vm86
#endif

#ifdef __NR_vm86old
#define SYS_vm86old __NR_vm86old
#endif

#ifdef __NR_vmsplice
#define SYS_vmsplice __NR_vmsplice
#endif

#ifdef __NR_vserver
#define SYS_vserver __NR_vserver
#endif

#ifdef __NR_wait4
#define SYS_wait4 __NR_wait4
#endif

#ifdef __NR_waitid
#define SYS_waitid __NR_waitid
#endif

#ifdef __NR_waitpid
#define SYS_waitpid __NR_waitpid
#endif

#ifdef __NR_write
#define SYS_write __NR_write
#endif

#ifdef __NR_writev
#define SYS_writev __NR_writev
#endif

#endif // __linux__

#endif // LLVM_LIBC_SYS_SYSCALL_H
