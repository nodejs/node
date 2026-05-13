set(LIBC_INCLUDE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBC_INCLUDE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

include(LLVMLibCHeaderRules)

# The GPU build wants to install files in the compiler's resource directory.
if(LIBC_TARGET_OS_IS_GPU)
  include(GetClangResourceDir)
endif()

add_subdirectory(llvm-libc-macros)
add_subdirectory(llvm-libc-types)

add_header(
  llvm_libc_common_h
  HDR
    __llvm-libc-common.h
)

# TODO: Can we simplify this macro expansion?
# https://github.com/llvm/llvm-project/issues/117254
macro(add_header_macro TARGET_NAME YAML_FILE GEN_HDR DEPENDS)
  add_gen_header(
    ${TARGET_NAME}
    YAML_FILE ${YAML_FILE}
    GEN_HDR ${GEN_HDR}
    ${DEPENDS}
    ${ARGN}
  )
endmacro()

add_header_macro(
  ctype
  ../libc/include/ctype.yaml
  ctype.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.locale_t
)

add_header_macro(
  dirent
  ../libc/include/dirent.yaml
  dirent.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.ino_t
    .llvm-libc-types.DIR
    .llvm-libc-types.struct_dirent
)

add_header_macro(
  fcntl
  ../libc/include/fcntl.yaml
  fcntl.h
  DEPENDS
    .llvm-libc-macros.fcntl_macros
    .llvm-libc-types.mode_t
    .llvm-libc-types.struct_flock
    .llvm-libc-types.struct_flock64
    .llvm-libc-types.off64_t
    .llvm-libc-types.pid_t
    .llvm-libc-types.off_t
    .llvm_libc_common_h
)

add_header_macro(
  dlfcn
  ../libc/include/dlfcn.yaml
  dlfcn.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.dl_info
)

add_header_macro(
  endian
  ../libc/include/endian.yaml
  endian.h
  DEPENDS
    .llvm-libc-macros.endian_macros
    .llvm_libc_common_h
)

add_header_macro(
  features
  ../libc/include/features.yaml
  features.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.features_macros
)

add_header_macro(
  fenv
  ../libc/include/fenv.yaml
  fenv.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.fenv_macros
    .llvm-libc-types.fenv_t
    .llvm-libc-types.fexcept_t
)

add_header_macro(
  stdint
  ../libc/include/stdint.yaml
  stdint.h
  DEPENDS
    .llvm-libc-macros.stdint_macros
)

add_header_macro(
  inttypes
  ../libc/include/inttypes.yaml
  inttypes.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.imaxdiv_t
    .llvm-libc-macros.inttypes_macros
    .stdint
)

add_header_macro(
  float
  ../libc/include/float.yaml
  float.h
  DEPENDS
    .llvm-libc-macros.float_macros
)

add_header_macro(
  limits
  ../libc/include/limits.yaml
  limits.h
  DEPENDS
    .llvm-libc-macros.limits_macros
)

add_header_macro(
  malloc
  ../libc/include/malloc.yaml
  malloc.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.malloc_macros
)

add_header_macro(
  math
  ../libc/include/math.yaml
  math.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.float16_macros
    .llvm-libc-macros.math_macros
    .llvm-libc-macros.math_function_macros
    .llvm-libc-types.double_t
    .llvm-libc-types.float_t
    .llvm-libc-types.float128
)

add_header_macro(
  stdfix
  ../libc/include/stdfix.yaml
  stdfix.h
  DEPENDS
    .llvm-libc-macros.stdfix_macros
    .llvm-libc-types.int_hk_t
    .llvm-libc-types.int_hr_t
    .llvm-libc-types.int_k_t
    .llvm-libc-types.int_lk_t
    .llvm-libc-types.int_lr_t
    .llvm-libc-types.int_r_t
    .llvm-libc-types.uint_uhk_t
    .llvm-libc-types.uint_uhr_t
    .llvm-libc-types.uint_uk_t
    .llvm-libc-types.uint_ulk_t
    .llvm-libc-types.uint_ulr_t
    .llvm-libc-types.uint_ur_t
)

add_header(
  syscall
  HDR
    syscall.h
)

add_header_macro(
  sysexits
  ../libc/include/sysexits.yaml
  sysexits.h
  DEPENDS
    .llvm-libc-macros.sysexits_macros
)


# TODO: This should be conditional on POSIX networking being included.
file(MAKE_DIRECTORY ${LIBC_INCLUDE_DIR}/arpa)

add_header_macro(
  arpa_inet
  ../libc/include/arpa/inet.yaml
  arpa/inet.h
  DEPENDS
    .llvm_libc_common_h
    .inttypes
    .llvm-libc-types.in_addr_t
    .llvm-libc-types.struct_in_addr
)

file(MAKE_DIRECTORY ${LIBC_INCLUDE_DIR}/netinet)

add_header_macro(
  netinet_in
  ../libc/include/netinet/in.yaml
  netinet/in.h
  DEPENDS
    .llvm-libc-macros.netinet_in_macros
    .llvm-libc-types.in_addr_t
    .llvm-libc-types.in_port_t
    .llvm-libc-types.sa_family_t
    .llvm_libc_common_h
)

add_header_macro(
  assert
  ../libc/include/assert.yaml
  assert.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.assert_macros
)

add_header_macro(
  byteswap
  ../libc/include/byteswap.yaml
  byteswap.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.byteswap_macros
)

add_header_macro(
  complex
  ../libc/include/complex.yaml
  complex.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.complex_macros
    .llvm-libc-types.float128
    .llvm-libc-types.cfloat128
    .llvm-libc-types.cfloat16
)

add_header_macro(
  cpio
  ../libc/include/cpio.yaml
  cpio.h
  DEPENDS
    .llvm_libc_common_h
)

add_header_macro(
  setjmp
  ../libc/include/setjmp.yaml
  setjmp.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.jmp_buf
    .llvm-libc-types.sigjmp_buf
)

add_header_macro(
  string
  ../libc/include/string.yaml
  string.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.annex_k_macros
    .llvm-libc-macros.null_macro
    .llvm-libc-types.errno_t
    .llvm-libc-types.rsize_t
    .llvm-libc-types.size_t
)

add_header_macro(
  strings
  ../libc/include/strings.yaml
  strings.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.size_t
)

add_header_macro(
  search
  ../libc/include/search.yaml
  search.h
  DEPENDS
    .llvm-libc-types.ACTION
    .llvm-libc-types.ENTRY
    .llvm-libc-types.VISIT
    .llvm-libc-types.posix_tnode
    .llvm-libc-types.__search_compare_t
    .llvm-libc-types.size_t
    .llvm-libc-types.struct_hsearch_data
    .llvm-libc-types.__action_closure_fn_t
    .llvm-libc-types.__action_fn_t
    .llvm-libc-types.__free_fn_t
    .llvm_libc_common_h
)

add_header_macro(
  time
  ../libc/include/time.yaml
  time.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.null_macro
    .llvm-libc-macros.time_macros
    .llvm-libc-types.clock_t
    .llvm-libc-types.errno_t
    .llvm-libc-types.time_t
    .llvm-libc-types.struct_tm
    .llvm-libc-types.struct_timespec
    .llvm-libc-types.struct_timeval
    .llvm-libc-types.clockid_t
    .llvm-libc-types.rsize_t
)

add_header_macro(
  threads
  ../libc/include/threads.yaml
  threads.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.__call_once_func_t
    .llvm-libc-types.once_flag
    .llvm-libc-types.cnd_t
    .llvm-libc-types.mtx_t
    .llvm-libc-types.thrd_t
    .llvm-libc-types.thrd_start_t
    .llvm-libc-types.tss_t
    .llvm-libc-types.tss_dtor_t
    .llvm-libc-types.struct_timespec
)

add_header_macro(
  errno
  ../libc/include/errno.yaml
  errno.h
  DEPENDS
    .llvm-libc-macros.generic_error_number_macros
    .llvm-libc-macros.error_number_macros
    .llvm-libc-types.errno_t
)

add_header_macro(
  signal
  ../libc/include/signal.yaml
  signal.h
  DEPENDS
    .llvm-libc-macros.signal_macros
    .llvm-libc-types.pid_t
    .llvm-libc-types.sig_atomic_t
    .llvm-libc-types.sighandler_t
    .llvm-libc-types.siginfo_t
    .llvm-libc-types.sigset_t
    .llvm-libc-types.stack_t
    .llvm-libc-types.struct_sigaction
    .llvm-libc-types.union_sigval
)

add_header_macro(
  stdbit
  ../libc/include/stdbit.yaml
  stdbit.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.stdbit_macros
)

add_header_macro(
  stdckdint
  ../libc/include/stdckdint.yaml
  stdckdint.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.stdckdint_macros
)

add_header_macro(
  stdio
  ../libc/include/stdio.yaml
  stdio.h
  DEPENDS
    .llvm-libc-macros._LIBC_MODULAR_FORMAT_PRINTF
    .llvm-libc-macros.file_seek_macros
    .llvm-libc-macros.null_macro
    .llvm-libc-macros.stdio_macros
    .llvm-libc-types.FILE
    .llvm-libc-types.cookie_io_functions_t
    .llvm-libc-types.errno_t
    .llvm-libc-types.off_t
    .llvm-libc-types.rsize_t
    .llvm-libc-types.size_t
    .llvm-libc-types.ssize_t
    .llvm_libc_common_h
)

add_header_macro(
  stdlib
  ../libc/include/stdlib.yaml
  stdlib.h
  DEPENDS
    .llvm-libc-macros.null_macro
    .llvm-libc-macros.stdlib_macros
    .llvm-libc-types.__atexithandler_t
    .llvm-libc-types.__qsortcompare_t
    .llvm-libc-types.__qsortrcompare_t
    .llvm-libc-types.__search_compare_t
    .llvm-libc-types.constraint_handler_t
    .llvm-libc-types.div_t
    .llvm-libc-types.errno_t
    .llvm-libc-types.ldiv_t
    .llvm-libc-types.lldiv_t
    .llvm-libc-types.locale_t
    .llvm-libc-types.rsize_t
    .llvm-libc-types.size_t
    .llvm_libc_common_h
)

add_header_macro(
  unistd
  ../libc/include/unistd.yaml
  unistd.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.file_seek_macros
    .llvm-libc-macros.unistd_macros
    .llvm-libc-types.__exec_argv_t
    .llvm-libc-types.__exec_envp_t
    .llvm-libc-types.off_t
    .llvm-libc-types.pid_t
    .llvm-libc-types.size_t
    .llvm-libc-types.ssize_t
    .llvm-libc-types.uid_t
    .llvm-libc-types.__getoptargv_t
)

add_header_macro(
  pthread
  ../libc/include/pthread.yaml
  pthread.h
  DEPENDS
    .llvm-libc-macros.pthread_macros
    .llvm-libc-types.__atfork_callback_t
    .llvm-libc-types.__pthread_once_func_t
    .llvm-libc-types.__pthread_start_t
    .llvm-libc-types.__pthread_tss_dtor_t
    .llvm-libc-types.clockid_t
    .llvm-libc-types.pthread_attr_t
    .llvm-libc-types.pthread_cond_t
    .llvm-libc-types.pthread_condattr_t
    .llvm-libc-types.pthread_key_t
    .llvm-libc-types.pthread_barrier_t
    .llvm-libc-types.pthread_barrierattr_t
    .llvm-libc-types.pthread_mutex_t
    .llvm-libc-types.pthread_mutexattr_t
    .llvm-libc-types.pthread_once_t
    .llvm-libc-types.pthread_rwlock_t
    .llvm-libc-types.pthread_rwlockattr_t
    .llvm-libc-types.pthread_spinlock_t
    .llvm-libc-types.pthread_t
    .llvm-libc-types.struct_timespec
    .llvm_libc_common_h
)

if(LIBC_TARGET_ARCHITECTURE STREQUAL "x86_64")
  add_header_macro(
    ucontext
    ../libc/include/ucontext.yaml
    ucontext.h
    DEPENDS
      .llvm_libc_common_h
      .llvm-libc-types.mcontext_t
      .llvm-libc-types.ucontext_t
      .llvm-libc-types.sigset_t
      .llvm-libc-types.stack_t
  )

  add_header_macro(
    sys_ucontext
    ../libc/include/sys/ucontext.yaml
    sys/ucontext.h
    DEPENDS
      .ucontext
  )
endif()

add_header_macro(
  sched
  ../libc/include/sched.yaml
  sched.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sched_macros
    .llvm-libc-types.cpu_set_t
    .llvm-libc-types.pid_t
    .llvm-libc-types.size_t
    .llvm-libc-types.struct_sched_param
    # Needed according to posix standard
    .llvm-libc-types.time_t
    .llvm-libc-types.struct_timespec
)

add_header_macro(
  spawn
  ../libc/include/spawn.yaml
  spawn.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.mode_t
    .llvm-libc-types.pid_t
    .llvm-libc-types.posix_spawnattr_t
    .llvm-libc-types.posix_spawn_file_actions_t
)

add_header_macro(
  link
  ../libc/include/link.yaml
  link.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.link_macros
    .llvm-libc-types.struct_dl_phdr_info
    .llvm-libc-types.__dl_iterate_phdr_callback_t
)

add_header_macro(
  elf
  ../libc/include/elf.yaml
  elf.h
  DEPENDS
    .llvm-libc-macros.elf_macros
    .llvm-libc-types.Elf32_Addr
    .llvm-libc-types.Elf32_Chdr
    .llvm-libc-types.Elf32_Dyn
    .llvm-libc-types.Elf32_Ehdr
    .llvm-libc-types.Elf32_Half
    .llvm-libc-types.Elf32_Lword
    .llvm-libc-types.Elf32_Nhdr
    .llvm-libc-types.Elf32_Off
    .llvm-libc-types.Elf32_Phdr
    .llvm-libc-types.Elf32_Rel
    .llvm-libc-types.Elf32_Rela
    .llvm-libc-types.Elf32_Shdr
    .llvm-libc-types.Elf32_Sword
    .llvm-libc-types.Elf32_Sym
    .llvm-libc-types.Elf32_Verdaux
    .llvm-libc-types.Elf32_Verdef
    .llvm-libc-types.Elf32_Vernaux
    .llvm-libc-types.Elf32_Verneed
    .llvm-libc-types.Elf32_Versym
    .llvm-libc-types.Elf32_Word
    .llvm-libc-types.Elf32_Xword
    .llvm-libc-types.Elf32_auxv_t
    .llvm-libc-types.Elf64_Addr
    .llvm-libc-types.Elf64_Chdr
    .llvm-libc-types.Elf64_Dyn
    .llvm-libc-types.Elf64_Ehdr
    .llvm-libc-types.Elf64_Half
    .llvm-libc-types.Elf64_Lword
    .llvm-libc-types.Elf64_Nhdr
    .llvm-libc-types.Elf64_Off
    .llvm-libc-types.Elf64_Phdr
    .llvm-libc-types.Elf64_Rel
    .llvm-libc-types.Elf64_Rela
    .llvm-libc-types.Elf64_Shdr
    .llvm-libc-types.Elf64_Sword
    .llvm-libc-types.Elf64_Sxword
    .llvm-libc-types.Elf64_Sym
    .llvm-libc-types.Elf64_Verdaux
    .llvm-libc-types.Elf64_Verdef
    .llvm-libc-types.Elf64_Vernaux
    .llvm-libc-types.Elf64_Verneed
    .llvm-libc-types.Elf64_Versym
    .llvm-libc-types.Elf64_Word
    .llvm-libc-types.Elf64_Xword
    .llvm-libc-types.Elf64_auxv_t
)

# TODO: Not all platforms will have a include/sys directory. Add the sys
# directory and the targets for sys/*.h files conditional to the OS requiring
# them.
file(MAKE_DIRECTORY ${LIBC_INCLUDE_DIR}/sys)

add_header_macro(
  sys_auxv
  ../libc/include/sys/auxv.yaml
  sys/auxv.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_auxv_macros
)

add_header_macro(
  sys_epoll
  ../libc/include/sys/epoll.yaml
  sys/epoll.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.struct_epoll_event
    .llvm-libc-types.struct_epoll_data
    .llvm-libc-types.sigset_t
    .llvm-libc-macros.sys_epoll_macros
)

add_header_macro(
  sys_ioctl
  ../libc/include/sys/ioctl.yaml
  sys/ioctl.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_ioctl_macros
)

add_header_macro(
  sys_ipc
  ../libc/include/sys/ipc.yaml
  sys/ipc.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_ipc_macros
    .llvm-libc-types.key_t
    .llvm-libc-types.uid_t
    .llvm-libc-types.gid_t
    .llvm-libc-types.mode_t
    .llvm-libc-types.struct_ipc_perm
)

add_header_macro(
  sys_mman
  ../libc/include/sys/mman.yaml
  sys/mman.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_mman_macros
    .llvm-libc-types.off_t
    .llvm-libc-types.size_t
    .llvm-libc-types.ssize_t
)

add_header_macro(
  sys_personality
  ../libc/include/sys/personality.yaml
  sys/personality.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_personality_macros
)

add_header_macro(
  sys_prctl
  ../libc/include/sys/prctl.yaml
  sys/prctl.h
  DEPENDS
    .llvm_libc_common_h
)

add_header(
  sys_queue
  HDR
    sys/queue.h
  DEPENDS
    .llvm-libc-macros.sys_queue_macros
)

add_header_macro(
  sys_random
  ../libc/include/sys/random.yaml
  sys/random.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_random_macros
    .llvm-libc-types.size_t
    .llvm-libc-types.ssize_t
)

add_header_macro(
  sys_resource
  ../libc/include/sys/resource.yaml
  sys/resource.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_resource_macros
    .llvm-libc-types.rlim_t
    .llvm-libc-types.struct_rlimit
)

add_header_macro(
  sys_stat
  ../libc/include/sys/stat.yaml
  sys/stat.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_stat_macros
    .llvm-libc-types.mode_t
    .llvm-libc-types.dev_t
    .llvm-libc-types.ino_t
    .llvm-libc-types.nlink_t
    .llvm-libc-types.uid_t
    .llvm-libc-types.gid_t
    .llvm-libc-types.off_t
    .llvm-libc-types.struct_timespec
    .llvm-libc-types.struct_timeval
    .llvm-libc-types.blksize_t
    .llvm-libc-types.blkcnt_t
    .llvm-libc-types.struct_stat
)

add_header_macro(
  sys_select
  ../libc/include/sys/select.yaml
  sys/select.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_select_macros
    .llvm-libc-types.fd_set
    .llvm-libc-types.sigset_t
    .llvm-libc-types.suseconds_t
    .llvm-libc-types.time_t
    .llvm-libc-types.struct_timespec
    .llvm-libc-types.struct_timeval
)

add_header_macro(
  sys_sem
  ../libc/include/sys/sem.yaml
  sys/sem.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_sem_macros
    .llvm-libc-types.key_t
    .llvm-libc-types.size_t
    .llvm-libc-types.struct_ipc_perm
    .llvm-libc-types.struct_semid_ds
    .llvm-libc-types.struct_sembuf
    .llvm-libc-types.struct_seminfo
)

add_header_macro(
  sys_sendfile
  ../libc/include/sys/sendfile.yaml
  sys/sendfile.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.off_t
    .llvm-libc-types.size_t
    .llvm-libc-types.ssize_t
)

add_header_macro(
  sys_socket
  ../libc/include/sys/socket.yaml
  sys/socket.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_socket_macros
    .llvm-libc-types.sa_family_t
    .llvm-libc-types.socklen_t
    .llvm-libc-types.struct_cmsghdr
    .llvm-libc-types.struct_iovec
    .llvm-libc-types.struct_linger
    .llvm-libc-types.struct_msghdr
    .llvm-libc-types.struct_sockaddr
    .llvm-libc-types.struct_sockaddr_storage
)

add_header_macro(
  sys_statvfs
  ../libc/include/sys/statvfs.yaml
  sys/statvfs.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.struct_statvfs
)

add_header_macro(
  sys_syscall
  ../libc/include/sys/syscall.yaml
  sys/syscall.h
  DEPENDS
)

add_header_macro(
  sys_time
  ../libc/include/sys/time.yaml
  sys/time.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_time_macros
    .llvm-libc-types.struct_itimerval
    .llvm-libc-types.struct_timeval
)

add_header_macro(
  sys_types
  ../libc/include/sys/types.yaml
  sys/types.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.blkcnt_t
    .llvm-libc-types.blksize_t
    .llvm-libc-types.clockid_t
    .llvm-libc-types.dev_t
    .llvm-libc-types.gid_t
    .llvm-libc-types.ino_t
    .llvm-libc-types.key_t
    .llvm-libc-types.mode_t
    .llvm-libc-types.nlink_t
    .llvm-libc-types.off_t
    .llvm-libc-types.pid_t
    .llvm-libc-types.pthread_attr_t
    .llvm-libc-types.pthread_cond_t
    .llvm-libc-types.pthread_key_t
    .llvm-libc-types.pthread_mutex_t
    .llvm-libc-types.pthread_mutexattr_t
    .llvm-libc-types.pthread_once_t
    .llvm-libc-types.pthread_t
    .llvm-libc-types.size_t
    .llvm-libc-types.ssize_t
    .llvm-libc-types.suseconds_t
    .llvm-libc-types.time_t
    .llvm-libc-types.uid_t
)

add_header_macro(
  sys_utsname
  ../libc/include/sys/utsname.yaml
  sys/utsname.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.struct_utsname
)

add_header_macro(
  sys_uio
  ../libc/include/sys/uio.yaml
  sys/uio.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.struct_iovec
    .llvm-libc-types.ssize_t
)

add_header_macro(
  sys_un
  ../libc/include/sys/un.yaml
  sys/un.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.struct_sockaddr_un
)

add_header_macro(
  sys_wait
  ../libc/include/sys/wait.yaml
  sys/wait.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.sys_wait_macros
    .llvm-libc-types.pid_t
    .llvm-libc-types.struct_rusage
    .llvm-libc-types.siginfo_t
)

add_header_macro(
  termios
  ../libc/include/termios.yaml
  termios.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.termios_macros
    .llvm-libc-types.cc_t
    .llvm-libc-types.pid_t
    .llvm-libc-types.speed_t
    .llvm-libc-types.struct_termios
    .llvm-libc-types.tcflag_t
)

add_header_macro(
  uchar
  ../libc/include/uchar.yaml
  uchar.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.mbstate_t
    .llvm-libc-types.char8_t
    .llvm-libc-types.char16_t
    .llvm-libc-types.char32_t
)

add_header_macro(
  wchar
  ../libc/include/wchar.yaml
  wchar.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.null_macro
    .llvm-libc-macros.wchar_macros
    .llvm-libc-types.mbstate_t
    .llvm-libc-types.rsize_t
    .llvm-libc-types.size_t
    .llvm-libc-types.wint_t
    .llvm-libc-types.wchar_t
)

add_header_macro(
  wctype
  ../libc/include/wctype.yaml
  wctype.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.wint_t
    .llvm-libc-types.wctype_t
)

add_header_macro(
  locale
  ../libc/include/locale.yaml
  locale.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-macros.locale_macros
    .llvm-libc-macros.null_macro
    .llvm-libc-types.locale_t
    .llvm-libc-types.struct_lconv
)

add_header_macro(
  poll
  ../libc/include/poll.yaml
  poll.h
  DEPENDS
    .llvm-libc-types.struct_pollfd
    .llvm-libc-types.nfds_t
    .llvm-libc-macros.poll-macros
  )

add_header_macro(
  nl_types
  ../libc/include/nl_types.yaml
  nl_types.h
  DEPENDS
    .llvm-libc-macros.nl_types_macros
    .llvm-libc-types.nl_catd
  )

# UEFI spec references "Uefi.h" so we use that name for compatibility
add_header_macro(
  uefi
  ../libc/include/Uefi.yaml
  Uefi.h
  DEPENDS
    .llvm_libc_common_h
    .llvm-libc-types.EFI_GUID
    .llvm-libc-types.EFI_STATUS
    .llvm-libc-types.EFI_SYSTEM_TABLE
)

if(NOT LLVM_LIBC_FULL_BUILD)
  # We don't install headers in non-fullbuild mode.
  return()
endif()

function(get_all_install_header_targets out_var)
  set(all_deps ${ARGN})
  foreach(target IN LISTS ARGN)
    get_target_property(deps ${target} DEPS)
    if(NOT deps)
      continue()
    endif()
    list(APPEND all_deps ${deps})
    get_all_install_header_targets(nested_deps ${deps})
    list(APPEND all_deps ${nested_deps})
  endforeach()
  list(REMOVE_DUPLICATES all_deps)
  set(${out_var} ${all_deps} PARENT_SCOPE)
endfunction(get_all_install_header_targets)

get_all_install_header_targets(all_install_header_targets ${TARGET_PUBLIC_HEADERS})
add_custom_target(generate-libc-headers DEPENDS ${all_install_header_targets})
add_library(libc-headers INTERFACE)
add_dependencies(libc-headers generate-libc-headers)
target_include_directories(libc-headers SYSTEM INTERFACE ${LIBC_INCLUDE_DIR})

foreach(target IN LISTS all_install_header_targets)
  get_target_property(header_file ${target} HEADER_FILE_PATH)
  if(NOT header_file)
    message(FATAL_ERROR "Installable header file '${target}' does not have the "
                        "HEADER_FILE_PATH property set.")
  endif()
  file(RELATIVE_PATH relative_path ${LIBC_INCLUDE_DIR} ${header_file})
  get_filename_component(nested_dir ${relative_path} DIRECTORY)
  install(FILES ${header_file}
          DESTINATION ${LIBC_INSTALL_INCLUDE_DIR}/${nested_dir}
          COMPONENT libc-headers)
  # The GPU optionally provides the supported declarations externally so
  # offloading languages like CUDA and OpenMP know what is supported by libc. We
  # install these in the compiler's resource directory at a preset location.
  if(LIBC_TARGET_OS_IS_GPU AND PACKAGE_VERSION)
    get_target_property(decls_file ${target} DECLS_FILE_PATH)
    if(NOT decls_file)
      continue()
    endif()
    get_clang_resource_dir(resource_dir SUBDIR include)
    file(RELATIVE_PATH relative_path ${LIBC_INCLUDE_DIR} ${decls_file})
    get_filename_component(nested_dir ${relative_path} DIRECTORY)
    set(install_dir
        ${CMAKE_INSTALL_PREFIX}/${resource_dir}/llvm_libc_wrappers/${nested_dir})
    install(FILES ${decls_file}
            DESTINATION ${install_dir}
            COMPONENT libc-headers)
  endif()
endforeach()

if(LLVM_LIBC_FULL_BUILD)
  add_custom_target(install-libc-headers
                    DEPENDS libc-headers
                    COMMAND "${CMAKE_COMMAND}"
                            -DCMAKE_INSTALL_COMPONENT=libc-headers
                            -P "${CMAKE_BINARY_DIR}/cmake_install.cmake")
  # Stripping is a no-op for headers
  add_custom_target(install-libc-headers-stripped DEPENDS install-libc-headers)
endif()
