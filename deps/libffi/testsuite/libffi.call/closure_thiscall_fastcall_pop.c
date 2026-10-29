/* Area:	closure, ffi_prep_closure_loc
   Purpose:	Check i386 THISCALL/FASTCALL closures pop the stack correctly.
   Limitations:	i386 + GNU inline asm only; a no-op elsewhere.
   PR:		none.
   Originator:	i386 closure stack-pop accounting regression.

   THISCALL and FASTCALL are callee-clean: the closure must remove its
   stack-resident arguments on return (ret $n).  When a 64-bit integer or
   a struct argument is placed on the stack, the closure return path used
   to compute the pop as cif->bytes - narg_reg*4 with narg_reg force-bumped
   to 2, discounting register slots that were never used and under-popping
   the stack.  A caller that relies on callee cleanup is then left with the
   argument bytes where its return address should be.

   This test invokes the generated closure through a minimal callee-clean
   call site and checks that ESP is balanced across the call (delta 0).
   Without the fix the delta is 8 (FASTCALL uint64) or 4 (THISCALL).  */

/* { dg-do run } */
#include "ffitest.h"

#if defined(__i386__) && defined(__GNUC__) && !defined(__APPLE__)

static uint64_t received;
static int ran;

static void
cb (ffi_cif *cif, void *resp, void **args, void *userdata)
{
  (void) cif; (void) resp; (void) userdata;
  received = *(uint64_t *) args[cif->nargs - 1];
  ran++;
}

/* Push an 8-byte stack argument, load ECX (the thiscall "this" register,
   ignored by the fastcall callee), call the closure, and return how many
   bytes the callee under-popped (0 == it popped exactly what was pushed).

   Every operand is read into a register up front, while ESP is still at
   its incoming value, so nothing is referenced through an ESP-relative
   memory operand after we start moving ESP (which would otherwise read a
   stale slot, on clang at -O2 in particular).  The stack is then 16-byte
   aligned at the call as the i386 psABI requires, so the -O2-built closure
   body may use aligned SSE without faulting; the alignment cancels out of
   the delta.  ESP is restored to its exact incoming value before the delta
   is stored, so a wrong pop cannot corrupt our frame.  Not using EBX keeps
   this compatible with -fPIC; the delta is returned via memory so no free
   register is needed for it.  */
static int
esp_delta (void *code, uint64_t stackarg, unsigned ecxv)
{
  unsigned delta;
  unsigned lo = (unsigned) stackarg;
  unsigned hi = (unsigned) (stackarg >> 32);
  __asm__ volatile (
      "movl %[lo], %%eax\n\t"       /* stash all operands in registers   */
      "movl %[hi], %%edx\n\t"       /* before ESP moves                  */
      "movl %[code], %%edi\n\t"
      "movl %[ecxv], %%ecx\n\t"     /* thiscall 'this'                   */
      "movl %%esp, %%esi\n\t"       /* remember the real esp             */
      "andl $-16, %%esp\n\t"        /* 16-byte align, then bias by the   */
      "subl $8, %%esp\n\t"          /* 8 arg bytes so 'call' is 0 mod 16 */
      "pushl %%edx\n\t"             /* high dword                        */
      "pushl %%eax\n\t"             /* low dword                         */
      "calll *%%edi\n\t"
      "movl %%esi, %%eax\n\t"       /* recompute esp just before the     */
      "andl $-16, %%eax\n\t"        /* pushes...                         */
      "subl $8, %%eax\n\t"
      "subl %%esp, %%eax\n\t"       /* eax = under-popped byte count     */
      "movl %%esi, %%esp\n\t"       /* restore before touching memory    */
      "movl %%eax, %[delta]\n\t"
      : [delta] "=m" (delta)
      : [lo] "m" (lo), [hi] "m" (hi), [code] "m" (code), [ecxv] "m" (ecxv)
      : "memory", "cc", "eax", "ecx", "edx", "esi", "edi");
  return (int) delta;
}

static int
check_abi (ffi_abi abi, unsigned nargs, ffi_type **atypes, unsigned ecx)
{
  ffi_cif cif;
  ffi_closure *closure;
  void *code;
  int delta;

  closure = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (closure != NULL);
  CHECK (ffi_prep_cif (&cif, abi, nargs, &ffi_type_void, atypes) == FFI_OK);
  CHECK (ffi_prep_closure_loc (closure, &cif, cb, NULL, code) == FFI_OK);

  ran = 0;
  received = 0;
  delta = esp_delta (code, 0x1122334455667788ULL, ecx);

  CHECK (ran == 1);
  CHECK (received == 0x1122334455667788ULL);
  ffi_closure_free (closure);
  return delta;
}

int
main (void)
{
  ffi_type *fastcall_args[1] = { &ffi_type_uint64 };
  ffi_type *thiscall_args[2] = { &ffi_type_pointer, &ffi_type_uint64 };
  int d;

  /* FASTCALL void cb(uint64_t): the uint64 is stack-resident; pop must be 8. */
  d = check_abi (FFI_FASTCALL, 1, fastcall_args, 0);
  printf ("FASTCALL uint64 esp delta: %d\n", d);
  CHECK (d == 0);

  /* THISCALL void cb(void*, uint64_t): 'this' in ECX, uint64 on the stack;
     pop must be 8 (not 4).  */
  d = check_abi (FFI_THISCALL, 2, thiscall_args, 0xdeadbeef);
  printf ("THISCALL this+uint64 esp delta: %d\n", d);
  CHECK (d == 0);

  exit (0);
}

#else

int
main (void)
{
  /* Not an i386 GNU target: nothing to check here. */
  exit (0);
}

#endif
