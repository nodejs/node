/* -----------------------------------------------------------------------
   ffi.c - Copyright (c) 2011 Timothy Wall
           Copyright (c) 2011 Plausible Labs Cooperative, Inc.
           Copyright (c) 2011 Anthony Green
	   Copyright (c) 2011 Free Software Foundation
           Copyright (c) 1998, 2008, 2011  Red Hat, Inc.
	   
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

#include <ffi.h>
#include <ffi_common.h>

#include <stdlib.h>

/* Forward declares. */
static int vfp_type_p (ffi_type *);
static void layout_vfp_args (ffi_cif *);

/* ffi_prep_args is called by the assembly routine once stack space
   has been allocated for the function's arguments
   
   The vfp_space parameter is the load area for VFP regs, the return
   value is cif->vfp_used (word bitset of VFP regs used for passing
   arguments). These are only used for the VFP hard-float ABI.
*/
int ffi_prep_args(char *stack, extended_cif *ecif, float *vfp_space)
{
  register unsigned int i, vi = 0;
  register void **p_argv;
  register char *argp;
  register ffi_type **p_arg;

  argp = stack;

  if ( ecif->cif->flags == FFI_TYPE_STRUCT ) {
    *(void **) argp = ecif->rvalue;
    argp += 4;
  }

  p_argv = ecif->avalue;

  for (i = ecif->cif->nargs, p_arg = ecif->cif->arg_types;
       (i != 0);
       i--, p_arg++)
    {
      size_t z;
      size_t alignment;

      /* Allocated in VFP registers. */
      if (ecif->cif->abi == FFI_VFP
	  && vi < ecif->cif->vfp_nargs && vfp_type_p (*p_arg))
	{
	  float* vfp_slot = vfp_space + ecif->cif->vfp_args[vi++];
	  if ((*p_arg)->type == FFI_TYPE_FLOAT)
	    *((float*)vfp_slot) = *((float*)*p_argv);
	  else if ((*p_arg)->type == FFI_TYPE_DOUBLE)
	    *((double*)vfp_slot) = *((double*)*p_argv);
	  else
	    memcpy(vfp_slot, *p_argv, (*p_arg)->size);
	  p_argv++;
	  continue;
	}

      /* Align if necessary */
      alignment = (*p_arg)->alignment;
#ifdef _WIN32_WCE
      if (alignment > 4)
	alignment = 4;
#endif
      if ((alignment - 1) & (unsigned) argp) {
	argp = (char *) ALIGN(argp, alignment);
      }

      if ((*p_arg)->type == FFI_TYPE_STRUCT)
	argp = (char *) ALIGN(argp, 4);

	  z = (*p_arg)->size;
	  if (z < sizeof(int))
	    {
	      z = sizeof(int);
	      switch ((*p_arg)->type)
		{
		case FFI_TYPE_SINT8:
		  *(signed int *) argp = (signed int)*(SINT8 *)(* p_argv);
		  break;
		  
		case FFI_TYPE_UINT8:
		  *(unsigned int *) argp = (unsigned int)*(UINT8 *)(* p_argv);
		  break;
		  
		case FFI_TYPE_SINT16:
		  *(signed int *) argp = (signed int)*(SINT16 *)(* p_argv);
		  break;
		  
		case FFI_TYPE_UINT16:
		  *(unsigned int *) argp = (unsigned int)*(UINT16 *)(* p_argv);
		  break;
		  
		case FFI_TYPE_STRUCT:
		  memcpy(argp, *p_argv, (*p_arg)->size);
		  break;

		default:
		  FFI_ASSERT(0);
		}
	    }
	  else if (z == sizeof(int))
	    {
	      *(unsigned int *) argp = (unsigned int)*(UINT32 *)(* p_argv);
	    }
	  else
	    {
	      memcpy(argp, *p_argv, z);
	    }
	  p_argv++;
	  argp += z;
    }

  /* Indicate the VFP registers used. */
  return ecif->cif->vfp_used;
}

/* Perform machine dependent cif processing */
ffi_status ffi_prep_cif_machdep(ffi_cif *cif)
{
  int type_code;
  /* Round the stack up to a multiple of 8 bytes.  This isn't needed 
     everywhere, but it is on some platforms, and it doesn't harm anything
     when it isn't needed.  */
  cif->bytes = (cif->bytes + 7) & ~7;

  /* Set the return type flag */
  switch (cif->rtype->type)
    {
    case FFI_TYPE_VOID:
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
      cif->flags = (unsigned) cif->rtype->type;
      break;

    case FFI_TYPE_SINT64:
    case FFI_TYPE_UINT64:
      cif->flags = (unsigned) FFI_TYPE_SINT64;
      break;

    case FFI_TYPE_STRUCT:
      if (cif->abi == FFI_VFP
	  && (type_code = vfp_type_p (cif->rtype)) != 0)
	{
	  /* A Composite Type passed in VFP registers, either
	     FFI_TYPE_STRUCT_VFP_FLOAT or FFI_TYPE_STRUCT_VFP_DOUBLE. */
	  cif->flags = (unsigned) type_code;
	}
      else if (cif->rtype->size <= 4)
	/* A Composite Type not larger than 4 bytes is returned in r0.  */
	cif->flags = (unsigned)FFI_TYPE_INT;
      else
	/* A Composite Type larger than 4 bytes, or whose size cannot
	   be determined statically ... is stored in memory at an
	   address passed [in r0].  */
	cif->flags = (unsigned)FFI_TYPE_STRUCT;
      break;

    default:
      cif->flags = FFI_TYPE_INT;
      break;
    }

  /* Map out the register placements of VFP register args.
     The VFP hard-float calling conventions are slightly more sophisticated than
     the base calling conventions, so we do it here instead of in ffi_prep_args(). */
  if (cif->abi == FFI_VFP)
    layout_vfp_args (cif);

  return FFI_OK;
}

/* Perform machine dependent cif processing for variadic calls */
ffi_status ffi_prep_cif_machdep_var(ffi_cif *cif,
				    unsigned int nfixedargs,
				    unsigned int ntotalargs)
{
  /* VFP variadic calls actually use the SYSV ABI */
  if (cif->abi == FFI_VFP)
	cif->abi = FFI_SYSV;

  return ffi_prep_cif_machdep(cif);
}

/* Prototypes for assembly functions, in sysv.S */
extern void ffi_call_SYSV (void (*fn)(void), extended_cif *, unsigned, unsigned, unsigned *);
extern void ffi_call_VFP (void (*fn)(void), extended_cif *, unsigned, unsigned, unsigned *);

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue)
{
  extended_cif ecif;

  int small_struct = (cif->flags == FFI_TYPE_INT 
		      && cif->rtype->type == FFI_TYPE_STRUCT);
  int vfp_struct = (cif->flags == FFI_TYPE_STRUCT_VFP_FLOAT
		    || cif->flags == FFI_TYPE_STRUCT_VFP_DOUBLE);

  ecif.cif = cif;
  ecif.avalue = avalue;

  unsigned int temp;
  
  /* If the return value is a struct and we don't have a return	*/
  /* value address then we need to make one		        */

  if ((rvalue == NULL) && 
      (cif->flags == FFI_TYPE_STRUCT))
    {
      ecif.rvalue = alloca(cif->rtype->size);
    }
  else if (small_struct)
    ecif.rvalue = &temp;
  else if (vfp_struct)
    {
      /* Largest case is double x 4. */
      ecif.rvalue = alloca(32);
    }
  else
    ecif.rvalue = rvalue;

  switch (cif->abi) 
    {
    case FFI_SYSV:
      ffi_call_SYSV (fn, &ecif, cif->bytes, cif->flags, ecif.rvalue);
      break;

    case FFI_VFP:
#ifdef __ARM_EABI__
      ffi_call_VFP (fn, &ecif, cif->bytes, cif->flags, ecif.rvalue);
      break;
#endif

    default:
      FFI_ASSERT(0);
      break;
    }
  if (small_struct)
    memcpy (rvalue, &temp, cif->rtype->size);
  else if (vfp_struct)
    memcpy (rvalue, ecif.rvalue, cif->rtype->size);
}

/** private members **/

static void ffi_prep_incoming_args_SYSV (char *stack, void **ret,
					 void** args, ffi_cif* cif, float *vfp_stack);

void ffi_closure_SYSV (ffi_closure *);

void ffi_closure_VFP (ffi_closure *);

/* This function is jumped to by the trampoline */

unsigned int
ffi_closure_SYSV_inner (closure, respp, args, vfp_args)
     ffi_closure *closure;
     void **respp;
     void *args;
     void *vfp_args;
{
  // our various things...
  ffi_cif       *cif;
  void         **arg_area;

  cif         = closure->cif;
  arg_area    = (void**) alloca (cif->nargs * sizeof (void*));  

  /* this call will initialize ARG_AREA, such that each
   * element in that array points to the corresponding 
   * value on the stack; and if the function returns
   * a structure, it will re-set RESP to point to the
   * structure return address.  */

  ffi_prep_incoming_args_SYSV(args, respp, arg_area, cif, vfp_args);

  (closure->fun) (cif, *respp, arg_area, closure->user_data);

  return cif->flags;
}

/*@-exportheader@*/
static void 
ffi_prep_incoming_args_SYSV(char *stack, void **rvalue,
			    void **avalue, ffi_cif *cif,
			    /* Used only under VFP hard-float ABI. */
			    float *vfp_stack)
/*@=exportheader@*/
{
  register unsigned int i, vi = 0;
  register void **p_argv;
  register char *argp;
  register ffi_type **p_arg;

  argp = stack;

  if ( cif->flags == FFI_TYPE_STRUCT ) {
    *rvalue = *(void **) argp;
    argp += 4;
  }

  p_argv = avalue;

  for (i = cif->nargs, p_arg = cif->arg_types; (i != 0); i--, p_arg++)
    {
      size_t z;
      size_t alignment;
  
      if (cif->abi == FFI_VFP
	  && vi < cif->vfp_nargs && vfp_type_p (*p_arg))
	{
	  *p_argv++ = (void*)(vfp_stack + cif->vfp_args[vi++]);
	  continue;
	}

      alignment = (*p_arg)->alignment;
      if (alignment < 4)
	alignment = 4;
#ifdef _WIN32_WCE
      else
	if (alignment > 4)
	  alignment = 4;
#endif
      /* Align if necessary */
      if ((alignment - 1) & (unsigned) argp) {
	argp = (char *) ALIGN(argp, alignment);
      }

      z = (*p_arg)->size;

      /* because we're little endian, this is what it turns into.   */

      *p_argv = (void*) argp;

      p_argv++;
      argp += z;
    }
  
  return;
}

/* How to make a trampoline.  */

extern unsigned int ffi_arm_trampoline[3];

#if FFI_EXEC_TRAMPOLINE_TABLE

#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

extern void *ffi_closure_trampoline_table_page;

typedef struct ffi_trampoline_table ffi_trampoline_table;
typedef struct ffi_trampoline_table_entry ffi_trampoline_table_entry;

struct ffi_trampoline_table {
  /* contigious writable and executable pages */
  vm_address_t config_page;
  vm_address_t trampoline_page;

  /* free list tracking */
  uint16_t free_count;
  ffi_trampoline_table_entry *free_list;
  ffi_trampoline_table_entry *free_list_pool;

  ffi_trampoline_table *prev;
  ffi_trampoline_table *next;
};

struct ffi_trampoline_table_entry {
  void *(*trampoline)();
  ffi_trampoline_table_entry *next;
};

/* Override the standard architecture trampoline size */
// XXX TODO - Fix
#undef FFI_TRAMPOLINE_SIZE
#define FFI_TRAMPOLINE_SIZE 12

/* The trampoline configuration is placed at 4080 bytes prior to the trampoline's entry point */
#define FFI_TRAMPOLINE_CODELOC_CONFIG(codeloc) ((void **) (((uint8_t *) codeloc) - 4080));

/* The first 16 bytes of the config page are unused, as they are unaddressable from the trampoline page. */
#define FFI_TRAMPOLINE_CONFIG_PAGE_OFFSET 16

/* Total number of trampolines that fit in one trampoline table */
#define FFI_TRAMPOLINE_COUNT ((PAGE_SIZE - FFI_TRAMPOLINE_CONFIG_PAGE_OFFSET) / FFI_TRAMPOLINE_SIZE)

static pthread_mutex_t ffi_trampoline_lock = PTHREAD_MUTEX_INITIALIZER;
static ffi_trampoline_table *ffi_trampoline_tables = NULL;

static ffi_trampoline_table *
ffi_trampoline_table_alloc ()
{
  ffi_trampoline_table *table = NULL;

  /* Loop until we can allocate two contigious pages */
  while (table == NULL) {
    vm_address_t config_page = 0x0;
    kern_return_t kt;

    /* Try to allocate two pages */
    kt = vm_allocate (mach_task_self (), &config_page, PAGE_SIZE*2, VM_FLAGS_ANYWHERE);
    if (kt != KERN_SUCCESS) {
      fprintf(stderr, "vm_allocate() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);
      break;
    }

    /* Now drop the second half of the allocation to make room for the trampoline table */
    vm_address_t trampoline_page = config_page+PAGE_SIZE;
    kt = vm_deallocate (mach_task_self (), trampoline_page, PAGE_SIZE);
    if (kt != KERN_SUCCESS) {
      fprintf(stderr, "vm_deallocate() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);
      break;
    }

    /* Remap the trampoline table to directly follow the config page */
    vm_prot_t cur_prot;
    vm_prot_t max_prot;

    kt = vm_remap (mach_task_self (), &trampoline_page, PAGE_SIZE, 0x0, FALSE, mach_task_self (), (vm_address_t) &ffi_closure_trampoline_table_page, FALSE, &cur_prot, &max_prot, VM_INHERIT_SHARE);

    /* If we lost access to the destination trampoline page, drop our config allocation mapping and retry */
    if (kt != KERN_SUCCESS) {
      /* Log unexpected failures */
      if (kt != KERN_NO_SPACE) {
        fprintf(stderr, "vm_remap() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);
      }

      vm_deallocate (mach_task_self (), config_page, PAGE_SIZE);
      continue;
    }

    /* We have valid trampoline and config pages */
    table = calloc (1, sizeof(ffi_trampoline_table));
    table->free_count = FFI_TRAMPOLINE_COUNT;
    table->config_page = config_page;
    table->trampoline_page = trampoline_page;

    /* Create and initialize the free list */
    table->free_list_pool = calloc(FFI_TRAMPOLINE_COUNT, sizeof(ffi_trampoline_table_entry));

    uint16_t i;
    for (i = 0; i < table->free_count; i++) {
      ffi_trampoline_table_entry *entry = &table->free_list_pool[i];
      entry->trampoline = (void *) (table->trampoline_page + (i * FFI_TRAMPOLINE_SIZE));

      if (i < table->free_count - 1)
        entry->next = &table->free_list_pool[i+1];
    }

    table->free_list = table->free_list_pool;
  }

  return table;
}

void *
ffi_closure_alloc (size_t size, void **code)
{
  /* Create the closure */
  ffi_closure *closure = malloc(size);
  if (closure == NULL)
    return NULL;

  pthread_mutex_lock(&ffi_trampoline_lock);

  /* Check for an active trampoline table with available entries. */
  ffi_trampoline_table *table = ffi_trampoline_tables;
  if (table == NULL || table->free_list == NULL) {
    table = ffi_trampoline_table_alloc ();
    if (table == NULL) {
      free(closure);
      return NULL;
    }

    /* Insert the new table at the top of the list */
    table->next = ffi_trampoline_tables;
    if (table->next != NULL)
        table->next->prev = table;

    ffi_trampoline_tables = table;
  }

  /* Claim the free entry */
  ffi_trampoline_table_entry *entry = ffi_trampoline_tables->free_list;
  ffi_trampoline_tables->free_list = entry->next;
  ffi_trampoline_tables->free_count--;
  entry->next = NULL;

  pthread_mutex_unlock(&ffi_trampoline_lock);

  /* Initialize the return values */
  *code = entry->trampoline;
  closure->trampoline_table = table;
  closure->trampoline_table_entry = entry;

  return closure;
}

void
ffi_closure_free (void *ptr)
{
  ffi_closure *closure = ptr;

  pthread_mutex_lock(&ffi_trampoline_lock);

  /* Fetch the table and entry references */
  ffi_trampoline_table *table = closure->trampoline_table;
  ffi_trampoline_table_entry *entry = closure->trampoline_table_entry;

  /* Return the entry to the free list */
  entry->next = table->free_list;
  table->free_list = entry;
  table->free_count++;

  /* If all trampolines within this table are free, and at least one other table exists, deallocate
   * the table */
  if (table->free_count == FFI_TRAMPOLINE_COUNT && ffi_trampoline_tables != table) {
    /* Remove from the list */
    if (table->prev != NULL)
      table->prev->next = table->next;

    if (table->next != NULL)
      table->next->prev = table->prev;

    /* Deallocate pages */
    kern_return_t kt;
    kt = vm_deallocate (mach_task_self (), table->config_page, PAGE_SIZE);
    if (kt != KERN_SUCCESS)
      fprintf(stderr, "vm_deallocate() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);

    kt = vm_deallocate (mach_task_self (), table->trampoline_page, PAGE_SIZE);
    if (kt != KERN_SUCCESS)
      fprintf(stderr, "vm_deallocate() failure: %d at %s:%d\n", kt, __FILE__, __LINE__);

    /* Deallocate free list */
    free (table->free_list_pool);
    free (table);
  } else if (ffi_trampoline_tables != table) {
    /* Otherwise, bump this table to the top of the list */
    table->prev = NULL;
    table->next = ffi_trampoline_tables;
    if (ffi_trampoline_tables != NULL)
      ffi_trampoline_tables->prev = table;

    ffi_trampoline_tables = table;
  }

  pthread_mutex_unlock (&ffi_trampoline_lock);

  /* Free the closure */
  free (closure);
}

#else

#define FFI_INIT_TRAMPOLINE(TRAMP,FUN,CTX)				\
({ unsigned char *__tramp = (unsigned char*)(TRAMP);			\
   unsigned int  __fun = (unsigned int)(FUN);				\
   unsigned int  __ctx = (unsigned int)(CTX);				\
   unsigned char *insns = (unsigned char *)(CTX);                       \
   memcpy (__tramp, ffi_arm_trampoline, sizeof ffi_arm_trampoline);     \
   *(unsigned int*) &__tramp[12] = __ctx;				\
   *(unsigned int*) &__tramp[16] = __fun;				\
   __clear_cache((&__tramp[0]), (&__tramp[19])); /* Clear data mapping.  */ \
   __clear_cache(insns, insns + 3 * sizeof (unsigned int));             \
                                                 /* Clear instruction   \
                                                    mapping.  */        \
 })

#endif

/* the cif must already be prep'ed */

ffi_status
ffi_prep_closure_loc (ffi_closure* closure,
		      ffi_cif* cif,
		      void (*fun)(ffi_cif*,void*,void**,void*),
		      void *user_data,
		      void *codeloc)
{
  void (*closure_func)(ffi_closure*) = NULL;

  if (cif->abi == FFI_SYSV)
    closure_func = &ffi_closure_SYSV;
#ifdef __ARM_EABI__
  else if (cif->abi == FFI_VFP)
    closure_func = &ffi_closure_VFP;
#endif
  else
    return FFI_BAD_ABI;
    
#if FFI_EXEC_TRAMPOLINE_TABLE
  void **config = FFI_TRAMPOLINE_CODELOC_CONFIG(codeloc);
  config[0] = closure;
  config[1] = closure_func;
#else
  FFI_INIT_TRAMPOLINE (&closure->tramp[0], \
		       closure_func,  \
		       codeloc);
#endif

  closure->cif  = cif;
  closure->user_data = user_data;
  closure->fun  = fun;

  return FFI_OK;
}

/* Below are routines for VFP hard-float support. */

static int rec_vfp_type_p (ffi_type *t, int *elt, int *elnum)
{
  switch (t->type)
    {
    case FFI_TYPE_FLOAT:
    case FFI_TYPE_DOUBLE:
      *elt = (int) t->type;
      *elnum = 1;
      return 1;

    case FFI_TYPE_STRUCT_VFP_FLOAT:
      *elt = FFI_TYPE_FLOAT;
      *elnum = t->size / sizeof (float);
      return 1;

    case FFI_TYPE_STRUCT_VFP_DOUBLE:
      *elt = FFI_TYPE_DOUBLE;
      *elnum = t->size / sizeof (double);
      return 1;

    case FFI_TYPE_STRUCT:;
      {
	int base_elt = 0, total_elnum = 0;
	ffi_type **el = t->elements;
	while (*el)
	  {
	    int el_elt = 0, el_elnum = 0;
	    if (! rec_vfp_type_p (*el, &el_elt, &el_elnum)
		|| (base_elt && base_elt != el_elt)
		|| total_elnum + el_elnum > 4)
	      return 0;
	    base_elt = el_elt;
	    total_elnum += el_elnum;
	    el++;
	  }
	*elnum = total_elnum;
	*elt = base_elt;
	return 1;
      }
    default: ;
    }
  return 0;
}

static int vfp_type_p (ffi_type *t)
{
  int elt, elnum;
  if (rec_vfp_type_p (t, &elt, &elnum))
    {
      if (t->type == FFI_TYPE_STRUCT)
	{
	  if (elnum == 1)
	    t->type = elt;
	  else
	    t->type = (elt == FFI_TYPE_FLOAT
		       ? FFI_TYPE_STRUCT_VFP_FLOAT
		       : FFI_TYPE_STRUCT_VFP_DOUBLE);
	}
      return (int) t->type;
    }
  return 0;
}

static void place_vfp_arg (ffi_cif *cif, ffi_type *t)
{
  int reg = cif->vfp_reg_free;
  int nregs = t->size / sizeof (float);
  int align = ((t->type == FFI_TYPE_STRUCT_VFP_FLOAT
		|| t->type == FFI_TYPE_FLOAT) ? 1 : 2);
  /* Align register number. */
  if ((reg & 1) && align == 2)
    reg++;
  while (reg + nregs <= 16)
    {
      int s, new_used = 0;
      for (s = reg; s < reg + nregs; s++)
	{
	  new_used |= (1 << s);
	  if (cif->vfp_used & (1 << s))
	    {
	      reg += align;
	      goto next_reg;
	    }
	}
      /* Found regs to allocate. */
      cif->vfp_used |= new_used;
      cif->vfp_args[cif->vfp_nargs++] = reg;

      /* Update vfp_reg_free. */
      if (cif->vfp_used & (1 << cif->vfp_reg_free))
	{
	  reg += nregs;
	  while (cif->vfp_used & (1 << reg))
	    reg += 1;
	  cif->vfp_reg_free = reg;
	}
      return;
    next_reg: ;
    }
}

static void layout_vfp_args (ffi_cif *cif)
{
  int i;
  /* Init VFP fields */
  cif->vfp_used = 0;
  cif->vfp_nargs = 0;
  cif->vfp_reg_free = 0;
  memset (cif->vfp_args, -1, 16); /* Init to -1. */

  for (i = 0; i < cif->nargs; i++)
    {
      ffi_type *t = cif->arg_types[i];
      if (vfp_type_p (t))
	place_vfp_arg (cif, t);
    }
}
