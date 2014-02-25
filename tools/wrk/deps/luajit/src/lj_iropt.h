/*
** Common header for IR emitter and optimizations.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_IROPT_H
#define _LJ_IROPT_H

#include <stdarg.h>

#include "lj_obj.h"
#include "lj_jit.h"

#if LJ_HASJIT
/* IR emitter. */
LJ_FUNC void LJ_FASTCALL lj_ir_growtop(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_ir_emit(jit_State *J);

/* Save current IR in J->fold.ins, but do not emit it (yet). */
static LJ_AINLINE void lj_ir_set_(jit_State *J, uint16_t ot, IRRef1 a, IRRef1 b)
{
  J->fold.ins.ot = ot; J->fold.ins.op1 = a; J->fold.ins.op2 = b;
}

#define lj_ir_set(J, ot, a, b) \
  lj_ir_set_(J, (uint16_t)(ot), (IRRef1)(a), (IRRef1)(b))

/* Get ref of next IR instruction and optionally grow IR.
** Note: this may invalidate all IRIns*!
*/
static LJ_AINLINE IRRef lj_ir_nextins(jit_State *J)
{
  IRRef ref = J->cur.nins;
  if (LJ_UNLIKELY(ref >= J->irtoplim)) lj_ir_growtop(J);
  J->cur.nins = ref + 1;
  return ref;
}

/* Interning of constants. */
LJ_FUNC TRef LJ_FASTCALL lj_ir_kint(jit_State *J, int32_t k);
LJ_FUNC void lj_ir_k64_freeall(jit_State *J);
LJ_FUNC TRef lj_ir_k64(jit_State *J, IROp op, cTValue *tv);
LJ_FUNC cTValue *lj_ir_k64_find(jit_State *J, uint64_t u64);
LJ_FUNC TRef lj_ir_knum_u64(jit_State *J, uint64_t u64);
LJ_FUNC TRef lj_ir_knumint(jit_State *J, lua_Number n);
LJ_FUNC TRef lj_ir_kint64(jit_State *J, uint64_t u64);
LJ_FUNC TRef lj_ir_kgc(jit_State *J, GCobj *o, IRType t);
LJ_FUNC TRef lj_ir_kptr_(jit_State *J, IROp op, void *ptr);
LJ_FUNC TRef lj_ir_knull(jit_State *J, IRType t);
LJ_FUNC TRef lj_ir_kslot(jit_State *J, TRef key, IRRef slot);

#if LJ_64
#define lj_ir_kintp(J, k)	lj_ir_kint64(J, (uint64_t)(k))
#else
#define lj_ir_kintp(J, k)	lj_ir_kint(J, (int32_t)(k))
#endif

static LJ_AINLINE TRef lj_ir_knum(jit_State *J, lua_Number n)
{
  TValue tv;
  tv.n = n;
  return lj_ir_knum_u64(J, tv.u64);
}

#define lj_ir_kstr(J, str)	lj_ir_kgc(J, obj2gco((str)), IRT_STR)
#define lj_ir_ktab(J, tab)	lj_ir_kgc(J, obj2gco((tab)), IRT_TAB)
#define lj_ir_kfunc(J, func)	lj_ir_kgc(J, obj2gco((func)), IRT_FUNC)
#define lj_ir_kptr(J, ptr)	lj_ir_kptr_(J, IR_KPTR, (ptr))
#define lj_ir_kkptr(J, ptr)	lj_ir_kptr_(J, IR_KKPTR, (ptr))

/* Special FP constants. */
#define lj_ir_knum_zero(J)	lj_ir_knum_u64(J, U64x(00000000,00000000))
#define lj_ir_knum_one(J)	lj_ir_knum_u64(J, U64x(3ff00000,00000000))
#define lj_ir_knum_tobit(J)	lj_ir_knum_u64(J, U64x(43380000,00000000))

/* Special 128 bit SIMD constants. */
#define lj_ir_knum_abs(J)	lj_ir_k64(J, IR_KNUM, LJ_KSIMD(J, LJ_KSIMD_ABS))
#define lj_ir_knum_neg(J)	lj_ir_k64(J, IR_KNUM, LJ_KSIMD(J, LJ_KSIMD_NEG))

/* Access to constants. */
LJ_FUNC void lj_ir_kvalue(lua_State *L, TValue *tv, const IRIns *ir);

/* Convert IR operand types. */
LJ_FUNC TRef LJ_FASTCALL lj_ir_tonumber(jit_State *J, TRef tr);
LJ_FUNC TRef LJ_FASTCALL lj_ir_tonum(jit_State *J, TRef tr);
LJ_FUNC TRef LJ_FASTCALL lj_ir_tostr(jit_State *J, TRef tr);

/* Miscellaneous IR ops. */
LJ_FUNC int lj_ir_numcmp(lua_Number a, lua_Number b, IROp op);
LJ_FUNC int lj_ir_strcmp(GCstr *a, GCstr *b, IROp op);
LJ_FUNC void lj_ir_rollback(jit_State *J, IRRef ref);

/* Emit IR instructions with on-the-fly optimizations. */
LJ_FUNC TRef LJ_FASTCALL lj_opt_fold(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_cse(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_cselim(jit_State *J, IRRef lim);

/* Special return values for the fold functions. */
enum {
  NEXTFOLD,		/* Couldn't fold, pass on. */
  RETRYFOLD,		/* Retry fold with modified fins. */
  KINTFOLD,		/* Return ref for int constant in fins->i. */
  FAILFOLD,		/* Guard would always fail. */
  DROPFOLD,		/* Guard eliminated. */
  MAX_FOLD
};

#define INTFOLD(k)	((J->fold.ins.i = (k)), (TRef)KINTFOLD)
#define INT64FOLD(k)	(lj_ir_kint64(J, (k)))
#define CONDFOLD(cond)	((TRef)FAILFOLD + (TRef)(cond))
#define LEFTFOLD	(J->fold.ins.op1)
#define RIGHTFOLD	(J->fold.ins.op2)
#define CSEFOLD		(lj_opt_cse(J))
#define EMITFOLD	(lj_ir_emit(J))

/* Load/store forwarding. */
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_aload(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_hload(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_uload(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_fload(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_xload(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_tab_len(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_fwd_hrefk(jit_State *J);
LJ_FUNC int LJ_FASTCALL lj_opt_fwd_href_nokey(jit_State *J);
LJ_FUNC int LJ_FASTCALL lj_opt_fwd_tptr(jit_State *J, IRRef lim);
LJ_FUNC int lj_opt_fwd_wasnonnil(jit_State *J, IROpT loadop, IRRef xref);

/* Dead-store elimination. */
LJ_FUNC TRef LJ_FASTCALL lj_opt_dse_ahstore(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_dse_ustore(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_dse_fstore(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_dse_xstore(jit_State *J);

/* Narrowing. */
LJ_FUNC TRef LJ_FASTCALL lj_opt_narrow_convert(jit_State *J);
LJ_FUNC TRef LJ_FASTCALL lj_opt_narrow_index(jit_State *J, TRef key);
LJ_FUNC TRef LJ_FASTCALL lj_opt_narrow_toint(jit_State *J, TRef tr);
LJ_FUNC TRef LJ_FASTCALL lj_opt_narrow_tobit(jit_State *J, TRef tr);
#if LJ_HASFFI
LJ_FUNC TRef LJ_FASTCALL lj_opt_narrow_cindex(jit_State *J, TRef key);
#endif
LJ_FUNC TRef lj_opt_narrow_arith(jit_State *J, TRef rb, TRef rc,
				 TValue *vb, TValue *vc, IROp op);
LJ_FUNC TRef lj_opt_narrow_unm(jit_State *J, TRef rc, TValue *vc);
LJ_FUNC TRef lj_opt_narrow_mod(jit_State *J, TRef rb, TRef rc, TValue *vc);
LJ_FUNC TRef lj_opt_narrow_pow(jit_State *J, TRef rb, TRef rc, TValue *vc);
LJ_FUNC IRType lj_opt_narrow_forl(jit_State *J, cTValue *forbase);

/* Optimization passes. */
LJ_FUNC void lj_opt_dce(jit_State *J);
LJ_FUNC int lj_opt_loop(jit_State *J);
#if LJ_SOFTFP || (LJ_32 && LJ_HASFFI)
LJ_FUNC void lj_opt_split(jit_State *J);
#else
#define lj_opt_split(J)		UNUSED(J)
#endif
LJ_FUNC void lj_opt_sink(jit_State *J);

#endif

#endif
