/* If these values change, sysv.S must be adapted!  */
#define FFI390_RET_DOUBLE	0
#define FFI390_RET_FLOAT	1
#define FFI390_RET_INT64	2
#define FFI390_RET_INT32	3
#define FFI390_RET_VOID		4

#define FFI360_RET_MASK		7
#define FFI390_RET_IN_MEM	8

#define FFI390_RET_STRUCT	(FFI390_RET_VOID | FFI390_RET_IN_MEM)


#if defined(FFI_EXEC_STATIC_TRAMP)
/*
 * For the trampoline code table mapping, a mapping size of 4K is chosen.
 */
#define FFI390_TRAMP_MAP_SHIFT	12
#define FFI390_TRAMP_MAP_SIZE	(1 << FFI390_TRAMP_MAP_SHIFT)
#define FFI390_TRAMP_SIZE	16

#endif
