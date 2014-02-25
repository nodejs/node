/*
** FOLD: Constant Folding, Algebraic Simplifications and Reassociation.
** ABCelim: Array Bounds Check Elimination.
** CSE: Common-Subexpression Elimination.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_opt_fold_c
#define LUA_CORE

#include <math.h>

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_str.h"
#include "lj_tab.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_carith.h"
#include "lj_vm.h"
#include "lj_strscan.h"

/* Here's a short description how the FOLD engine processes instructions:
**
** The FOLD engine receives a single instruction stored in fins (J->fold.ins).
** The instruction and its operands are used to select matching fold rules.
** These are applied iteratively until a fixed point is reached.
**
** The 8 bit opcode of the instruction itself plus the opcodes of the
** two instructions referenced by its operands form a 24 bit key
** 'ins left right' (unused operands -> 0, literals -> lowest 8 bits).
**
** This key is used for partial matching against the fold rules. The
** left/right operand fields of the key are successively masked with
** the 'any' wildcard, from most specific to least specific:
**
**   ins left right
**   ins any  right
**   ins left any
**   ins any  any
**
** The masked key is used to lookup a matching fold rule in a semi-perfect
** hash table. If a matching rule is found, the related fold function is run.
** Multiple rules can share the same fold function. A fold rule may return
** one of several special values:
**
** - NEXTFOLD means no folding was applied, because an additional test
**   inside the fold function failed. Matching continues against less
**   specific fold rules. Finally the instruction is passed on to CSE.
**
** - RETRYFOLD means the instruction was modified in-place. Folding is
**   retried as if this instruction had just been received.
**
** All other return values are terminal actions -- no further folding is
** applied:
**
** - INTFOLD(i) returns a reference to the integer constant i.
**
** - LEFTFOLD and RIGHTFOLD return the left/right operand reference
**   without emitting an instruction.
**
** - CSEFOLD and EMITFOLD pass the instruction directly to CSE or emit
**   it without passing through any further optimizations.
**
** - FAILFOLD, DROPFOLD and CONDFOLD only apply to instructions which have
**   no result (e.g. guarded assertions): FAILFOLD means the guard would
**   always fail, i.e. the current trace is pointless. DROPFOLD means
**   the guard is always true and has been eliminated. CONDFOLD is a
**   shortcut for FAILFOLD + cond (i.e. drop if true, otherwise fail).
**
** - Any other return value is interpreted as an IRRef or TRef. This
**   can be a reference to an existing or a newly created instruction.
**   Only the least-significant 16 bits (IRRef1) are used to form a TRef
**   which is finally returned to the caller.
**
** The FOLD engine receives instructions both from the trace recorder and
** substituted instructions from LOOP unrolling. This means all types
** of instructions may end up here, even though the recorder bypasses
** FOLD in some cases. Thus all loads, stores and allocations must have
** an any/any rule to avoid being passed on to CSE.
**
** Carefully read the following requirements before adding or modifying
** any fold rules:
**
** Requirement #1: All fold rules must preserve their destination type.
**
** Consistently use INTFOLD() (KINT result) or lj_ir_knum() (KNUM result).
** Never use lj_ir_knumint() which can have either a KINT or KNUM result.
**
** Requirement #2: Fold rules should not create *new* instructions which
** reference operands *across* PHIs.
**
** E.g. a RETRYFOLD with 'fins->op1 = fleft->op1' is invalid if the
** left operand is a PHI. Then fleft->op1 would point across the PHI
** frontier to an invariant instruction. Adding a PHI for this instruction
** would be counterproductive. The solution is to add a barrier which
** prevents folding across PHIs, i.e. 'PHIBARRIER(fleft)' in this case.
** The only exception is for recurrences with high latencies like
** repeated int->num->int conversions.
**
** One could relax this condition a bit if the referenced instruction is
** a PHI, too. But this often leads to worse code due to excessive
** register shuffling.
**
** Note: returning *existing* instructions (e.g. LEFTFOLD) is ok, though.
** Even returning fleft->op1 would be ok, because a new PHI will added,
** if needed. But again, this leads to excessive register shuffling and
** should be avoided.
**
** Requirement #3: The set of all fold rules must be monotonic to guarantee
** termination.
**
** The goal is optimization, so one primarily wants to add strength-reducing
** rules. This means eliminating an instruction or replacing an instruction
** with one or more simpler instructions. Don't add fold rules which point
** into the other direction.
**
** Some rules (like commutativity) do not directly reduce the strength of
** an instruction, but enable other fold rules (e.g. by moving constants
** to the right operand). These rules must be made unidirectional to avoid
** cycles.
**
** Rule of thumb: the trace recorder expands the IR and FOLD shrinks it.
*/

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)		(&J->cur.ir[(ref)])
#define fins		(&J->fold.ins)
#define fleft		(&J->fold.left)
#define fright		(&J->fold.right)
#define knumleft	(ir_knum(fleft)->n)
#define knumright	(ir_knum(fright)->n)

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Fold function type. Fastcall on x86 significantly reduces their size. */
typedef IRRef (LJ_FASTCALL *FoldFunc)(jit_State *J);

/* Macros for the fold specs, so buildvm can recognize them. */
#define LJFOLD(x)
#define LJFOLDX(x)
#define LJFOLDF(name)	static TRef LJ_FASTCALL fold_##name(jit_State *J)
/* Note: They must be at the start of a line or buildvm ignores them! */

/* Barrier to prevent using operands across PHIs. */
#define PHIBARRIER(ir)	if (irt_isphi((ir)->t)) return NEXTFOLD

/* Barrier to prevent folding across a GC step.
** GC steps can only happen at the head of a trace and at LOOP.
** And the GC is only driven forward if there is at least one allocation.
*/
#define gcstep_barrier(J, ref) \
  ((ref) < J->chain[IR_LOOP] && \
   (J->chain[IR_SNEW] || J->chain[IR_XSNEW] || \
    J->chain[IR_TNEW] || J->chain[IR_TDUP] || \
    J->chain[IR_CNEW] || J->chain[IR_CNEWI] || J->chain[IR_TOSTR]))

/* -- Constant folding for FP numbers ------------------------------------- */

LJFOLD(ADD KNUM KNUM)
LJFOLD(SUB KNUM KNUM)
LJFOLD(MUL KNUM KNUM)
LJFOLD(DIV KNUM KNUM)
LJFOLD(NEG KNUM KNUM)
LJFOLD(ABS KNUM KNUM)
LJFOLD(ATAN2 KNUM KNUM)
LJFOLD(LDEXP KNUM KNUM)
LJFOLD(MIN KNUM KNUM)
LJFOLD(MAX KNUM KNUM)
LJFOLDF(kfold_numarith)
{
  lua_Number a = knumleft;
  lua_Number b = knumright;
  lua_Number y = lj_vm_foldarith(a, b, fins->o - IR_ADD);
  return lj_ir_knum(J, y);
}

LJFOLD(LDEXP KNUM KINT)
LJFOLDF(kfold_ldexp)
{
#if LJ_TARGET_X86ORX64
  UNUSED(J);
  return NEXTFOLD;
#else
  return lj_ir_knum(J, ldexp(knumleft, fright->i));
#endif
}

LJFOLD(FPMATH KNUM any)
LJFOLDF(kfold_fpmath)
{
  lua_Number a = knumleft;
  lua_Number y = lj_vm_foldfpm(a, fins->op2);
  return lj_ir_knum(J, y);
}

LJFOLD(POW KNUM KINT)
LJFOLDF(kfold_numpow)
{
  lua_Number a = knumleft;
  lua_Number b = (lua_Number)fright->i;
  lua_Number y = lj_vm_foldarith(a, b, IR_POW - IR_ADD);
  return lj_ir_knum(J, y);
}

/* Must not use kfold_kref for numbers (could be NaN). */
LJFOLD(EQ KNUM KNUM)
LJFOLD(NE KNUM KNUM)
LJFOLD(LT KNUM KNUM)
LJFOLD(GE KNUM KNUM)
LJFOLD(LE KNUM KNUM)
LJFOLD(GT KNUM KNUM)
LJFOLD(ULT KNUM KNUM)
LJFOLD(UGE KNUM KNUM)
LJFOLD(ULE KNUM KNUM)
LJFOLD(UGT KNUM KNUM)
LJFOLDF(kfold_numcomp)
{
  return CONDFOLD(lj_ir_numcmp(knumleft, knumright, (IROp)fins->o));
}

/* -- Constant folding for 32 bit integers -------------------------------- */

static int32_t kfold_intop(int32_t k1, int32_t k2, IROp op)
{
  switch (op) {
  case IR_ADD: k1 += k2; break;
  case IR_SUB: k1 -= k2; break;
  case IR_MUL: k1 *= k2; break;
  case IR_MOD: k1 = lj_vm_modi(k1, k2); break;
  case IR_NEG: k1 = -k1; break;
  case IR_BAND: k1 &= k2; break;
  case IR_BOR: k1 |= k2; break;
  case IR_BXOR: k1 ^= k2; break;
  case IR_BSHL: k1 <<= (k2 & 31); break;
  case IR_BSHR: k1 = (int32_t)((uint32_t)k1 >> (k2 & 31)); break;
  case IR_BSAR: k1 >>= (k2 & 31); break;
  case IR_BROL: k1 = (int32_t)lj_rol((uint32_t)k1, (k2 & 31)); break;
  case IR_BROR: k1 = (int32_t)lj_ror((uint32_t)k1, (k2 & 31)); break;
  case IR_MIN: k1 = k1 < k2 ? k1 : k2; break;
  case IR_MAX: k1 = k1 > k2 ? k1 : k2; break;
  default: lua_assert(0); break;
  }
  return k1;
}

LJFOLD(ADD KINT KINT)
LJFOLD(SUB KINT KINT)
LJFOLD(MUL KINT KINT)
LJFOLD(MOD KINT KINT)
LJFOLD(NEG KINT KINT)
LJFOLD(BAND KINT KINT)
LJFOLD(BOR KINT KINT)
LJFOLD(BXOR KINT KINT)
LJFOLD(BSHL KINT KINT)
LJFOLD(BSHR KINT KINT)
LJFOLD(BSAR KINT KINT)
LJFOLD(BROL KINT KINT)
LJFOLD(BROR KINT KINT)
LJFOLD(MIN KINT KINT)
LJFOLD(MAX KINT KINT)
LJFOLDF(kfold_intarith)
{
  return INTFOLD(kfold_intop(fleft->i, fright->i, (IROp)fins->o));
}

LJFOLD(ADDOV KINT KINT)
LJFOLD(SUBOV KINT KINT)
LJFOLD(MULOV KINT KINT)
LJFOLDF(kfold_intovarith)
{
  lua_Number n = lj_vm_foldarith((lua_Number)fleft->i, (lua_Number)fright->i,
				 fins->o - IR_ADDOV);
  int32_t k = lj_num2int(n);
  if (n != (lua_Number)k)
    return FAILFOLD;
  return INTFOLD(k);
}

LJFOLD(BNOT KINT)
LJFOLDF(kfold_bnot)
{
  return INTFOLD(~fleft->i);
}

LJFOLD(BSWAP KINT)
LJFOLDF(kfold_bswap)
{
  return INTFOLD((int32_t)lj_bswap((uint32_t)fleft->i));
}

LJFOLD(LT KINT KINT)
LJFOLD(GE KINT KINT)
LJFOLD(LE KINT KINT)
LJFOLD(GT KINT KINT)
LJFOLD(ULT KINT KINT)
LJFOLD(UGE KINT KINT)
LJFOLD(ULE KINT KINT)
LJFOLD(UGT KINT KINT)
LJFOLD(ABC KINT KINT)
LJFOLDF(kfold_intcomp)
{
  int32_t a = fleft->i, b = fright->i;
  switch ((IROp)fins->o) {
  case IR_LT: return CONDFOLD(a < b);
  case IR_GE: return CONDFOLD(a >= b);
  case IR_LE: return CONDFOLD(a <= b);
  case IR_GT: return CONDFOLD(a > b);
  case IR_ULT: return CONDFOLD((uint32_t)a < (uint32_t)b);
  case IR_UGE: return CONDFOLD((uint32_t)a >= (uint32_t)b);
  case IR_ULE: return CONDFOLD((uint32_t)a <= (uint32_t)b);
  case IR_ABC:
  case IR_UGT: return CONDFOLD((uint32_t)a > (uint32_t)b);
  default: lua_assert(0); return FAILFOLD;
  }
}

LJFOLD(UGE any KINT)
LJFOLDF(kfold_intcomp0)
{
  if (fright->i == 0)
    return DROPFOLD;
  return NEXTFOLD;
}

/* -- Constant folding for 64 bit integers -------------------------------- */

static uint64_t kfold_int64arith(uint64_t k1, uint64_t k2, IROp op)
{
  switch (op) {
#if LJ_64 || LJ_HASFFI
  case IR_ADD: k1 += k2; break;
  case IR_SUB: k1 -= k2; break;
#endif
#if LJ_HASFFI
  case IR_MUL: k1 *= k2; break;
  case IR_BAND: k1 &= k2; break;
  case IR_BOR: k1 |= k2; break;
  case IR_BXOR: k1 ^= k2; break;
#endif
  default: UNUSED(k2); lua_assert(0); break;
  }
  return k1;
}

LJFOLD(ADD KINT64 KINT64)
LJFOLD(SUB KINT64 KINT64)
LJFOLD(MUL KINT64 KINT64)
LJFOLD(BAND KINT64 KINT64)
LJFOLD(BOR KINT64 KINT64)
LJFOLD(BXOR KINT64 KINT64)
LJFOLDF(kfold_int64arith)
{
  return INT64FOLD(kfold_int64arith(ir_k64(fleft)->u64,
				    ir_k64(fright)->u64, (IROp)fins->o));
}

LJFOLD(DIV KINT64 KINT64)
LJFOLD(MOD KINT64 KINT64)
LJFOLD(POW KINT64 KINT64)
LJFOLDF(kfold_int64arith2)
{
#if LJ_HASFFI
  uint64_t k1 = ir_k64(fleft)->u64, k2 = ir_k64(fright)->u64;
  if (irt_isi64(fins->t)) {
    k1 = fins->o == IR_DIV ? lj_carith_divi64((int64_t)k1, (int64_t)k2) :
	 fins->o == IR_MOD ? lj_carith_modi64((int64_t)k1, (int64_t)k2) :
			     lj_carith_powi64((int64_t)k1, (int64_t)k2);
  } else {
    k1 = fins->o == IR_DIV ? lj_carith_divu64(k1, k2) :
	 fins->o == IR_MOD ? lj_carith_modu64(k1, k2) :
			     lj_carith_powu64(k1, k2);
  }
  return INT64FOLD(k1);
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

LJFOLD(BSHL KINT64 KINT)
LJFOLD(BSHR KINT64 KINT)
LJFOLD(BSAR KINT64 KINT)
LJFOLD(BROL KINT64 KINT)
LJFOLD(BROR KINT64 KINT)
LJFOLDF(kfold_int64shift)
{
#if LJ_HASFFI || LJ_64
  uint64_t k = ir_k64(fleft)->u64;
  int32_t sh = (fright->i & 63);
  switch ((IROp)fins->o) {
  case IR_BSHL: k <<= sh; break;
#if LJ_HASFFI
  case IR_BSHR: k >>= sh; break;
  case IR_BSAR: k = (uint64_t)((int64_t)k >> sh); break;
  case IR_BROL: k = lj_rol(k, sh); break;
  case IR_BROR: k = lj_ror(k, sh); break;
#endif
  default: lua_assert(0); break;
  }
  return INT64FOLD(k);
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

LJFOLD(BNOT KINT64)
LJFOLDF(kfold_bnot64)
{
#if LJ_HASFFI
  return INT64FOLD(~ir_k64(fleft)->u64);
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

LJFOLD(BSWAP KINT64)
LJFOLDF(kfold_bswap64)
{
#if LJ_HASFFI
  return INT64FOLD(lj_bswap64(ir_k64(fleft)->u64));
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

LJFOLD(LT KINT64 KINT64)
LJFOLD(GE KINT64 KINT64)
LJFOLD(LE KINT64 KINT64)
LJFOLD(GT KINT64 KINT64)
LJFOLD(ULT KINT64 KINT64)
LJFOLD(UGE KINT64 KINT64)
LJFOLD(ULE KINT64 KINT64)
LJFOLD(UGT KINT64 KINT64)
LJFOLDF(kfold_int64comp)
{
#if LJ_HASFFI
  uint64_t a = ir_k64(fleft)->u64, b = ir_k64(fright)->u64;
  switch ((IROp)fins->o) {
  case IR_LT: return CONDFOLD(a < b);
  case IR_GE: return CONDFOLD(a >= b);
  case IR_LE: return CONDFOLD(a <= b);
  case IR_GT: return CONDFOLD(a > b);
  case IR_ULT: return CONDFOLD((uint64_t)a < (uint64_t)b);
  case IR_UGE: return CONDFOLD((uint64_t)a >= (uint64_t)b);
  case IR_ULE: return CONDFOLD((uint64_t)a <= (uint64_t)b);
  case IR_UGT: return CONDFOLD((uint64_t)a > (uint64_t)b);
  default: lua_assert(0); return FAILFOLD;
  }
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

LJFOLD(UGE any KINT64)
LJFOLDF(kfold_int64comp0)
{
#if LJ_HASFFI
  if (ir_k64(fright)->u64 == 0)
    return DROPFOLD;
  return NEXTFOLD;
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

/* -- Constant folding for strings ---------------------------------------- */

LJFOLD(SNEW KKPTR KINT)
LJFOLDF(kfold_snew_kptr)
{
  GCstr *s = lj_str_new(J->L, (const char *)ir_kptr(fleft), (size_t)fright->i);
  return lj_ir_kstr(J, s);
}

LJFOLD(SNEW any KINT)
LJFOLDF(kfold_snew_empty)
{
  if (fright->i == 0)
    return lj_ir_kstr(J, &J2G(J)->strempty);
  return NEXTFOLD;
}

LJFOLD(STRREF KGC KINT)
LJFOLDF(kfold_strref)
{
  GCstr *str = ir_kstr(fleft);
  lua_assert((MSize)fright->i <= str->len);
  return lj_ir_kkptr(J, (char *)strdata(str) + fright->i);
}

LJFOLD(STRREF SNEW any)
LJFOLDF(kfold_strref_snew)
{
  PHIBARRIER(fleft);
  if (irref_isk(fins->op2) && fright->i == 0) {
    return fleft->op1;  /* strref(snew(ptr, len), 0) ==> ptr */
  } else {
    /* Reassociate: strref(snew(strref(str, a), len), b) ==> strref(str, a+b) */
    IRIns *ir = IR(fleft->op1);
    IRRef1 str = ir->op1;  /* IRIns * is not valid across emitir. */
    lua_assert(ir->o == IR_STRREF);
    PHIBARRIER(ir);
    fins->op2 = emitir(IRTI(IR_ADD), ir->op2, fins->op2);  /* Clobbers fins! */
    fins->op1 = str;
    fins->ot = IRT(IR_STRREF, IRT_P32);
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(CALLN CARG IRCALL_lj_str_cmp)
LJFOLDF(kfold_strcmp)
{
  if (irref_isk(fleft->op1) && irref_isk(fleft->op2)) {
    GCstr *a = ir_kstr(IR(fleft->op1));
    GCstr *b = ir_kstr(IR(fleft->op2));
    return INTFOLD(lj_str_cmp(a, b));
  }
  return NEXTFOLD;
}

/* -- Constant folding of pointer arithmetic ------------------------------ */

LJFOLD(ADD KGC KINT)
LJFOLD(ADD KGC KINT64)
LJFOLDF(kfold_add_kgc)
{
  GCobj *o = ir_kgc(fleft);
#if LJ_64
  ptrdiff_t ofs = (ptrdiff_t)ir_kint64(fright)->u64;
#else
  ptrdiff_t ofs = fright->i;
#endif
#if LJ_HASFFI
  if (irt_iscdata(fleft->t)) {
    CType *ct = ctype_raw(ctype_ctsG(J2G(J)), gco2cd(o)->ctypeid);
    if (ctype_isnum(ct->info) || ctype_isenum(ct->info) ||
	ctype_isptr(ct->info) || ctype_isfunc(ct->info) ||
	ctype_iscomplex(ct->info) || ctype_isvector(ct->info))
      return lj_ir_kkptr(J, (char *)o + ofs);
  }
#endif
  return lj_ir_kptr(J, (char *)o + ofs);
}

LJFOLD(ADD KPTR KINT)
LJFOLD(ADD KPTR KINT64)
LJFOLD(ADD KKPTR KINT)
LJFOLD(ADD KKPTR KINT64)
LJFOLDF(kfold_add_kptr)
{
  void *p = ir_kptr(fleft);
#if LJ_64
  ptrdiff_t ofs = (ptrdiff_t)ir_kint64(fright)->u64;
#else
  ptrdiff_t ofs = fright->i;
#endif
  return lj_ir_kptr_(J, fleft->o, (char *)p + ofs);
}

LJFOLD(ADD any KGC)
LJFOLD(ADD any KPTR)
LJFOLD(ADD any KKPTR)
LJFOLDF(kfold_add_kright)
{
  if (fleft->o == IR_KINT || fleft->o == IR_KINT64) {
    IRRef1 tmp = fins->op1; fins->op1 = fins->op2; fins->op2 = tmp;
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

/* -- Constant folding of conversions ------------------------------------- */

LJFOLD(TOBIT KNUM KNUM)
LJFOLDF(kfold_tobit)
{
  return INTFOLD(lj_num2bit(knumleft));
}

LJFOLD(CONV KINT IRCONV_NUM_INT)
LJFOLDF(kfold_conv_kint_num)
{
  return lj_ir_knum(J, (lua_Number)fleft->i);
}

LJFOLD(CONV KINT IRCONV_NUM_U32)
LJFOLDF(kfold_conv_kintu32_num)
{
  return lj_ir_knum(J, (lua_Number)(uint32_t)fleft->i);
}

LJFOLD(CONV KINT IRCONV_INT_I8)
LJFOLD(CONV KINT IRCONV_INT_U8)
LJFOLD(CONV KINT IRCONV_INT_I16)
LJFOLD(CONV KINT IRCONV_INT_U16)
LJFOLDF(kfold_conv_kint_ext)
{
  int32_t k = fleft->i;
  if ((fins->op2 & IRCONV_SRCMASK) == IRT_I8) k = (int8_t)k;
  else if ((fins->op2 & IRCONV_SRCMASK) == IRT_U8) k = (uint8_t)k;
  else if ((fins->op2 & IRCONV_SRCMASK) == IRT_I16) k = (int16_t)k;
  else k = (uint16_t)k;
  return INTFOLD(k);
}

LJFOLD(CONV KINT IRCONV_I64_INT)
LJFOLD(CONV KINT IRCONV_U64_INT)
LJFOLD(CONV KINT IRCONV_I64_U32)
LJFOLD(CONV KINT IRCONV_U64_U32)
LJFOLDF(kfold_conv_kint_i64)
{
  if ((fins->op2 & IRCONV_SEXT))
    return INT64FOLD((uint64_t)(int64_t)fleft->i);
  else
    return INT64FOLD((uint64_t)(int64_t)(uint32_t)fleft->i);
}

LJFOLD(CONV KINT64 IRCONV_NUM_I64)
LJFOLDF(kfold_conv_kint64_num_i64)
{
  return lj_ir_knum(J, (lua_Number)(int64_t)ir_kint64(fleft)->u64);
}

LJFOLD(CONV KINT64 IRCONV_NUM_U64)
LJFOLDF(kfold_conv_kint64_num_u64)
{
  return lj_ir_knum(J, (lua_Number)ir_kint64(fleft)->u64);
}

LJFOLD(CONV KINT64 IRCONV_INT_I64)
LJFOLD(CONV KINT64 IRCONV_U32_I64)
LJFOLDF(kfold_conv_kint64_int_i64)
{
  return INTFOLD((int32_t)ir_kint64(fleft)->u64);
}

LJFOLD(CONV KNUM IRCONV_INT_NUM)
LJFOLDF(kfold_conv_knum_int_num)
{
  lua_Number n = knumleft;
  if (!(fins->op2 & IRCONV_TRUNC)) {
    int32_t k = lj_num2int(n);
    if (irt_isguard(fins->t) && n != (lua_Number)k) {
      /* We're about to create a guard which always fails, like CONV +1.5.
      ** Some pathological loops cause this during LICM, e.g.:
      **   local x,k,t = 0,1.5,{1,[1.5]=2}
      **   for i=1,200 do x = x+ t[k]; k = k == 1 and 1.5 or 1 end
      **   assert(x == 300)
      */
      return FAILFOLD;
    }
    return INTFOLD(k);
  } else {
    return INTFOLD((int32_t)n);
  }
}

LJFOLD(CONV KNUM IRCONV_U32_NUM)
LJFOLDF(kfold_conv_knum_u32_num)
{
  lua_assert((fins->op2 & IRCONV_TRUNC));
#ifdef _MSC_VER
  {  /* Workaround for MSVC bug. */
    volatile uint32_t u = (uint32_t)knumleft;
    return INTFOLD((int32_t)u);
  }
#else
  return INTFOLD((int32_t)(uint32_t)knumleft);
#endif
}

LJFOLD(CONV KNUM IRCONV_I64_NUM)
LJFOLDF(kfold_conv_knum_i64_num)
{
  lua_assert((fins->op2 & IRCONV_TRUNC));
  return INT64FOLD((uint64_t)(int64_t)knumleft);
}

LJFOLD(CONV KNUM IRCONV_U64_NUM)
LJFOLDF(kfold_conv_knum_u64_num)
{
  lua_assert((fins->op2 & IRCONV_TRUNC));
  return INT64FOLD(lj_num2u64(knumleft));
}

LJFOLD(TOSTR KNUM)
LJFOLDF(kfold_tostr_knum)
{
  return lj_ir_kstr(J, lj_str_fromnum(J->L, &knumleft));
}

LJFOLD(TOSTR KINT)
LJFOLDF(kfold_tostr_kint)
{
  return lj_ir_kstr(J, lj_str_fromint(J->L, fleft->i));
}

LJFOLD(STRTO KGC)
LJFOLDF(kfold_strto)
{
  TValue n;
  if (lj_strscan_num(ir_kstr(fleft), &n))
    return lj_ir_knum(J, numV(&n));
  return FAILFOLD;
}

/* -- Constant folding of equality checks --------------------------------- */

/* Don't constant-fold away FLOAD checks against KNULL. */
LJFOLD(EQ FLOAD KNULL)
LJFOLD(NE FLOAD KNULL)
LJFOLDX(lj_opt_cse)

/* But fold all other KNULL compares, since only KNULL is equal to KNULL. */
LJFOLD(EQ any KNULL)
LJFOLD(NE any KNULL)
LJFOLD(EQ KNULL any)
LJFOLD(NE KNULL any)
LJFOLD(EQ KINT KINT)  /* Constants are unique, so same refs <==> same value. */
LJFOLD(NE KINT KINT)
LJFOLD(EQ KINT64 KINT64)
LJFOLD(NE KINT64 KINT64)
LJFOLD(EQ KGC KGC)
LJFOLD(NE KGC KGC)
LJFOLDF(kfold_kref)
{
  return CONDFOLD((fins->op1 == fins->op2) ^ (fins->o == IR_NE));
}

/* -- Algebraic shortcuts ------------------------------------------------- */

LJFOLD(FPMATH FPMATH IRFPM_FLOOR)
LJFOLD(FPMATH FPMATH IRFPM_CEIL)
LJFOLD(FPMATH FPMATH IRFPM_TRUNC)
LJFOLDF(shortcut_round)
{
  IRFPMathOp op = (IRFPMathOp)fleft->op2;
  if (op == IRFPM_FLOOR || op == IRFPM_CEIL || op == IRFPM_TRUNC)
    return LEFTFOLD;  /* round(round_left(x)) = round_left(x) */
  return NEXTFOLD;
}

LJFOLD(ABS ABS KNUM)
LJFOLDF(shortcut_left)
{
  return LEFTFOLD;  /* f(g(x)) ==> g(x) */
}

LJFOLD(ABS NEG KNUM)
LJFOLDF(shortcut_dropleft)
{
  PHIBARRIER(fleft);
  fins->op1 = fleft->op1;  /* abs(neg(x)) ==> abs(x) */
  return RETRYFOLD;
}

/* Note: no safe shortcuts with STRTO and TOSTR ("1e2" ==> +100 ==> "100"). */
LJFOLD(NEG NEG any)
LJFOLD(BNOT BNOT)
LJFOLD(BSWAP BSWAP)
LJFOLDF(shortcut_leftleft)
{
  PHIBARRIER(fleft);  /* See above. Fold would be ok, but not beneficial. */
  return fleft->op1;  /* f(g(x)) ==> x */
}

/* -- FP algebraic simplifications ---------------------------------------- */

/* FP arithmetic is tricky -- there's not much to simplify.
** Please note the following common pitfalls before sending "improvements":
**   x+0 ==> x  is INVALID for x=-0
**   0-x ==> -x is INVALID for x=+0
**   x*0 ==> 0  is INVALID for x=-0, x=+-Inf or x=NaN
*/

LJFOLD(ADD NEG any)
LJFOLDF(simplify_numadd_negx)
{
  PHIBARRIER(fleft);
  fins->o = IR_SUB;  /* (-a) + b ==> b - a */
  fins->op1 = fins->op2;
  fins->op2 = fleft->op1;
  return RETRYFOLD;
}

LJFOLD(ADD any NEG)
LJFOLDF(simplify_numadd_xneg)
{
  PHIBARRIER(fright);
  fins->o = IR_SUB;  /* a + (-b) ==> a - b */
  fins->op2 = fright->op1;
  return RETRYFOLD;
}

LJFOLD(SUB any KNUM)
LJFOLDF(simplify_numsub_k)
{
  lua_Number n = knumright;
  if (n == 0.0)  /* x - (+-0) ==> x */
    return LEFTFOLD;
  return NEXTFOLD;
}

LJFOLD(SUB NEG KNUM)
LJFOLDF(simplify_numsub_negk)
{
  PHIBARRIER(fleft);
  fins->op2 = fleft->op1;  /* (-x) - k ==> (-k) - x */
  fins->op1 = (IRRef1)lj_ir_knum(J, -knumright);
  return RETRYFOLD;
}

LJFOLD(SUB any NEG)
LJFOLDF(simplify_numsub_xneg)
{
  PHIBARRIER(fright);
  fins->o = IR_ADD;  /* a - (-b) ==> a + b */
  fins->op2 = fright->op1;
  return RETRYFOLD;
}

LJFOLD(MUL any KNUM)
LJFOLD(DIV any KNUM)
LJFOLDF(simplify_nummuldiv_k)
{
  lua_Number n = knumright;
  if (n == 1.0) {  /* x o 1 ==> x */
    return LEFTFOLD;
  } else if (n == -1.0) {  /* x o -1 ==> -x */
    fins->o = IR_NEG;
    fins->op2 = (IRRef1)lj_ir_knum_neg(J);
    return RETRYFOLD;
  } else if (fins->o == IR_MUL && n == 2.0) {  /* x * 2 ==> x + x */
    fins->o = IR_ADD;
    fins->op2 = fins->op1;
    return RETRYFOLD;
  } else if (fins->o == IR_DIV) {  /* x / 2^k ==> x * 2^-k */
    uint64_t u = ir_knum(fright)->u64;
    uint32_t ex = ((uint32_t)(u >> 52) & 0x7ff);
    if ((u & U64x(000fffff,ffffffff)) == 0 && ex - 1 < 0x7fd) {
      u = (u & ((uint64_t)1 << 63)) | ((uint64_t)(0x7fe - ex) << 52);
      fins->o = IR_MUL;  /* Multiply by exact reciprocal. */
      fins->op2 = lj_ir_knum_u64(J, u);
      return RETRYFOLD;
    }
  }
  return NEXTFOLD;
}

LJFOLD(MUL NEG KNUM)
LJFOLD(DIV NEG KNUM)
LJFOLDF(simplify_nummuldiv_negk)
{
  PHIBARRIER(fleft);
  fins->op1 = fleft->op1;  /* (-a) o k ==> a o (-k) */
  fins->op2 = (IRRef1)lj_ir_knum(J, -knumright);
  return RETRYFOLD;
}

LJFOLD(MUL NEG NEG)
LJFOLD(DIV NEG NEG)
LJFOLDF(simplify_nummuldiv_negneg)
{
  PHIBARRIER(fleft);
  PHIBARRIER(fright);
  fins->op1 = fleft->op1;  /* (-a) o (-b) ==> a o b */
  fins->op2 = fright->op1;
  return RETRYFOLD;
}

LJFOLD(POW any KINT)
LJFOLDF(simplify_numpow_xk)
{
  int32_t k = fright->i;
  TRef ref = fins->op1;
  if (k == 0)  /* x ^ 0 ==> 1 */
    return lj_ir_knum_one(J);  /* Result must be a number, not an int. */
  if (k == 1)  /* x ^ 1 ==> x */
    return LEFTFOLD;
  if ((uint32_t)(k+65536) > 2*65536u)  /* Limit code explosion. */
    return NEXTFOLD;
  if (k < 0) {  /* x ^ (-k) ==> (1/x) ^ k. */
    ref = emitir(IRTN(IR_DIV), lj_ir_knum_one(J), ref);
    k = -k;
  }
  /* Unroll x^k for 1 <= k <= 65536. */
  for (; (k & 1) == 0; k >>= 1)  /* Handle leading zeros. */
    ref = emitir(IRTN(IR_MUL), ref, ref);
  if ((k >>= 1) != 0) {  /* Handle trailing bits. */
    TRef tmp = emitir(IRTN(IR_MUL), ref, ref);
    for (; k != 1; k >>= 1) {
      if (k & 1)
	ref = emitir(IRTN(IR_MUL), ref, tmp);
      tmp = emitir(IRTN(IR_MUL), tmp, tmp);
    }
    ref = emitir(IRTN(IR_MUL), ref, tmp);
  }
  return ref;
}

LJFOLD(POW KNUM any)
LJFOLDF(simplify_numpow_kx)
{
  lua_Number n = knumleft;
  if (n == 2.0) {  /* 2.0 ^ i ==> ldexp(1.0, tonum(i)) */
    fins->o = IR_CONV;
#if LJ_TARGET_X86ORX64
    fins->op1 = fins->op2;
    fins->op2 = IRCONV_NUM_INT;
    fins->op2 = (IRRef1)lj_opt_fold(J);
#endif
    fins->op1 = (IRRef1)lj_ir_knum_one(J);
    fins->o = IR_LDEXP;
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

/* -- Simplify conversions ------------------------------------------------ */

LJFOLD(CONV CONV IRCONV_NUM_INT)  /* _NUM */
LJFOLDF(shortcut_conv_num_int)
{
  PHIBARRIER(fleft);
  /* Only safe with a guarded conversion to int. */
  if ((fleft->op2 & IRCONV_SRCMASK) == IRT_NUM && irt_isguard(fleft->t))
    return fleft->op1;  /* f(g(x)) ==> x */
  return NEXTFOLD;
}

LJFOLD(CONV CONV IRCONV_INT_NUM)  /* _INT */
LJFOLD(CONV CONV IRCONV_U32_NUM)  /* _U32*/
LJFOLDF(simplify_conv_int_num)
{
  /* Fold even across PHI to avoid expensive num->int conversions in loop. */
  if ((fleft->op2 & IRCONV_SRCMASK) ==
      ((fins->op2 & IRCONV_DSTMASK) >> IRCONV_DSH))
    return fleft->op1;
  return NEXTFOLD;
}

LJFOLD(CONV CONV IRCONV_I64_NUM)  /* _INT or _U32 */
LJFOLD(CONV CONV IRCONV_U64_NUM)  /* _INT or _U32 */
LJFOLDF(simplify_conv_i64_num)
{
  PHIBARRIER(fleft);
  if ((fleft->op2 & IRCONV_SRCMASK) == IRT_INT) {
    /* Reduce to a sign-extension. */
    fins->op1 = fleft->op1;
    fins->op2 = ((IRT_I64<<5)|IRT_INT|IRCONV_SEXT);
    return RETRYFOLD;
  } else if ((fleft->op2 & IRCONV_SRCMASK) == IRT_U32) {
#if LJ_TARGET_X64
    return fleft->op1;
#else
    /* Reduce to a zero-extension. */
    fins->op1 = fleft->op1;
    fins->op2 = (IRT_I64<<5)|IRT_U32;
    return RETRYFOLD;
#endif
  }
  return NEXTFOLD;
}

LJFOLD(CONV CONV IRCONV_INT_I64)  /* _INT or _U32 */
LJFOLD(CONV CONV IRCONV_INT_U64)  /* _INT or _U32 */
LJFOLD(CONV CONV IRCONV_U32_I64)  /* _INT or _U32 */
LJFOLD(CONV CONV IRCONV_U32_U64)  /* _INT or _U32 */
LJFOLDF(simplify_conv_int_i64)
{
  int src;
  PHIBARRIER(fleft);
  src = (fleft->op2 & IRCONV_SRCMASK);
  if (src == IRT_INT || src == IRT_U32) {
    if (src == ((fins->op2 & IRCONV_DSTMASK) >> IRCONV_DSH)) {
      return fleft->op1;
    } else {
      fins->op2 = ((fins->op2 & IRCONV_DSTMASK) | src);
      fins->op1 = fleft->op1;
      return RETRYFOLD;
    }
  }
  return NEXTFOLD;
}

LJFOLD(CONV CONV IRCONV_FLOAT_NUM)  /* _FLOAT */
LJFOLDF(simplify_conv_flt_num)
{
  PHIBARRIER(fleft);
  if ((fleft->op2 & IRCONV_SRCMASK) == IRT_FLOAT)
    return fleft->op1;
  return NEXTFOLD;
}

/* Shortcut TOBIT + IRT_NUM <- IRT_INT/IRT_U32 conversion. */
LJFOLD(TOBIT CONV KNUM)
LJFOLDF(simplify_tobit_conv)
{
  if ((fleft->op2 & IRCONV_SRCMASK) == IRT_INT ||
      (fleft->op2 & IRCONV_SRCMASK) == IRT_U32) {
    /* Fold even across PHI to avoid expensive num->int conversions in loop. */
    lua_assert(irt_isnum(fleft->t));
    return fleft->op1;
  }
  return NEXTFOLD;
}

/* Shortcut floor/ceil/round + IRT_NUM <- IRT_INT/IRT_U32 conversion. */
LJFOLD(FPMATH CONV IRFPM_FLOOR)
LJFOLD(FPMATH CONV IRFPM_CEIL)
LJFOLD(FPMATH CONV IRFPM_TRUNC)
LJFOLDF(simplify_floor_conv)
{
  if ((fleft->op2 & IRCONV_SRCMASK) == IRT_INT ||
      (fleft->op2 & IRCONV_SRCMASK) == IRT_U32)
    return LEFTFOLD;
  return NEXTFOLD;
}

/* Strength reduction of widening. */
LJFOLD(CONV any IRCONV_I64_INT)
LJFOLD(CONV any IRCONV_U64_INT)
LJFOLDF(simplify_conv_sext)
{
  IRRef ref = fins->op1;
  int64_t ofs = 0;
  if (!(fins->op2 & IRCONV_SEXT))
    return NEXTFOLD;
  PHIBARRIER(fleft);
  if (fleft->o == IR_XLOAD && (irt_isu8(fleft->t) || irt_isu16(fleft->t)))
    goto ok_reduce;
  if (fleft->o == IR_ADD && irref_isk(fleft->op2)) {
    ofs = (int64_t)IR(fleft->op2)->i;
    ref = fleft->op1;
  }
  /* Use scalar evolution analysis results to strength-reduce sign-extension. */
  if (ref == J->scev.idx) {
    IRRef lo = J->scev.dir ? J->scev.start : J->scev.stop;
    lua_assert(irt_isint(J->scev.t));
    if (lo && IR(lo)->i + ofs >= 0) {
    ok_reduce:
#if LJ_TARGET_X64
      /* Eliminate widening. All 32 bit ops do an implicit zero-extension. */
      return LEFTFOLD;
#else
      /* Reduce to a (cheaper) zero-extension. */
      fins->op2 &= ~IRCONV_SEXT;
      return RETRYFOLD;
#endif
    }
  }
  return NEXTFOLD;
}

/* Strength reduction of narrowing. */
LJFOLD(CONV ADD IRCONV_INT_I64)
LJFOLD(CONV SUB IRCONV_INT_I64)
LJFOLD(CONV MUL IRCONV_INT_I64)
LJFOLD(CONV ADD IRCONV_INT_U64)
LJFOLD(CONV SUB IRCONV_INT_U64)
LJFOLD(CONV MUL IRCONV_INT_U64)
LJFOLD(CONV ADD IRCONV_U32_I64)
LJFOLD(CONV SUB IRCONV_U32_I64)
LJFOLD(CONV MUL IRCONV_U32_I64)
LJFOLD(CONV ADD IRCONV_U32_U64)
LJFOLD(CONV SUB IRCONV_U32_U64)
LJFOLD(CONV MUL IRCONV_U32_U64)
LJFOLDF(simplify_conv_narrow)
{
  IROp op = (IROp)fleft->o;
  IRType t = irt_type(fins->t);
  IRRef op1 = fleft->op1, op2 = fleft->op2, mode = fins->op2;
  PHIBARRIER(fleft);
  op1 = emitir(IRTI(IR_CONV), op1, mode);
  op2 = emitir(IRTI(IR_CONV), op2, mode);
  fins->ot = IRT(op, t);
  fins->op1 = op1;
  fins->op2 = op2;
  return RETRYFOLD;
}

/* Special CSE rule for CONV. */
LJFOLD(CONV any any)
LJFOLDF(cse_conv)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_CSE)) {
    IRRef op1 = fins->op1, op2 = (fins->op2 & IRCONV_MODEMASK);
    uint8_t guard = irt_isguard(fins->t);
    IRRef ref = J->chain[IR_CONV];
    while (ref > op1) {
      IRIns *ir = IR(ref);
      /* Commoning with stronger checks is ok. */
      if (ir->op1 == op1 && (ir->op2 & IRCONV_MODEMASK) == op2 &&
	  irt_isguard(ir->t) >= guard)
	return ref;
      ref = ir->prev;
    }
  }
  return EMITFOLD;  /* No fallthrough to regular CSE. */
}

/* FP conversion narrowing. */
LJFOLD(TOBIT ADD KNUM)
LJFOLD(TOBIT SUB KNUM)
LJFOLD(CONV ADD IRCONV_INT_NUM)
LJFOLD(CONV SUB IRCONV_INT_NUM)
LJFOLD(CONV ADD IRCONV_I64_NUM)
LJFOLD(CONV SUB IRCONV_I64_NUM)
LJFOLDF(narrow_convert)
{
  PHIBARRIER(fleft);
  /* Narrowing ignores PHIs and repeating it inside the loop is not useful. */
  if (J->chain[IR_LOOP])
    return NEXTFOLD;
  lua_assert(fins->o != IR_CONV || (fins->op2&IRCONV_CONVMASK) != IRCONV_TOBIT);
  return lj_opt_narrow_convert(J);
}

/* -- Integer algebraic simplifications ----------------------------------- */

LJFOLD(ADD any KINT)
LJFOLD(ADDOV any KINT)
LJFOLD(SUBOV any KINT)
LJFOLDF(simplify_intadd_k)
{
  if (fright->i == 0)  /* i o 0 ==> i */
    return LEFTFOLD;
  return NEXTFOLD;
}

LJFOLD(MULOV any KINT)
LJFOLDF(simplify_intmul_k)
{
  if (fright->i == 0)  /* i * 0 ==> 0 */
    return RIGHTFOLD;
  if (fright->i == 1)  /* i * 1 ==> i */
    return LEFTFOLD;
  if (fright->i == 2) {  /* i * 2 ==> i + i */
    fins->o = IR_ADDOV;
    fins->op2 = fins->op1;
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(SUB any KINT)
LJFOLDF(simplify_intsub_k)
{
  if (fright->i == 0)  /* i - 0 ==> i */
    return LEFTFOLD;
  fins->o = IR_ADD;  /* i - k ==> i + (-k) */
  fins->op2 = (IRRef1)lj_ir_kint(J, -fright->i);  /* Overflow for -2^31 ok. */
  return RETRYFOLD;
}

LJFOLD(SUB KINT any)
LJFOLD(SUB KINT64 any)
LJFOLDF(simplify_intsub_kleft)
{
  if (fleft->o == IR_KINT ? (fleft->i == 0) : (ir_kint64(fleft)->u64 == 0)) {
    fins->o = IR_NEG;  /* 0 - i ==> -i */
    fins->op1 = fins->op2;
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(ADD any KINT64)
LJFOLDF(simplify_intadd_k64)
{
  if (ir_kint64(fright)->u64 == 0)  /* i + 0 ==> i */
    return LEFTFOLD;
  return NEXTFOLD;
}

LJFOLD(SUB any KINT64)
LJFOLDF(simplify_intsub_k64)
{
  uint64_t k = ir_kint64(fright)->u64;
  if (k == 0)  /* i - 0 ==> i */
    return LEFTFOLD;
  fins->o = IR_ADD;  /* i - k ==> i + (-k) */
  fins->op2 = (IRRef1)lj_ir_kint64(J, (uint64_t)-(int64_t)k);
  return RETRYFOLD;
}

static TRef simplify_intmul_k(jit_State *J, int32_t k)
{
  /* Note: many more simplifications are possible, e.g. 2^k1 +- 2^k2.
  ** But this is mainly intended for simple address arithmetic.
  ** Also it's easier for the backend to optimize the original multiplies.
  */
  if (k == 1) {  /* i * 1 ==> i */
    return LEFTFOLD;
  } else if ((k & (k-1)) == 0) {  /* i * 2^k ==> i << k */
    fins->o = IR_BSHL;
    fins->op2 = lj_ir_kint(J, lj_fls((uint32_t)k));
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(MUL any KINT)
LJFOLDF(simplify_intmul_k32)
{
  if (fright->i == 0)  /* i * 0 ==> 0 */
    return INTFOLD(0);
  else if (fright->i > 0)
    return simplify_intmul_k(J, fright->i);
  return NEXTFOLD;
}

LJFOLD(MUL any KINT64)
LJFOLDF(simplify_intmul_k64)
{
  if (ir_kint64(fright)->u64 == 0)  /* i * 0 ==> 0 */
    return INT64FOLD(0);
#if LJ_64
  /* NYI: SPLIT for BSHL and 32 bit backend support. */
  else if (ir_kint64(fright)->u64 < 0x80000000u)
    return simplify_intmul_k(J, (int32_t)ir_kint64(fright)->u64);
#endif
  return NEXTFOLD;
}

LJFOLD(MOD any KINT)
LJFOLDF(simplify_intmod_k)
{
  int32_t k = fright->i;
  lua_assert(k != 0);
  if (k > 0 && (k & (k-1)) == 0) {  /* i % (2^k) ==> i & (2^k-1) */
    fins->o = IR_BAND;
    fins->op2 = lj_ir_kint(J, k-1);
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(MOD KINT any)
LJFOLDF(simplify_intmod_kleft)
{
  if (fleft->i == 0)
    return INTFOLD(0);
  return NEXTFOLD;
}

LJFOLD(SUB any any)
LJFOLD(SUBOV any any)
LJFOLDF(simplify_intsub)
{
  if (fins->op1 == fins->op2 && !irt_isnum(fins->t))  /* i - i ==> 0 */
    return irt_is64(fins->t) ? INT64FOLD(0) : INTFOLD(0);
  return NEXTFOLD;
}

LJFOLD(SUB ADD any)
LJFOLDF(simplify_intsubadd_leftcancel)
{
  if (!irt_isnum(fins->t)) {
    PHIBARRIER(fleft);
    if (fins->op2 == fleft->op1)  /* (i + j) - i ==> j */
      return fleft->op2;
    if (fins->op2 == fleft->op2)  /* (i + j) - j ==> i */
      return fleft->op1;
  }
  return NEXTFOLD;
}

LJFOLD(SUB SUB any)
LJFOLDF(simplify_intsubsub_leftcancel)
{
  if (!irt_isnum(fins->t)) {
    PHIBARRIER(fleft);
    if (fins->op2 == fleft->op1) {  /* (i - j) - i ==> 0 - j */
      fins->op1 = (IRRef1)lj_ir_kint(J, 0);
      fins->op2 = fleft->op2;
      return RETRYFOLD;
    }
  }
  return NEXTFOLD;
}

LJFOLD(SUB any SUB)
LJFOLDF(simplify_intsubsub_rightcancel)
{
  if (!irt_isnum(fins->t)) {
    PHIBARRIER(fright);
    if (fins->op1 == fright->op1)  /* i - (i - j) ==> j */
      return fright->op2;
  }
  return NEXTFOLD;
}

LJFOLD(SUB any ADD)
LJFOLDF(simplify_intsubadd_rightcancel)
{
  if (!irt_isnum(fins->t)) {
    PHIBARRIER(fright);
    if (fins->op1 == fright->op1) {  /* i - (i + j) ==> 0 - j */
      fins->op2 = fright->op2;
      fins->op1 = (IRRef1)lj_ir_kint(J, 0);
      return RETRYFOLD;
    }
    if (fins->op1 == fright->op2) {  /* i - (j + i) ==> 0 - j */
      fins->op2 = fright->op1;
      fins->op1 = (IRRef1)lj_ir_kint(J, 0);
      return RETRYFOLD;
    }
  }
  return NEXTFOLD;
}

LJFOLD(SUB ADD ADD)
LJFOLDF(simplify_intsubaddadd_cancel)
{
  if (!irt_isnum(fins->t)) {
    PHIBARRIER(fleft);
    PHIBARRIER(fright);
    if (fleft->op1 == fright->op1) {  /* (i + j1) - (i + j2) ==> j1 - j2 */
      fins->op1 = fleft->op2;
      fins->op2 = fright->op2;
      return RETRYFOLD;
    }
    if (fleft->op1 == fright->op2) {  /* (i + j1) - (j2 + i) ==> j1 - j2 */
      fins->op1 = fleft->op2;
      fins->op2 = fright->op1;
      return RETRYFOLD;
    }
    if (fleft->op2 == fright->op1) {  /* (j1 + i) - (i + j2) ==> j1 - j2 */
      fins->op1 = fleft->op1;
      fins->op2 = fright->op2;
      return RETRYFOLD;
    }
    if (fleft->op2 == fright->op2) {  /* (j1 + i) - (j2 + i) ==> j1 - j2 */
      fins->op1 = fleft->op1;
      fins->op2 = fright->op1;
      return RETRYFOLD;
    }
  }
  return NEXTFOLD;
}

LJFOLD(BAND any KINT)
LJFOLD(BAND any KINT64)
LJFOLDF(simplify_band_k)
{
  int64_t k = fright->o == IR_KINT ? (int64_t)fright->i :
				     (int64_t)ir_k64(fright)->u64;
  if (k == 0)  /* i & 0 ==> 0 */
    return RIGHTFOLD;
  if (k == -1)  /* i & -1 ==> i */
    return LEFTFOLD;
  return NEXTFOLD;
}

LJFOLD(BOR any KINT)
LJFOLD(BOR any KINT64)
LJFOLDF(simplify_bor_k)
{
  int64_t k = fright->o == IR_KINT ? (int64_t)fright->i :
				     (int64_t)ir_k64(fright)->u64;
  if (k == 0)  /* i | 0 ==> i */
    return LEFTFOLD;
  if (k == -1)  /* i | -1 ==> -1 */
    return RIGHTFOLD;
  return NEXTFOLD;
}

LJFOLD(BXOR any KINT)
LJFOLD(BXOR any KINT64)
LJFOLDF(simplify_bxor_k)
{
  int64_t k = fright->o == IR_KINT ? (int64_t)fright->i :
				     (int64_t)ir_k64(fright)->u64;
  if (k == 0)  /* i xor 0 ==> i */
    return LEFTFOLD;
  if (k == -1) {  /* i xor -1 ==> ~i */
    fins->o = IR_BNOT;
    fins->op2 = 0;
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(BSHL any KINT)
LJFOLD(BSHR any KINT)
LJFOLD(BSAR any KINT)
LJFOLD(BROL any KINT)
LJFOLD(BROR any KINT)
LJFOLDF(simplify_shift_ik)
{
  int32_t mask = irt_is64(fins->t) ? 63 : 31;
  int32_t k = (fright->i & mask);
  if (k == 0)  /* i o 0 ==> i */
    return LEFTFOLD;
  if (k == 1 && fins->o == IR_BSHL) {  /* i << 1 ==> i + i */
    fins->o = IR_ADD;
    fins->op2 = fins->op1;
    return RETRYFOLD;
  }
  if (k != fright->i) {  /* i o k ==> i o (k & mask) */
    fins->op2 = (IRRef1)lj_ir_kint(J, k);
    return RETRYFOLD;
  }
#ifndef LJ_TARGET_UNIFYROT
  if (fins->o == IR_BROR) {  /* bror(i, k) ==> brol(i, (-k)&mask) */
    fins->o = IR_BROL;
    fins->op2 = (IRRef1)lj_ir_kint(J, (-k)&mask);
    return RETRYFOLD;
  }
#endif
  return NEXTFOLD;
}

LJFOLD(BSHL any BAND)
LJFOLD(BSHR any BAND)
LJFOLD(BSAR any BAND)
LJFOLD(BROL any BAND)
LJFOLD(BROR any BAND)
LJFOLDF(simplify_shift_andk)
{
  IRIns *irk = IR(fright->op2);
  PHIBARRIER(fright);
  if ((fins->o < IR_BROL ? LJ_TARGET_MASKSHIFT : LJ_TARGET_MASKROT) &&
      irk->o == IR_KINT) {  /* i o (j & mask) ==> i o j */
    int32_t mask = irt_is64(fins->t) ? 63 : 31;
    int32_t k = irk->i & mask;
    if (k == mask) {
      fins->op2 = fright->op1;
      return RETRYFOLD;
    }
  }
  return NEXTFOLD;
}

LJFOLD(BSHL KINT any)
LJFOLD(BSHR KINT any)
LJFOLD(BSHL KINT64 any)
LJFOLD(BSHR KINT64 any)
LJFOLDF(simplify_shift1_ki)
{
  int64_t k = fleft->o == IR_KINT ? (int64_t)fleft->i :
				    (int64_t)ir_k64(fleft)->u64;
  if (k == 0)  /* 0 o i ==> 0 */
    return LEFTFOLD;
  return NEXTFOLD;
}

LJFOLD(BSAR KINT any)
LJFOLD(BROL KINT any)
LJFOLD(BROR KINT any)
LJFOLD(BSAR KINT64 any)
LJFOLD(BROL KINT64 any)
LJFOLD(BROR KINT64 any)
LJFOLDF(simplify_shift2_ki)
{
  int64_t k = fleft->o == IR_KINT ? (int64_t)fleft->i :
				    (int64_t)ir_k64(fleft)->u64;
  if (k == 0 || k == -1)  /* 0 o i ==> 0; -1 o i ==> -1 */
    return LEFTFOLD;
  return NEXTFOLD;
}

LJFOLD(BSHL BAND KINT)
LJFOLD(BSHR BAND KINT)
LJFOLD(BROL BAND KINT)
LJFOLD(BROR BAND KINT)
LJFOLDF(simplify_shiftk_andk)
{
  IRIns *irk = IR(fleft->op2);
  PHIBARRIER(fleft);
  if (irk->o == IR_KINT) {  /* (i & k1) o k2 ==> (i o k2) & (k1 o k2) */
    int32_t k = kfold_intop(irk->i, fright->i, (IROp)fins->o);
    fins->op1 = fleft->op1;
    fins->op1 = (IRRef1)lj_opt_fold(J);
    fins->op2 = (IRRef1)lj_ir_kint(J, k);
    fins->ot = IRTI(IR_BAND);
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(BAND BSHL KINT)
LJFOLD(BAND BSHR KINT)
LJFOLDF(simplify_andk_shiftk)
{
  IRIns *irk = IR(fleft->op2);
  if (irk->o == IR_KINT &&
      kfold_intop(-1, irk->i, (IROp)fleft->o) == fright->i)
    return LEFTFOLD;  /* (i o k1) & k2 ==> i, if (-1 o k1) == k2 */
  return NEXTFOLD;
}

/* -- Reassociation ------------------------------------------------------- */

LJFOLD(ADD ADD KINT)
LJFOLD(MUL MUL KINT)
LJFOLD(BAND BAND KINT)
LJFOLD(BOR BOR KINT)
LJFOLD(BXOR BXOR KINT)
LJFOLDF(reassoc_intarith_k)
{
  IRIns *irk = IR(fleft->op2);
  if (irk->o == IR_KINT) {
    int32_t k = kfold_intop(irk->i, fright->i, (IROp)fins->o);
    if (k == irk->i)  /* (i o k1) o k2 ==> i o k1, if (k1 o k2) == k1. */
      return LEFTFOLD;
    PHIBARRIER(fleft);
    fins->op1 = fleft->op1;
    fins->op2 = (IRRef1)lj_ir_kint(J, k);
    return RETRYFOLD;  /* (i o k1) o k2 ==> i o (k1 o k2) */
  }
  return NEXTFOLD;
}

LJFOLD(ADD ADD KINT64)
LJFOLD(MUL MUL KINT64)
LJFOLD(BAND BAND KINT64)
LJFOLD(BOR BOR KINT64)
LJFOLD(BXOR BXOR KINT64)
LJFOLDF(reassoc_intarith_k64)
{
#if LJ_HASFFI || LJ_64
  IRIns *irk = IR(fleft->op2);
  if (irk->o == IR_KINT64) {
    uint64_t k = kfold_int64arith(ir_k64(irk)->u64,
				  ir_k64(fright)->u64, (IROp)fins->o);
    PHIBARRIER(fleft);
    fins->op1 = fleft->op1;
    fins->op2 = (IRRef1)lj_ir_kint64(J, k);
    return RETRYFOLD;  /* (i o k1) o k2 ==> i o (k1 o k2) */
  }
  return NEXTFOLD;
#else
  UNUSED(J); lua_assert(0); return FAILFOLD;
#endif
}

LJFOLD(MIN MIN any)
LJFOLD(MAX MAX any)
LJFOLD(BAND BAND any)
LJFOLD(BOR BOR any)
LJFOLDF(reassoc_dup)
{
  if (fins->op2 == fleft->op1 || fins->op2 == fleft->op2)
    return LEFTFOLD;  /* (a o b) o a ==> a o b; (a o b) o b ==> a o b */
  return NEXTFOLD;
}

LJFOLD(BXOR BXOR any)
LJFOLDF(reassoc_bxor)
{
  PHIBARRIER(fleft);
  if (fins->op2 == fleft->op1)  /* (a xor b) xor a ==> b */
    return fleft->op2;
  if (fins->op2 == fleft->op2)  /* (a xor b) xor b ==> a */
    return fleft->op1;
  return NEXTFOLD;
}

LJFOLD(BSHL BSHL KINT)
LJFOLD(BSHR BSHR KINT)
LJFOLD(BSAR BSAR KINT)
LJFOLD(BROL BROL KINT)
LJFOLD(BROR BROR KINT)
LJFOLDF(reassoc_shift)
{
  IRIns *irk = IR(fleft->op2);
  PHIBARRIER(fleft);  /* The (shift any KINT) rule covers k2 == 0 and more. */
  if (irk->o == IR_KINT) {  /* (i o k1) o k2 ==> i o (k1 + k2) */
    int32_t mask = irt_is64(fins->t) ? 63 : 31;
    int32_t k = (irk->i & mask) + (fright->i & mask);
    if (k > mask) {  /* Combined shift too wide? */
      if (fins->o == IR_BSHL || fins->o == IR_BSHR)
	return mask == 31 ? INTFOLD(0) : INT64FOLD(0);
      else if (fins->o == IR_BSAR)
	k = mask;
      else
	k &= mask;
    }
    fins->op1 = fleft->op1;
    fins->op2 = (IRRef1)lj_ir_kint(J, k);
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(MIN MIN KNUM)
LJFOLD(MAX MAX KNUM)
LJFOLD(MIN MIN KINT)
LJFOLD(MAX MAX KINT)
LJFOLDF(reassoc_minmax_k)
{
  IRIns *irk = IR(fleft->op2);
  if (irk->o == IR_KNUM) {
    lua_Number a = ir_knum(irk)->n;
    lua_Number y = lj_vm_foldarith(a, knumright, fins->o - IR_ADD);
    if (a == y)  /* (x o k1) o k2 ==> x o k1, if (k1 o k2) == k1. */
      return LEFTFOLD;
    PHIBARRIER(fleft);
    fins->op1 = fleft->op1;
    fins->op2 = (IRRef1)lj_ir_knum(J, y);
    return RETRYFOLD;  /* (x o k1) o k2 ==> x o (k1 o k2) */
  } else if (irk->o == IR_KINT) {
    int32_t a = irk->i;
    int32_t y = kfold_intop(a, fright->i, fins->o);
    if (a == y)  /* (x o k1) o k2 ==> x o k1, if (k1 o k2) == k1. */
      return LEFTFOLD;
    PHIBARRIER(fleft);
    fins->op1 = fleft->op1;
    fins->op2 = (IRRef1)lj_ir_kint(J, y);
    return RETRYFOLD;  /* (x o k1) o k2 ==> x o (k1 o k2) */
  }
  return NEXTFOLD;
}

LJFOLD(MIN MAX any)
LJFOLD(MAX MIN any)
LJFOLDF(reassoc_minmax_left)
{
  if (fins->op2 == fleft->op1 || fins->op2 == fleft->op2)
    return RIGHTFOLD;  /* (b o1 a) o2 b ==> b; (a o1 b) o2 b ==> b */
  return NEXTFOLD;
}

LJFOLD(MIN any MAX)
LJFOLD(MAX any MIN)
LJFOLDF(reassoc_minmax_right)
{
  if (fins->op1 == fright->op1 || fins->op1 == fright->op2)
    return LEFTFOLD;  /* a o2 (a o1 b) ==> a; a o2 (b o1 a) ==> a */
  return NEXTFOLD;
}

/* -- Array bounds check elimination -------------------------------------- */

/* Eliminate ABC across PHIs to handle t[i-1] forwarding case.
** ABC(asize, (i+k)+(-k)) ==> ABC(asize, i), but only if it already exists.
** Could be generalized to (i+k1)+k2 ==> i+(k1+k2), but needs better disambig.
*/
LJFOLD(ABC any ADD)
LJFOLDF(abc_fwd)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_ABC)) {
    if (irref_isk(fright->op2)) {
      IRIns *add2 = IR(fright->op1);
      if (add2->o == IR_ADD && irref_isk(add2->op2) &&
	  IR(fright->op2)->i == -IR(add2->op2)->i) {
	IRRef ref = J->chain[IR_ABC];
	IRRef lim = add2->op1;
	if (fins->op1 > lim) lim = fins->op1;
	while (ref > lim) {
	  IRIns *ir = IR(ref);
	  if (ir->op1 == fins->op1 && ir->op2 == add2->op1)
	    return DROPFOLD;
	  ref = ir->prev;
	}
      }
    }
  }
  return NEXTFOLD;
}

/* Eliminate ABC for constants.
** ABC(asize, k1), ABC(asize k2) ==> ABC(asize, max(k1, k2))
** Drop second ABC if k2 is lower. Otherwise patch first ABC with k2.
*/
LJFOLD(ABC any KINT)
LJFOLDF(abc_k)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_ABC)) {
    IRRef ref = J->chain[IR_ABC];
    IRRef asize = fins->op1;
    while (ref > asize) {
      IRIns *ir = IR(ref);
      if (ir->op1 == asize && irref_isk(ir->op2)) {
	int32_t k = IR(ir->op2)->i;
	if (fright->i > k)
	  ir->op2 = fins->op2;
	return DROPFOLD;
      }
      ref = ir->prev;
    }
    return EMITFOLD;  /* Already performed CSE. */
  }
  return NEXTFOLD;
}

/* Eliminate invariant ABC inside loop. */
LJFOLD(ABC any any)
LJFOLDF(abc_invar)
{
  if (!irt_isint(fins->t) && J->chain[IR_LOOP])  /* Currently marked as PTR. */
    return DROPFOLD;
  return NEXTFOLD;
}

/* -- Commutativity ------------------------------------------------------- */

/* The refs of commutative ops are canonicalized. Lower refs go to the right.
** Rationale behind this:
** - It (also) moves constants to the right.
** - It reduces the number of FOLD rules (e.g. (BOR any KINT) suffices).
** - It helps CSE to find more matches.
** - The assembler generates better code with constants at the right.
*/

LJFOLD(ADD any any)
LJFOLD(MUL any any)
LJFOLD(ADDOV any any)
LJFOLD(MULOV any any)
LJFOLDF(comm_swap)
{
  if (fins->op1 < fins->op2) {  /* Move lower ref to the right. */
    IRRef1 tmp = fins->op1;
    fins->op1 = fins->op2;
    fins->op2 = tmp;
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(EQ any any)
LJFOLD(NE any any)
LJFOLDF(comm_equal)
{
  /* For non-numbers only: x == x ==> drop; x ~= x ==> fail */
  if (fins->op1 == fins->op2 && !irt_isnum(fins->t))
    return CONDFOLD(fins->o == IR_EQ);
  return fold_comm_swap(J);
}

LJFOLD(LT any any)
LJFOLD(GE any any)
LJFOLD(LE any any)
LJFOLD(GT any any)
LJFOLD(ULT any any)
LJFOLD(UGE any any)
LJFOLD(ULE any any)
LJFOLD(UGT any any)
LJFOLDF(comm_comp)
{
  /* For non-numbers only: x <=> x ==> drop; x <> x ==> fail */
  if (fins->op1 == fins->op2 && !irt_isnum(fins->t))
    return CONDFOLD((fins->o ^ (fins->o >> 1)) & 1);
  if (fins->op1 < fins->op2) {  /* Move lower ref to the right. */
    IRRef1 tmp = fins->op1;
    fins->op1 = fins->op2;
    fins->op2 = tmp;
    fins->o ^= 3; /* GT <-> LT, GE <-> LE, does not affect U */
    return RETRYFOLD;
  }
  return NEXTFOLD;
}

LJFOLD(BAND any any)
LJFOLD(BOR any any)
LJFOLD(MIN any any)
LJFOLD(MAX any any)
LJFOLDF(comm_dup)
{
  if (fins->op1 == fins->op2)  /* x o x ==> x */
    return LEFTFOLD;
  return fold_comm_swap(J);
}

LJFOLD(BXOR any any)
LJFOLDF(comm_bxor)
{
  if (fins->op1 == fins->op2)  /* i xor i ==> 0 */
    return irt_is64(fins->t) ? INT64FOLD(0) : INTFOLD(0);
  return fold_comm_swap(J);
}

/* -- Simplification of compound expressions ------------------------------ */

static TRef kfold_xload(jit_State *J, IRIns *ir, const void *p)
{
  int32_t k;
  switch (irt_type(ir->t)) {
  case IRT_NUM: return lj_ir_knum_u64(J, *(uint64_t *)p);
  case IRT_I8: k = (int32_t)*(int8_t *)p; break;
  case IRT_U8: k = (int32_t)*(uint8_t *)p; break;
  case IRT_I16: k = (int32_t)(int16_t)lj_getu16(p); break;
  case IRT_U16: k = (int32_t)(uint16_t)lj_getu16(p); break;
  case IRT_INT: case IRT_U32: k = (int32_t)lj_getu32(p); break;
  case IRT_I64: case IRT_U64: return lj_ir_kint64(J, *(uint64_t *)p);
  default: return 0;
  }
  return lj_ir_kint(J, k);
}

/* Turn: string.sub(str, a, b) == kstr
** into: string.byte(str, a) == string.byte(kstr, 1) etc.
** Note: this creates unaligned XLOADs on x86/x64.
*/
LJFOLD(EQ SNEW KGC)
LJFOLD(NE SNEW KGC)
LJFOLDF(merge_eqne_snew_kgc)
{
  GCstr *kstr = ir_kstr(fright);
  int32_t len = (int32_t)kstr->len;
  lua_assert(irt_isstr(fins->t));

#if LJ_TARGET_UNALIGNED
#define FOLD_SNEW_MAX_LEN	4  /* Handle string lengths 0, 1, 2, 3, 4. */
#define FOLD_SNEW_TYPE8		IRT_I8	/* Creates shorter immediates. */
#else
#define FOLD_SNEW_MAX_LEN	1  /* Handle string lengths 0 or 1. */
#define FOLD_SNEW_TYPE8		IRT_U8  /* Prefer unsigned loads. */
#endif

  PHIBARRIER(fleft);
  if (len <= FOLD_SNEW_MAX_LEN) {
    IROp op = (IROp)fins->o;
    IRRef strref = fleft->op1;
    lua_assert(IR(strref)->o == IR_STRREF);
    if (op == IR_EQ) {
      emitir(IRTGI(IR_EQ), fleft->op2, lj_ir_kint(J, len));
      /* Caveat: fins/fleft/fright is no longer valid after emitir. */
    } else {
      /* NE is not expanded since this would need an OR of two conds. */
      if (!irref_isk(fleft->op2))  /* Only handle the constant length case. */
	return NEXTFOLD;
      if (IR(fleft->op2)->i != len)
	return DROPFOLD;
    }
    if (len > 0) {
      /* A 4 byte load for length 3 is ok -- all strings have an extra NUL. */
      uint16_t ot = (uint16_t)(len == 1 ? IRT(IR_XLOAD, FOLD_SNEW_TYPE8) :
			       len == 2 ? IRT(IR_XLOAD, IRT_U16) :
			       IRTI(IR_XLOAD));
      TRef tmp = emitir(ot, strref,
			IRXLOAD_READONLY | (len > 1 ? IRXLOAD_UNALIGNED : 0));
      TRef val = kfold_xload(J, IR(tref_ref(tmp)), strdata(kstr));
      if (len == 3)
	tmp = emitir(IRTI(IR_BAND), tmp,
		     lj_ir_kint(J, LJ_ENDIAN_SELECT(0x00ffffff, 0xffffff00)));
      fins->op1 = (IRRef1)tmp;
      fins->op2 = (IRRef1)val;
      fins->ot = (IROpT)IRTGI(op);
      return RETRYFOLD;
    } else {
      return DROPFOLD;
    }
  }
  return NEXTFOLD;
}

/* -- Loads --------------------------------------------------------------- */

/* Loads cannot be folded or passed on to CSE in general.
** Alias analysis is needed to check for forwarding opportunities.
**
** Caveat: *all* loads must be listed here or they end up at CSE!
*/

LJFOLD(ALOAD any)
LJFOLDX(lj_opt_fwd_aload)

/* From HREF fwd (see below). Must eliminate, not supported by fwd/backend. */
LJFOLD(HLOAD KKPTR)
LJFOLDF(kfold_hload_kkptr)
{
  UNUSED(J);
  lua_assert(ir_kptr(fleft) == niltvg(J2G(J)));
  return TREF_NIL;
}

LJFOLD(HLOAD any)
LJFOLDX(lj_opt_fwd_hload)

LJFOLD(ULOAD any)
LJFOLDX(lj_opt_fwd_uload)

LJFOLD(CALLL any IRCALL_lj_tab_len)
LJFOLDX(lj_opt_fwd_tab_len)

/* Upvalue refs are really loads, but there are no corresponding stores.
** So CSE is ok for them, except for UREFO across a GC step (see below).
** If the referenced function is const, its upvalue addresses are const, too.
** This can be used to improve CSE by looking for the same address,
** even if the upvalues originate from a different function.
*/
LJFOLD(UREFO KGC any)
LJFOLD(UREFC KGC any)
LJFOLDF(cse_uref)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_CSE)) {
    IRRef ref = J->chain[fins->o];
    GCfunc *fn = ir_kfunc(fleft);
    GCupval *uv = gco2uv(gcref(fn->l.uvptr[(fins->op2 >> 8)]));
    while (ref > 0) {
      IRIns *ir = IR(ref);
      if (irref_isk(ir->op1)) {
	GCfunc *fn2 = ir_kfunc(IR(ir->op1));
	if (gco2uv(gcref(fn2->l.uvptr[(ir->op2 >> 8)])) == uv) {
	  if (fins->o == IR_UREFO && gcstep_barrier(J, ref))
	    break;
	  return ref;
	}
      }
      ref = ir->prev;
    }
  }
  return EMITFOLD;
}

LJFOLD(HREFK any any)
LJFOLDX(lj_opt_fwd_hrefk)

LJFOLD(HREF TNEW any)
LJFOLDF(fwd_href_tnew)
{
  if (lj_opt_fwd_href_nokey(J))
    return lj_ir_kkptr(J, niltvg(J2G(J)));
  return NEXTFOLD;
}

LJFOLD(HREF TDUP KPRI)
LJFOLD(HREF TDUP KGC)
LJFOLD(HREF TDUP KNUM)
LJFOLDF(fwd_href_tdup)
{
  TValue keyv;
  lj_ir_kvalue(J->L, &keyv, fright);
  if (lj_tab_get(J->L, ir_ktab(IR(fleft->op1)), &keyv) == niltvg(J2G(J)) &&
      lj_opt_fwd_href_nokey(J))
    return lj_ir_kkptr(J, niltvg(J2G(J)));
  return NEXTFOLD;
}

/* We can safely FOLD/CSE array/hash refs and field loads, since there
** are no corresponding stores. But we need to check for any NEWREF with
** an aliased table, as it may invalidate all of the pointers and fields.
** Only HREF needs the NEWREF check -- AREF and HREFK already depend on
** FLOADs. And NEWREF itself is treated like a store (see below).
*/
LJFOLD(FLOAD TNEW IRFL_TAB_ASIZE)
LJFOLDF(fload_tab_tnew_asize)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD) && lj_opt_fwd_tptr(J, fins->op1))
    return INTFOLD(fleft->op1);
  return NEXTFOLD;
}

LJFOLD(FLOAD TNEW IRFL_TAB_HMASK)
LJFOLDF(fload_tab_tnew_hmask)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD) && lj_opt_fwd_tptr(J, fins->op1))
    return INTFOLD((1 << fleft->op2)-1);
  return NEXTFOLD;
}

LJFOLD(FLOAD TDUP IRFL_TAB_ASIZE)
LJFOLDF(fload_tab_tdup_asize)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD) && lj_opt_fwd_tptr(J, fins->op1))
    return INTFOLD((int32_t)ir_ktab(IR(fleft->op1))->asize);
  return NEXTFOLD;
}

LJFOLD(FLOAD TDUP IRFL_TAB_HMASK)
LJFOLDF(fload_tab_tdup_hmask)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD) && lj_opt_fwd_tptr(J, fins->op1))
    return INTFOLD((int32_t)ir_ktab(IR(fleft->op1))->hmask);
  return NEXTFOLD;
}

LJFOLD(HREF any any)
LJFOLD(FLOAD any IRFL_TAB_ARRAY)
LJFOLD(FLOAD any IRFL_TAB_NODE)
LJFOLD(FLOAD any IRFL_TAB_ASIZE)
LJFOLD(FLOAD any IRFL_TAB_HMASK)
LJFOLDF(fload_tab_ah)
{
  TRef tr = lj_opt_cse(J);
  return lj_opt_fwd_tptr(J, tref_ref(tr)) ? tr : EMITFOLD;
}

/* Strings are immutable, so we can safely FOLD/CSE the related FLOAD. */
LJFOLD(FLOAD KGC IRFL_STR_LEN)
LJFOLDF(fload_str_len_kgc)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD))
    return INTFOLD((int32_t)ir_kstr(fleft)->len);
  return NEXTFOLD;
}

LJFOLD(FLOAD SNEW IRFL_STR_LEN)
LJFOLDF(fload_str_len_snew)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD)) {
    PHIBARRIER(fleft);
    return fleft->op2;
  }
  return NEXTFOLD;
}

/* The C type ID of cdata objects is immutable. */
LJFOLD(FLOAD KGC IRFL_CDATA_CTYPEID)
LJFOLDF(fload_cdata_typeid_kgc)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD))
    return INTFOLD((int32_t)ir_kcdata(fleft)->ctypeid);
  return NEXTFOLD;
}

/* Get the contents of immutable cdata objects. */
LJFOLD(FLOAD KGC IRFL_CDATA_PTR)
LJFOLD(FLOAD KGC IRFL_CDATA_INT)
LJFOLD(FLOAD KGC IRFL_CDATA_INT64)
LJFOLDF(fload_cdata_int64_kgc)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD)) {
    void *p = cdataptr(ir_kcdata(fleft));
    if (irt_is64(fins->t))
      return INT64FOLD(*(uint64_t *)p);
    else
      return INTFOLD(*(int32_t *)p);
  }
  return NEXTFOLD;
}

LJFOLD(FLOAD CNEW IRFL_CDATA_CTYPEID)
LJFOLD(FLOAD CNEWI IRFL_CDATA_CTYPEID)
LJFOLDF(fload_cdata_typeid_cnew)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD))
    return fleft->op1;  /* No PHI barrier needed. CNEW/CNEWI op1 is const. */
  return NEXTFOLD;
}

/* Pointer, int and int64 cdata objects are immutable. */
LJFOLD(FLOAD CNEWI IRFL_CDATA_PTR)
LJFOLD(FLOAD CNEWI IRFL_CDATA_INT)
LJFOLD(FLOAD CNEWI IRFL_CDATA_INT64)
LJFOLDF(fload_cdata_ptr_int64_cnew)
{
  if (LJ_LIKELY(J->flags & JIT_F_OPT_FOLD))
    return fleft->op2;  /* Fold even across PHI to avoid allocations. */
  return NEXTFOLD;
}

LJFOLD(FLOAD any IRFL_STR_LEN)
LJFOLD(FLOAD any IRFL_CDATA_CTYPEID)
LJFOLD(FLOAD any IRFL_CDATA_PTR)
LJFOLD(FLOAD any IRFL_CDATA_INT)
LJFOLD(FLOAD any IRFL_CDATA_INT64)
LJFOLD(VLOAD any any)  /* Vararg loads have no corresponding stores. */
LJFOLDX(lj_opt_cse)

/* All other field loads need alias analysis. */
LJFOLD(FLOAD any any)
LJFOLDX(lj_opt_fwd_fload)

/* This is for LOOP only. Recording handles SLOADs internally. */
LJFOLD(SLOAD any any)
LJFOLDF(fwd_sload)
{
  if ((fins->op2 & IRSLOAD_FRAME)) {
    TRef tr = lj_opt_cse(J);
    return tref_ref(tr) < J->chain[IR_RETF] ? EMITFOLD : tr;
  } else {
    lua_assert(J->slot[fins->op1] != 0);
    return J->slot[fins->op1];
  }
}

/* Only fold for KKPTR. The pointer _and_ the contents must be const. */
LJFOLD(XLOAD KKPTR any)
LJFOLDF(xload_kptr)
{
  TRef tr = kfold_xload(J, fins, ir_kptr(fleft));
  return tr ? tr : NEXTFOLD;
}

LJFOLD(XLOAD any any)
LJFOLDX(lj_opt_fwd_xload)

/* -- Write barriers ------------------------------------------------------ */

/* Write barriers are amenable to CSE, but not across any incremental
** GC steps.
**
** The same logic applies to open upvalue references, because a stack
** may be resized during a GC step (not the current stack, but maybe that
** of a coroutine).
*/
LJFOLD(TBAR any)
LJFOLD(OBAR any any)
LJFOLD(UREFO any any)
LJFOLDF(barrier_tab)
{
  TRef tr = lj_opt_cse(J);
  if (gcstep_barrier(J, tref_ref(tr)))  /* CSE across GC step? */
    return EMITFOLD;  /* Raw emit. Assumes fins is left intact by CSE. */
  return tr;
}

LJFOLD(TBAR TNEW)
LJFOLD(TBAR TDUP)
LJFOLDF(barrier_tnew_tdup)
{
  /* New tables are always white and never need a barrier. */
  if (fins->op1 < J->chain[IR_LOOP])  /* Except across a GC step. */
    return NEXTFOLD;
  return DROPFOLD;
}

/* -- Stores and allocations ---------------------------------------------- */

/* Stores and allocations cannot be folded or passed on to CSE in general.
** But some stores can be eliminated with dead-store elimination (DSE).
**
** Caveat: *all* stores and allocs must be listed here or they end up at CSE!
*/

LJFOLD(ASTORE any any)
LJFOLD(HSTORE any any)
LJFOLDX(lj_opt_dse_ahstore)

LJFOLD(USTORE any any)
LJFOLDX(lj_opt_dse_ustore)

LJFOLD(FSTORE any any)
LJFOLDX(lj_opt_dse_fstore)

LJFOLD(XSTORE any any)
LJFOLDX(lj_opt_dse_xstore)

LJFOLD(NEWREF any any)  /* Treated like a store. */
LJFOLD(CALLS any any)
LJFOLD(CALLL any any)  /* Safeguard fallback. */
LJFOLD(CALLXS any any)
LJFOLD(XBAR)
LJFOLD(RETF any any)  /* Modifies BASE. */
LJFOLD(TNEW any any)
LJFOLD(TDUP any)
LJFOLD(CNEW any any)
LJFOLD(XSNEW any any)
LJFOLDX(lj_ir_emit)

/* ------------------------------------------------------------------------ */

/* Every entry in the generated hash table is a 32 bit pattern:
**
** xxxxxxxx iiiiiii lllllll rrrrrrrrrr
**
**   xxxxxxxx = 8 bit index into fold function table
**    iiiiiii = 7 bit folded instruction opcode
**    lllllll = 7 bit left instruction opcode
** rrrrrrrrrr = 8 bit right instruction opcode or 10 bits from literal field
*/

#include "lj_folddef.h"

/* ------------------------------------------------------------------------ */

/* Fold IR instruction. */
TRef LJ_FASTCALL lj_opt_fold(jit_State *J)
{
  uint32_t key, any;
  IRRef ref;

  if (LJ_UNLIKELY((J->flags & JIT_F_OPT_MASK) != JIT_F_OPT_DEFAULT)) {
    lua_assert(((JIT_F_OPT_FOLD|JIT_F_OPT_FWD|JIT_F_OPT_CSE|JIT_F_OPT_DSE) |
		JIT_F_OPT_DEFAULT) == JIT_F_OPT_DEFAULT);
    /* Folding disabled? Chain to CSE, but not for loads/stores/allocs. */
    if (!(J->flags & JIT_F_OPT_FOLD) && irm_kind(lj_ir_mode[fins->o]) == IRM_N)
      return lj_opt_cse(J);

    /* No FOLD, forwarding or CSE? Emit raw IR for loads, except for SLOAD. */
    if ((J->flags & (JIT_F_OPT_FOLD|JIT_F_OPT_FWD|JIT_F_OPT_CSE)) !=
		    (JIT_F_OPT_FOLD|JIT_F_OPT_FWD|JIT_F_OPT_CSE) &&
	irm_kind(lj_ir_mode[fins->o]) == IRM_L && fins->o != IR_SLOAD)
      return lj_ir_emit(J);

    /* No FOLD or DSE? Emit raw IR for stores. */
    if ((J->flags & (JIT_F_OPT_FOLD|JIT_F_OPT_DSE)) !=
		    (JIT_F_OPT_FOLD|JIT_F_OPT_DSE) &&
	irm_kind(lj_ir_mode[fins->o]) == IRM_S)
      return lj_ir_emit(J);
  }

  /* Fold engine start/retry point. */
retry:
  /* Construct key from opcode and operand opcodes (unless literal/none). */
  key = ((uint32_t)fins->o << 17);
  if (fins->op1 >= J->cur.nk) {
    key += (uint32_t)IR(fins->op1)->o << 10;
    *fleft = *IR(fins->op1);
  }
  if (fins->op2 >= J->cur.nk) {
    key += (uint32_t)IR(fins->op2)->o;
    *fright = *IR(fins->op2);
  } else {
    key += (fins->op2 & 0x3ffu);  /* Literal mask. Must include IRCONV_*MASK. */
  }

  /* Check for a match in order from most specific to least specific. */
  any = 0;
  for (;;) {
    uint32_t k = key | (any & 0x1ffff);
    uint32_t h = fold_hashkey(k);
    uint32_t fh = fold_hash[h];  /* Lookup key in semi-perfect hash table. */
    if ((fh & 0xffffff) == k || (fh = fold_hash[h+1], (fh & 0xffffff) == k)) {
      ref = (IRRef)tref_ref(fold_func[fh >> 24](J));
      if (ref != NEXTFOLD)
	break;
    }
    if (any == 0xfffff)  /* Exhausted folding. Pass on to CSE. */
      return lj_opt_cse(J);
    any = (any | (any >> 10)) ^ 0xffc00;
  }

  /* Return value processing, ordered by frequency. */
  if (LJ_LIKELY(ref >= MAX_FOLD))
    return TREF(ref, irt_t(IR(ref)->t));
  if (ref == RETRYFOLD)
    goto retry;
  if (ref == KINTFOLD)
    return lj_ir_kint(J, fins->i);
  if (ref == FAILFOLD)
    lj_trace_err(J, LJ_TRERR_GFAIL);
  lua_assert(ref == DROPFOLD);
  return REF_DROP;
}

/* -- Common-Subexpression Elimination ------------------------------------ */

/* CSE an IR instruction. This is very fast due to the skip-list chains. */
TRef LJ_FASTCALL lj_opt_cse(jit_State *J)
{
  /* Avoid narrow to wide store-to-load forwarding stall */
  IRRef2 op12 = (IRRef2)fins->op1 + ((IRRef2)fins->op2 << 16);
  IROp op = fins->o;
  if (LJ_LIKELY(J->flags & JIT_F_OPT_CSE)) {
    /* Limited search for same operands in per-opcode chain. */
    IRRef ref = J->chain[op];
    IRRef lim = fins->op1;
    if (fins->op2 > lim) lim = fins->op2;  /* Relies on lit < REF_BIAS. */
    while (ref > lim) {
      if (IR(ref)->op12 == op12)
	return TREF(ref, irt_t(IR(ref)->t));  /* Common subexpression found. */
      ref = IR(ref)->prev;
    }
  }
  /* Otherwise emit IR (inlined for speed). */
  {
    IRRef ref = lj_ir_nextins(J);
    IRIns *ir = IR(ref);
    ir->prev = J->chain[op];
    ir->op12 = op12;
    J->chain[op] = (IRRef1)ref;
    ir->o = fins->o;
    J->guardemit.irt |= fins->t.irt;
    return TREF(ref, irt_t((ir->t = fins->t)));
  }
}

/* CSE with explicit search limit. */
TRef LJ_FASTCALL lj_opt_cselim(jit_State *J, IRRef lim)
{
  IRRef ref = J->chain[fins->o];
  IRRef2 op12 = (IRRef2)fins->op1 + ((IRRef2)fins->op2 << 16);
  while (ref > lim) {
    if (IR(ref)->op12 == op12)
      return ref;
    ref = IR(ref)->prev;
  }
  return lj_ir_emit(J);
}

/* ------------------------------------------------------------------------ */

#undef IR
#undef fins
#undef fleft
#undef fright
#undef knumleft
#undef knumright
#undef emitir

#endif
