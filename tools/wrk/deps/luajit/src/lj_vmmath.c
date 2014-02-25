/*
** Math helper functions for assembler VM.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_vmmath_c
#define LUA_CORE

#include <errno.h>
#include <math.h>

#include "lj_obj.h"
#include "lj_ir.h"
#include "lj_vm.h"

/* -- Helper functions for generated machine code ------------------------- */

#if LJ_TARGET_X86ORX64
/* Wrapper functions to avoid linker issues on OSX. */
LJ_FUNCA double lj_vm_sinh(double x) { return sinh(x); }
LJ_FUNCA double lj_vm_cosh(double x) { return cosh(x); }
LJ_FUNCA double lj_vm_tanh(double x) { return tanh(x); }
#endif

#if !LJ_TARGET_X86ORX64
double lj_vm_foldarith(double x, double y, int op)
{
  switch (op) {
  case IR_ADD - IR_ADD: return x+y; break;
  case IR_SUB - IR_ADD: return x-y; break;
  case IR_MUL - IR_ADD: return x*y; break;
  case IR_DIV - IR_ADD: return x/y; break;
  case IR_MOD - IR_ADD: return x-lj_vm_floor(x/y)*y; break;
  case IR_POW - IR_ADD: return pow(x, y); break;
  case IR_NEG - IR_ADD: return -x; break;
  case IR_ABS - IR_ADD: return fabs(x); break;
#if LJ_HASJIT
  case IR_ATAN2 - IR_ADD: return atan2(x, y); break;
  case IR_LDEXP - IR_ADD: return ldexp(x, (int)y); break;
  case IR_MIN - IR_ADD: return x > y ? y : x; break;
  case IR_MAX - IR_ADD: return x < y ? y : x; break;
#endif
  default: return x;
  }
}
#endif

#if LJ_HASJIT

#ifdef LUAJIT_NO_LOG2
double lj_vm_log2(double a)
{
  return log(a) * 1.4426950408889634074;
}
#endif

#ifdef LUAJIT_NO_EXP2
double lj_vm_exp2(double a)
{
  return exp(a * 0.6931471805599453);
}
#endif

#if !(LJ_TARGET_ARM || LJ_TARGET_PPC)
int32_t LJ_FASTCALL lj_vm_modi(int32_t a, int32_t b)
{
  uint32_t y, ua, ub;
  lua_assert(b != 0);  /* This must be checked before using this function. */
  ua = a < 0 ? (uint32_t)-a : (uint32_t)a;
  ub = b < 0 ? (uint32_t)-b : (uint32_t)b;
  y = ua % ub;
  if (y != 0 && (a^b) < 0) y = y - ub;
  if (((int32_t)y^b) < 0) y = (uint32_t)-(int32_t)y;
  return (int32_t)y;
}
#endif

#if !LJ_TARGET_X86ORX64
/* Unsigned x^k. */
static double lj_vm_powui(double x, uint32_t k)
{
  double y;
  lua_assert(k != 0);
  for (; (k & 1) == 0; k >>= 1) x *= x;
  y = x;
  if ((k >>= 1) != 0) {
    for (;;) {
      x *= x;
      if (k == 1) break;
      if (k & 1) y *= x;
      k >>= 1;
    }
    y *= x;
  }
  return y;
}

/* Signed x^k. */
double lj_vm_powi(double x, int32_t k)
{
  if (k > 1)
    return lj_vm_powui(x, (uint32_t)k);
  else if (k == 1)
    return x;
  else if (k == 0)
    return 1.0;
  else
    return 1.0 / lj_vm_powui(x, (uint32_t)-k);
}

/* Computes fpm(x) for extended math functions. */
double lj_vm_foldfpm(double x, int fpm)
{
  switch (fpm) {
  case IRFPM_FLOOR: return lj_vm_floor(x);
  case IRFPM_CEIL: return lj_vm_ceil(x);
  case IRFPM_TRUNC: return lj_vm_trunc(x);
  case IRFPM_SQRT: return sqrt(x);
  case IRFPM_EXP: return exp(x);
  case IRFPM_EXP2: return lj_vm_exp2(x);
  case IRFPM_LOG: return log(x);
  case IRFPM_LOG2: return lj_vm_log2(x);
  case IRFPM_LOG10: return log10(x);
  case IRFPM_SIN: return sin(x);
  case IRFPM_COS: return cos(x);
  case IRFPM_TAN: return tan(x);
  default: lua_assert(0);
  }
  return 0;
}
#endif

#if LJ_HASFFI
int lj_vm_errno(void)
{
  return errno;
}
#endif

#endif
