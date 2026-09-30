add_header(
  mcontext_t
  HDR
    mcontext_t.h
)

add_header(
  struct_user_regs_struct
  HDR
    struct_user_regs_struct.h
)

add_header(
  ucontext_t
  HDR
    ucontext_t.h
  DEPENDS
    .mcontext_t
    libc.include.llvm-libc-types.sigset_t
    libc.include.llvm-libc-types.stack_t
)
