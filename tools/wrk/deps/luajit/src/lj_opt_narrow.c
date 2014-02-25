/*
** NARROW: Narrowing of numbers to integers (double to int32_t).
** STRIPOV: Stripping of overflow checks.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_opt_narrow_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_bc.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_vm.h"
#include "lj_strscan.h"

/* Rationale for narrowing optimizations:
**
** Lua has only a single number type and this is a FP double by default.
** Narrowing doubles to integers does not pay off for the interpreter on a
** current-generation x86/x64 machine. Most FP operations need the same
** amount of execution resources as their integer counterparts, except
** with slightly longer latencies. Longer latencies are a non-issue for
** the interpreter, since they are usually hidden by other overhead.
**
** The total CPU execution bandwidth is the sum of the bandwidth of the FP
** and the integer units, because they execute in parallel. The FP units
** have an equal or higher bandwidth than the integer units. Not using
** them means losing execution bandwidth. Moving work away from them to
** the already quite busy integer units is a losing proposition.
**
** The situation for JIT-compiled code is a bit different: the higher code
** density makes the extra latencies much more visible. Tight loops expose
** the latencies for updating the induction variables. Array indexing
** requires narrowing conversions with high latencies and additional
** guards (to check that the index is really an integer). And many common
** optimizations only work on integers.
**
** One solution would be speculative, eager narrowing of all number loads.
** This causes many problems, like losing -0 or the need to resolve type
** mismatches between traces. It also effectively forces the integer type
** to have overflow-checking semantics. This impedes many basic
** optimizations and requires adding overflow checks to all integer
** arithmetic operations (whereas FP arithmetics can do without).
**
** Always replacing an FP op with an integer op plus an overflow check is
** counter-productive on a current-generation super-scalar CPU. Although
** the overflow check branches are highly predictable, they will clog the
** execution port for the branch unit and tie up reorder buffers. This is
** turning a pure data-flow dependency into a different data-flow
** dependency (with slightly lower latency) *plus* a control dependency.
** In general, you don't want to do this since latencies due to data-flow
** dependencies can be well hidden by out-of-order execution.
**
** A better solution is to keep all numbers as FP values and only narrow
** when it's beneficial to do so. LuaJIT uses predictive narrowing for
** induction variables and demand-driven narrowing for index expressions,
** integer arguments and bit operations. Additionally it can eliminate or
** hoist most of the resulting overflow checks. Regular arithmetic
** computations are never narrowed to integers.
**
** The integer type in the IR has convenient wrap-around semantics and
** ignores overflow. Extra operations have been added for
** overflow-checking arithmetic (ADDOV/SUBOV) instead of an extra type.
** Apart from reducing overall complexity of the compiler, this also
** nicely solves the problem where you want to apply algebraic
** simplifications to ADD, but not to ADDOV. And the x86/x64 assembler can
** use lea instead of an add for integer ADD, but not for ADDOV (lea does
** not affect the flags, but it helps to avoid register moves).
**
**
** All of the above has to be reconsidered for architectures with slow FP
** operations or without a hardware FPU. The dual-number mode of LuaJIT
** addresses this issue. Arithmetic operations are performed on integers
** as far as possible and overflow checks are added as needed.
**
** This implies that narrowing for integer arguments and bit operations
** should also strip overflow checks, e.g. replace ADDOV with ADD. The
** original overflow guards are weak and can be eliminated by DCE, if
** there's no other use.
**
** A slight twist is that it's usually beneficial to use overflow-checked
** integer arithmetics if all inputs are already integers. This is the only
** change that affects the single-number mode, too.
*/

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)			(&J->cur.ir[(ref)])
#define fins			(&J->fold.ins)

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

#define emitir_raw(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

/* -- Elimination of narrowing type conversions --------------------------- */

/* Narrowing of index expressions and bit operations is demand-driven. The
** trace recorder emits a narrowing type conversion (CONV.int.num or TOBIT)
** in all of these cases (e.g. array indexing or string indexing). FOLD
** already takes care of eliminating simple redundant conversions like
** CONV.int.num(CONV.num.int(x)) ==> x.
**
** But the surrounding code is FP-heavy and arithmetic operations are
** performed on FP numbers (for the single-number mode). Consider a common
** example such as 'x=t[i+1]', with 'i' already an integer (due to induction
** variable narrowing). The index expression would be recorded as
**   CONV.int.num(ADD(CONV.num.int(i), 1))
** which is clearly suboptimal.
**
** One can do better by recursively backpropagating the narrowing type
** conversion across FP arithmetic operations. This turns FP ops into
** their corresponding integer counterparts. Depending on the semantics of
** the conversion they also need to check for overflow. Currently only ADD
** and SUB are supported.
**
** The above example can be rewritten as
**   ADDOV(CONV.int.num(CONV.num.int(i)), 1)
** and then into ADDOV(i, 1) after folding of the conversions. The original
** FP ops remain in the IR and are eliminated by DCE since all references to
** them are gone.
**
** [In dual-number mode the trace recorder already emits ADDOV etc., but
** this can be further reduced. See below.]
**
** Special care has to be taken to avoid narrowing across an operation
** which is potentially operating on non-integral operands. One obvious
** case is when an expression contains a non-integral constant, but ends
** up as an integer index at runtime (like t[x+1.5] with x=0.5).
**
** Operations with two non-constant operands illustrate a similar problem
** (like t[a+b] with a=1.5 and b=2.5). Backpropagation has to stop there,
** unless it can be proven that either operand is integral (e.g. by CSEing
** a previous conversion). As a not-so-obvious corollary this logic also
** applies for a whole expression tree (e.g. t[(a+1)+(b+1)]).
**
** Correctness of the transformation is guaranteed by avoiding to expand
** the tree by adding more conversions than the one we would need to emit
** if not backpropagating. TOBIT employs a more optimistic rule, because
** the conversion has special semantics, designed to make the life of the
** compiler writer easier. ;-)
**
** Using on-the-fly backpropagation of an expression tree doesn't work
** because it's unknown whether the transform is correct until the end.
** This either requires IR rollback and cache invalidation for every
** subtree or a two-pass algorithm. The former didn't work out too well,
** so the code now combines a recursive collector with a stack-based
** emitter.
**
** [A recursive backpropagation algorithm with backtracking, employing
** skip-list lookup and round-robin caching, emitting stack operations
** on-the-fly for a stack-based interpreter -- and all of that in a meager
** kilobyte? Yep, compilers are a great treasure chest. Throw away your
** textbooks and read the codebase of a compiler today!]
**
** There's another optimization opportunity for array indexing: it's
** always accompanied by an array bounds-check. The outermost overflow
** check may be delegated to the ABC operation. This works because ABC is
** an unsigned comparison and wrap-around due to overflow creates negative
** numbers.
**
** But this optimization is only valid for constants that cannot overflow
** an int32_t into the range of valid array indexes [0..2^27+1). A check
** for +-2^30 is safe since -2^31 - 2^30 wraps to 2^30 and 2^31-1 + 2^30
** wraps to -2^30-1.
**
** It's also good enough in practice, since e.g. t[i+1] or t[i-10] are
** quite common. So the above example finally ends up as ADD(i, 1)!
**
** Later on, the assembler is able to fuse the whole array reference and
** the ADD into the memory operands of loads and other instructions. This
** is why LuaJIT is able to generate very pretty (and fast) machine code
** for array indexing. And that, my dear, concludes another story about
** one of the hidden secrets of LuaJIT ...
*/

/* Maximum backpropagation depth and maximum stack size. */
#define NARROW_MAX_BACKPROP	100
#define NARROW_MAX_STACK	256

/* The stack machine has a 32 bit instruction format: [IROpT | IRRef1]
** The lower 16 bits hold a reference (or 0). The upper 16 bits hold
** the IR opcode + type or one of the following special opcodes:
*/
enum {
  NARROW_REF,		/* Push ref. */
  NARROW_CONV,		/* Push conversion of ref. */
  NARROW_SEXT,		/* Push sign-extension of ref. */
  NARROW_INT		/* Push KINT ref. The next code holds an int32_t. */
};

typedef uint32_t NarrowIns;

#define NARROWINS(op, ref)	(((op) << 16) + (ref))
#define narrow_op(ins)		((IROpT)((ins) >> 16))
#define narrow_ref(ins)		((IRRef1)(ins))

/* Context used for narrowing of type conversions. */
typedef struct NarrowConv {
  jit_State *J;		/* JIT compiler state. */
  NarrowIns *sp;	/* Current stack pointer. */
  NarrowIns *maxsp;	/* Maximum stack pointer minus redzone. */
  int lim;		/* Limit on the number of emitted conversions. */
  IRRef mode;		/* Conversion mode (IRCONV_*). */
  IRType t;		/* Destination type: IRT_INT or IRT_I64. */
  NarrowIns stack[NARROW_MAX_STACK];  /* Stack holding stack-machine code. */
} NarrowConv;

/* Lookup a reference in the backpropagation cache. */
static BPropEntry *narrow_bpc_get(jit_State *J, IRRef1 key, IRRef mode)
{
  ptrdiff_t i;
  for (i = 0; i < BPROP_SLOTS; i++) {
    BPropEntry *bp = &J->bpropcache[i];
    /* Stronger checks are ok, too. */
    if (bp->key == key && bp->mode >= mode &&
	((bp->mode ^ mode) & IRCONV_MODEMASK) == 0)
      return bp;
  }
  return NULL;
}

/* Add an entry to the backpropagation cache. */
static void narrow_bpc_set(jit_State *J, IRRef1 key, IRRef1 val, IRRef mode)
{
  uint32_t slot = J->bpropslot;
  BPropEntry *bp = &J->bpropcache[slot];
  J->bpropslot = (slot + 1) & (BPROP_SLOTS-1);
  bp->key = key;
  bp->val = val;
  bp->mode = mode;
}

/* Backpropagate overflow stripping. */
static void narrow_stripov_backprop(NarrowConv *nc, IRRef ref, int depth)
{
  jit_State *J = nc->J;
  IRIns *ir = IR(ref);
  if (ir->o == IR_ADDOV || ir->o == IR_SUBOV ||
      (ir->o == IR_MULOV && (nc->mode & IRCONV_CONVMASK) == IRCONV_ANY)) {
    BPropEntry *bp = narrow_bpc_get(nc->J, ref, IRCONV_TOBIT);
    if (bp) {
      ref = bp->val;
    } else if (++depth < NARROW_MAX_BACKPROP && nc->sp < nc->maxsp) {
      narrow_stripov_backprop(nc, ir->op1, depth);
      narrow_stripov_backprop(nc, ir->op2, depth);
      *nc->sp++ = NARROWINS(IRT(ir->o - IR_ADDOV + IR_ADD, IRT_INT), ref);
      return;
    }
  }
  *nc->sp++ = NARROWINS(NARROW_REF, ref);
}

/* Backpropagate narrowing conversion. Return number of needed conversions. */
static int narrow_conv_backprop(NarrowConv *nc, IRRef ref, int depth)
{
  jit_State *J = nc->J;
  IRIns *ir = IR(ref);
  IRRef cref;

  /* Check the easy cases first. */
  if (ir->o == IR_CONV && (ir->op2 & IRCONV_SRCMASK) == IRT_INT) {
    if ((nc->mode & IRCONV_CONVMASK) <= IRCONV_ANY)
      narrow_stripov_backprop(nc, ir->op1, depth+1);
    else
      *nc->sp++ = NARROWINS(NARROW_REF, ir->op1);  /* Undo conversion. */
    if (nc->t == IRT_I64)
      *nc->sp++ = NARROWINS(NARROW_SEXT, 0);  /* Sign-extend integer. */
    return 0;
  } else if (ir->o == IR_KNUM) {  /* Narrow FP constant. */
    lua_Number n = ir_knum(ir)->n;
    if ((nc->mode & IRCONV_CONVMASK) == IRCONV_TOBIT) {
      /* Allows a wider range of constants. */
      int64_t k64 = (int64_t)n;
      if (n == (lua_Number)k64) {  /* Only if const doesn't lose precision. */
	*nc->sp++ = NARROWINS(NARROW_INT, 0);
	*nc->sp++ = (NarrowIns)k64;  /* But always truncate to 32 bits. */
	return 0;
      }
    } else {
      int32_t k = lj_num2int(n);
      /* Only if constant is a small integer. */
      if (checki16(k) && n == (lua_Number)k) {
	*nc->sp++ = NARROWINS(NARROW_INT, 0);
	*nc->sp++ = (NarrowIns)k;
	return 0;
      }
    }
    return 10;  /* Never narrow other FP constants (this is rare). */
  }

  /* Try to CSE the conversion. Stronger checks are ok, too. */
  cref = J->chain[fins->o];
  while (cref > ref) {
    IRIns *cr = IR(cref);
    if (cr->op1 == ref &&
	(fins->o == IR_TOBIT ||
	 ((cr->op2 & IRCONV_MODEMASK) == (nc->mode & IRCONV_MODEMASK) &&
	  irt_isguard(cr->t) >= irt_isguard(fins->t)))) {
      *nc->sp++ = NARROWINS(NARROW_REF, cref);
      return 0;  /* Already there, no additional conversion needed. */
    }
    cref = cr->prev;
  }

  /* Backpropagate across ADD/SUB. */
  if (ir->o == IR_ADD || ir->o == IR_SUB) {
    /* Try cache lookup first. */
    IRRef mode = nc->mode;
    BPropEntry *bp;
    /* Inner conversions need a stronger check. */
    if ((mode & IRCONV_CONVMASK) == IRCONV_INDEX && depth > 0)
      mode += IRCONV_CHECK-IRCONV_INDEX;
    bp = narrow_bpc_get(nc->J, (IRRef1)ref, mode);
    if (bp) {
      *nc->sp++ = NARROWINS(NARROW_REF, bp->val);
      return 0;
    } else if (nc->t == IRT_I64) {
      /* Try sign-extending from an existing (checked) conversion to int. */
      mode = (IRT_INT<<5)|IRT_NUM|IRCONV_INDEX;
      bp = narrow_bpc_get(nc->J, (IRRef1)ref, mode);
      if (bp) {
	*nc->sp++ = NARROWINS(NARROW_REF, bp->val);
	*nc->sp++ = NARROWINS(NARROW_SEXT, 0);
	return 0;
      }
    }
    if (++depth < NARROW_MAX_BACKPROP && nc->sp < nc->maxsp) {
      NarrowIns *savesp = nc->sp;
      int count = narrow_conv_backprop(nc, ir->op1, depth);
      count += narrow_conv_backprop(nc, ir->op2, depth);
      if (count <= nc->lim) {  /* Limit total number of conversions. */
	*nc->sp++ = NARROWINS(IRT(ir->o, nc->t), ref);
	return count;
      }
      nc->sp = savesp;  /* Too many conversions, need to backtrack. */
    }
  }

  /* Otherwise add a conversion. */
  *nc->sp++ = NARROWINS(NARROW_CONV, ref);
  return 1;
}

/* Emit the conversions collected during backpropagation. */
static IRRef narrow_conv_emit(jit_State *J, NarrowConv *nc)
{
  /* The fins fields must be saved now -- emitir() overwrites them. */
  IROpT guardot = irt_isguard(fins->t) ? IRTG(IR_ADDOV-IR_ADD, 0) : 0;
  IROpT convot = fins->ot;
  IRRef1 convop2 = fins->op2;
  NarrowIns *next = nc->stack;  /* List of instructions from backpropagation. */
  NarrowIns *last = nc->sp;
  NarrowIns *sp = nc->stack;  /* Recycle the stack to store operands. */
  while (next < last) {  /* Simple stack machine to process the ins. list. */
    NarrowIns ref = *next++;
    IROpT op = narrow_op(ref);
    if (op == NARROW_REF) {
      *sp++ = ref;
    } else if (op == NARROW_CONV) {
      *sp++ = emitir_raw(convot, ref, convop2);  /* Raw emit avoids a loop. */
    } else if (op == NARROW_SEXT) {
      lua_assert(sp >= nc->stack+1);
      sp[-1] = emitir(IRT(IR_CONV, IRT_I64), sp[-1],
		      (IRT_I64<<5)|IRT_INT|IRCONV_SEXT);
    } else if (op == NARROW_INT) {
      lua_assert(next < last);
      *sp++ = nc->t == IRT_I64 ?
	      lj_ir_kint64(J, (int64_t)(int32_t)*next++) :
	      lj_ir_kint(J, *next++);
    } else {  /* Regular IROpT. Pops two operands and pushes one result. */
      IRRef mode = nc->mode;
      lua_assert(sp >= nc->stack+2);
      sp--;
      /* Omit some overflow checks for array indexing. See comments above. */
      if ((mode & IRCONV_CONVMASK) == IRCONV_INDEX) {
	if (next == last && irref_isk(narrow_ref(sp[0])) &&
	  (uint32_t)IR(narrow_ref(sp[0]))->i + 0x40000000u < 0x80000000u)
	  guardot = 0;
	else  /* Otherwise cache a stronger check. */
	  mode += IRCONV_CHECK-IRCONV_INDEX;
      }
      sp[-1] = emitir(op+guardot, sp[-1], sp[0]);
      /* Add to cache. */
      if (narrow_ref(ref))
	narrow_bpc_set(J, narrow_ref(ref), narrow_ref(sp[-1]), mode);
    }
  }
  lua_assert(sp == nc->stack+1);
  return nc->stack[0];
}

/* Narrow a type conversion of an arithmetic operation. */
TRef LJ_FASTCALL lj_opt_narrow_convert(jit_State *J)
{
  if ((J->flags & JIT_F_OPT_NARROW)) {
    NarrowConv nc;
    nc.J = J;
    nc.sp = nc.stack;
    nc.maxsp = &nc.stack[NARROW_MAX_STACK-4];
    nc.t = irt_type(fins->t);
    if (fins->o == IR_TOBIT) {
      nc.mode = IRCONV_TOBIT;  /* Used only in the backpropagation cache. */
      nc.lim = 2;  /* TOBIT can use a more optimistic rule. */
    } else {
      nc.mode = fins->op2;
      nc.lim = 1;
    }
    if (narrow_conv_backprop(&nc, fins->op1, 0) <= nc.lim)
      return narrow_conv_emit(J, &nc);
  }
  return NEXTFOLD;
}

/* -- Narrowing of implicit conversions ----------------------------------- */

/* Recursively strip overflow checks. */
static TRef narrow_stripov(jit_State *J, TRef tr, int lastop, IRRef mode)
{
  IRRef ref = tref_ref(tr);
  IRIns *ir = IR(ref);
  int op = ir->o;
  if (op >= IR_ADDOV && op <= lastop) {
    BPropEntry *bp = narrow_bpc_get(J, ref, mode);
    if (bp) {
      return TREF(bp->val, irt_t(IR(bp->val)->t));
    } else {
      IRRef op1 = ir->op1, op2 = ir->op2;  /* The IR may be reallocated. */
      op1 = narrow_stripov(J, op1, lastop, mode);
      op2 = narrow_stripov(J, op2, lastop, mode);
      tr = emitir(IRT(op - IR_ADDOV + IR_ADD,
		      ((mode & IRCONV_DSTMASK) >> IRCONV_DSH)), op1, op2);
      narrow_bpc_set(J, ref, tref_ref(tr), mode);
    }
  } else if (LJ_64 && (mode & IRCONV_SEXT) && !irt_is64(ir->t)) {
    tr = emitir(IRT(IR_CONV, IRT_INTP), tr, mode);
  }
  return tr;
}

/* Narrow array index. */
TRef LJ_FASTCALL lj_opt_narrow_index(jit_State *J, TRef tr)
{
  IRIns *ir;
  lua_assert(tref_isnumber(tr));
  if (tref_isnum(tr))  /* Conversion may be narrowed, too. See above. */
    return emitir(IRTGI(IR_CONV), tr, IRCONV_INT_NUM|IRCONV_INDEX);
  /* Omit some overflow checks for array indexing. See comments above. */
  ir = IR(tref_ref(tr));
  if ((ir->o == IR_ADDOV || ir->o == IR_SUBOV) && irref_isk(ir->op2) &&
      (uint32_t)IR(ir->op2)->i + 0x40000000u < 0x80000000u)
    return emitir(IRTI(ir->o - IR_ADDOV + IR_ADD), ir->op1, ir->op2);
  return tr;
}

/* Narrow conversion to integer operand (overflow undefined). */
TRef LJ_FASTCALL lj_opt_narrow_toint(jit_State *J, TRef tr)
{
  if (tref_isstr(tr))
    tr = emitir(IRTG(IR_STRTO, IRT_NUM), tr, 0);
  if (tref_isnum(tr))  /* Conversion may be narrowed, too. See above. */
    return emitir(IRTI(IR_CONV), tr, IRCONV_INT_NUM|IRCONV_ANY);
  if (!tref_isinteger(tr))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  /*
  ** Undefined overflow semantics allow stripping of ADDOV, SUBOV and MULOV.
  ** Use IRCONV_TOBIT for the cache entries, since the semantics are the same.
  */
  return narrow_stripov(J, tr, IR_MULOV, (IRT_INT<<5)|IRT_INT|IRCONV_TOBIT);
}

/* Narrow conversion to bitop operand (overflow wrapped). */
TRef LJ_FASTCALL lj_opt_narrow_tobit(jit_State *J, TRef tr)
{
  if (tref_isstr(tr))
    tr = emitir(IRTG(IR_STRTO, IRT_NUM), tr, 0);
  if (tref_isnum(tr))  /* Conversion may be narrowed, too. See above. */
    return emitir(IRTI(IR_TOBIT), tr, lj_ir_knum_tobit(J));
  if (!tref_isinteger(tr))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  /*
  ** Wrapped overflow semantics allow stripping of ADDOV and SUBOV.
  ** MULOV cannot be stripped due to precision widening.
  */
  return narrow_stripov(J, tr, IR_SUBOV, (IRT_INT<<5)|IRT_INT|IRCONV_TOBIT);
}

#if LJ_HASFFI
/* Narrow C array index (overflow undefined). */
TRef LJ_FASTCALL lj_opt_narrow_cindex(jit_State *J, TRef tr)
{
  lua_assert(tref_isnumber(tr));
  if (tref_isnum(tr))
    return emitir(IRT(IR_CONV, IRT_INTP), tr,
		  (IRT_INTP<<5)|IRT_NUM|IRCONV_TRUNC|IRCONV_ANY);
  /* Undefined overflow semantics allow stripping of ADDOV, SUBOV and MULOV. */
  return narrow_stripov(J, tr, IR_MULOV,
			LJ_64 ? ((IRT_INTP<<5)|IRT_INT|IRCONV_SEXT) :
				((IRT_INTP<<5)|IRT_INT|IRCONV_TOBIT));
}
#endif

/* -- Narrowing of arithmetic operators ----------------------------------- */

/* Check whether a number fits into an int32_t (-0 is ok, too). */
static int numisint(lua_Number n)
{
  return (n == (lua_Number)lj_num2int(n));
}

/* Narrowing of arithmetic operations. */
TRef lj_opt_narrow_arith(jit_State *J, TRef rb, TRef rc,
			 TValue *vb, TValue *vc, IROp op)
{
  if (tref_isstr(rb)) {
    rb = emitir(IRTG(IR_STRTO, IRT_NUM), rb, 0);
    lj_strscan_num(strV(vb), vb);
  }
  if (tref_isstr(rc)) {
    rc = emitir(IRTG(IR_STRTO, IRT_NUM), rc, 0);
    lj_strscan_num(strV(vc), vc);
  }
  /* Must not narrow MUL in non-DUALNUM variant, because it loses -0. */
  if ((op >= IR_ADD && op <= (LJ_DUALNUM ? IR_MUL : IR_SUB)) &&
      tref_isinteger(rb) && tref_isinteger(rc) &&
      numisint(lj_vm_foldarith(numberVnum(vb), numberVnum(vc),
			       (int)op - (int)IR_ADD)))
    return emitir(IRTGI((int)op - (int)IR_ADD + (int)IR_ADDOV), rb, rc);
  if (!tref_isnum(rb)) rb = emitir(IRTN(IR_CONV), rb, IRCONV_NUM_INT);
  if (!tref_isnum(rc)) rc = emitir(IRTN(IR_CONV), rc, IRCONV_NUM_INT);
  return emitir(IRTN(op), rb, rc);
}

/* Narrowing of unary minus operator. */
TRef lj_opt_narrow_unm(jit_State *J, TRef rc, TValue *vc)
{
  if (tref_isstr(rc)) {
    rc = emitir(IRTG(IR_STRTO, IRT_NUM), rc, 0);
    lj_strscan_num(strV(vc), vc);
  }
  if (tref_isinteger(rc)) {
    if ((uint32_t)numberVint(vc) != 0x80000000u)
      return emitir(IRTGI(IR_SUBOV), lj_ir_kint(J, 0), rc);
    rc = emitir(IRTN(IR_CONV), rc, IRCONV_NUM_INT);
  }
  return emitir(IRTN(IR_NEG), rc, lj_ir_knum_neg(J));
}

/* Narrowing of modulo operator. */
TRef lj_opt_narrow_mod(jit_State *J, TRef rb, TRef rc, TValue *vc)
{
  TRef tmp;
  if (tvisstr(vc) && !lj_strscan_num(strV(vc), vc))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  if ((LJ_DUALNUM || (J->flags & JIT_F_OPT_NARROW)) &&
      tref_isinteger(rb) && tref_isinteger(rc) &&
      (tvisint(vc) ? intV(vc) != 0 : !tviszero(vc))) {
    emitir(IRTGI(IR_NE), rc, lj_ir_kint(J, 0));
    return emitir(IRTI(IR_MOD), rb, rc);
  }
  /* b % c ==> b - floor(b/c)*c */
  rb = lj_ir_tonum(J, rb);
  rc = lj_ir_tonum(J, rc);
  tmp = emitir(IRTN(IR_DIV), rb, rc);
  tmp = emitir(IRTN(IR_FPMATH), tmp, IRFPM_FLOOR);
  tmp = emitir(IRTN(IR_MUL), tmp, rc);
  return emitir(IRTN(IR_SUB), rb, tmp);
}

/* Narrowing of power operator or math.pow. */
TRef lj_opt_narrow_pow(jit_State *J, TRef rb, TRef rc, TValue *vc)
{
  if (tvisstr(vc) && !lj_strscan_num(strV(vc), vc))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  /* Narrowing must be unconditional to preserve (-x)^i semantics. */
  if (tvisint(vc) || numisint(numV(vc))) {
    int checkrange = 0;
    /* Split pow is faster for bigger exponents. But do this only for (+k)^i. */
    if (tref_isk(rb) && (int32_t)ir_knum(IR(tref_ref(rb)))->u32.hi >= 0) {
      int32_t k = numberVint(vc);
      if (!(k >= -65536 && k <= 65536)) goto split_pow;
      checkrange = 1;
    }
    if (!tref_isinteger(rc)) {
      if (tref_isstr(rc))
	rc = emitir(IRTG(IR_STRTO, IRT_NUM), rc, 0);
      /* Guarded conversion to integer! */
      rc = emitir(IRTGI(IR_CONV), rc, IRCONV_INT_NUM|IRCONV_CHECK);
    }
    if (checkrange && !tref_isk(rc)) {  /* Range guard: -65536 <= i <= 65536 */
      TRef tmp = emitir(IRTI(IR_ADD), rc, lj_ir_kint(J, 65536));
      emitir(IRTGI(IR_ULE), tmp, lj_ir_kint(J, 2*65536));
    }
    return emitir(IRTN(IR_POW), rb, rc);
  }
split_pow:
  /* FOLD covers most cases, but some are easier to do here. */
  if (tref_isk(rb) && tvispone(ir_knum(IR(tref_ref(rb)))))
    return rb;  /* 1 ^ x ==> 1 */
  rc = lj_ir_tonum(J, rc);
  if (tref_isk(rc) && ir_knum(IR(tref_ref(rc)))->n == 0.5)
    return emitir(IRTN(IR_FPMATH), rb, IRFPM_SQRT);  /* x ^ 0.5 ==> sqrt(x) */
  /* Split up b^c into exp2(c*log2(b)). Assembler may rejoin later. */
  rb = emitir(IRTN(IR_FPMATH), rb, IRFPM_LOG2);
  rc = emitir(IRTN(IR_MUL), rb, rc);
  return emitir(IRTN(IR_FPMATH), rc, IRFPM_EXP2);
}

/* -- Predictive narrowing of induction variables ------------------------- */

/* Narrow a single runtime value. */
static int narrow_forl(jit_State *J, cTValue *o)
{
  if (tvisint(o)) return 1;
  if (LJ_DUALNUM || (J->flags & JIT_F_OPT_NARROW)) return numisint(numV(o));
  return 0;
}

/* Narrow the FORL index type by looking at the runtime values. */
IRType lj_opt_narrow_forl(jit_State *J, cTValue *tv)
{
  lua_assert(tvisnumber(&tv[FORL_IDX]) &&
	     tvisnumber(&tv[FORL_STOP]) &&
	     tvisnumber(&tv[FORL_STEP]));
  /* Narrow only if the runtime values of start/stop/step are all integers. */
  if (narrow_forl(J, &tv[FORL_IDX]) &&
      narrow_forl(J, &tv[FORL_STOP]) &&
      narrow_forl(J, &tv[FORL_STEP])) {
    /* And if the loop index can't possibly overflow. */
    lua_Number step = numberVnum(&tv[FORL_STEP]);
    lua_Number sum = numberVnum(&tv[FORL_STOP]) + step;
    if (0 <= step ? (sum <= 2147483647.0) : (sum >= -2147483648.0))
      return IRT_INT;
  }
  return IRT_NUM;
}

#undef IR
#undef fins
#undef emitir
#undef emitir_raw

#endif
