add_header(off64_t HDR off64_t.h)
add_header(size_t HDR size_t.h)
add_header(
  rsize_t
  HDR
    rsize_t.h
  DEPENDS
    .size_t
    libc.include.llvm-libc-macros.annex_k_macros
)
add_header(ssize_t HDR ssize_t.h)
add_header(__atfork_callback_t HDR __atfork_callback_t.h)
add_header(__search_compare_t HDR __search_compare_t.h)
add_header(__call_once_func_t HDR __call_once_func_t.h)
add_header(__dl_iterate_phdr_callback_t HDR __dl_iterate_phdr_callback_t.h DEPENDS .size_t)
add_header(__exec_argv_t HDR __exec_argv_t.h)
add_header(__exec_envp_t HDR __exec_envp_t.h)
add_header(__futex_word HDR __futex_word.h)
add_header(pid_t HDR pid_t.h)
add_header(__mutex_type HDR __mutex_type.h DEPENDS .__futex_word .pid_t .size_t)
add_header(__barrier_type HDR __barrier_type.h)
add_header(__pthread_once_func_t HDR __pthread_once_func_t.h)
add_header(__pthread_start_t HDR __pthread_start_t.h)
add_header(__pthread_tss_dtor_t HDR __pthread_tss_dtor_t.h)
add_header(__qsortcompare_t HDR __qsortcompare_t.h)
add_header(__qsortrcompare_t HDR __qsortrcompare_t.h)
add_header(__thread_type HDR __thread_type.h)
add_header(blkcnt_t HDR blkcnt_t.h)
add_header(blksize_t HDR blksize_t.h)
add_header(cc_t HDR cc_t.h)
add_header(clock_t HDR clock_t.h)
add_header(clockid_t HDR clockid_t.h)
add_header(cnd_t HDR cnd_t.h DEPENDS .__futex_word)
add_header(cookie_io_functions_t HDR cookie_io_functions_t.h DEPENDS .off64_t .ssize_t)
add_header(cpu_set_t HDR cpu_set_t.h)
add_header(double_t HDR double_t.h)
add_header(DIR HDR DIR.h)
add_header(dev_t HDR dev_t.h)
add_header(div_t HDR div_t.h)
add_header(errno_t HDR errno_t.h DEPENDS libc.include.llvm-libc-macros.annex_k_macros)
add_header(
  constraint_handler_t
  HDR
    constraint_handler_t.h
  DEPENDS
    .errno_t
    libc.include.llvm-libc-macros.annex_k_macros
)
add_header(ldiv_t HDR ldiv_t.h)
add_header(lldiv_t HDR lldiv_t.h)
add_header(FILE HDR FILE.h)
add_header(fd_set HDR fd_set.h DEPENDS libc.include.llvm-libc-macros.sys_select_macros)
add_header(fenv_t HDR fenv_t.h)
add_header(fexcept_t HDR fexcept_t.h)
add_header(float_t HDR float_t.h)
add_header(gid_t HDR gid_t.h)
add_header(uid_t HDR uid_t.h)
add_header(imaxdiv_t HDR imaxdiv_t.h)
add_header(in_addr_t HDR in_addr_t.h)
add_header(in_port_t HDR in_port_t.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(struct_in_addr HDR struct_in_addr.h DEPENDS .in_addr_t)
add_header(ino_t HDR ino_t.h)
add_header(key_t HDR key_t.h)
add_header(mbstate_t HDR mbstate_t.h)
add_header(mode_t HDR mode_t.h)
add_header(mtx_t HDR mtx_t.h DEPENDS .__futex_word .__mutex_type)
add_header(nfds_t HDR nfds_t.h)
add_header(nl_catd HDR nl_catd.h)
add_header(nlink_t HDR nlink_t.h)
add_header(off_t HDR off_t.h)
add_header(once_flag HDR once_flag.h DEPENDS .__futex_word)
add_header(posix_spawn_file_actions_t HDR posix_spawn_file_actions_t.h)
add_header(posix_spawnattr_t HDR posix_spawnattr_t.h)
add_header(posix_tnode HDR posix_tnode.h)
add_header(pthread_attr_t HDR pthread_attr_t.h DEPENDS .size_t)
add_header(pthread_cond_t HDR pthread_cond_t.h DEPENDS .__futex_word .size_t)
add_header(pthread_condattr_t HDR pthread_condattr_t.h DEPENDS .clockid_t)
add_header(pthread_key_t HDR pthread_key_t.h)
add_header(pthread_mutex_t HDR pthread_mutex_t.h DEPENDS .__futex_word .__mutex_type)
add_header(pthread_mutexattr_t HDR pthread_mutexattr_t.h)
add_header(pthread_barrier_t HDR pthread_barrier_t.h DEPENDS .__barrier_type)
add_header(pthread_barrierattr_t HDR pthread_barrierattr_t.h)
add_header(pthread_once_t HDR pthread_once_t.h DEPENDS .__futex_word)
add_header(pthread_rwlock_t HDR pthread_rwlock_t.h DEPENDS .__futex_word .pid_t)
add_header(pthread_rwlockattr_t HDR pthread_rwlockattr_t.h)
add_header(pthread_spinlock_t HDR pthread_spinlock_t.h DEPENDS .pid_t)
add_header(pthread_t HDR pthread_t.h DEPENDS .__thread_type)
add_header(rlim_t HDR rlim_t.h)
if(LIBC_TYPES_TIME_T_IS_32_BIT)
  add_header(time_t HDR time_t_32.h DEST_HDR time_t.h)
else()
  add_header(time_t HDR time_t_64.h DEST_HDR time_t.h)
endif()
add_header(sighandler_t HDR sighandler_t.h)
add_header(stack_t HDR stack_t.h DEPENDS .size_t)
add_header(suseconds_t HDR suseconds_t.h)
add_header(struct_dirent HDR struct_dirent.h DEPENDS .ino_t .off_t)
add_header(struct_dl_phdr_info HDR struct_dl_phdr_info.h DEPENDS .size_t libc.include.llvm-libc-macros.link_macros)
add_header(dl_info HDR Dl_info.h)
add_header(struct_f_owner_ex HDR struct_f_owner_ex.h DEPENDS .pid_t)
add_header(struct_flock HDR struct_flock.h DEPENDS .off_t .pid_t)
add_header(struct_flock64 HDR struct_flock64.h DEPENDS .off64_t .pid_t)
add_header(
  struct_ipc_perm
  HDR struct_ipc_perm.h
  DEPENDS
    .key_t
    .uid_t
    .gid_t
    .mode_t
)
add_header(struct_pollfd HDR struct_pollfd.h)
add_header(struct_rlimit HDR struct_rlimit.h DEPENDS .rlim_t)
add_header(struct_sched_param HDR struct_sched_param.h)
add_header(struct_sembuf HDR struct_sembuf.h)
add_header(struct_seminfo HDR struct_seminfo.h)
add_header(
  struct_semid_ds
  HDR struct_semid_ds.h
  DEPENDS
    .struct_ipc_perm
    .time_t
)
add_header(struct_timeval HDR struct_timeval.h DEPENDS .suseconds_t .time_t)
add_header(struct_itimerval HDR struct_itimerval.h DEPENDS .struct_timeval)
add_header(struct_rusage HDR struct_rusage.h DEPENDS .struct_timeval)
add_header(union_sigval HDR union_sigval.h)
add_header(siginfo_t HDR siginfo_t.h DEPENDS .union_sigval .pid_t .uid_t .clock_t)
add_header(sig_atomic_t HDR sig_atomic_t.h)
add_header(sigset_t HDR sigset_t.h DEPENDS libc.include.llvm-libc-macros.signal_macros)
set(mcontext_deps)
set(ucontext_deps .sigset_t .stack_t)

if(LIBC_TARGET_ARCHITECTURE STREQUAL "x86_64")
  file(COPY x86_64 DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  add_header(
    mcontext_t_arch
    HDR
      x86_64/mcontext_t.h
  )
  add_header(
    ucontext_t_arch
    HDR
      x86_64/ucontext_t.h
    DEPENDS
      .mcontext_t_arch
      .sigset_t
      .stack_t
  )
  list(APPEND mcontext_deps .mcontext_t_arch)
  list(APPEND ucontext_deps .ucontext_t_arch)
endif()

add_header(mcontext_t HDR mcontext_t.h DEPENDS ${mcontext_deps})
add_header(ucontext_t HDR ucontext_t.h DEPENDS .mcontext_t ${ucontext_deps})
add_header(__jmp_buf HDR __jmp_buf.h DEPENDS .sigset_t)
add_header(jmp_buf HDR jmp_buf.h DEPENDS .__jmp_buf)
add_header(sigjmp_buf HDR sigjmp_buf.h DEPENDS .__jmp_buf)
add_header(struct_sigaction HDR struct_sigaction.h DEPENDS .sigset_t .siginfo_t)
add_header(struct_timespec HDR struct_timespec.h DEPENDS .time_t)
add_header(
  struct_stat
  HDR struct_stat.h
  DEPENDS
    .dev_t .ino_t .mode_t .nlink_t .uid_t .gid_t .off_t .struct_timespec
    .blksize_t .blkcnt_t
)
add_header(struct_tm HDR struct_tm.h)
add_header(struct_utsname HDR struct_utsname.h)
add_header(thrd_start_t HDR thrd_start_t.h)
add_header(thrd_t HDR thrd_t.h DEPENDS .__thread_type)
add_header(tss_t HDR tss_t.h)
add_header(tss_dtor_t HDR tss_dtor_t.h)
add_header(__atexithandler_t HDR __atexithandler_t.h)
add_header(speed_t HDR speed_t.h)
add_header(tcflag_t HDR tcflag_t.h)
add_header(struct_termios HDR struct_termios.h DEPENDS .cc_t .speed_t .tcflag_t)
add_header(__getoptargv_t HDR __getoptargv_t.h)
add_header(wchar_t HDR wchar_t.h)
add_header(char8_t HDR char8_t.h)
add_header(
    char16_t
  HDR
    char16_t.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
)
add_header(
    char32_t
  HDR
    char32_t.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
)
add_header(wint_t HDR wint_t.h)
add_header(wctype_t HDR wctype_t.h)
add_header(sa_family_t HDR sa_family_t.h)
add_header(socklen_t HDR socklen_t.h)
add_header(struct_sockaddr_un HDR struct_sockaddr_un.h DEPENDS .sa_family_t)
add_header(struct_sockaddr HDR struct_sockaddr.h DEPENDS .sa_family_t)
add_header(struct_sockaddr_storage HDR struct_sockaddr_storage.h DEPENDS .sa_family_t)
add_header(struct_iovec HDR struct_iovec.h DEPENDS .size_t)
add_header(struct_linger HDR struct_linger.h)
add_header(struct_cmsghdr HDR struct_cmsghdr.h DEPENDS .size_t)
add_header(struct_msghdr HDR struct_msghdr.h DEPENDS .size_t .socklen_t .struct_iovec)
add_header(ACTION HDR ACTION.h)
add_header(ENTRY HDR ENTRY.h)
add_header(VISIT HDR VISIT.h)
add_header(struct_hsearch_data HDR struct_hsearch_data.h)
add_header(struct_epoll_event HDR struct_epoll_event.h)
add_header(struct_epoll_data HDR struct_epoll_data.h)
add_header(
  float128
  HDR
    float128.h
  DEPENDS
    libc.include.llvm-libc-macros.float_macros
)
add_header(
  cfloat128
  HDR
    cfloat128.h
  DEPENDS
    libc.include.llvm-libc-macros.cfloat128_macros
)
add_header(
  cfloat16
  HDR
    cfloat16.h
  DEPENDS
    libc.include.llvm-libc-macros.cfloat16_macros
)
add_header(fsblkcnt_t HDR fsblkcnt_t.h)
add_header(fsfilcnt_t HDR fsfilcnt_t.h)
add_header(
  struct_statvfs
HDR
  struct_statvfs.h
DEPENDS
  .fsblkcnt_t
  .fsfilcnt_t
)
add_header(locale_t HDR locale_t.h)
add_header(struct_lconv HDR struct_lconv.h)
add_header(int_hk_t HDR int_hk_t.h)
add_header(int_hr_t HDR int_hr_t.h)
add_header(int_k_t HDR int_k_t.h)
add_header(int_lk_t HDR int_lk_t.h)
add_header(int_lr_t HDR int_lr_t.h)
add_header(int_r_t HDR int_r_t.h)
add_header(uint_uhk_t HDR uint_uhk_t.h)
add_header(uint_uhr_t HDR uint_uhr_t.h)
add_header(uint_uk_t HDR uint_uk_t.h)
add_header(uint_ulk_t HDR uint_ulk_t.h)
add_header(uint_ulr_t HDR uint_ulr_t.h)
add_header(uint_ur_t HDR uint_ur_t.h)

# ELF
add_header(Elf32_Addr HDR Elf32_Addr.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_Half HDR Elf32_Half.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_Lword HDR Elf32_Lword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_Off HDR Elf32_Off.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_Sword HDR Elf32_Sword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_Word HDR Elf32_Word.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_Xword HDR Elf32_Xword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf32_auxv_t HDR Elf32_auxv_t.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(
  Elf32_Chdr
  HDR
    Elf32_Chdr.h
  DEPENDS
    .Elf32_Word
)
add_header(
  Elf32_Dyn
  HDR
    Elf32_Dyn.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Sword
    .Elf32_Word
)
add_header(
  Elf32_Ehdr
  HDR
    Elf32_Ehdr.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Half
    .Elf32_Off
    .Elf32_Word
)
add_header(Elf32_Nhdr HDR Elf32_Nhdr.h DEPENDS .Elf32_Word)
add_header(
  Elf32_Phdr
  HDR
    Elf32_Phdr.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Off
    .Elf32_Word
)
add_header(
  Elf32_Rel
  HDR
    Elf32_Rel.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Word
)
add_header(
  Elf32_Rela
  HDR
    Elf32_Rela.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Sword
    .Elf32_Word
)
add_header(
  Elf32_Shdr
  HDR
    Elf32_Shdr.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Off
    .Elf32_Word
)
add_header(
  Elf32_Sym
  HDR
    Elf32_Sym.h
  DEPENDS
    .Elf32_Addr
    .Elf32_Half
    .Elf32_Word
)
add_header(
  Elf32_Verdaux
  HDR
    Elf32_Verdaux.h
  DEPENDS
    .Elf32_Word
)
add_header(
  Elf32_Verdef
  HDR
    Elf32_Verdef.h
  DEPENDS
    .Elf32_Half
    .Elf32_Word
)
add_header(
  Elf32_Vernaux
  HDR
    Elf32_Vernaux.h
  DEPENDS
    .Elf32_Half
    .Elf32_Word
)
add_header(
  Elf32_Verneed
  HDR
    Elf32_Verneed.h
  DEPENDS
    .Elf32_Half
    .Elf32_Word
)
add_header(
  Elf32_Versym
  HDR
    Elf32_Versym.h
  DEPENDS
  .Elf32_Half
)

add_header(Elf64_Addr HDR Elf64_Addr.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Half HDR Elf64_Half.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Lword HDR Elf64_Lword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Off HDR Elf64_Off.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Sword HDR Elf64_Sword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Sxword HDR Elf64_Sxword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Word HDR Elf64_Word.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_Xword HDR Elf64_Xword.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(Elf64_auxv_t HDR Elf64_auxv_t.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(
  Elf64_Chdr
  HDR
    Elf64_Chdr.h
  DEPENDS
    .Elf64_Word
    .Elf64_Xword
)
add_header(
  Elf64_Dyn
  HDR
    Elf64_Dyn.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Off
    .Elf64_Sxword
    .Elf64_Xword
)
add_header(
  Elf64_Ehdr
  HDR
    Elf64_Ehdr.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Half
    .Elf64_Off
    .Elf64_Word
)
add_header(Elf64_Nhdr HDR Elf64_Nhdr.h DEPENDS .Elf64_Word)
add_header(
  Elf64_Phdr
  HDR
    Elf64_Phdr.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Off
    .Elf64_Word
    .Elf64_Xword
)
add_header(
  Elf64_Rel
  HDR
    Elf64_Rel.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Xword
)
add_header(
  Elf64_Rela
  HDR
    Elf64_Rela.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Sxword
    .Elf64_Xword
)
add_header(
  Elf64_Shdr
  HDR
    Elf64_Shdr.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Off
    .Elf64_Word
    .Elf64_Xword
)
add_header(
  Elf64_Sym
  HDR
    Elf64_Sym.h
  DEPENDS
    .Elf64_Addr
    .Elf64_Half
    .Elf64_Word
    .Elf64_Xword
)
add_header(
  Elf64_Verdaux
  HDR
    Elf64_Verdaux.h
  DEPENDS
    .Elf64_Word
)
add_header(
  Elf64_Verdef
  HDR
    Elf64_Verdef.h
  DEPENDS
    .Elf64_Half
    .Elf64_Word
)
add_header(
  Elf64_Vernaux
  HDR
    Elf64_Vernaux.h
  DEPENDS
    .Elf64_Half
    .Elf64_Word
)
add_header(
  Elf64_Verneed
  HDR
    Elf64_Verneed.h
  DEPENDS
    .Elf64_Half
    .Elf64_Word
)
add_header(
  Elf64_Versym
  HDR
    Elf64_Versym.h
  DEPENDS
  .Elf64_Half
)

# UEFI
add_header(EFI_GUID HDR EFI_GUID.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(EFI_CONFIGURATION_TABLE HDR EFI_CONFIGURATION_TABLE.h DEPENDS .EFI_GUID)

add_header(EFI_PHYSICAL_ADDRESS HDR EFI_PHYSICAL_ADDRESS.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(EFI_VIRTUAL_ADDRESS HDR EFI_VIRTUAL_ADDRESS.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)

add_header(EFI_MEMORY_DESCRIPTOR
  HDR
    EFI_MEMORY_DESCRIPTOR.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
    .EFI_PHYSICAL_ADDRESS
    .EFI_VIRTUAL_ADDRESS
)

add_header(EFI_ALLOCATE_TYPE HDR EFI_ALLOCATE_TYPE.h)
add_header(EFI_EVENT HDR EFI_EVENT.h)
add_header(EFI_INTERFACE_TYPE HDR EFI_INTERFACE_TYPE.h)
add_header(EFI_LOCATE_SEARCH_TYPE HDR EFI_LOCATE_SEARCH_TYPE.h)
add_header(EFI_MEMORY_TYPE HDR EFI_MEMORY_TYPE.h)
add_header(EFI_HANDLE HDR EFI_HANDLE.h)
add_header(EFI_TIME HDR EFI_TIME.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)
add_header(EFI_TIMER_DELAY HDR EFI_TIMER_DELAY.h)
add_header(EFI_TPL HDR EFI_TPL.h DEPENDS .size_t)
add_header(EFI_STATUS HDR EFI_STATUS.h DEPENDS libc.include.llvm-libc-macros.stdint_macros)

add_header(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY
  HDR
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
    .EFI_HANDLE
)

add_header(EFI_CAPSULE
  HDR
    EFI_CAPSULE.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
    .EFI_GUID
)

add_header(EFI_TABLE_HEADER
  HDR
    EFI_TABLE_HEADER.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
)

add_header(EFI_DEVICE_PATH_PROTOCOL
  HDR
    EFI_DEVICE_PATH_PROTOCOL.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
)

add_header(EFI_SIMPLE_TEXT_INPUT_PROTOCOL
  HDR
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL.h
  DEPENDS
    libc.include.llvm-libc-macros.EFIAPI_macros
    libc.include.llvm-libc-macros.stdint_macros
    .EFI_EVENT
    .EFI_STATUS
    .char16_t
)

add_header(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
  HDR
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL.h
  DEPENDS
    libc.include.llvm-libc-macros.stdint_macros
    .EFI_STATUS
    .size_t
)

add_header(EFI_BOOT_SERVICES
  HDR
    EFI_BOOT_SERVICES.h
  DEPENDS
    libc.include.llvm-libc-macros.EFIAPI_macros
    .EFI_ALLOCATE_TYPE
    .EFI_DEVICE_PATH_PROTOCOL
    .EFI_EVENT
    .EFI_INTERFACE_TYPE
    .EFI_LOCATE_SEARCH_TYPE
    .EFI_MEMORY_DESCRIPTOR
    .EFI_MEMORY_TYPE
    .EFI_OPEN_PROTOCOL_INFORMATION_ENTRY
    .EFI_PHYSICAL_ADDRESS
    .EFI_STATUS
    .EFI_TABLE_HEADER
    .EFI_TIMER_DELAY
    .EFI_TPL
    .char16_t
)

add_header(EFI_RUNTIME_SERVICES
  HDR
    EFI_RUNTIME_SERVICES.h
  DEPENDS
    .EFI_CAPSULE
    .EFI_STATUS
    .EFI_TABLE_HEADER
    .EFI_TIME
    .char16_t
)

add_header(EFI_SYSTEM_TABLE
  HDR
    EFI_SYSTEM_TABLE.h
  DEPENDS
    .EFI_BOOT_SERVICES
    .EFI_CONFIGURATION_TABLE
    .EFI_HANDLE
    .EFI_RUNTIME_SERVICES
    .EFI_SIMPLE_TEXT_INPUT_PROTOCOL
    .EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
    .EFI_STATUS
    .EFI_TABLE_HEADER
    .char16_t
)
add_header(__action_closure_fn_t 
  HDR 
    __action_closure_fn_t.h 
  DEPENDS 
    .posix_tnode 
    .VISIT
)
add_header(__action_fn_t 
  HDR 
    __action_fn_t.h 
  DEPENDS 
    .posix_tnode 
    .VISIT
)
add_header(__free_fn_t HDR __free_fn_t.h)
