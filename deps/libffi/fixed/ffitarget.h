/*
 * This file is written by Node.js,
 * and the original code of the libffi library does not include this file.
 */
#ifndef LIBFFI_FIXED_TARGET_H
#define LIBFFI_FIXED_TARGET_H

#ifndef LIBFFI_H
# error "Please do not include ffitarget.h directly into your source.  Use ffi.h instead."
#endif

#if defined _M_X64 || defined __x86_64__
# define X86_64
# if defined _WIN32
#  define X86_WIN64
# elif defined __APPLE__
#  define X86_DARWIN
# endif
# include "../src/x86/ffitarget.h"
#elif defined _M_IX86 || defined __i386__
# define X86
# if defined _WIN32
#  define X86_WIN32
# elif defined __APPLE__
#  define X86_DARWIN
# endif
# include "../src/x86/ffitarget.h"
#elif defined _M_ARM64 || defined __aarch64__
# define AARCH64
# include "../src/aarch64/ffitarget.h"
#elif defined _M_ARM || defined __arm__
# define ARM
# include "../src/arm/ffitarget.h"
#else
# error "Unsupported platform"
#endif

#endif
