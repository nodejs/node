/* -----------------------------------------------------------------------
   ffi_powerpc.h - Copyright (C) 2013 IBM
                   Copyright (C) 2011 Anthony Green
                   Copyright (C) 2011 Kyle Moffett
                   Copyright (C) 2008 Red Hat, Inc
                   Copyright (C) 2007, 2008 Free Software Foundation, Inc
                   Copyright (c) 1998 Geoffrey Keating

   PowerPC Foreign Function Interface

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   ``Software''), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be included
   in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------- */

/*
 * Closure jump table indexes
 */

/* Common to ppc32 and ppc64 */
#define PPC_LD_NONE		0
#define PPC_LD_R3		1
#define PPC_LD_R3R4		2
#define PPC_LD_F32		3
#define PPC_LD_F64		4
#define PPC_LD_F128		5
#define PPC_LD_U8		6
#define PPC_LD_S8		7
#define PPC_LD_U16		8
#define PPC_LD_S16		9

#ifndef POWERPC64

#define PPC_LD_U32		PPC_LD_R3
#define PPC_LD_S32		PPC_LD_R3
#define PPC_LD_PTR		PPC_LD_R3
#define PPC_LD_I64		PPC_LD_R3R4

/* Needed for soft-float long-double. */
#define PPC32_LD_R3R6		10

/* Needed for FFI_SYSV small structure returns.  */
#ifdef __LITTLE_ENDIAN__
#define PPC32_SYSV_LD_STRUCT_3	PPC32_LD_R3
#define PPC32_SYSV_LD_STRUCT_5	PPC32_LD_R3R4
#define PPC32_SYSV_LD_STRUCT_6	PPC32_LD_R3R4
#define PPC32_SYSV_LD_STRUCT_7	PPC32_LD_R3R4
#else
#define PPC32_SYSV_LD_STRUCT_3	11
#define PPC32_SYSV_LD_STRUCT_5	12
#define PPC32_SYSV_LD_STRUCT_6	13
#define PPC32_SYSV_LD_STRUCT_7	14
#endif

#else /* POWERPC64 */

#define PPC_LD_U32		10
#define PPC_LD_S32		11
#define PPC_LD_PTR		PPC_LD_R3
#define PPC_LD_I64		PPC_LD_R3

/* Used by ELFv2 for homogenous structure returns.  */
#define PPC64_LD_VECTOR		12
#define PPC64_LD_VECTOR_HOMOG	13
#define PPC64_LD_FLOAT_HOMOG	14
#define PPC64_LD_DOUBLE_HOMOG	15
#ifdef __LITTLE_ENDIAN__
#define PPC64_LD_STRUCT_3	PPC_LD_U32
#define PPC64_LD_STRUCT_5	PPC_LD_I64
#define PPC64_LD_STRUCT_6	PPC_LD_I64
#define PPC64_LD_STRUCT_7	PPC_LD_I64
#else
#define PPC64_LD_STRUCT_3	16
#define PPC64_LD_STRUCT_5	17
#define PPC64_LD_STRUCT_6	18
#define PPC64_LD_STRUCT_7	19
#endif

#endif /* POWERPC64 */

#ifndef LIBFFI_ASM

enum {
  /* The assembly depends on these exact flags.  */
  /* These go in cr7 */
  FLAG_RETURNS_SMST     = 1 << (31-31), /* Used for FFI_SYSV small structs.  */
  FLAG_RETURNS_NOTHING  = 1 << (31-30),
  FLAG_RETURNS_FP       = 1 << (31-29),
  FLAG_RETURNS_VEC      = 1 << (31-28),

  /* These go in cr6 */
  FLAG_RETURNS_64BITS   = 1 << (31-27),
  FLAG_RETURNS_128BITS  = 1 << (31-26),

  FLAG_COMPAT           = 1 << (31- 8), /* Not used by assembly */

  /* These go in cr1 */
  FLAG_ARG_NEEDS_COPY   = 1 << (31- 7), /* Used by sysv code */
  FLAG_ARG_NEEDS_PSAVE  = FLAG_ARG_NEEDS_COPY, /* Used by linux64 code */
  FLAG_FP_ARGUMENTS     = 1 << (31- 6), /* cr1.eq; specified by ABI */
  FLAG_4_GPR_ARGUMENTS  = 1 << (31- 5),
  FLAG_RETVAL_REFERENCE = 1 << (31- 4),
  FLAG_VEC_ARGUMENTS    = 1 << (31- 3),
};

typedef union
{
  float f;
  double d;
} ffi_dblfl;

#if defined(__FLOAT128_TYPE__) && defined(__HAVE_FLOAT128)
typedef _Float128 float128;
#elif defined(__FLOAT128__)
typedef __float128 float128;
#else
typedef char float128[16] __attribute__((aligned(16)));
#endif

void FFI_HIDDEN ffi_closure_SYSV (void);
void FFI_HIDDEN ffi_go_closure_sysv (void);
void FFI_HIDDEN ffi_call_SYSV(extended_cif *, void (*)(void), void *,
			      unsigned, void *, int);

void FFI_HIDDEN ffi_prep_types_sysv (ffi_abi);
ffi_status FFI_HIDDEN ffi_prep_cif_sysv (ffi_cif *);
ffi_status FFI_HIDDEN ffi_prep_closure_loc_sysv (ffi_closure *,
						 ffi_cif *,
						 void (*) (ffi_cif *, void *,
							   void **, void *),
						 void *, void *);
int FFI_HIDDEN ffi_closure_helper_SYSV (ffi_cif *,
					void (*) (ffi_cif *, void *,
						  void **, void *),
					void *, void *, unsigned long *,
					ffi_dblfl *, unsigned long *);

void FFI_HIDDEN ffi_call_LINUX64(extended_cif *, void (*) (void), void *,
				 unsigned long, void *, long);
void FFI_HIDDEN ffi_closure_LINUX64 (void);
void FFI_HIDDEN ffi_go_closure_linux64 (void);

void FFI_HIDDEN ffi_prep_types_linux64 (ffi_abi);
ffi_status FFI_HIDDEN ffi_prep_cif_linux64 (ffi_cif *);
ffi_status FFI_HIDDEN ffi_prep_cif_linux64_var (ffi_cif *, unsigned int,
						unsigned int);
void FFI_HIDDEN ffi_prep_args64 (extended_cif *, unsigned long *const);
ffi_status FFI_HIDDEN ffi_prep_closure_loc_linux64 (ffi_closure *, ffi_cif *,
						    void (*) (ffi_cif *, void *,
							      void **, void *),
						    void *, void *);
int FFI_HIDDEN ffi_closure_helper_LINUX64 (ffi_cif *,
					   void (*) (ffi_cif *, void *,
						     void **, void *),
					   void *, void *,
					   unsigned long *, ffi_dblfl *,
					   float128 *);

#endif /* !LIBFFI_ASM */
