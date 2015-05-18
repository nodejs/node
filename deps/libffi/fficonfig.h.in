/* fficonfig.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
#undef AC_APPLE_UNIVERSAL_BUILD

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
#undef CRAY_STACKSEG_END

/* Define to 1 if using `alloca.c'. */
#undef C_ALLOCA

/* Define to the flags needed for the .section .eh_frame directive. */
#undef EH_FRAME_FLAGS

/* Define this if you want extra debugging. */
#undef FFI_DEBUG

/* Cannot use PROT_EXEC on this target, so, we revert to alternative means */
#undef FFI_EXEC_TRAMPOLINE_TABLE

/* Define this if you want to enable pax emulated trampolines */
#undef FFI_MMAP_EXEC_EMUTRAMP_PAX

/* Cannot use malloc on this target, so, we revert to alternative means */
#undef FFI_MMAP_EXEC_WRIT

/* Define this is you do not want support for the raw API. */
#undef FFI_NO_RAW_API

/* Define this is you do not want support for aggregate types. */
#undef FFI_NO_STRUCTS

/* Define to 1 if you have `alloca', as a function or macro. */
#undef HAVE_ALLOCA

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#undef HAVE_ALLOCA_H

/* Define if your assembler supports .ascii. */
#undef HAVE_AS_ASCII_PSEUDO_OP

/* Define if your assembler supports .cfi_* directives. */
#undef HAVE_AS_CFI_PSEUDO_OP

/* Define if your assembler supports .register. */
#undef HAVE_AS_REGISTER_PSEUDO_OP

/* Define if your assembler and linker support unaligned PC relative relocs.
   */
#undef HAVE_AS_SPARC_UA_PCREL

/* Define if your assembler supports .string. */
#undef HAVE_AS_STRING_PSEUDO_OP

/* Define if your assembler supports unwind section type. */
#undef HAVE_AS_X86_64_UNWIND_SECTION_TYPE

/* Define if your assembler supports PC relative relocs. */
#undef HAVE_AS_X86_PCREL

/* Define to 1 if you have the <dlfcn.h> header file. */
#undef HAVE_DLFCN_H

/* Define if __attribute__((visibility("hidden"))) is supported. */
#undef HAVE_HIDDEN_VISIBILITY_ATTRIBUTE

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if you have the long double type and it is bigger than a double */
#undef HAVE_LONG_DOUBLE

/* Define to 1 if you have the `memcpy' function. */
#undef HAVE_MEMCPY

/* Define to 1 if you have the <memory.h> header file. */
#undef HAVE_MEMORY_H

/* Define to 1 if you have the `mmap' function. */
#undef HAVE_MMAP

/* Define if mmap with MAP_ANON(YMOUS) works. */
#undef HAVE_MMAP_ANON

/* Define if mmap of /dev/zero works. */
#undef HAVE_MMAP_DEV_ZERO

/* Define if read-only mmap of a plain file works. */
#undef HAVE_MMAP_FILE

/* Define if .eh_frame sections should be read-only. */
#undef HAVE_RO_EH_FRAME

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if you have the <stdlib.h> header file. */
#undef HAVE_STDLIB_H

/* Define to 1 if you have the <strings.h> header file. */
#undef HAVE_STRINGS_H

/* Define to 1 if you have the <string.h> header file. */
#undef HAVE_STRING_H

/* Define to 1 if you have the <sys/mman.h> header file. */
#undef HAVE_SYS_MMAN_H

/* Define to 1 if you have the <sys/stat.h> header file. */
#undef HAVE_SYS_STAT_H

/* Define to 1 if you have the <sys/types.h> header file. */
#undef HAVE_SYS_TYPES_H

/* Define to 1 if you have the <unistd.h> header file. */
#undef HAVE_UNISTD_H

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#undef LT_OBJDIR

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
#undef NO_MINUS_C_MINUS_O

/* Name of package */
#undef PACKAGE

/* Define to the address where bug reports for this package should be sent. */
#undef PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#undef PACKAGE_NAME

/* Define to the full name and version of this package. */
#undef PACKAGE_STRING

/* Define to the one symbol short name of this package. */
#undef PACKAGE_TARNAME

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
#undef PACKAGE_VERSION

/* The size of `double', as computed by sizeof. */
#undef SIZEOF_DOUBLE

/* The size of `long double', as computed by sizeof. */
#undef SIZEOF_LONG_DOUBLE

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
#undef STACK_DIRECTION

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS

/* Define if symbols are underscored. */
#undef SYMBOL_UNDERSCORE

/* Define this if you are using Purify and want to suppress spurious messages.
   */
#undef USING_PURIFY

/* Version number of package */
#undef VERSION

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  undef WORDS_BIGENDIAN
# endif
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
#undef size_t


#ifdef HAVE_HIDDEN_VISIBILITY_ATTRIBUTE
#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name) .hidden name
#else
#define FFI_HIDDEN __attribute__ ((visibility ("hidden")))
#endif
#else
#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name)
#else
#define FFI_HIDDEN
#endif
#endif

