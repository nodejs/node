/* ====================================================================
 * Copyright (c) 2005 The OpenSSL Project. Rights for redistribution
 * and usage in source and binary forms are granted according to the
 * OpenSSL license.
 */

#include <stdio.h>
#if defined(__DECC)
# include <c_asm.h>
# pragma __nostandard
#endif

#include "e_os.h"

#if !defined(POINTER_TO_FUNCTION_IS_POINTER_TO_1ST_INSTRUCTION)
# if	(defined(__sun) && (defined(__sparc) || defined(__sparcv9)))	|| \
	(defined(__sgi) && (defined(__mips) || defined(mips)))		|| \
	(defined(__osf__) && defined(__alpha))				|| \
	(defined(__linux) && (defined(__arm) || defined(__arm__)))	|| \
	(defined(__i386) || defined(__i386__))				|| \
	(defined(__x86_64) || defined(__x86_64__))			|| \
	(defined(vax) || defined(__vax__))
#  define POINTER_TO_FUNCTION_IS_POINTER_TO_1ST_INSTRUCTION
# endif
#endif

#if defined(__xlC__) && __xlC__>=0x600 && (defined(_POWER) || defined(_ARCH_PPC))
static void *instruction_pointer_xlc(void);
# pragma mc_func instruction_pointer_xlc {\
	"7c0802a6"	/* mflr	r0  */	\
	"48000005"	/* bl	$+4 */	\
	"7c6802a6"	/* mflr	r3  */	\
	"7c0803a6"	/* mtlr	r0  */	}
# pragma reg_killed_by instruction_pointer_xlc gr0 gr3
# define INSTRUCTION_POINTER_IMPLEMENTED(ret) (ret=instruction_pointer_xlc());
#endif

#ifdef FIPS_START
#define FIPS_ref_point FIPS_text_start
/* Some compilers put string literals into a separate segment. As we
 * are mostly interested to hash AES tables in .rodata, we declare
 * reference points accordingly. In case you wonder, the values are
 * big-endian encoded variable names, just to prevent these arrays
 * from being merged by linker. */
const unsigned int FIPS_rodata_start[]=
	{ 0x46495053, 0x5f726f64, 0x6174615f, 0x73746172 };
#else
#define FIPS_ref_point FIPS_text_end
const unsigned int FIPS_rodata_end[]=
	{ 0x46495053, 0x5f726f64, 0x6174615f, 0x656e645b };
#endif

/*
 * I declare reference function as static in order to avoid certain
 * pitfalls in -dynamic linker behaviour...
 */
static void *instruction_pointer(void)
{ void *ret=NULL;
/* These are ABI-neutral CPU-specific snippets. ABI-neutrality means
 * that they are designed to work under any OS running on particular
 * CPU, which is why you don't find any #ifdef THIS_OR_THAT_OS in
 * this function. */
#if	defined(INSTRUCTION_POINTER_IMPLEMENTED)
    INSTRUCTION_POINTER_IMPLEMENTED(ret);
#elif	defined(__GNUC__) && __GNUC__>=2
# if	defined(__alpha) || defined(__alpha__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    __asm __volatile (	"br	%0,1f\n1:" : "=r"(ret) );
# elif	defined(__i386) || defined(__i386__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    __asm __volatile (	"call 1f\n1:	popl %0" : "=r"(ret) );
    ret = (void *)((size_t)ret&~3UL); /* align for better performance */
# elif	defined(__ia64) || defined(__ia64__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    __asm __volatile (	"mov	%0=ip" : "=r"(ret) );
# elif	defined(__hppa) || defined(__hppa__) || defined(__pa_risc)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    __asm __volatile (	"blr	%%r0,%0\n\tnop" : "=r"(ret) );
    ret = (void *)((size_t)ret&~3UL); /* mask privilege level */
# elif	defined(__mips) || defined(__mips__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    void *scratch;
    __asm __volatile (	"move	%1,$31\n\t"	/* save ra */
			"bal	.+8; nop\n\t"
			"move	%0,$31\n\t"
			"move	$31,%1"		/* restore ra */
			: "=r"(ret),"=r"(scratch) );
# elif	defined(__ppc__) || defined(__powerpc) || defined(__powerpc__) || \
	defined(__POWERPC__) || defined(_POWER) || defined(__PPC__) || \
	defined(__PPC64__) || defined(__powerpc64__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    void *scratch;
    __asm __volatile (	"mfspr	%1,8\n\t"	/* save lr */
			"bl	$+4\n\t"
			"mfspr	%0,8\n\t"	/* mflr ret */
			"mtspr	8,%1"		/* restore lr */
			: "=r"(ret),"=r"(scratch) );
# elif	defined(__s390__) || defined(__s390x__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    __asm __volatile (	"bras	%0,1f\n1:" : "=r"(ret) );
    ret = (void *)((size_t)ret&~3UL);
# elif	defined(__sparc) || defined(__sparc__) || defined(__sparcv9)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    void *scratch;
    __asm __volatile (	"mov	%%o7,%1\n\t"
			"call	.+8; nop\n\t"
		  	"mov	%%o7,%0\n\t"
			"mov	%1,%%o7"
			: "=r"(ret),"=r"(scratch) );
# elif	defined(__x86_64) || defined(__x86_64__)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    __asm __volatile (	"leaq	0(%%rip),%0" : "=r"(ret) );
    ret = (void *)((size_t)ret&~3UL); /* align for better performance */
# endif
#elif	defined(__DECC) && defined(__alpha)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    ret = (void *)(size_t)asm("br %v0,1f\n1:");
#elif   defined(_MSC_VER) && defined(_M_IX86)
#   define INSTRUCTION_POINTER_IMPLEMENTED
    void *scratch;
    _asm {
            call    self
    self:   pop     eax
            mov     scratch,eax
         }
    ret = (void *)((size_t)scratch&~3UL);
#endif
  return ret;
}

/*
 * This function returns pointer to an instruction in the vicinity of
 * its entry point, but not outside this object module. This guarantees
 * that sequestered code is covered...
 */
void *FIPS_ref_point()
{
#if	defined(INSTRUCTION_POINTER_IMPLEMENTED)
    return instruction_pointer();
/* Below we essentially cover vendor compilers which do not support
 * inline assembler... */
#elif	defined(_AIX)
    struct { void *ip,*gp,*env; } *p = (void *)instruction_pointer;
    return p->ip;
#elif	defined(_HPUX_SOURCE)
# if	defined(__hppa) || defined(__hppa__)
    struct { void *i[4]; } *p = (void *)FIPS_ref_point;

    if (sizeof(p) == 8)	/* 64-bit */
	return p->i[2];
    else if ((size_t)p & 2)
    {	p = (void *)((size_t)p&~3UL);
	return p->i[0];
    }
    else
	return (void *)p;
# elif	defined(__ia64) || defined(__ia64__)
    struct { unsigned long long ip,gp; } *p=(void *)instruction_pointer;
    return (void *)(size_t)p->ip;
# endif
#elif	(defined(__VMS) || defined(VMS)) && !(defined(vax) || defined(__vax__))
    /* applies to both alpha and ia64 */
    struct { unsigned __int64 opaque,ip; } *p=(void *)instruction_pointer;
    return (void *)(size_t)p->ip;
#elif	defined(__VOS__)
    /* applies to both pa-risc and ia32 */
    struct { void *dp,*ip,*gp; } *p = (void *)instruction_pointer;
    return p->ip;
#elif	defined(_WIN32)
# if	defined(_WIN64) && defined(_M_IA64)
    struct { void *ip,*gp; } *p = (void *)FIPS_ref_point;
    return p->ip;
# else
    return (void *)FIPS_ref_point;
# endif
/*
 * In case you wonder why there is no #ifdef __linux. All Linux targets
 * are GCC-based and therefore are covered by instruction_pointer above
 * [well, some are covered by by the one below]...
 */ 
#elif	defined(POINTER_TO_FUNCTION_IS_POINTER_TO_1ST_INSTRUCTION)
    return (void *)instruction_pointer;
#else
    return NULL;
#endif
}
