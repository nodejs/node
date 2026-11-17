/* Area:	ffi_call, closure_call
   Purpose:	Check complex arguments after the argument registers run low.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run { xfail mips64*-*-* } } */
/* XFAIL: mips N64 complex-argument ABI bug; the little-endian variant
   fails the complex-double case too.  See libffi issue #1071.  */

#include "complex_defs_double.inc"
#include "complex_regs.inc"
