#if PLATFORM_WINDOWS || PLATFORM_MAC
# define IDENTIFIER(n)  _##n
#else /* Linux */
# define IDENTIFIER(n)  n
#endif

.globl IDENTIFIER(asm_function)
IDENTIFIER(asm_function):
  movl $41, %eax
  ret
