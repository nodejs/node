/* Area:	ffi_call, closure_call
   Purpose:	Check complex arguments after the argument registers run low.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run { xfail mips64*-*-* } } */
/* XFAIL: mips N64 ABI bug in complex-float/longdouble args and fp-struct
   returns; see libffi issue #1071.  */

#include "complex_defs_float.inc"
#include "complex_regs.inc"
