#if PLATFORM_WINDOWS || PLATFORM_MAC
# define IDENTIFIER(n)  _##n
#else /* Linux */
# define IDENTIFIER(n)  n
#endif

.globl IDENTIFIER(lib1_function)
IDENTIFIER(lib1_function):
  movl $42, %eax
  ret
