/* -----------------------------------------------------------------------
   ffi64.c - Copyright (c) 2013  The Written Word, Inc.
             Copyright (c) 2011  Anthony Green
             Copyright (c) 2008, 2010  Red Hat, Inc.
             Copyright (c) 2002, 2007  Bo Thorsen <bo@suse.de>
             
   x86-64 Foreign Function Interface 

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

#include <ffi.h>
#include <ffi_common.h>

#include <stdlib.h>
#include <stdarg.h>

#ifdef __x86_64__

#define MAX_GPR_REGS 6
#define MAX_SSE_REGS 8

#if defined(__INTEL_COMPILER)
#define UINT128 __m128
#else
#if defined(__SUNPRO_C)
#include <sunmedia_types.h>
#define UINT128 __m128i
#else
#define UINT128 __int128_t
#endif
#endif

union big_int_union
{
  UINT32 i32;
  UINT64 i64;
  UINT128 i128;
};

struct register_args
{
  /* Registers for argument passing.  */
  UINT64 gpr[MAX_GPR_REGS];
  union big_int_union sse[MAX_SSE_REGS]; 
};

extern void ffi_call_unix64 (void *args, unsigned long bytes, unsigned flags,
			     void *raddr, void (*fnaddr)(void), unsigned ssecount);

/* All reference to register classes here is identical to the code in
   gcc/config/i386/i386.c. Do *not* change one without the other.  */

/* Register class used for passing given 64bit part of the argument.
   These represent classes as documented by the PS ABI, with the
   exception of SSESF, SSEDF classes, that are basically SSE class,
   just gcc will use SF or DFmode move instead of DImode to avoid
   reformatting penalties.

   Similary we play games with INTEGERSI_CLASS to use cheaper SImode moves
   whenever possible (upper half does contain padding).  */
enum x86_64_reg_class
  {
    X86_64_NO_CLASS,
    X86_64_INTEGER_CLASS,
    X86_64_INTEGERSI_CLASS,
    X86_64_SSE_CLASS,
    X86_64_SSESF_CLASS,
    X86_64_SSEDF_CLASS,
    X86_64_SSEUP_CLASS,
    X86_64_X87_CLASS,
    X86_64_X87UP_CLASS,
    X86_64_COMPLEX_X87_CLASS,
    X86_64_MEMORY_CLASS
  };

#define MAX_CLASSES 4

#define SSE_CLASS_P(X)	((X) >= X86_64_SSE_CLASS && X <= X86_64_SSEUP_CLASS)

/* x86-64 register passing implementation.  See x86-64 ABI for details.  Goal
   of this code is to classify each 8bytes of incoming argument by the register
   class and assign registers accordingly.  */

/* Return the union class of CLASS1 and CLASS2.
   See the x86-64 PS ABI for details.  */

static enum x86_64_reg_class
merge_classes (enum x86_64_reg_class class1, enum x86_64_reg_class class2)
{
  /* Rule #1: If both classes are equal, this is the resulting class.  */
  if (class1 == class2)
    return class1;

  /* Rule #2: If one of the classes is NO_CLASS, the resulting class is
     the other class.  */
  if (class1 == X86_64_NO_CLASS)
    return class2;
  if (class2 == X86_64_NO_CLASS)
    return class1;

  /* Rule #3: If one of the classes is MEMORY, the result is MEMORY.  */
  if (class1 == X86_64_MEMORY_CLASS || class2 == X86_64_MEMORY_CLASS)
    return X86_64_MEMORY_CLASS;

  /* Rule #4: If one of the classes is INTEGER, the result is INTEGER.  */
  if ((class1 == X86_64_INTEGERSI_CLASS && class2 == X86_64_SSESF_CLASS)
      || (class2 == X86_64_INTEGERSI_CLASS && class1 == X86_64_SSESF_CLASS))
    return X86_64_INTEGERSI_CLASS;
  if (class1 == X86_64_INTEGER_CLASS || class1 == X86_64_INTEGERSI_CLASS
      || class2 == X86_64_INTEGER_CLASS || class2 == X86_64_INTEGERSI_CLASS)
    return X86_64_INTEGER_CLASS;

  /* Rule #5: If one of the classes is X87, X87UP, or COMPLEX_X87 class,
     MEMORY is used.  */
  if (class1 == X86_64_X87_CLASS
      || class1 == X86_64_X87UP_CLASS
      || class1 == X86_64_COMPLEX_X87_CLASS
      || class2 == X86_64_X87_CLASS
      || class2 == X86_64_X87UP_CLASS
      || class2 == X86_64_COMPLEX_X87_CLASS)
    return X86_64_MEMORY_CLASS;

  /* Rule #6: Otherwise class SSE is used.  */
  return X86_64_SSE_CLASS;
}

/* Classify the argument of type TYPE and mode MODE.
   CLASSES will be filled by the register class used to pass each word
   of the operand.  The number of words is returned.  In case the parameter
   should be passed in memory, 0 is returned. As a special case for zero
   sized containers, classes[0] will be NO_CLASS and 1 is returned.

   See the x86-64 PS ABI for details.
*/
static int
classify_argument (ffi_type *type, enum x86_64_reg_class classes[],
		   size_t byte_offset)
{
  switch (type->type)
    {
    case FFI_TYPE_UINT8:
    case FFI_TYPE_SINT8:
    case FFI_TYPE_UINT16:
    case FFI_TYPE_SINT16:
    case FFI_TYPE_UINT32:
    case FFI_TYPE_SINT32:
    case FFI_TYPE_UINT64:
    case FFI_TYPE_SINT64:
    case FFI_TYPE_POINTER:
      {
	int size = byte_offset + type->size;

	if (size <= 4)
	  {
	    classes[0] = X86_64_INTEGERSI_CLASS;
	    return 1;
	  }
	else if (size <= 8)
	  {
	    classes[0] = X86_64_INTEGER_CLASS;
	    return 1;
	  }
	else if (size <= 12)
	  {
	    classes[0] = X86_64_INTEGER_CLASS;
	    classes[1] = X86_64_INTEGERSI_CLASS;
	    return 2;
	  }
	else if (size <= 16)
	  {
	    classes[0] = classes[1] = X86_64_INTEGERSI_CLASS;
	    return 2;
	  }
	else
	  FFI_ASSERT (0);
      }
    case FFI_TYPE_FLOAT:
      if (!(byte_offset % 8))
	classes[0] = X86_64_SSESF_CLASS;
      else
	classes[0] = X86_64_SSE_CLASS;
      return 1;
    case FFI_TYPE_DOUBLE:
      classes[0] = X86_64_SSEDF_CLASS;
      return 1;
    case FFI_TYPE_LONGDOUBLE:
      classes[0] = X86_64_X87_CLASS;
      classes[1] = X86_64_X87UP_CLASS;
      return 2;
    case FFI_TYPE_STRUCT:
      {
	const int UNITS_PER_WORD = 8;
	int words = (type->size + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
	ffi_type **ptr; 
	int i;
	enum x86_64_reg_class subclasses[MAX_CLASSES];

	/* If the struct is larger than 32 bytes, pass it on the stack.  */
	if (type->size > 32)
	  return 0;

	for (i = 0; i < words; i++)
	  classes[i] = X86_64_NO_CLASS;

	/* Zero sized arrays or structures are NO_CLASS.  We return 0 to
	   signalize memory class, so handle it as special case.  */
	if (!words)
	  {
	    classes[0] = X86_64_NO_CLASS;
	    return 1;
	  }

	/* Merge the fields of structure.  */
	for (ptr = type->elements; *ptr != NULL; ptr++)
	  {
	    int num;

	    byte_offset = ALIGN (byte_offset, (*ptr)->alignment);

	    num = classify_argument (*ptr, subclasses, byte_offset % 8);
	    if (num == 0)
	      return 0;
	    for (i = 0; i < num; i++)
	      {
		int pos = byte_offset / 8;
		classes[i + pos] =
		  merge_classes (subclasses[i], classes[i + pos]);
	      }

	    byte_offset += (*ptr)->size;
	  }

	if (words > 2)
	  {
	    /* When size > 16 bytes, if the first one isn't
	       X86_64_SSE_CLASS or any other ones aren't
	       X86_64_SSEUP_CLASS, everything should be passed in
	       memory.  */
	    if (classes[0] != X86_64_SSE_CLASS)
	      return 0;

	    for (i = 1; i < words; i++)
	      if (classes[i] != X86_64_SSEUP_CLASS)
		return 0;
	  }

	/* Final merger cleanup.  */
	for (i = 0; i < words; i++)
	  {
	    /* If one class is MEMORY, everything should be passed in
	       memory.  */
	    if (classes[i] == X86_64_MEMORY_CLASS)
	      return 0;

	    /* The X86_64_SSEUP_CLASS should be always preceded by
	       X86_64_SSE_CLASS or X86_64_SSEUP_CLASS.  */
	    if (classes[i] == X86_64_SSEUP_CLASS
		&& classes[i - 1] != X86_64_SSE_CLASS
		&& classes[i - 1] != X86_64_SSEUP_CLASS)
	      {
		/* The first one should never be X86_64_SSEUP_CLASS.  */
		FFI_ASSERT (i != 0);
		classes[i] = X86_64_SSE_CLASS;
	      }

	    /*  If X86_64_X87UP_CLASS isn't preceded by X86_64_X87_CLASS,
		everything should be passed in memory.  */
	    if (classes[i] == X86_64_X87UP_CLASS
		&& (classes[i - 1] != X86_64_X87_CLASS))
	      {
		/* The first one should never be X86_64_X87UP_CLASS.  */
		FFI_ASSERT (i != 0);
		return 0;
	      }
	  }
	return words;
      }

    default:
      FFI_ASSERT(0);
    }
  return 0; /* Never reached.  */
}

/* Examine the argument and return set number of register required in each
   class.  Return zero iff parameter should be passed in memory, otherwise
   the number of registers.  */

static int
examine_argument (ffi_type *type, enum x86_64_reg_class classes[MAX_CLASSES],
		  _Bool in_return, int *pngpr, int *pnsse)
{
  int i, n, ngpr, nsse;

  n = classify_argument (type, classes, 0);
  if (n == 0)
    return 0;

  ngpr = nsse = 0;
  for (i = 0; i < n; ++i)
    switch (classes[i])
      {
      case X86_64_INTEGER_CLASS:
      case X86_64_INTEGERSI_CLASS:
	ngpr++;
	break;
      case X86_64_SSE_CLASS:
      case X86_64_SSESF_CLASS:
      case X86_64_SSEDF_CLASS:
	nsse++;
	break;
      case X86_64_NO_CLASS:
      case X86_64_SSEUP_CLASS:
	break;
      case X86_64_X87_CLASS:
      case X86_64_X87UP_CLASS:
      case X86_64_COMPLEX_X87_CLASS:
	return in_return != 0;
      default:
	abort ();
      }

  *pngpr = ngpr;
  *pnsse = nsse;

  return n;
}

/* Perform machine dependent cif processing.  */

ffi_status
ffi_prep_cif_machdep (ffi_cif *cif)
{
  int gprcount, ssecount, i, avn, n, ngpr, nsse, flags;
  enum x86_64_reg_class classes[MAX_CLASSES];
  size_t bytes;

  gprcount = ssecount = 0;

  flags = cif->rtype->type;
  if (flags != FFI_TYPE_VOID)
    {
      n = examine_argument (cif->rtype, classes, 1, &ngpr, &nsse);
      if (n == 0)
	{
	  /* The return value is passed in memory.  A pointer to that
	     memory is the first argument.  Allocate a register for it.  */
	  gprcount++;
	  /* We don't have to do anything in asm for the return.  */
	  flags = FFI_TYPE_VOID;
	}
      else if (flags == FFI_TYPE_STRUCT)
	{
	  /* Mark which registers the result appears in.  */
	  _Bool sse0 = SSE_CLASS_P (classes[0]);
	  _Bool sse1 = n == 2 && SSE_CLASS_P (classes[1]);
	  if (sse0 && !sse1)
	    flags |= 1 << 8;
	  else if (!sse0 && sse1)
	    flags |= 1 << 9;
	  else if (sse0 && sse1)
	    flags |= 1 << 10;
	  /* Mark the true size of the structure.  */
	  flags |= cif->rtype->size << 12;
	}
    }

  /* Go over all arguments and determine the way they should be passed.
     If it's in a register and there is space for it, let that be so. If
     not, add it's size to the stack byte count.  */
  for (bytes = 0, i = 0, avn = cif->nargs; i < avn; i++)
    {
      if (examine_argument (cif->arg_types[i], classes, 0, &ngpr, &nsse) == 0
	  || gprcount + ngpr > MAX_GPR_REGS
	  || ssecount + nsse > MAX_SSE_REGS)
	{
	  long align = cif->arg_types[i]->alignment;

	  if (align < 8)
	    align = 8;

	  bytes = ALIGN (bytes, align);
	  bytes += cif->arg_types[i]->size;
	}
      else
	{
	  gprcount += ngpr;
	  ssecount += nsse;
	}
    }
  if (ssecount)
    flags |= 1 << 11;
  cif->flags = flags;
  cif->bytes = ALIGN (bytes, 8);

  return FFI_OK;
}

void
ffi_call (ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  enum x86_64_reg_class classes[MAX_CLASSES];
  char *stack, *argp;
  ffi_type **arg_types;
  int gprcount, ssecount, ngpr, nsse, i, avn;
  _Bool ret_in_memory;
  struct register_args *reg_args;

  /* Can't call 32-bit mode from 64-bit mode.  */
  FFI_ASSERT (cif->abi == FFI_UNIX64);

  /* If the return value is a struct and we don't have a return value
     address then we need to make one.  Note the setting of flags to
     VOID above in ffi_prep_cif_machdep.  */
  ret_in_memory = (cif->rtype->type == FFI_TYPE_STRUCT
		   && (cif->flags & 0xff) == FFI_TYPE_VOID);
  if (rvalue == NULL && ret_in_memory)
    rvalue = alloca (cif->rtype->size);

  /* Allocate the space for the arguments, plus 4 words of temp space.  */
  stack = alloca (sizeof (struct register_args) + cif->bytes + 4*8);
  reg_args = (struct register_args *) stack;
  argp = stack + sizeof (struct register_args);

  gprcount = ssecount = 0;

  /* If the return value is passed in memory, add the pointer as the
     first integer argument.  */
  if (ret_in_memory)
    reg_args->gpr[gprcount++] = (unsigned long) rvalue;

  avn = cif->nargs;
  arg_types = cif->arg_types;

  for (i = 0; i < avn; ++i)
    {
      size_t size = arg_types[i]->size;
      int n;

      n = examine_argument (arg_types[i], classes, 0, &ngpr, &nsse);
      if (n == 0
	  || gprcount + ngpr > MAX_GPR_REGS
	  || ssecount + nsse > MAX_SSE_REGS)
	{
	  long align = arg_types[i]->alignment;

	  /* Stack arguments are *always* at least 8 byte aligned.  */
	  if (align < 8)
	    align = 8;

	  /* Pass this argument in memory.  */
	  argp = (void *) ALIGN (argp, align);
	  memcpy (argp, avalue[i], size);
	  argp += size;
	}
      else
	{
	  /* The argument is passed entirely in registers.  */
	  char *a = (char *) avalue[i];
	  int j;

	  for (j = 0; j < n; j++, a += 8, size -= 8)
	    {
	      switch (classes[j])
		{
		case X86_64_INTEGER_CLASS:
		case X86_64_INTEGERSI_CLASS:
		  /* Sign-extend integer arguments passed in general
		     purpose registers, to cope with the fact that
		     LLVM incorrectly assumes that this will be done
		     (the x86-64 PS ABI does not specify this). */
		  switch (arg_types[i]->type)
		    {
		    case FFI_TYPE_SINT8:
		      *(SINT64 *)&reg_args->gpr[gprcount] = (SINT64) *((SINT8 *) a);
		      break;
		    case FFI_TYPE_SINT16:
		      *(SINT64 *)&reg_args->gpr[gprcount] = (SINT64) *((SINT16 *) a);
		      break;
		    case FFI_TYPE_SINT32:
		      *(SINT64 *)&reg_args->gpr[gprcount] = (SINT64) *((SINT32 *) a);
		      break;
		    default:
		      reg_args->gpr[gprcount] = 0;
		      memcpy (&reg_args->gpr[gprcount], a, size < 8 ? size : 8);
		    }
		  gprcount++;
		  break;
		case X86_64_SSE_CLASS:
		case X86_64_SSEDF_CLASS:
		  reg_args->sse[ssecount++].i64 = *(UINT64 *) a;
		  break;
		case X86_64_SSESF_CLASS:
		  reg_args->sse[ssecount++].i32 = *(UINT32 *) a;
		  break;
		default:
		  abort();
		}
	    }
	}
    }

  ffi_call_unix64 (stack, cif->bytes + sizeof (struct register_args),
		   cif->flags, rvalue, fn, ssecount);
}


extern void ffi_closure_unix64(void);

ffi_status
ffi_prep_closure_loc (ffi_closure* closure,
		      ffi_cif* cif,
		      void (*fun)(ffi_cif*, void*, void**, void*),
		      void *user_data,
		      void *codeloc)
{
  volatile unsigned short *tramp;

  /* Sanity check on the cif ABI.  */
  {
    int abi = cif->abi;
    if (UNLIKELY (! (abi > FFI_FIRST_ABI && abi < FFI_LAST_ABI)))
      return FFI_BAD_ABI;
  }

  tramp = (volatile unsigned short *) &closure->tramp[0];

  tramp[0] = 0xbb49;		/* mov <code>, %r11	*/
  *((unsigned long long * volatile) &tramp[1])
    = (unsigned long) ffi_closure_unix64;
  tramp[5] = 0xba49;		/* mov <data>, %r10	*/
  *((unsigned long long * volatile) &tramp[6])
    = (unsigned long) codeloc;

  /* Set the carry bit iff the function uses any sse registers.
     This is clc or stc, together with the first byte of the jmp.  */
  tramp[10] = cif->flags & (1 << 11) ? 0x49f9 : 0x49f8;

  tramp[11] = 0xe3ff;			/* jmp *%r11    */

  closure->cif = cif;
  closure->fun = fun;
  closure->user_data = user_data;

  return FFI_OK;
}

int
ffi_closure_unix64_inner(ffi_closure *closure, void *rvalue,
			 struct register_args *reg_args, char *argp)
{
  ffi_cif *cif;
  void **avalue;
  ffi_type **arg_types;
  long i, avn;
  int gprcount, ssecount, ngpr, nsse;
  int ret;

  cif = closure->cif;
  avalue = alloca(cif->nargs * sizeof(void *));
  gprcount = ssecount = 0;

  ret = cif->rtype->type;
  if (ret != FFI_TYPE_VOID)
    {
      enum x86_64_reg_class classes[MAX_CLASSES];
      int n = examine_argument (cif->rtype, classes, 1, &ngpr, &nsse);
      if (n == 0)
	{
	  /* The return value goes in memory.  Arrange for the closure
	     return value to go directly back to the original caller.  */
	  rvalue = (void *) (unsigned long) reg_args->gpr[gprcount++];
	  /* We don't have to do anything in asm for the return.  */
	  ret = FFI_TYPE_VOID;
	}
      else if (ret == FFI_TYPE_STRUCT && n == 2)
	{
	  /* Mark which register the second word of the structure goes in.  */
	  _Bool sse0 = SSE_CLASS_P (classes[0]);
	  _Bool sse1 = SSE_CLASS_P (classes[1]);
	  if (!sse0 && sse1)
	    ret |= 1 << 8;
	  else if (sse0 && !sse1)
	    ret |= 1 << 9;
	}
    }

  avn = cif->nargs;
  arg_types = cif->arg_types;
  
  for (i = 0; i < avn; ++i)
    {
      enum x86_64_reg_class classes[MAX_CLASSES];
      int n;

      n = examine_argument (arg_types[i], classes, 0, &ngpr, &nsse);
      if (n == 0
	  || gprcount + ngpr > MAX_GPR_REGS
	  || ssecount + nsse > MAX_SSE_REGS)
	{
	  long align = arg_types[i]->alignment;

	  /* Stack arguments are *always* at least 8 byte aligned.  */
	  if (align < 8)
	    align = 8;

	  /* Pass this argument in memory.  */
	  argp = (void *) ALIGN (argp, align);
	  avalue[i] = argp;
	  argp += arg_types[i]->size;
	}
      /* If the argument is in a single register, or two consecutive
	 integer registers, then we can use that address directly.  */
      else if (n == 1
	       || (n == 2 && !(SSE_CLASS_P (classes[0])
			       || SSE_CLASS_P (classes[1]))))
	{
	  /* The argument is in a single register.  */
	  if (SSE_CLASS_P (classes[0]))
	    {
	      avalue[i] = &reg_args->sse[ssecount];
	      ssecount += n;
	    }
	  else
	    {
	      avalue[i] = &reg_args->gpr[gprcount];
	      gprcount += n;
	    }
	}
      /* Otherwise, allocate space to make them consecutive.  */
      else
	{
	  char *a = alloca (16);
	  int j;

	  avalue[i] = a;
	  for (j = 0; j < n; j++, a += 8)
	    {
	      if (SSE_CLASS_P (classes[j]))
		memcpy (a, &reg_args->sse[ssecount++], 8);
	      else
		memcpy (a, &reg_args->gpr[gprcount++], 8);
	    }
	}
    }

  /* Invoke the closure.  */
  closure->fun (cif, rvalue, avalue, closure->user_data);

  /* Tell assembly how to perform return type promotions.  */
  return ret;
}

#endif /* __x86_64__ */
