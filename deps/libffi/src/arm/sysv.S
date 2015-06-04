/* -----------------------------------------------------------------------
   sysv.S - Copyright (c) 1998, 2008, 2011 Red Hat, Inc.
	    Copyright (c) 2011 Plausible Labs Cooperative, Inc.
   
   ARM Foreign Function Interface 

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
   DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

#define LIBFFI_ASM	
#include <fficonfig.h>
#include <ffi.h>
#ifdef HAVE_MACHINE_ASM_H
#include <machine/asm.h>
#else
#ifdef __USER_LABEL_PREFIX__
#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

/* Use the right prefix for global labels.  */
#define CNAME(x) CONCAT1 (__USER_LABEL_PREFIX__, x)
#else
#define CNAME(x) x
#endif
#ifdef __APPLE__
#define ENTRY(x) .globl _##x; _##x:
#else
#define ENTRY(x) .globl CNAME(x); .type CNAME(x),%function; CNAME(x):
#endif /* __APPLE__ */
#endif

#ifdef __ELF__
#define LSYM(x) .x
#else
#define LSYM(x) x
#endif

/* Use the SOFTFP return value ABI on Mac OS X, as per the iOS ABI
  Function Call Guide */
#ifdef __APPLE__
#define __SOFTFP__
#endif

/* We need a better way of testing for this, but for now, this is all 
   we can do.  */
@ This selects the minimum architecture level required.
#define __ARM_ARCH__ 3

#if defined(__ARM_ARCH_4__) || defined(__ARM_ARCH_4T__)
# undef __ARM_ARCH__
# define __ARM_ARCH__ 4
#endif
        
#if defined(__ARM_ARCH_5__) || defined(__ARM_ARCH_5T__) \
	|| defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) \
	|| defined(__ARM_ARCH_5TEJ__)
# undef __ARM_ARCH__
# define __ARM_ARCH__ 5
#endif

#if defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
        || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
        || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) \
	|| defined(__ARM_ARCH_6M__)
# undef __ARM_ARCH__
# define __ARM_ARCH__ 6
#endif

#if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) \
        || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) \
	|| defined(__ARM_ARCH_7EM__)
# undef __ARM_ARCH__
# define __ARM_ARCH__ 7
#endif

#if __ARM_ARCH__ >= 5
# define call_reg(x)	blx	x
#elif defined (__ARM_ARCH_4T__)
# define call_reg(x)	mov	lr, pc ; bx	x
# if defined(__thumb__) || defined(__THUMB_INTERWORK__)
#  define __INTERWORKING__
# endif
#else
# define call_reg(x)	mov	lr, pc ; mov	pc, x
#endif

/* Conditionally compile unwinder directives.  */
#ifdef __ARM_EABI__
#define UNWIND
#else
#define UNWIND @
#endif	

	
#if defined(__thumb__) && !defined(__THUMB_INTERWORK__)
.macro	ARM_FUNC_START name
	.text
	.align 0
	.thumb
	.thumb_func
#ifdef __APPLE__
	ENTRY($0)
#else
	ENTRY(\name)
#endif
	bx	pc
	nop
	.arm
	UNWIND .fnstart
/* A hook to tell gdb that we've switched to ARM mode.  Also used to call
   directly from other local arm routines.  */
#ifdef __APPLE__
_L__$0:
#else
_L__\name:
#endif
.endm
#else
.macro	ARM_FUNC_START name
	.text
	.align 0
	.arm
#ifdef __APPLE__
	ENTRY($0)
#else
	ENTRY(\name)
#endif
	UNWIND .fnstart
.endm
#endif

.macro	RETLDM	regs=, cond=, dirn=ia
#if defined (__INTERWORKING__)
	.ifc "\regs",""
	ldr\cond	lr, [sp], #4
	.else
	ldm\cond\dirn	sp!, {\regs, lr}
	.endif
	bx\cond	lr
#else
	.ifc "\regs",""
	ldr\cond	pc, [sp], #4
	.else
	ldm\cond\dirn	sp!, {\regs, pc}
	.endif
#endif
.endm

	@ r0:   ffi_prep_args
	@ r1:   &ecif
	@ r2:   cif->bytes
	@ r3:   fig->flags
	@ sp+0: ecif.rvalue

	@ This assumes we are using gas.
ARM_FUNC_START ffi_call_SYSV
	@ Save registers
        stmfd	sp!, {r0-r3, fp, lr}
	UNWIND .save	{r0-r3, fp, lr}
	mov	fp, sp

	UNWIND .setfp	fp, sp

	@ Make room for all of the new args.
	sub	sp, fp, r2

	@ Place all of the ffi_prep_args in position
	mov	r0, sp
	@     r1 already set

	@ Call ffi_prep_args(stack, &ecif)
	bl	CNAME(ffi_prep_args)

	@ move first 4 parameters in registers
	ldmia	sp, {r0-r3}

	@ and adjust stack
	sub	lr, fp, sp	@ cif->bytes == fp - sp
	ldr	ip, [fp]	@ load fn() in advance
	cmp	lr, #16
	movhs	lr, #16
	add	sp, sp, lr

	@ call (fn) (...)
	call_reg(ip)
	
	@ Remove the space we pushed for the args
	mov	sp, fp

	@ Load r2 with the pointer to storage for the return value
	ldr	r2, [sp, #24]

	@ Load r3 with the return type code 
	ldr	r3, [sp, #12]

	@ If the return value pointer is NULL, assume no return value.
	cmp	r2, #0
	beq	LSYM(Lepilogue)

@ return INT
	cmp	r3, #FFI_TYPE_INT
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	cmpne	r3, #FFI_TYPE_FLOAT
#endif
	streq	r0, [r2]
	beq	LSYM(Lepilogue)

	@ return INT64
	cmp	r3, #FFI_TYPE_SINT64
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	cmpne	r3, #FFI_TYPE_DOUBLE
#endif
	stmeqia	r2, {r0, r1}

#if !defined(__SOFTFP__) && !defined(__ARM_EABI__)
	beq	LSYM(Lepilogue)

@ return FLOAT
	cmp	r3, #FFI_TYPE_FLOAT
	stfeqs	f0, [r2]
	beq	LSYM(Lepilogue)

@ return DOUBLE or LONGDOUBLE
	cmp	r3, #FFI_TYPE_DOUBLE
	stfeqd	f0, [r2]
#endif

LSYM(Lepilogue):
#if defined (__INTERWORKING__)
	ldmia   sp!, {r0-r3,fp, lr}
	bx	lr
#else
	ldmia   sp!, {r0-r3,fp, pc}
#endif

.ffi_call_SYSV_end:
	UNWIND .fnend
#ifdef __ELF__
        .size    CNAME(ffi_call_SYSV),.ffi_call_SYSV_end-CNAME(ffi_call_SYSV)
#endif


/*
	unsigned int FFI_HIDDEN
	ffi_closure_SYSV_inner (closure, respp, args)
	     ffi_closure *closure;
	     void **respp;
  	     void *args;
*/

ARM_FUNC_START ffi_closure_SYSV
	UNWIND .pad #16
	add	ip, sp, #16
	stmfd	sp!, {ip, lr}
	UNWIND .save	{r0, lr}
	add	r2, sp, #8
	UNWIND .pad #16
	sub	sp, sp, #16
	str	sp, [sp, #8]
	add	r1, sp, #8
	bl	CNAME(ffi_closure_SYSV_inner)
	cmp	r0, #FFI_TYPE_INT
	beq	.Lretint

	cmp	r0, #FFI_TYPE_FLOAT
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	beq	.Lretint
#else
	beq	.Lretfloat
#endif

	cmp	r0, #FFI_TYPE_DOUBLE
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	beq	.Lretlonglong
#else
	beq	.Lretdouble
#endif

	cmp	r0, #FFI_TYPE_LONGDOUBLE
#if defined(__SOFTFP__) || defined(__ARM_EABI__)
	beq	.Lretlonglong
#else
	beq	.Lretlongdouble
#endif

	cmp	r0, #FFI_TYPE_SINT64
	beq	.Lretlonglong
.Lclosure_epilogue:
	add	sp, sp, #16
	ldmfd	sp, {sp, pc}
.Lretint:
	ldr	r0, [sp]
	b	.Lclosure_epilogue
.Lretlonglong:
	ldr	r0, [sp]
	ldr	r1, [sp, #4]
	b	.Lclosure_epilogue

#if !defined(__SOFTFP__) && !defined(__ARM_EABI__)
.Lretfloat:
	ldfs	f0, [sp]
	b	.Lclosure_epilogue
.Lretdouble:
	ldfd	f0, [sp]
	b	.Lclosure_epilogue
.Lretlongdouble:
	ldfd	f0, [sp]
	b	.Lclosure_epilogue
#endif

.ffi_closure_SYSV_end:
	UNWIND .fnend
#ifdef __ELF__
        .size    CNAME(ffi_closure_SYSV),.ffi_closure_SYSV_end-CNAME(ffi_closure_SYSV)
#endif


/* Below are VFP hard-float ABI call and closure implementations.
   Add VFP FPU directive here. This is only compiled into the library
   under EABI.  */
#ifdef __ARM_EABI__
	.fpu	vfp

	@ r0:   fn
	@ r1:   &ecif
	@ r2:   cif->bytes
	@ r3:   fig->flags
	@ sp+0: ecif.rvalue

ARM_FUNC_START ffi_call_VFP
	@ Save registers
        stmfd	sp!, {r0-r3, fp, lr}
	UNWIND .save	{r0-r3, fp, lr}
	mov	fp, sp
	UNWIND .setfp	fp, sp

	@ Make room for all of the new args.
	sub	sp, sp, r2

	@ Make room for loading VFP args
	sub	sp, sp, #64

	@ Place all of the ffi_prep_args in position
	mov	r0, sp
	@     r1 already set
	sub	r2, fp, #64   @ VFP scratch space

	@ Call ffi_prep_args(stack, &ecif, vfp_space)
	bl	CNAME(ffi_prep_args)

	@ Load VFP register args if needed
	cmp	r0, #0
	beq	LSYM(Lbase_args)

	@ Load only d0 if possible
	cmp	r0, #3
	sub	ip, fp, #64
	flddle	d0, [ip]
	fldmiadgt	ip, {d0-d7}

LSYM(Lbase_args):
	@ move first 4 parameters in registers
	ldmia	sp, {r0-r3}

	@ and adjust stack
	sub	lr, ip, sp	@ cif->bytes == (fp - 64) - sp
	ldr	ip, [fp]	@ load fn() in advance
        cmp	lr, #16
	movhs	lr, #16
        add	sp, sp, lr

	@ call (fn) (...)
	call_reg(ip)

	@ Remove the space we pushed for the args
	mov	sp, fp

	@ Load r2 with the pointer to storage for
	@ the return value
	ldr	r2, [sp, #24]

	@ Load r3 with the return type code 
	ldr	r3, [sp, #12]

	@ If the return value pointer is NULL,
	@ assume no return value.
	cmp	r2, #0
	beq	LSYM(Lepilogue_vfp)

	cmp	r3, #FFI_TYPE_INT
	streq	r0, [r2]
	beq	LSYM(Lepilogue_vfp)

	cmp	r3, #FFI_TYPE_SINT64
	stmeqia	r2, {r0, r1}
	beq	LSYM(Lepilogue_vfp)

	cmp	r3, #FFI_TYPE_FLOAT
	fstseq	s0, [r2]
	beq	LSYM(Lepilogue_vfp)
	
	cmp	r3, #FFI_TYPE_DOUBLE
	fstdeq	d0, [r2]
	beq	LSYM(Lepilogue_vfp)

	cmp	r3, #FFI_TYPE_STRUCT_VFP_FLOAT
	cmpne	r3, #FFI_TYPE_STRUCT_VFP_DOUBLE
	fstmiadeq	r2, {d0-d3}

LSYM(Lepilogue_vfp):
	RETLDM	"r0-r3,fp"

.ffi_call_VFP_end:
	UNWIND .fnend
        .size    CNAME(ffi_call_VFP),.ffi_call_VFP_end-CNAME(ffi_call_VFP)


ARM_FUNC_START ffi_closure_VFP
	fstmfdd	sp!, {d0-d7}
	@ r0-r3, then d0-d7
	UNWIND .pad #80
	add	ip, sp, #80
	stmfd	sp!, {ip, lr}
	UNWIND .save	{r0, lr}
	add	r2, sp, #72
	add	r3, sp, #8
	UNWIND .pad #72
	sub	sp, sp, #72
	str	sp, [sp, #64]
	add	r1, sp, #64
	bl	CNAME(ffi_closure_SYSV_inner)

	cmp	r0, #FFI_TYPE_INT
	beq	.Lretint_vfp

	cmp	r0, #FFI_TYPE_FLOAT
	beq	.Lretfloat_vfp

	cmp	r0, #FFI_TYPE_DOUBLE
	cmpne	r0, #FFI_TYPE_LONGDOUBLE
	beq	.Lretdouble_vfp

	cmp	r0, #FFI_TYPE_SINT64
	beq	.Lretlonglong_vfp

	cmp	r0, #FFI_TYPE_STRUCT_VFP_FLOAT
	beq	.Lretfloat_struct_vfp

	cmp	r0, #FFI_TYPE_STRUCT_VFP_DOUBLE
	beq	.Lretdouble_struct_vfp
	
.Lclosure_epilogue_vfp:
	add	sp, sp, #72
	ldmfd	sp, {sp, pc}

.Lretfloat_vfp:
	flds	s0, [sp]
	b	.Lclosure_epilogue_vfp
.Lretdouble_vfp:
	fldd	d0, [sp]
	b	.Lclosure_epilogue_vfp
.Lretint_vfp:
	ldr	r0, [sp]
	b	.Lclosure_epilogue_vfp
.Lretlonglong_vfp:
	ldmia	sp, {r0, r1}
	b	.Lclosure_epilogue_vfp
.Lretfloat_struct_vfp:
	fldmiad	sp, {d0-d1}
	b	.Lclosure_epilogue_vfp
.Lretdouble_struct_vfp:
	fldmiad	sp, {d0-d3}
	b	.Lclosure_epilogue_vfp

.ffi_closure_VFP_end:
	UNWIND .fnend
        .size    CNAME(ffi_closure_VFP),.ffi_closure_VFP_end-CNAME(ffi_closure_VFP)
#endif

ENTRY(ffi_arm_trampoline)
	stmfd sp!, {r0-r3}
	ldr r0, [pc]
	ldr pc, [pc]

#if defined __ELF__ && defined __linux__
	.section	.note.GNU-stack,"",%progbits
#endif
