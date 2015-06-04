/* -----------------------------------------------------------------------
   ffi.c - Copyright (C) 2011 Anthony Green
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

#include <ffi.h>
#include <ffi_common.h>

#include <stdlib.h>
#include <stdio.h>


extern void ffi_closure_SYSV (void);
extern void FFI_HIDDEN ffi_closure_LINUX64 (void);

enum {
  /* The assembly depends on these exact flags.  */
  FLAG_RETURNS_SMST	= 1 << (31-31), /* Used for FFI_SYSV small structs.  */
  FLAG_RETURNS_NOTHING  = 1 << (31-30), /* These go in cr7 */
#ifndef __NO_FPRS__
  FLAG_RETURNS_FP       = 1 << (31-29),
#endif
  FLAG_RETURNS_64BITS   = 1 << (31-28),

  FLAG_RETURNS_128BITS  = 1 << (31-27), /* cr6  */

  FLAG_SYSV_SMST_R4     = 1 << (31-26), /* use r4 for FFI_SYSV 8 byte
					   structs.  */
  FLAG_SYSV_SMST_R3     = 1 << (31-25), /* use r3 for FFI_SYSV 4 byte
					   structs.  */

  FLAG_ARG_NEEDS_COPY   = 1 << (31- 7),
#ifndef __NO_FPRS__
  FLAG_FP_ARGUMENTS     = 1 << (31- 6), /* cr1.eq; specified by ABI */
#endif
  FLAG_4_GPR_ARGUMENTS  = 1 << (31- 5),
  FLAG_RETVAL_REFERENCE = 1 << (31- 4)
};

/* About the SYSV ABI.  */
#define ASM_NEEDS_REGISTERS 4
#define NUM_GPR_ARG_REGISTERS 8
#ifndef __NO_FPRS__
# define NUM_FPR_ARG_REGISTERS 8
#endif

/* ffi_prep_args_SYSV is called by the assembly routine once stack space
   has been allocated for the function's arguments.

   The stack layout we want looks like this:

   |   Return address from ffi_call_SYSV 4bytes	|	higher addresses
   |--------------------------------------------|
   |   Previous backchain pointer	4	|       stack pointer here
   |--------------------------------------------|<+ <<<	on entry to
   |   Saved r28-r31			4*4	| |	ffi_call_SYSV
   |--------------------------------------------| |
   |   GPR registers r3-r10		8*4	| |	ffi_call_SYSV
   |--------------------------------------------| |
   |   FPR registers f1-f8 (optional)	8*8	| |
   |--------------------------------------------| |	stack	|
   |   Space for copied structures		| |	grows	|
   |--------------------------------------------| |	down    V
   |   Parameters that didn't fit in registers  | |
   |--------------------------------------------| |	lower addresses
   |   Space for callee's LR		4	| |
   |--------------------------------------------| |	stack pointer here
   |   Current backchain pointer	4	|-/	during
   |--------------------------------------------|   <<<	ffi_call_SYSV

*/

void
ffi_prep_args_SYSV (extended_cif *ecif, unsigned *const stack)
{
  const unsigned bytes = ecif->cif->bytes;
  const unsigned flags = ecif->cif->flags;

  typedef union {
    char *c;
    unsigned *u;
    long long *ll;
    float *f;
    double *d;
  } valp;

  /* 'stacktop' points at the previous backchain pointer.  */
  valp stacktop;

  /* 'gpr_base' points at the space for gpr3, and grows upwards as
     we use GPR registers.  */
  valp gpr_base;
  int intarg_count;

#ifndef __NO_FPRS__
  /* 'fpr_base' points at the space for fpr1, and grows upwards as
     we use FPR registers.  */
  valp fpr_base;
  int fparg_count;
#endif

  /* 'copy_space' grows down as we put structures in it.  It should
     stay 16-byte aligned.  */
  valp copy_space;

  /* 'next_arg' grows up as we put parameters in it.  */
  valp next_arg;

  int i;
  ffi_type **ptr;
  union {
    void **v;
    char **c;
    signed char **sc;
    unsigned char **uc;
    signed short **ss;
    unsigned short **us;
    unsigned int **ui;
    long long **ll;
    float **f;
    double **d;
  } p_argv;
  size_t struct_copy_size;
  unsigned gprvalue;

  stacktop.c = (char *) stack + bytes;
  gpr_base.u = stacktop.u - ASM_NEEDS_REGISTERS - NUM_GPR_ARG_REGISTERS;
  intarg_count = 0;
#ifndef __NO_FPRS__
  double double_tmp;
  fpr_base.d = gpr_base.d - NUM_FPR_ARG_REGISTERS;
  fparg_count = 0;
  copy_space.c = ((flags & FLAG_FP_ARGUMENTS) ? fpr_base.c : gpr_base.c);
#else
  copy_space.c = gpr_base.c;
#endif
  next_arg.u = stack + 2;

  /* Check that everything starts aligned properly.  */
  FFI_ASSERT (((unsigned long) (char *) stack & 0xF) == 0);
  FFI_ASSERT (((unsigned long) copy_space.c & 0xF) == 0);
  FFI_ASSERT (((unsigned long) stacktop.c & 0xF) == 0);
  FFI_ASSERT ((bytes & 0xF) == 0);
  FFI_ASSERT (copy_space.c >= next_arg.c);

  /* Deal with return values that are actually pass-by-reference.  */
  if (flags & FLAG_RETVAL_REFERENCE)
    {
      *gpr_base.u++ = (unsigned long) (char *) ecif->rvalue;
      intarg_count++;
    }

  /* Now for the arguments.  */
  p_argv.v = ecif->avalue;
  for (ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, ptr++, p_argv.v++)
    {
      unsigned short typenum = (*ptr)->type;

      /* We may need to handle some values depending on ABI */
      if (ecif->cif->abi == FFI_LINUX_SOFT_FLOAT) {
		if (typenum == FFI_TYPE_FLOAT)
			typenum = FFI_TYPE_UINT32;
		if (typenum == FFI_TYPE_DOUBLE)
			typenum = FFI_TYPE_UINT64;
		if (typenum == FFI_TYPE_LONGDOUBLE)
			typenum = FFI_TYPE_UINT128;
      } else if (ecif->cif->abi != FFI_LINUX) {
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
		if (typenum == FFI_TYPE_LONGDOUBLE)
			typenum = FFI_TYPE_STRUCT;
#endif
      }

      /* Now test the translated value */
      switch (typenum) {
#ifndef __NO_FPRS__
	case FFI_TYPE_FLOAT:
	  /* With FFI_LINUX_SOFT_FLOAT floats are handled like UINT32.  */
	  double_tmp = **p_argv.f;
	  if (fparg_count >= NUM_FPR_ARG_REGISTERS)
	    {
	      *next_arg.f = (float) double_tmp;
	      next_arg.u += 1;
	      intarg_count++;
	    }
	  else
	    *fpr_base.d++ = double_tmp;
	  fparg_count++;
	  FFI_ASSERT (flags & FLAG_FP_ARGUMENTS);
	  break;

	case FFI_TYPE_DOUBLE:
	  /* With FFI_LINUX_SOFT_FLOAT doubles are handled like UINT64.  */
	  double_tmp = **p_argv.d;

	  if (fparg_count >= NUM_FPR_ARG_REGISTERS)
	    {
	      if (intarg_count >= NUM_GPR_ARG_REGISTERS
		  && intarg_count % 2 != 0)
		{
		  intarg_count++;
		  next_arg.u++;
		}
	      *next_arg.d = double_tmp;
	      next_arg.u += 2;
	    }
	  else
	    *fpr_base.d++ = double_tmp;
	  fparg_count++;
	  FFI_ASSERT (flags & FLAG_FP_ARGUMENTS);
	  break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	case FFI_TYPE_LONGDOUBLE:
	      double_tmp = (*p_argv.d)[0];

	      if (fparg_count >= NUM_FPR_ARG_REGISTERS - 1)
		{
		  if (intarg_count >= NUM_GPR_ARG_REGISTERS
		      && intarg_count % 2 != 0)
		    {
		      intarg_count++;
		      next_arg.u++;
		    }
		  *next_arg.d = double_tmp;
		  next_arg.u += 2;
		  double_tmp = (*p_argv.d)[1];
		  *next_arg.d = double_tmp;
		  next_arg.u += 2;
		}
	      else
		{
		  *fpr_base.d++ = double_tmp;
		  double_tmp = (*p_argv.d)[1];
		  *fpr_base.d++ = double_tmp;
		}

	      fparg_count += 2;
	      FFI_ASSERT (flags & FLAG_FP_ARGUMENTS);
	  break;
#endif
#endif /* have FPRs */

	/*
	 * The soft float ABI for long doubles works like this, a long double
	 * is passed in four consecutive GPRs if available.  A maximum of 2
	 * long doubles can be passed in gprs.  If we do not have 4 GPRs
	 * left, the long double is passed on the stack, 4-byte aligned.
	 */
	case FFI_TYPE_UINT128: {
		unsigned int int_tmp = (*p_argv.ui)[0];
		unsigned int ii;
		if (intarg_count >= NUM_GPR_ARG_REGISTERS - 3) {
			if (intarg_count < NUM_GPR_ARG_REGISTERS)
				intarg_count += NUM_GPR_ARG_REGISTERS - intarg_count;
			*(next_arg.u++) = int_tmp;
			for (ii = 1; ii < 4; ii++) {
				int_tmp = (*p_argv.ui)[ii];
				*(next_arg.u++) = int_tmp;
			}
		} else {
			*(gpr_base.u++) = int_tmp;
			for (ii = 1; ii < 4; ii++) {
				int_tmp = (*p_argv.ui)[ii];
				*(gpr_base.u++) = int_tmp;
			}
		}
		intarg_count += 4;
		break;
	}

	case FFI_TYPE_UINT64:
	case FFI_TYPE_SINT64:
	  if (intarg_count == NUM_GPR_ARG_REGISTERS-1)
	    intarg_count++;
	  if (intarg_count >= NUM_GPR_ARG_REGISTERS)
	    {
	      if (intarg_count % 2 != 0)
		{
		  intarg_count++;
		  next_arg.u++;
		}
	      *next_arg.ll = **p_argv.ll;
	      next_arg.u += 2;
	    }
	  else
	    {
	      /* whoops: abi states only certain register pairs
	       * can be used for passing long long int
	       * specifically (r3,r4), (r5,r6), (r7,r8),
	       * (r9,r10) and if next arg is long long but
	       * not correct starting register of pair then skip
	       * until the proper starting register
	       */
	      if (intarg_count % 2 != 0)
		{
		  intarg_count ++;
		  gpr_base.u++;
		}
	      *gpr_base.ll++ = **p_argv.ll;
	    }
	  intarg_count += 2;
	  break;

	case FFI_TYPE_STRUCT:
	  struct_copy_size = ((*ptr)->size + 15) & ~0xF;
	  copy_space.c -= struct_copy_size;
	  memcpy (copy_space.c, *p_argv.c, (*ptr)->size);

	  gprvalue = (unsigned long) copy_space.c;

	  FFI_ASSERT (copy_space.c > next_arg.c);
	  FFI_ASSERT (flags & FLAG_ARG_NEEDS_COPY);
	  goto putgpr;

	case FFI_TYPE_UINT8:
	  gprvalue = **p_argv.uc;
	  goto putgpr;
	case FFI_TYPE_SINT8:
	  gprvalue = **p_argv.sc;
	  goto putgpr;
	case FFI_TYPE_UINT16:
	  gprvalue = **p_argv.us;
	  goto putgpr;
	case FFI_TYPE_SINT16:
	  gprvalue = **p_argv.ss;
	  goto putgpr;

	case FFI_TYPE_INT:
	case FFI_TYPE_UINT32:
	case FFI_TYPE_SINT32:
	case FFI_TYPE_POINTER:

	  gprvalue = **p_argv.ui;

	putgpr:
	  if (intarg_count >= NUM_GPR_ARG_REGISTERS)
	    *next_arg.u++ = gprvalue;
	  else
	    *gpr_base.u++ = gprvalue;
	  intarg_count++;
	  break;
	}
    }

  /* Check that we didn't overrun the stack...  */
  FFI_ASSERT (copy_space.c >= next_arg.c);
  FFI_ASSERT (gpr_base.u <= stacktop.u - ASM_NEEDS_REGISTERS);
  /* The assert below is testing that the number of integer arguments agrees
     with the number found in ffi_prep_cif_machdep().  However, intarg_count
     is incremeneted whenever we place an FP arg on the stack, so account for
     that before our assert test.  */
#ifndef __NO_FPRS__
  if (fparg_count > NUM_FPR_ARG_REGISTERS)
    intarg_count -= fparg_count - NUM_FPR_ARG_REGISTERS;
  FFI_ASSERT (fpr_base.u
	      <= stacktop.u - ASM_NEEDS_REGISTERS - NUM_GPR_ARG_REGISTERS);
#endif
  FFI_ASSERT (flags & FLAG_4_GPR_ARGUMENTS || intarg_count <= 4);
}

/* About the LINUX64 ABI.  */
enum {
  NUM_GPR_ARG_REGISTERS64 = 8,
  NUM_FPR_ARG_REGISTERS64 = 13
};
enum { ASM_NEEDS_REGISTERS64 = 4 };

/* ffi_prep_args64 is called by the assembly routine once stack space
   has been allocated for the function's arguments.

   The stack layout we want looks like this:

   |   Ret addr from ffi_call_LINUX64	8bytes	|	higher addresses
   |--------------------------------------------|
   |   CR save area			8bytes	|
   |--------------------------------------------|
   |   Previous backchain pointer	8	|	stack pointer here
   |--------------------------------------------|<+ <<<	on entry to
   |   Saved r28-r31			4*8	| |	ffi_call_LINUX64
   |--------------------------------------------| |
   |   GPR registers r3-r10		8*8	| |
   |--------------------------------------------| |
   |   FPR registers f1-f13 (optional)	13*8	| |
   |--------------------------------------------| |
   |   Parameter save area		        | |
   |--------------------------------------------| |
   |   TOC save area			8	| |
   |--------------------------------------------| |	stack	|
   |   Linker doubleword		8	| |	grows	|
   |--------------------------------------------| |	down	V
   |   Compiler doubleword		8	| |
   |--------------------------------------------| |	lower addresses
   |   Space for callee's LR		8	| |
   |--------------------------------------------| |
   |   CR save area			8	| |
   |--------------------------------------------| |	stack pointer here
   |   Current backchain pointer	8	|-/	during
   |--------------------------------------------|   <<<	ffi_call_LINUX64

*/

void FFI_HIDDEN
ffi_prep_args64 (extended_cif *ecif, unsigned long *const stack)
{
  const unsigned long bytes = ecif->cif->bytes;
  const unsigned long flags = ecif->cif->flags;

  typedef union {
    char *c;
    unsigned long *ul;
    float *f;
    double *d;
  } valp;

  /* 'stacktop' points at the previous backchain pointer.  */
  valp stacktop;

  /* 'next_arg' points at the space for gpr3, and grows upwards as
     we use GPR registers, then continues at rest.  */
  valp gpr_base;
  valp gpr_end;
  valp rest;
  valp next_arg;

  /* 'fpr_base' points at the space for fpr3, and grows upwards as
     we use FPR registers.  */
  valp fpr_base;
  int fparg_count;

  int i, words;
  ffi_type **ptr;
  double double_tmp;
  union {
    void **v;
    char **c;
    signed char **sc;
    unsigned char **uc;
    signed short **ss;
    unsigned short **us;
    signed int **si;
    unsigned int **ui;
    unsigned long **ul;
    float **f;
    double **d;
  } p_argv;
  unsigned long gprvalue;

  stacktop.c = (char *) stack + bytes;
  gpr_base.ul = stacktop.ul - ASM_NEEDS_REGISTERS64 - NUM_GPR_ARG_REGISTERS64;
  gpr_end.ul = gpr_base.ul + NUM_GPR_ARG_REGISTERS64;
  rest.ul = stack + 6 + NUM_GPR_ARG_REGISTERS64;
  fpr_base.d = gpr_base.d - NUM_FPR_ARG_REGISTERS64;
  fparg_count = 0;
  next_arg.ul = gpr_base.ul;

  /* Check that everything starts aligned properly.  */
  FFI_ASSERT (((unsigned long) (char *) stack & 0xF) == 0);
  FFI_ASSERT (((unsigned long) stacktop.c & 0xF) == 0);
  FFI_ASSERT ((bytes & 0xF) == 0);

  /* Deal with return values that are actually pass-by-reference.  */
  if (flags & FLAG_RETVAL_REFERENCE)
    *next_arg.ul++ = (unsigned long) (char *) ecif->rvalue;

  /* Now for the arguments.  */
  p_argv.v = ecif->avalue;
  for (ptr = ecif->cif->arg_types, i = ecif->cif->nargs;
       i > 0;
       i--, ptr++, p_argv.v++)
    {
      switch ((*ptr)->type)
	{
	case FFI_TYPE_FLOAT:
	  double_tmp = **p_argv.f;
	  *next_arg.f = (float) double_tmp;
	  if (++next_arg.ul == gpr_end.ul)
	    next_arg.ul = rest.ul;
	  if (fparg_count < NUM_FPR_ARG_REGISTERS64)
	    *fpr_base.d++ = double_tmp;
	  fparg_count++;
	  FFI_ASSERT (flags & FLAG_FP_ARGUMENTS);
	  break;

	case FFI_TYPE_DOUBLE:
	  double_tmp = **p_argv.d;
	  *next_arg.d = double_tmp;
	  if (++next_arg.ul == gpr_end.ul)
	    next_arg.ul = rest.ul;
	  if (fparg_count < NUM_FPR_ARG_REGISTERS64)
	    *fpr_base.d++ = double_tmp;
	  fparg_count++;
	  FFI_ASSERT (flags & FLAG_FP_ARGUMENTS);
	  break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	case FFI_TYPE_LONGDOUBLE:
	  double_tmp = (*p_argv.d)[0];
	  *next_arg.d = double_tmp;
	  if (++next_arg.ul == gpr_end.ul)
	    next_arg.ul = rest.ul;
	  if (fparg_count < NUM_FPR_ARG_REGISTERS64)
	    *fpr_base.d++ = double_tmp;
	  fparg_count++;
	  double_tmp = (*p_argv.d)[1];
	  *next_arg.d = double_tmp;
	  if (++next_arg.ul == gpr_end.ul)
	    next_arg.ul = rest.ul;
	  if (fparg_count < NUM_FPR_ARG_REGISTERS64)
	    *fpr_base.d++ = double_tmp;
	  fparg_count++;
	  FFI_ASSERT (__LDBL_MANT_DIG__ == 106);
	  FFI_ASSERT (flags & FLAG_FP_ARGUMENTS);
	  break;
#endif

	case FFI_TYPE_STRUCT:
	  words = ((*ptr)->size + 7) / 8;
	  if (next_arg.ul >= gpr_base.ul && next_arg.ul + words > gpr_end.ul)
	    {
	      size_t first = gpr_end.c - next_arg.c;
	      memcpy (next_arg.c, *p_argv.c, first);
	      memcpy (rest.c, *p_argv.c + first, (*ptr)->size - first);
	      next_arg.c = rest.c + words * 8 - first;
	    }
	  else
	    {
	      char *where = next_arg.c;

	      /* Structures with size less than eight bytes are passed
		 left-padded.  */
	      if ((*ptr)->size < 8)
		where += 8 - (*ptr)->size;

	      memcpy (where, *p_argv.c, (*ptr)->size);
	      next_arg.ul += words;
	      if (next_arg.ul == gpr_end.ul)
		next_arg.ul = rest.ul;
	    }
	  break;

	case FFI_TYPE_UINT8:
	  gprvalue = **p_argv.uc;
	  goto putgpr;
	case FFI_TYPE_SINT8:
	  gprvalue = **p_argv.sc;
	  goto putgpr;
	case FFI_TYPE_UINT16:
	  gprvalue = **p_argv.us;
	  goto putgpr;
	case FFI_TYPE_SINT16:
	  gprvalue = **p_argv.ss;
	  goto putgpr;
	case FFI_TYPE_UINT32:
	  gprvalue = **p_argv.ui;
	  goto putgpr;
	case FFI_TYPE_INT:
	case FFI_TYPE_SINT32:
	  gprvalue = **p_argv.si;
	  goto putgpr;

	case FFI_TYPE_UINT64:
	case FFI_TYPE_SINT64:
	case FFI_TYPE_POINTER:
	  gprvalue = **p_argv.ul;
	putgpr:
	  *next_arg.ul++ = gprvalue;
	  if (next_arg.ul == gpr_end.ul)
	    next_arg.ul = rest.ul;
	  break;
	}
    }

  FFI_ASSERT (flags & FLAG_4_GPR_ARGUMENTS
	      || (next_arg.ul >= gpr_base.ul
		  && next_arg.ul <= gpr_base.ul + 4));
}



/* Perform machine dependent cif processing */
ffi_status
ffi_prep_cif_machdep (ffi_cif *cif)
{
  /* All this is for the SYSV and LINUX64 ABI.  */
  int i;
  ffi_type **ptr;
  unsigned bytes;
  int fparg_count = 0, intarg_count = 0;
  unsigned flags = 0;
  unsigned struct_copy_size = 0;
  unsigned type = cif->rtype->type;
  unsigned size = cif->rtype->size;

  if (cif->abi != FFI_LINUX64)
    {
      /* All the machine-independent calculation of cif->bytes will be wrong.
	 Redo the calculation for SYSV.  */

      /* Space for the frame pointer, callee's LR, and the asm's temp regs.  */
      bytes = (2 + ASM_NEEDS_REGISTERS) * sizeof (int);

      /* Space for the GPR registers.  */
      bytes += NUM_GPR_ARG_REGISTERS * sizeof (int);
    }
  else
    {
      /* 64-bit ABI.  */

      /* Space for backchain, CR, LR, cc/ld doubleword, TOC and the asm's temp
	 regs.  */
      bytes = (6 + ASM_NEEDS_REGISTERS64) * sizeof (long);

      /* Space for the mandatory parm save area and general registers.  */
      bytes += 2 * NUM_GPR_ARG_REGISTERS64 * sizeof (long);
    }

  /* Return value handling.  The rules for SYSV are as follows:
     - 32-bit (or less) integer values are returned in gpr3;
     - Structures of size <= 4 bytes also returned in gpr3;
     - 64-bit integer values and structures between 5 and 8 bytes are returned
     in gpr3 and gpr4;
     - Single/double FP values are returned in fpr1;
     - Larger structures are allocated space and a pointer is passed as
     the first argument.
     - long doubles (if not equivalent to double) are returned in
     fpr1,fpr2 for Linux and as for large structs for SysV.
     For LINUX64:
     - integer values in gpr3;
     - Structures/Unions by reference;
     - Single/double FP values in fpr1, long double in fpr1,fpr2.
     - soft-float float/doubles are treated as UINT32/UINT64 respectivley.
     - soft-float long doubles are returned in gpr3-gpr6.  */
  /* First translate for softfloat/nonlinux */
  if (cif->abi == FFI_LINUX_SOFT_FLOAT) {
	if (type == FFI_TYPE_FLOAT)
		type = FFI_TYPE_UINT32;
	if (type == FFI_TYPE_DOUBLE)
		type = FFI_TYPE_UINT64;
	if (type == FFI_TYPE_LONGDOUBLE)
		type = FFI_TYPE_UINT128;
  } else if (cif->abi != FFI_LINUX && cif->abi != FFI_LINUX64) {
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	if (type == FFI_TYPE_LONGDOUBLE)
		type = FFI_TYPE_STRUCT;
#endif
  }

  switch (type)
    {
#ifndef __NO_FPRS__
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
    case FFI_TYPE_LONGDOUBLE:
      flags |= FLAG_RETURNS_128BITS;
      /* Fall through.  */
#endif
    case FFI_TYPE_DOUBLE:
      flags |= FLAG_RETURNS_64BITS;
      /* Fall through.  */
    case FFI_TYPE_FLOAT:
      flags |= FLAG_RETURNS_FP;
      break;
#endif

    case FFI_TYPE_UINT128:
      flags |= FLAG_RETURNS_128BITS;
      /* Fall through.  */
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
      flags |= FLAG_RETURNS_64BITS;
      break;

    case FFI_TYPE_STRUCT:
      if (cif->abi == FFI_SYSV)
	{
	  /* The final SYSV ABI says that structures smaller or equal 8 bytes
	     are returned in r3/r4. The FFI_GCC_SYSV ABI instead returns them
	     in memory.  */

	  /* Treat structs with size <= 8 bytes.  */
	  if (size <= 8)
	    {
	      flags |= FLAG_RETURNS_SMST;
	      /* These structs are returned in r3. We pack the type and the
		 precalculated shift value (needed in the sysv.S) into flags.
		 The same applies for the structs returned in r3/r4.  */
	      if (size <= 4)
		{
		  flags |= FLAG_SYSV_SMST_R3;
		  flags |= 8 * (4 - size) << 8;
		  break;
		}
	      /* These structs are returned in r3 and r4. See above.   */
	      if  (size <= 8)
		{
		  flags |= FLAG_SYSV_SMST_R3 | FLAG_SYSV_SMST_R4;
		  flags |= 8 * (8 - size) << 8;
		  break;
		}
	    }
	}

      intarg_count++;
      flags |= FLAG_RETVAL_REFERENCE;
      /* Fall through.  */
    case FFI_TYPE_VOID:
      flags |= FLAG_RETURNS_NOTHING;
      break;

    default:
      /* Returns 32-bit integer, or similar.  Nothing to do here.  */
      break;
    }

  if (cif->abi != FFI_LINUX64)
    /* The first NUM_GPR_ARG_REGISTERS words of integer arguments, and the
       first NUM_FPR_ARG_REGISTERS fp arguments, go in registers; the rest
       goes on the stack.  Structures and long doubles (if not equivalent
       to double) are passed as a pointer to a copy of the structure.
       Stuff on the stack needs to keep proper alignment.  */
    for (ptr = cif->arg_types, i = cif->nargs; i > 0; i--, ptr++)
      {
	unsigned short typenum = (*ptr)->type;

	/* We may need to handle some values depending on ABI */
	if (cif->abi == FFI_LINUX_SOFT_FLOAT) {
		if (typenum == FFI_TYPE_FLOAT)
			typenum = FFI_TYPE_UINT32;
		if (typenum == FFI_TYPE_DOUBLE)
			typenum = FFI_TYPE_UINT64;
		if (typenum == FFI_TYPE_LONGDOUBLE)
			typenum = FFI_TYPE_UINT128;
	} else if (cif->abi != FFI_LINUX && cif->abi != FFI_LINUX64) {
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
		if (typenum == FFI_TYPE_LONGDOUBLE)
			typenum = FFI_TYPE_STRUCT;
#endif
	}

	switch (typenum) {
#ifndef __NO_FPRS__
	  case FFI_TYPE_FLOAT:
	    fparg_count++;
	    /* floating singles are not 8-aligned on stack */
	    break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	  case FFI_TYPE_LONGDOUBLE:
	    fparg_count++;
	    /* Fall thru */
#endif
	  case FFI_TYPE_DOUBLE:
	    fparg_count++;
	    /* If this FP arg is going on the stack, it must be
	       8-byte-aligned.  */
	    if (fparg_count > NUM_FPR_ARG_REGISTERS
		&& intarg_count >= NUM_GPR_ARG_REGISTERS
		&& intarg_count % 2 != 0)
	      intarg_count++;
	    break;
#endif
	  case FFI_TYPE_UINT128:
		/*
		 * A long double in FFI_LINUX_SOFT_FLOAT can use only a set
		 * of four consecutive gprs. If we do not have enough, we
		 * have to adjust the intarg_count value.
		 */
		if (intarg_count >= NUM_GPR_ARG_REGISTERS - 3
				&& intarg_count < NUM_GPR_ARG_REGISTERS)
			intarg_count = NUM_GPR_ARG_REGISTERS;
		intarg_count += 4;
		break;

	  case FFI_TYPE_UINT64:
	  case FFI_TYPE_SINT64:
	    /* 'long long' arguments are passed as two words, but
	       either both words must fit in registers or both go
	       on the stack.  If they go on the stack, they must
	       be 8-byte-aligned.

	       Also, only certain register pairs can be used for
	       passing long long int -- specifically (r3,r4), (r5,r6),
	       (r7,r8), (r9,r10).
	    */
	    if (intarg_count == NUM_GPR_ARG_REGISTERS-1
		|| intarg_count % 2 != 0)
	      intarg_count++;
	    intarg_count += 2;
	    break;

	  case FFI_TYPE_STRUCT:
	    /* We must allocate space for a copy of these to enforce
	       pass-by-value.  Pad the space up to a multiple of 16
	       bytes (the maximum alignment required for anything under
	       the SYSV ABI).  */
	    struct_copy_size += ((*ptr)->size + 15) & ~0xF;
	    /* Fall through (allocate space for the pointer).  */

	  case FFI_TYPE_POINTER:
	  case FFI_TYPE_INT:
	  case FFI_TYPE_UINT32:
	  case FFI_TYPE_SINT32:
	  case FFI_TYPE_UINT16:
	  case FFI_TYPE_SINT16:
	  case FFI_TYPE_UINT8:
	  case FFI_TYPE_SINT8:
	    /* Everything else is passed as a 4-byte word in a GPR, either
	       the object itself or a pointer to it.  */
	    intarg_count++;
	    break;
	  default:
		FFI_ASSERT (0);
	  }
      }
  else
    for (ptr = cif->arg_types, i = cif->nargs; i > 0; i--, ptr++)
      {
	switch ((*ptr)->type)
	  {
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	  case FFI_TYPE_LONGDOUBLE:
	    if (cif->abi == FFI_LINUX_SOFT_FLOAT)
	      intarg_count += 4;
	    else
	      {
		fparg_count += 2;
		intarg_count += 2;
	      }
	    break;
#endif
	  case FFI_TYPE_FLOAT:
	  case FFI_TYPE_DOUBLE:
	    fparg_count++;
	    intarg_count++;
	    break;

	  case FFI_TYPE_STRUCT:
	    intarg_count += ((*ptr)->size + 7) / 8;
	    break;

	  case FFI_TYPE_POINTER:
	  case FFI_TYPE_UINT64:
	  case FFI_TYPE_SINT64:
	  case FFI_TYPE_INT:
	  case FFI_TYPE_UINT32:
	  case FFI_TYPE_SINT32:
	  case FFI_TYPE_UINT16:
	  case FFI_TYPE_SINT16:
	  case FFI_TYPE_UINT8:
	  case FFI_TYPE_SINT8:
	    /* Everything else is passed as a 8-byte word in a GPR, either
	       the object itself or a pointer to it.  */
	    intarg_count++;
	    break;
	  default:
		FFI_ASSERT (0);
	  }
      }

#ifndef __NO_FPRS__
  if (fparg_count != 0)
    flags |= FLAG_FP_ARGUMENTS;
#endif
  if (intarg_count > 4)
    flags |= FLAG_4_GPR_ARGUMENTS;
  if (struct_copy_size != 0)
    flags |= FLAG_ARG_NEEDS_COPY;

  if (cif->abi != FFI_LINUX64)
    {
#ifndef __NO_FPRS__
      /* Space for the FPR registers, if needed.  */
      if (fparg_count != 0)
	bytes += NUM_FPR_ARG_REGISTERS * sizeof (double);
#endif

      /* Stack space.  */
      if (intarg_count > NUM_GPR_ARG_REGISTERS)
	bytes += (intarg_count - NUM_GPR_ARG_REGISTERS) * sizeof (int);
#ifndef __NO_FPRS__
      if (fparg_count > NUM_FPR_ARG_REGISTERS)
	bytes += (fparg_count - NUM_FPR_ARG_REGISTERS) * sizeof (double);
#endif
    }
  else
    {
#ifndef __NO_FPRS__
      /* Space for the FPR registers, if needed.  */
      if (fparg_count != 0)
	bytes += NUM_FPR_ARG_REGISTERS64 * sizeof (double);
#endif

      /* Stack space.  */
      if (intarg_count > NUM_GPR_ARG_REGISTERS64)
	bytes += (intarg_count - NUM_GPR_ARG_REGISTERS64) * sizeof (long);
    }

  /* The stack space allocated needs to be a multiple of 16 bytes.  */
  bytes = (bytes + 15) & ~0xF;

  /* Add in the space for the copied structures.  */
  bytes += struct_copy_size;

  cif->flags = flags;
  cif->bytes = bytes;

  return FFI_OK;
}

extern void ffi_call_SYSV(extended_cif *, unsigned, unsigned, unsigned *,
			  void (*fn)(void));
extern void FFI_HIDDEN ffi_call_LINUX64(extended_cif *, unsigned long,
					unsigned long, unsigned long *,
					void (*fn)(void));

void
ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  /*
   * The final SYSV ABI says that structures smaller or equal 8 bytes
   * are returned in r3/r4. The FFI_GCC_SYSV ABI instead returns them
   * in memory.
   *
   * Just to keep things simple for the assembly code, we will always
   * bounce-buffer struct return values less than or equal to 8 bytes.
   * This allows the ASM to handle SYSV small structures by directly
   * writing r3 and r4 to memory without worrying about struct size.
   */
  unsigned int smst_buffer[2];
  extended_cif ecif;
  unsigned int rsize = 0;

  ecif.cif = cif;
  ecif.avalue = avalue;

  /* Ensure that we have a valid struct return value */
  ecif.rvalue = rvalue;
  if (cif->rtype->type == FFI_TYPE_STRUCT) {
    rsize = cif->rtype->size;
    if (rsize <= 8)
      ecif.rvalue = smst_buffer;
    else if (!rvalue)
      ecif.rvalue = alloca(rsize);
  }

  switch (cif->abi)
    {
#ifndef POWERPC64
# ifndef __NO_FPRS__
    case FFI_SYSV:
    case FFI_GCC_SYSV:
    case FFI_LINUX:
# endif
    case FFI_LINUX_SOFT_FLOAT:
      ffi_call_SYSV (&ecif, -cif->bytes, cif->flags, ecif.rvalue, fn);
      break;
#else
    case FFI_LINUX64:
      ffi_call_LINUX64 (&ecif, -(long) cif->bytes, cif->flags, ecif.rvalue, fn);
      break;
#endif
    default:
      FFI_ASSERT (0);
      break;
    }

  /* Check for a bounce-buffered return value */
  if (rvalue && ecif.rvalue == smst_buffer)
    memcpy(rvalue, smst_buffer, rsize);
}


#ifndef POWERPC64
#define MIN_CACHE_LINE_SIZE 8

static void
flush_icache (char *wraddr, char *xaddr, int size)
{
  int i;
  for (i = 0; i < size; i += MIN_CACHE_LINE_SIZE)
    __asm__ volatile ("icbi 0,%0;" "dcbf 0,%1;"
		      : : "r" (xaddr + i), "r" (wraddr + i) : "memory");
  __asm__ volatile ("icbi 0,%0;" "dcbf 0,%1;" "sync;" "isync;"
		    : : "r"(xaddr + size - 1), "r"(wraddr + size - 1)
		    : "memory");
}
#endif

ffi_status
ffi_prep_closure_loc (ffi_closure *closure,
		      ffi_cif *cif,
		      void (*fun) (ffi_cif *, void *, void **, void *),
		      void *user_data,
		      void *codeloc)
{
#ifdef POWERPC64
  void **tramp = (void **) &closure->tramp[0];

  if (cif->abi != FFI_LINUX64)
    return FFI_BAD_ABI;
  /* Copy function address and TOC from ffi_closure_LINUX64.  */
  memcpy (tramp, (char *) ffi_closure_LINUX64, 16);
  tramp[2] = codeloc;
#else
  unsigned int *tramp;

  if (! (cif->abi == FFI_GCC_SYSV 
	 || cif->abi == FFI_SYSV
	 || cif->abi == FFI_LINUX
	 || cif->abi == FFI_LINUX_SOFT_FLOAT))
    return FFI_BAD_ABI;

  tramp = (unsigned int *) &closure->tramp[0];
  tramp[0] = 0x7c0802a6;  /*   mflr    r0 */
  tramp[1] = 0x4800000d;  /*   bl      10 <trampoline_initial+0x10> */
  tramp[4] = 0x7d6802a6;  /*   mflr    r11 */
  tramp[5] = 0x7c0803a6;  /*   mtlr    r0 */
  tramp[6] = 0x800b0000;  /*   lwz     r0,0(r11) */
  tramp[7] = 0x816b0004;  /*   lwz     r11,4(r11) */
  tramp[8] = 0x7c0903a6;  /*   mtctr   r0 */
  tramp[9] = 0x4e800420;  /*   bctr */
  *(void **) &tramp[2] = (void *) ffi_closure_SYSV; /* function */
  *(void **) &tramp[3] = codeloc;                   /* context */

  /* Flush the icache.  */
  flush_icache ((char *)tramp, (char *)codeloc, FFI_TRAMPOLINE_SIZE);
#endif

  closure->cif = cif;
  closure->fun = fun;
  closure->user_data = user_data;

  return FFI_OK;
}

typedef union
{
  float f;
  double d;
} ffi_dblfl;

int ffi_closure_helper_SYSV (ffi_closure *, void *, unsigned long *,
			     ffi_dblfl *, unsigned long *);

/* Basically the trampoline invokes ffi_closure_SYSV, and on
 * entry, r11 holds the address of the closure.
 * After storing the registers that could possibly contain
 * parameters to be passed into the stack frame and setting
 * up space for a return value, ffi_closure_SYSV invokes the
 * following helper function to do most of the work
 */

int
ffi_closure_helper_SYSV (ffi_closure *closure, void *rvalue,
			 unsigned long *pgr, ffi_dblfl *pfr,
			 unsigned long *pst)
{
  /* rvalue is the pointer to space for return value in closure assembly */
  /* pgr is the pointer to where r3-r10 are stored in ffi_closure_SYSV */
  /* pfr is the pointer to where f1-f8 are stored in ffi_closure_SYSV  */
  /* pst is the pointer to outgoing parameter stack in original caller */

  void **          avalue;
  ffi_type **      arg_types;
  long             i, avn;
#ifndef __NO_FPRS__
  long             nf = 0;   /* number of floating registers already used */
#endif
  long             ng = 0;   /* number of general registers already used */

  ffi_cif *cif = closure->cif;
  unsigned       size     = cif->rtype->size;
  unsigned short rtypenum = cif->rtype->type;

  avalue = alloca (cif->nargs * sizeof (void *));

  /* First translate for softfloat/nonlinux */
  if (cif->abi == FFI_LINUX_SOFT_FLOAT) {
	if (rtypenum == FFI_TYPE_FLOAT)
		rtypenum = FFI_TYPE_UINT32;
	if (rtypenum == FFI_TYPE_DOUBLE)
		rtypenum = FFI_TYPE_UINT64;
	if (rtypenum == FFI_TYPE_LONGDOUBLE)
		rtypenum = FFI_TYPE_UINT128;
  } else if (cif->abi != FFI_LINUX && cif->abi != FFI_LINUX64) {
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	if (rtypenum == FFI_TYPE_LONGDOUBLE)
		rtypenum = FFI_TYPE_STRUCT;
#endif
  }


  /* Copy the caller's structure return value address so that the closure
     returns the data directly to the caller.
     For FFI_SYSV the result is passed in r3/r4 if the struct size is less
     or equal 8 bytes.  */
  if (rtypenum == FFI_TYPE_STRUCT && ((cif->abi != FFI_SYSV) || (size > 8))) {
      rvalue = (void *) *pgr;
      ng++;
      pgr++;
    }

  i = 0;
  avn = cif->nargs;
  arg_types = cif->arg_types;

  /* Grab the addresses of the arguments from the stack frame.  */
  while (i < avn) {
      unsigned short typenum = arg_types[i]->type;

      /* We may need to handle some values depending on ABI */
      if (cif->abi == FFI_LINUX_SOFT_FLOAT) {
		if (typenum == FFI_TYPE_FLOAT)
			typenum = FFI_TYPE_UINT32;
		if (typenum == FFI_TYPE_DOUBLE)
			typenum = FFI_TYPE_UINT64;
		if (typenum == FFI_TYPE_LONGDOUBLE)
			typenum = FFI_TYPE_UINT128;
      } else if (cif->abi != FFI_LINUX && cif->abi != FFI_LINUX64) {
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
		if (typenum == FFI_TYPE_LONGDOUBLE)
			typenum = FFI_TYPE_STRUCT;
#endif
      }

      switch (typenum) {
#ifndef __NO_FPRS__
	case FFI_TYPE_FLOAT:
	  /* unfortunately float values are stored as doubles
	   * in the ffi_closure_SYSV code (since we don't check
	   * the type in that routine).
	   */

	  /* there are 8 64bit floating point registers */

	  if (nf < 8)
	    {
	      double temp = pfr->d;
	      pfr->f = (float) temp;
	      avalue[i] = pfr;
	      nf++;
	      pfr++;
	    }
	  else
	    {
	      /* FIXME? here we are really changing the values
	       * stored in the original calling routines outgoing
	       * parameter stack.  This is probably a really
	       * naughty thing to do but...
	       */
	      avalue[i] = pst;
	      pst += 1;
	    }
	  break;

	case FFI_TYPE_DOUBLE:
	  /* On the outgoing stack all values are aligned to 8 */
	  /* there are 8 64bit floating point registers */

	  if (nf < 8)
	    {
	      avalue[i] = pfr;
	      nf++;
	      pfr++;
	    }
	  else
	    {
	      if (((long) pst) & 4)
		pst++;
	      avalue[i] = pst;
	      pst += 2;
	    }
	  break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	case FFI_TYPE_LONGDOUBLE:
	  if (nf < 7)
	    {
	      avalue[i] = pfr;
	      pfr += 2;
	      nf += 2;
	    }
	  else
	    {
	      if (((long) pst) & 4)
		pst++;
	      avalue[i] = pst;
	      pst += 4;
	      nf = 8;
	    }
	  break;
#endif
#endif /* have FPRS */

	case FFI_TYPE_UINT128:
		/*
		 * Test if for the whole long double, 4 gprs are available.
		 * otherwise the stuff ends up on the stack.
		 */
		if (ng < 5) {
			avalue[i] = pgr;
			pgr += 4;
			ng += 4;
		} else {
			avalue[i] = pst;
			pst += 4;
			ng = 8+4;
		}
		break;

	case FFI_TYPE_SINT8:
	case FFI_TYPE_UINT8:
	  /* there are 8 gpr registers used to pass values */
	  if (ng < 8)
	    {
	      avalue[i] = (char *) pgr + 3;
	      ng++;
	      pgr++;
	    }
	  else
	    {
	      avalue[i] = (char *) pst + 3;
	      pst++;
	    }
	  break;

	case FFI_TYPE_SINT16:
	case FFI_TYPE_UINT16:
	  /* there are 8 gpr registers used to pass values */
	  if (ng < 8)
	    {
	      avalue[i] = (char *) pgr + 2;
	      ng++;
	      pgr++;
	    }
	  else
	    {
	      avalue[i] = (char *) pst + 2;
	      pst++;
	    }
	  break;

	case FFI_TYPE_SINT32:
	case FFI_TYPE_UINT32:
	case FFI_TYPE_POINTER:
	  /* there are 8 gpr registers used to pass values */
	  if (ng < 8)
	    {
	      avalue[i] = pgr;
	      ng++;
	      pgr++;
	    }
	  else
	    {
	      avalue[i] = pst;
	      pst++;
	    }
	  break;

	case FFI_TYPE_STRUCT:
	  /* Structs are passed by reference. The address will appear in a
	     gpr if it is one of the first 8 arguments.  */
	  if (ng < 8)
	    {
	      avalue[i] = (void *) *pgr;
	      ng++;
	      pgr++;
	    }
	  else
	    {
	      avalue[i] = (void *) *pst;
	      pst++;
	    }
	  break;

	case FFI_TYPE_SINT64:
	case FFI_TYPE_UINT64:
	  /* passing long long ints are complex, they must
	   * be passed in suitable register pairs such as
	   * (r3,r4) or (r5,r6) or (r6,r7), or (r7,r8) or (r9,r10)
	   * and if the entire pair aren't available then the outgoing
	   * parameter stack is used for both but an alignment of 8
	   * must will be kept.  So we must either look in pgr
	   * or pst to find the correct address for this type
	   * of parameter.
	   */
	  if (ng < 7)
	    {
	      if (ng & 0x01)
		{
		  /* skip r4, r6, r8 as starting points */
		  ng++;
		  pgr++;
		}
	      avalue[i] = pgr;
	      ng += 2;
	      pgr += 2;
	    }
	  else
	    {
	      if (((long) pst) & 4)
		pst++;
	      avalue[i] = pst;
	      pst += 2;
	      ng = 8;
	    }
	  break;

	default:
		FFI_ASSERT (0);
	}

      i++;
    }


  (closure->fun) (cif, rvalue, avalue, closure->user_data);

  /* Tell ffi_closure_SYSV how to perform return type promotions.
     Because the FFI_SYSV ABI returns the structures <= 8 bytes in r3/r4
     we have to tell ffi_closure_SYSV how to treat them. We combine the base
     type FFI_SYSV_TYPE_SMALL_STRUCT - 1  with the size of the struct.
     So a one byte struct gets the return type 16. Return type 1 to 15 are
     already used and we never have a struct with size zero. That is the reason
     for the subtraction of 1. See the comment in ffitarget.h about ordering.
  */
  if (cif->abi == FFI_SYSV && rtypenum == FFI_TYPE_STRUCT && size <= 8)
    return (FFI_SYSV_TYPE_SMALL_STRUCT - 1) + size;
  return rtypenum;
}

int FFI_HIDDEN ffi_closure_helper_LINUX64 (ffi_closure *, void *,
					   unsigned long *, ffi_dblfl *);

int FFI_HIDDEN
ffi_closure_helper_LINUX64 (ffi_closure *closure, void *rvalue,
			    unsigned long *pst, ffi_dblfl *pfr)
{
  /* rvalue is the pointer to space for return value in closure assembly */
  /* pst is the pointer to parameter save area
     (r3-r10 are stored into its first 8 slots by ffi_closure_LINUX64) */
  /* pfr is the pointer to where f1-f13 are stored in ffi_closure_LINUX64 */

  void **avalue;
  ffi_type **arg_types;
  long i, avn;
  ffi_cif *cif;
  ffi_dblfl *end_pfr = pfr + NUM_FPR_ARG_REGISTERS64;

  cif = closure->cif;
  avalue = alloca (cif->nargs * sizeof (void *));

  /* Copy the caller's structure return value address so that the closure
     returns the data directly to the caller.  */
  if (cif->rtype->type == FFI_TYPE_STRUCT)
    {
      rvalue = (void *) *pst;
      pst++;
    }

  i = 0;
  avn = cif->nargs;
  arg_types = cif->arg_types;

  /* Grab the addresses of the arguments from the stack frame.  */
  while (i < avn)
    {
      switch (arg_types[i]->type)
	{
	case FFI_TYPE_SINT8:
	case FFI_TYPE_UINT8:
	  avalue[i] = (char *) pst + 7;
	  pst++;
	  break;

	case FFI_TYPE_SINT16:
	case FFI_TYPE_UINT16:
	  avalue[i] = (char *) pst + 6;
	  pst++;
	  break;

	case FFI_TYPE_SINT32:
	case FFI_TYPE_UINT32:
	  avalue[i] = (char *) pst + 4;
	  pst++;
	  break;

	case FFI_TYPE_SINT64:
	case FFI_TYPE_UINT64:
	case FFI_TYPE_POINTER:
	  avalue[i] = pst;
	  pst++;
	  break;

	case FFI_TYPE_STRUCT:
	  /* Structures with size less than eight bytes are passed
	     left-padded.  */
	  if (arg_types[i]->size < 8)
	    avalue[i] = (char *) pst + 8 - arg_types[i]->size;
	  else
	    avalue[i] = pst;
	  pst += (arg_types[i]->size + 7) / 8;
	  break;

	case FFI_TYPE_FLOAT:
	  /* unfortunately float values are stored as doubles
	   * in the ffi_closure_LINUX64 code (since we don't check
	   * the type in that routine).
	   */

	  /* there are 13 64bit floating point registers */

	  if (pfr < end_pfr)
	    {
	      double temp = pfr->d;
	      pfr->f = (float) temp;
	      avalue[i] = pfr;
	      pfr++;
	    }
	  else
	    avalue[i] = pst;
	  pst++;
	  break;

	case FFI_TYPE_DOUBLE:
	  /* On the outgoing stack all values are aligned to 8 */
	  /* there are 13 64bit floating point registers */

	  if (pfr < end_pfr)
	    {
	      avalue[i] = pfr;
	      pfr++;
	    }
	  else
	    avalue[i] = pst;
	  pst++;
	  break;

#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
	case FFI_TYPE_LONGDOUBLE:
	  if (pfr + 1 < end_pfr)
	    {
	      avalue[i] = pfr;
	      pfr += 2;
	    }
	  else
	    {
	      if (pfr < end_pfr)
		{
		  /* Passed partly in f13 and partly on the stack.
		     Move it all to the stack.  */
		  *pst = *(unsigned long *) pfr;
		  pfr++;
		}
	      avalue[i] = pst;
	    }
	  pst += 2;
	  break;
#endif

	default:
	  FFI_ASSERT (0);
	}

      i++;
    }


  (closure->fun) (cif, rvalue, avalue, closure->user_data);

  /* Tell ffi_closure_LINUX64 how to perform return type promotions.  */
  return cif->rtype->type;
}
