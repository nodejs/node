/*
 * This file is written by Node.js,
 * and the original code of the libffi library does not include this file.
 */
#if defined _M_X64 || defined __x86_64__
# if defined _WIN32
#  include "../src/x86/ffiw64.c"
# else
#  include "../src/x86/ffi64.c"
# endif
#elif defined _M_IX86 || defined __i386__
# include "../src/x86/ffi.c"
#elif defined _M_ARM64 || defined __aarch64__
# include "../src/aarch64/ffi.c"
#elif defined _M_ARM || defined __arm__
# include "../src/arm/ffi.c"
#else
# error "Unsupported platform"
#endif
