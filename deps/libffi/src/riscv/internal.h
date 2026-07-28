#ifdef FFI_EXEC_STATIC_TRAMP
/* For the trampoline table mapping, a mapping size of 4K (base page size)
   is chosen.  */
#define RISCV_TRAMP_MAP_SHIFT	12
#define RISCV_TRAMP_MAP_SIZE	(1 << RISCV_TRAMP_MAP_SHIFT)
#define RISCV_TRAMP_SIZE	16
#endif /* FFI_EXEC_STATIC_TRAMP */
