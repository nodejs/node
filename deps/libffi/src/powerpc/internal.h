#ifdef FFI_EXEC_STATIC_TRAMP
/* For the trampoline code table mapping, a mapping size of 64K is chosen.  */
#define PPC_TRAMP_MAP_SHIFT	16
#define PPC_TRAMP_MAP_SIZE	(1 << PPC_TRAMP_MAP_SHIFT)
# ifdef __PCREL__
#  define PPC_TRAMP_SIZE	24
# else
#  define PPC_TRAMP_SIZE	40
# endif /* __PCREL__ */
#endif /* FFI_EXEC_STATIC_TRAMP */
