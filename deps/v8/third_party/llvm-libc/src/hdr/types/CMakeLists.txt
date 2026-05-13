add_proxy_header_library(
  char8_t
  HDRS
    char8_t.h
  DEPENDS
    libc.hdr.uchar_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.char8_t
    libc.include.uchar
)

add_proxy_header_library(
  char32_t
  HDRS
    char32_t.h
  DEPENDS
    libc.hdr.uchar_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.char32_t
    libc.include.uchar
)

add_proxy_header_library(
  mbstate_t
  HDRS
    mbstate_t.h
  DEPENDS
    libc.include.llvm-libc-types.mbstate_t
  FULL_BUILD_DEPENDS
    libc.include.uchar
)

add_proxy_header_library(
  div_t
  HDRS
    div_t.h
  DEPENDS
    libc.hdr.stdlib_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.div_t
    libc.include.stdlib
)

add_proxy_header_library(
  struct_dl_phdr_info
  HDRS
    struct_dl_phdr_info.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_dl_phdr_info
)

add_proxy_header_library(
  ldiv_t
  HDRS
    ldiv_t.h
  DEPENDS
    libc.hdr.stdlib_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.ldiv_t
    libc.include.stdlib
)

add_proxy_header_library(
  lldiv_t
  HDRS
    lldiv_t.h
  DEPENDS
    libc.hdr.stdlib_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.lldiv_t
    libc.include.stdlib
)

add_proxy_header_library(
  sigset_t
  HDRS
    sigset_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.sigset_t
)

add_proxy_header_library(
  siginfo_t
  HDRS
    siginfo_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.siginfo_t
)

add_proxy_header_library(
  struct_epoll_event
  HDRS
    struct_epoll_event.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_epoll_event
)

add_proxy_header_library(
  struct_flock
  HDRS
    struct_flock.h
  DEPENDS
    libc.hdr.fcntl_overlay
  FULL_BUILD_DEPENDS
    libc.hdr.stdint_proxy
    libc.include.llvm-libc-types.struct_flock
)

add_proxy_header_library(
  struct_flock64
  HDRS
    struct_flock64.h
  DEPENDS
    libc.hdr.fcntl_overlay
  FULL_BUILD_DEPENDS
    libc.hdr.stdint_proxy
    libc.include.llvm-libc-types.struct_flock64
)

add_proxy_header_library(
  struct_f_owner_ex
  HDRS
    struct_f_owner_ex.h
  DEPENDS
    libc.hdr.fcntl_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_f_owner_ex
)

add_proxy_header_library(
  struct_timespec
  HDRS
    struct_timespec.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_timespec
)

add_proxy_header_library(
  struct_tm
  HDRS
    struct_tm.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_tm
)

add_proxy_header_library(
  rsize_t
  HDRS
    rsize_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.rsize_t
)

add_proxy_header_library(
  size_t
  HDRS
    size_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.size_t
)

add_proxy_header_library(
  struct_stat
  HDRS
    struct_stat.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_stat
)

add_proxy_header_library(
  ssize_t
  HDRS
    ssize_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.ssize_t
)

add_proxy_header_library(
  mode_t
  HDRS
    mode_t.h
  DEPENDS
    libc.hdr.fcntl_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.mode_t
    libc.include.fcntl
)

add_proxy_header_library(
  constraint_handler_t
  HDRS
    constraint_handler_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.constraint_handler_t
)

add_proxy_header_library(
  errno_t
  HDRS
    errno_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.errno_t
)

add_proxy_header_library(
  fenv_t
  HDRS
    fenv_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.fenv_t
    libc.include.fenv
)

add_proxy_header_library(
  fexcept_t
  HDRS
    fexcept_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.fexcept_t
    libc.include.fenv
)

add_proxy_header_library(
  time_t
  HDRS
    time_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.time_t
    libc.include.time
)

add_proxy_header_library(
  clockid_t
  HDRS
    clockid_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.clockid_t
)

add_proxy_header_library(
  clock_t
  HDRS
    clock_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.clock_t
    libc.include.time
)

add_proxy_header_library(
  suseconds_t
  HDRS
    suseconds_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.suseconds_t
    libc.include.sys_time
)

add_proxy_header_library(
  struct_timeval
  HDRS
    struct_timeval.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_timeval
    libc.include.sys_time
)

add_proxy_header_library(
  struct_itimerval
  HDRS
    struct_itimerval.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_itimerval
    libc.include.sys_time
)

add_proxy_header_library(
  key_t
  HDRS
    key_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.key_t
    libc.include.sys_ipc
)

add_proxy_header_library(
  pid_t
  HDRS
    pid_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.pid_t
)

add_proxy_header_library(
  pthread_barrier_t
  HDRS
    pthread_barrier_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.pthread_barrier_t
)

add_proxy_header_library(
  pthread_barrierattr_t
  HDRS
    pthread_barrierattr_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.pthread_barrierattr_t
)

add_proxy_header_library(
  atexithandler_t
  HDRS
    atexithandler_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.atexithandler_t
    libc.include.stdlib
)

add_proxy_header_library(
  struct_sigaction
  HDRS
    struct_sigaction.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_sigaction
    libc.include.signal
)

add_proxy_header_library(
  FILE
  HDRS
    FILE.h
  DEPENDS
    libc.hdr.stdio_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.FILE
    libc.include.stdio
)

add_proxy_header_library(
  off_t
  HDRS
    off_t.h
  DEPENDS
    libc.hdr.stdio_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.off_t
    libc.include.stdio
)

add_proxy_header_library(
  cookie_io_functions_t
  HDRS
    cookie_io_functions_t.h
  DEPENDS
    libc.hdr.stdio_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.cookie_io_functions_t
    libc.include.stdio
)

add_proxy_header_library(
  locale_t
  HDRS
    locale_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.locale_t
    libc.include.locale
)

add_proxy_header_library(
  stack_t
  HDRS
    stack_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.stack_t
    libc.include.signal
)

add_proxy_header_library(
  jmp_buf
  HDRS
    jmp_buf.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.jmp_buf
    libc.include.setjmp
)

add_proxy_header_library(
  sigjmp_buf
  HDRS
    sigjmp_buf.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.sigjmp_buf
    libc.include.setjmp
)

add_proxy_header_library(
  struct_linger
  HDRS
    struct_linger.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_linger
    libc.include.sys_socket
)

add_proxy_header_library(
  struct_cmsghdr
  HDRS
    struct_cmsghdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_cmsghdr
    libc.include.sys_socket
)

add_proxy_header_library(
  struct_msghdr
  HDRS
    struct_msghdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_msghdr
    libc.include.sys_socket
)

add_proxy_header_library(
  struct_sockaddr
  HDRS
    struct_sockaddr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_sockaddr
    libc.include.sys_socket
)

add_proxy_header_library(
  struct_sockaddr_storage
  HDRS
    struct_sockaddr_storage.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_sockaddr_storage
    libc.include.sys_socket
)

add_proxy_header_library(
  struct_sockaddr_un
  HDRS
    struct_sockaddr_un.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_sockaddr_un
    libc.include.sys_socket
)

add_proxy_header_library(
  socklen_t
  HDRS
    socklen_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.socklen_t
    libc.include.sys_socket
)

add_proxy_header_library(
  wchar_t
  HDRS
    wchar_t.h
  DEPENDS
    libc.hdr.wchar_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.wchar_t
    libc.include.wchar
)

add_proxy_header_library(
  wint_t
  HDRS
    wint_t.h
  DEPENDS
    libc.hdr.wchar_overlay
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.wint_t
    libc.include.wchar
)


add_proxy_header_library(
  wctype_t
  HDRS
    wctype_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.wctype_t
)

add_proxy_header_library(
  uid_t
  HDRS
    uid_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.uid_t
)

add_proxy_header_library(
  struct_iovec
  HDRS
    struct_iovec.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_iovec
    libc.include.sys_uio
)

add_proxy_header_library(
  nfds_t
  HDRS
    nfds_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.nfds_t
)

add_proxy_header_library(
  struct_pollfd
  HDRS
    struct_pollfd.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_pollfd
)

add_proxy_header_library(
  cpu_set_t
  HDRS
    cpu_set_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.cpu_set_t
    libc.include.sched
)

add_proxy_header_library(
  ENTRY
  HDRS
    ENTRY.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.ENTRY
    libc.include.search
)

add_proxy_header_library(
  ACTION
  HDRS
    ACTION.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.ACTION
    libc.include.search
)

add_proxy_header_library(
  struct_sched_param
  HDRS
    struct_sched_param.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_sched_param
    libc.include.sched
)

add_proxy_header_library(
  dl_info
  HDRS
    dl_info.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.dl_info
    libc.include.dlfcn
)

add_proxy_header_library(
  struct_rlimit
  HDRS
    struct_rlimit.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_rlimit
    libc.include.sys_resource
)

add_proxy_header_library(
  struct_semid_ds
  HDRS
    struct_semid_ds.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_semid_ds
    libc.include.sys_sem
)

add_proxy_header_library(
  struct_sembuf
  HDRS
    struct_sembuf.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_sembuf
    libc.include.sys_sem
)

add_proxy_header_library(
  struct_seminfo
  HDRS
    struct_seminfo.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.struct_seminfo
    libc.include.sys_sem
)

add_proxy_header_library(
  gid_t
  HDRS
    gid_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.gid_t
)

add_proxy_header_library(
  Elf32_Addr
  HDRS
    Elf32_Addr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Addr
)

add_proxy_header_library(
  Elf32_Chdr
  HDRS
    Elf32_Chdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Chdr
)

add_proxy_header_library(
  Elf32_Dyn
  HDRS
    Elf32_Dyn.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Dyn
)

add_proxy_header_library(
  Elf32_Ehdr
  HDRS
    Elf32_Ehdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Ehdr
)

add_proxy_header_library(
  Elf32_Half
  HDRS
    Elf32_Half.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Half
)

add_proxy_header_library(
  Elf32_Lword
  HDRS
    Elf32_Lword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Lword
)

add_proxy_header_library(
  Elf32_Nhdr
  HDRS
    Elf32_Nhdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Nhdr
)

add_proxy_header_library(
  Elf32_Off
  HDRS
    Elf32_Off.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Off
)

add_proxy_header_library(
  Elf32_Phdr
  HDRS
    Elf32_Phdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Phdr
)

add_proxy_header_library(
  Elf32_Rela
  HDRS
    Elf32_Rela.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Rela
)

add_proxy_header_library(
  Elf32_Rel
  HDRS
    Elf32_Rel.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Rel
)

add_proxy_header_library(
  Elf32_Shdr
  HDRS
    Elf32_Shdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Shdr
)

add_proxy_header_library(
  Elf32_Sword
  HDRS
    Elf32_Sword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Sword
)

add_proxy_header_library(
  Elf32_Sym
  HDRS
    Elf32_Sym.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Sym
)

add_proxy_header_library(
  Elf32_Verdaux
  HDRS
    Elf32_Verdaux.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Verdaux
)

add_proxy_header_library(
  Elf32_Verdef
  HDRS
    Elf32_Verdef.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Verdef
)

add_proxy_header_library(
  Elf32_Vernaux
  HDRS
    Elf32_Vernaux.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Vernaux
)

add_proxy_header_library(
  Elf32_Verneed
  HDRS
    Elf32_Verneed.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Verneed
)

add_proxy_header_library(
  Elf32_Versym
  HDRS
    Elf32_Versym.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Versym
)

add_proxy_header_library(
  Elf32_Word
  HDRS
    Elf32_Word.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Word
)

add_proxy_header_library(
  Elf32_Xword
  HDRS
  Elf32_Xword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_Xword
)

add_proxy_header_library(
  Elf32_auxv_t
  HDRS
  Elf32_auxv_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf32_auxv_t
)

add_proxy_header_library(
  Elf64_Addr
  HDRS
    Elf64_Addr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Addr
)

add_proxy_header_library(
  Elf64_Chdr
  HDRS
    Elf64_Chdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Chdr
)

add_proxy_header_library(
  Elf64_Dyn
  HDRS
    Elf64_Dyn.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Dyn
)

add_proxy_header_library(
  Elf64_Ehdr
  HDRS
    Elf64_Ehdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Ehdr
)

add_proxy_header_library(
  Elf64_Half
  HDRS
    Elf64_Half.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Half
)

add_proxy_header_library(
  Elf64_Lword
  HDRS
    Elf64_Lword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Lword
)

add_proxy_header_library(
  Elf64_Nhdr
  HDRS
    Elf64_Nhdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Nhdr
)

add_proxy_header_library(
  Elf64_Off
  HDRS
    Elf64_Off.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Off
)

add_proxy_header_library(
  Elf64_Phdr
  HDRS
    Elf64_Phdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Phdr
)

add_proxy_header_library(
  Elf64_Rela
  HDRS
    Elf64_Rela.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Rela
)

add_proxy_header_library(
  Elf64_Rel
  HDRS
    Elf64_Rel.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Rel
)

add_proxy_header_library(
  Elf64_Shdr
  HDRS
    Elf64_Shdr.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Shdr
)

add_proxy_header_library(
  Elf64_Sword
  HDRS
    Elf64_Sword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Sword
)

add_proxy_header_library(
  Elf64_Sxword
  HDRS
    Elf64_Sxword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Sxword
)

add_proxy_header_library(
  Elf64_Sym
  HDRS
    Elf64_Sym.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Sym
)

add_proxy_header_library(
  Elf64_Word
  HDRS
    Elf64_Word.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Word
)

add_proxy_header_library(
  Elf64_Xword
  HDRS
    Elf64_Xword.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Xword
)

add_proxy_header_library(
  Elf64_auxv_t
  HDRS
  Elf64_auxv_t.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_auxv_t
)

add_proxy_header_library(
  Elf64_Verdaux
  HDRS
    Elf64_Verdaux.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Verdaux
)

add_proxy_header_library(
  Elf64_Verdef
  HDRS
    Elf64_Verdef.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Verdef
)

add_proxy_header_library(
  Elf64_Vernaux
  HDRS
    Elf64_Vernaux.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Vernaux
)

add_proxy_header_library(
  Elf64_Verneed
  HDRS
    Elf64_Verneed.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Verneed
)

add_proxy_header_library(
  Elf64_Versym
  HDRS
    Elf64_Versym.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.Elf64_Versym
)

add_proxy_header_library(
  posix_tnode
  HDRS
    posix_tnode.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.posix_tnode
    libc.include.search
)

add_proxy_header_library(
  VISIT
  HDRS
    VISIT.h
  FULL_BUILD_DEPENDS
    libc.include.llvm-libc-types.VISIT
    libc.include.search
)
