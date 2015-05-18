/* -----------------------------------------------------------------------
   ffi.c - Copyright (c) 2012  Anthony Green
           Copyright (c) 1998, 2001, 2007, 2008  Red Hat, Inc.
   
   Alpha Foreign Function Interface 

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

/* Force FFI_TYPE_LONGDOUBLE to be different than FFI_TYPE_DOUBLE;
   all further uses in this file will refer to the 128-bit type.  */
#if defined(__LONG_DOUBLE_128__)
# if FFI_TYPE_LONGDOUBLE != 4
#  error FFI_TYPE_LONGDOUBLE out of date
# endif
#else
# undef FFI_TYPE_LONGDOUBLE
# define FFI_TYPE_LONGDOUBLE 4
#endif

extern void ffi_call_osf(void *, unsigned long, unsigned, void *, void (*)(void))
  FFI_HIDDEN;
extern void ffi_closure_osf(void) FFI_HIDDEN;


ffi_status
ffi_prep_cif_machdep(ffi_cif *cif)
{
  /* Adjust cif->bytes to represent a minimum 6 words for the temporary
     register argument loading area.  */
  if (cif->bytes < 6*FFI_SIZEOF_ARG)
    cif->bytes = 6*FFI_SIZEOF_ARG;

  /* Set the return type flag */
  switch (cif->rtype->type)
    {
    case FFI_TYPE_STRUCT:
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
      cif->flags = cif->rtype->type;
      break;

    case FFI_TYPE_LONGDOUBLE:
      /* 128-bit long double is returned in memory, like a struct.  */
      cif->flags = FFI_TYPE_STRUCT;
      break;

    default:
      cif->flags = FFI_TYPE_INT;
      break;
    }
  
  return FFI_OK;
}


void
ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  unsigned long *stack, *argp;
  long i, avn;
  ffi_type **arg_types;
  
  /* If the return value is a struct and we don't have a return
     value address then we need to make one.  */
  if (rvalue == NULL && cif->flags == FFI_TYPE_STRUCT)
    rvalue = alloca(cif->rtype->size);

  /* Allocate the space for the arguments, plus 4 words of temp
     space for ffi_call_osf.  */
  argp = stack = alloca(cif->bytes + 4*FFI_SIZEOF_ARG);

  if (cif->flags == FFI_TYPE_STRUCT)
    *(void **) argp++ = rvalue;

  i = 0;
  avn = cif->nargs;
  arg_types = cif->arg_types;

  while (i < avn)
    {
      size_t size = (*arg_types)->size;

      switch ((*arg_types)->type)
	{
	case FFI_TYPE_SINT8:
	  *(SINT64 *) argp = *(SINT8 *)(* avalue);
	  break;
		  
	case FFI_TYPE_UINT8:
	  *(SINT64 *) argp = *(UINT8 *)(* avalue);
	  break;
		  
	case FFI_TYPE_SINT16:
	  *(SINT64 *) argp = *(SINT16 *)(* avalue);
	  break;
		  
	case FFI_TYPE_UINT16:
	  *(SINT64 *) argp = *(UINT16 *)(* avalue);
	  break;
		  
	case FFI_TYPE_SINT32:
	case FFI_TYPE_UINT32:
	  /* Note that unsigned 32-bit quantities are sign extended.  */
	  *(SINT64 *) argp = *(SINT32 *)(* avalue);
	  break;
		  
	case FFI_TYPE_SINT64:
	case FFI_TYPE_UINT64:
	case FFI_TYPE_POINTER:
	  *(UINT64 *) argp = *(UINT64 *)(* avalue);
	  break;

	case FFI_TYPE_FLOAT:
	  if (argp - stack < 6)
	    {
	      /* Note the conversion -- all the fp regs are loaded as
		 doubles.  The in-register format is the same.  */
	      *(double *) argp = *(float *)(* avalue);
	    }
	  else
	    *(float *) argp = *(float *)(* avalue);
	  break;

	case FFI_TYPE_DOUBLE:
	  *(double *) argp = *(double *)(* avalue);
	  break;

	case FFI_TYPE_LONGDOUBLE:
	  /* 128-bit long double is passed by reference.  */
	  *(long double **) argp = (long double *)(* avalue);
	  size = sizeof (long double *);
	  break;

	case FFI_TYPE_STRUCT:
	  memcpy(argp, *avalue, (*arg_types)->size);
	  break;

	default:
	  FFI_ASSERT(0);
	}

      argp += ALIGN(size, FFI_SIZEOF_ARG) / FFI_SIZEOF_ARG;
      i++, arg_types++, avalue++;
    }

  ffi_call_osf(stack, cif->bytes, cif->flags, rvalue, fn);
}


ffi_status
ffi_prep_closure_loc (ffi_closure* closure,
		      ffi_cif* cif,
		      void (*fun)(ffi_cif*, void*, void**, void*),
		      void *user_data,
		      void *codeloc)
{
  unsigned int *tramp;

  if (cif->abi != FFI_OSF)
    return FFI_BAD_ABI;

  tramp = (unsigned int *) &closure->tramp[0];
  tramp[0] = 0x47fb0401;	/* mov $27,$1		*/
  tramp[1] = 0xa77b0010;	/* ldq $27,16($27)	*/
  tramp[2] = 0x6bfb0000;	/* jmp $31,($27),0	*/
  tramp[3] = 0x47ff041f;	/* nop			*/
  *(void **) &tramp[4] = ffi_closure_osf;

  closure->cif = cif;
  closure->fun = fun;
  closure->user_data = user_data;

  /* Flush the Icache.

     Tru64 UNIX as doesn't understand the imb mnemonic, so use call_pal
     instead, since both Compaq as and gas can handle it.

     0x86 is PAL_imb in Tru64 UNIX <alpha/pal.h>.  */
  asm volatile ("call_pal 0x86" : : : "memory");

  return FFI_OK;
}


long FFI_HIDDEN
ffi_closure_osf_inner(ffi_closure *closure, void *rvalue, unsigned long *argp)
{
  ffi_cif *cif;
  void **avalue;
  ffi_type **arg_types;
  long i, avn, argn;

  cif = closure->cif;
  avalue = alloca(cif->nargs * sizeof(void *));

  argn = 0;

  /* Copy the caller's structure return address to that the closure
     returns the data directly to the caller.  */
  if (cif->flags == FFI_TYPE_STRUCT)
    {
      rvalue = (void *) argp[0];
      argn = 1;
    }

  i = 0;
  avn = cif->nargs;
  arg_types = cif->arg_types;
  
  /* Grab the addresses of the arguments from the stack frame.  */
  while (i < avn)
    {
      size_t size = arg_types[i]->size;

      switch (arg_types[i]->type)
	{
	case FFI_TYPE_SINT8:
	case FFI_TYPE_UINT8:
	case FFI_TYPE_SINT16:
	case FFI_TYPE_UINT16:
	case FFI_TYPE_SINT32:
	case FFI_TYPE_UINT32:
	case FFI_TYPE_SINT64:
	case FFI_TYPE_UINT64:
	case FFI_TYPE_POINTER:
	case FFI_TYPE_STRUCT:
	  avalue[i] = &argp[argn];
	  break;

	case FFI_TYPE_FLOAT:
	  if (argn < 6)
	    {
	      /* Floats coming from registers need conversion from double
	         back to float format.  */
	      *(float *)&argp[argn - 6] = *(double *)&argp[argn - 6];
	      avalue[i] = &argp[argn - 6];
	    }
	  else
	    avalue[i] = &argp[argn];
	  break;

	case FFI_TYPE_DOUBLE:
	  avalue[i] = &argp[argn - (argn < 6 ? 6 : 0)];
	  break;

	case FFI_TYPE_LONGDOUBLE:
	  /* 128-bit long double is passed by reference.  */
	  avalue[i] = (long double *) argp[argn];
	  size = sizeof (long double *);
	  break;

	default:
	  abort ();
	}

      argn += ALIGN(size, FFI_SIZEOF_ARG) / FFI_SIZEOF_ARG;
      i++;
    }

  /* Invoke the closure.  */
  closure->fun (cif, rvalue, avalue, closure->user_data);

  /* Tell ffi_closure_osf how to perform return type promotions.  */
  return cif->rtype->type;
}
