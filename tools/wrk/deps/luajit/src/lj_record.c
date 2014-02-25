/*
** Trace recorder (bytecode -> SSA IR).
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_record_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_bc.h"
#include "lj_ff.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_record.h"
#include "lj_ffrecord.h"
#include "lj_snap.h"
#include "lj_dispatch.h"
#include "lj_vm.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)			(&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Emit raw IR without passing through optimizations. */
#define emitir_raw(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

/* -- Sanity checks ------------------------------------------------------- */

#ifdef LUA_USE_ASSERT
/* Sanity check the whole IR -- sloooow. */
static void rec_check_ir(jit_State *J)
{
  IRRef i, nins = J->cur.nins, nk = J->cur.nk;
  lua_assert(nk <= REF_BIAS && nins >= REF_BIAS && nins < 65536);
  for (i = nins-1; i >= nk; i--) {
    IRIns *ir = IR(i);
    uint32_t mode = lj_ir_mode[ir->o];
    IRRef op1 = ir->op1;
    IRRef op2 = ir->op2;
    switch (irm_op1(mode)) {
    case IRMnone: lua_assert(op1 == 0); break;
    case IRMref: lua_assert(op1 >= nk);
      lua_assert(i >= REF_BIAS ? op1 < i : op1 > i); break;
    case IRMlit: break;
    case IRMcst: lua_assert(i < REF_BIAS); continue;
    }
    switch (irm_op2(mode)) {
    case IRMnone: lua_assert(op2 == 0); break;
    case IRMref: lua_assert(op2 >= nk);
      lua_assert(i >= REF_BIAS ? op2 < i : op2 > i); break;
    case IRMlit: break;
    case IRMcst: lua_assert(0); break;
    }
    if (ir->prev) {
      lua_assert(ir->prev >= nk);
      lua_assert(i >= REF_BIAS ? ir->prev < i : ir->prev > i);
      lua_assert(ir->o == IR_NOP || IR(ir->prev)->o == ir->o);
    }
  }
}

/* Compare stack slots and frames of the recorder and the VM. */
static void rec_check_slots(jit_State *J)
{
  BCReg s, nslots = J->baseslot + J->maxslot;
  int32_t depth = 0;
  cTValue *base = J->L->base - J->baseslot;
  lua_assert(J->baseslot >= 1 && J->baseslot < LJ_MAX_JSLOTS);
  lua_assert(J->baseslot == 1 || (J->slot[J->baseslot-1] & TREF_FRAME));
  lua_assert(nslots < LJ_MAX_JSLOTS);
  for (s = 0; s < nslots; s++) {
    TRef tr = J->slot[s];
    if (tr) {
      cTValue *tv = &base[s];
      IRRef ref = tref_ref(tr);
      IRIns *ir;
      lua_assert(ref >= J->cur.nk && ref < J->cur.nins);
      ir = IR(ref);
      lua_assert(irt_t(ir->t) == tref_t(tr));
      if (s == 0) {
	lua_assert(tref_isfunc(tr));
      } else if ((tr & TREF_FRAME)) {
	GCfunc *fn = gco2func(frame_gc(tv));
	BCReg delta = (BCReg)(tv - frame_prev(tv));
	lua_assert(tref_isfunc(tr));
	if (tref_isk(tr)) lua_assert(fn == ir_kfunc(ir));
	lua_assert(s > delta ? (J->slot[s-delta] & TREF_FRAME) : (s == delta));
	depth++;
      } else if ((tr & TREF_CONT)) {
	lua_assert(ir_kptr(ir) == gcrefp(tv->gcr, void));
	lua_assert((J->slot[s+1] & TREF_FRAME));
	depth++;
      } else {
	if (tvisnumber(tv))
	  lua_assert(tref_isnumber(tr));  /* Could be IRT_INT etc., too. */
	else
	  lua_assert(itype2irt(tv) == tref_type(tr));
	if (tref_isk(tr)) {  /* Compare constants. */
	  TValue tvk;
	  lj_ir_kvalue(J->L, &tvk, ir);
	  if (!(tvisnum(&tvk) && tvisnan(&tvk)))
	    lua_assert(lj_obj_equal(tv, &tvk));
	  else
	    lua_assert(tvisnum(tv) && tvisnan(tv));
	}
      }
    }
  }
  lua_assert(J->framedepth == depth);
}
#endif

/* -- Type handling and specialization ------------------------------------ */

/* Note: these functions return tagged references (TRef). */

/* Specialize a slot to a specific type. Note: slot can be negative! */
static TRef sloadt(jit_State *J, int32_t slot, IRType t, int mode)
{
  /* Caller may set IRT_GUARD in t. */
  TRef ref = emitir_raw(IRT(IR_SLOAD, t), (int32_t)J->baseslot+slot, mode);
  J->base[slot] = ref;
  return ref;
}

/* Specialize a slot to the runtime type. Note: slot can be negative! */
static TRef sload(jit_State *J, int32_t slot)
{
  IRType t = itype2irt(&J->L->base[slot]);
  TRef ref = emitir_raw(IRTG(IR_SLOAD, t), (int32_t)J->baseslot+slot,
			IRSLOAD_TYPECHECK);
  if (irtype_ispri(t)) ref = TREF_PRI(t);  /* Canonicalize primitive refs. */
  J->base[slot] = ref;
  return ref;
}

/* Get TRef from slot. Load slot and specialize if not done already. */
#define getslot(J, s)	(J->base[(s)] ? J->base[(s)] : sload(J, (int32_t)(s)))

/* Get TRef for current function. */
static TRef getcurrf(jit_State *J)
{
  if (J->base[-1])
    return J->base[-1];
  lua_assert(J->baseslot == 1);
  return sloadt(J, -1, IRT_FUNC, IRSLOAD_READONLY);
}

/* Compare for raw object equality.
** Returns 0 if the objects are the same.
** Returns 1 if they are different, but the same type.
** Returns 2 for two different types.
** Comparisons between primitives always return 1 -- no caller cares about it.
*/
int lj_record_objcmp(jit_State *J, TRef a, TRef b, cTValue *av, cTValue *bv)
{
  int diff = !lj_obj_equal(av, bv);
  if (!tref_isk2(a, b)) {  /* Shortcut, also handles primitives. */
    IRType ta = tref_isinteger(a) ? IRT_INT : tref_type(a);
    IRType tb = tref_isinteger(b) ? IRT_INT : tref_type(b);
    if (ta != tb) {
      /* Widen mixed number/int comparisons to number/number comparison. */
      if (ta == IRT_INT && tb == IRT_NUM) {
	a = emitir(IRTN(IR_CONV), a, IRCONV_NUM_INT);
	ta = IRT_NUM;
      } else if (ta == IRT_NUM && tb == IRT_INT) {
	b = emitir(IRTN(IR_CONV), b, IRCONV_NUM_INT);
      } else {
	return 2;  /* Two different types are never equal. */
      }
    }
    emitir(IRTG(diff ? IR_NE : IR_EQ, ta), a, b);
  }
  return diff;
}

/* Constify a value. Returns 0 for non-representable object types. */
TRef lj_record_constify(jit_State *J, cTValue *o)
{
  if (tvisgcv(o))
    return lj_ir_kgc(J, gcV(o), itype2irt(o));
  else if (tvisint(o))
    return lj_ir_kint(J, intV(o));
  else if (tvisnum(o))
    return lj_ir_knumint(J, numV(o));
  else if (tvisbool(o))
    return TREF_PRI(itype2irt(o));
  else
    return 0;  /* Can't represent lightuserdata (pointless). */
}

/* -- Record loop ops ----------------------------------------------------- */

/* Loop event. */
typedef enum {
  LOOPEV_LEAVE,		/* Loop is left or not entered. */
  LOOPEV_ENTERLO,	/* Loop is entered with a low iteration count left. */
  LOOPEV_ENTER		/* Loop is entered. */
} LoopEvent;

/* Canonicalize slots: convert integers to numbers. */
static void canonicalize_slots(jit_State *J)
{
  BCReg s;
  if (LJ_DUALNUM) return;
  for (s = J->baseslot+J->maxslot-1; s >= 1; s--) {
    TRef tr = J->slot[s];
    if (tref_isinteger(tr)) {
      IRIns *ir = IR(tref_ref(tr));
      if (!(ir->o == IR_SLOAD && (ir->op2 & IRSLOAD_READONLY)))
	J->slot[s] = emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
    }
  }
}

/* Stop recording. */
static void rec_stop(jit_State *J, TraceLink linktype, TraceNo lnk)
{
  lj_trace_end(J);
  J->cur.linktype = (uint8_t)linktype;
  J->cur.link = (uint16_t)lnk;
  /* Looping back at the same stack level? */
  if (lnk == J->cur.traceno && J->framedepth + J->retdepth == 0) {
    if ((J->flags & JIT_F_OPT_LOOP))  /* Shall we try to create a loop? */
      goto nocanon;  /* Do not canonicalize or we lose the narrowing. */
    if (J->cur.root)  /* Otherwise ensure we always link to the root trace. */
      J->cur.link = J->cur.root;
  }
  canonicalize_slots(J);
nocanon:
  /* Note: all loop ops must set J->pc to the following instruction! */
  lj_snap_add(J);  /* Add loop snapshot. */
  J->needsnap = 0;
  J->mergesnap = 1;  /* In case recording continues. */
}

/* Search bytecode backwards for a int/num constant slot initializer. */
static TRef find_kinit(jit_State *J, const BCIns *endpc, BCReg slot, IRType t)
{
  /* This algorithm is rather simplistic and assumes quite a bit about
  ** how the bytecode is generated. It works fine for FORI initializers,
  ** but it won't necessarily work in other cases (e.g. iterator arguments).
  ** It doesn't do anything fancy, either (like backpropagating MOVs).
  */
  const BCIns *pc, *startpc = proto_bc(J->pt);
  for (pc = endpc-1; pc > startpc; pc--) {
    BCIns ins = *pc;
    BCOp op = bc_op(ins);
    /* First try to find the last instruction that stores to this slot. */
    if (bcmode_a(op) == BCMbase && bc_a(ins) <= slot) {
      return 0;  /* Multiple results, e.g. from a CALL or KNIL. */
    } else if (bcmode_a(op) == BCMdst && bc_a(ins) == slot) {
      if (op == BC_KSHORT || op == BC_KNUM) {  /* Found const. initializer. */
	/* Now try to verify there's no forward jump across it. */
	const BCIns *kpc = pc;
	for (; pc > startpc; pc--)
	  if (bc_op(*pc) == BC_JMP) {
	    const BCIns *target = pc+bc_j(*pc)+1;
	    if (target > kpc && target <= endpc)
	      return 0;  /* Conditional assignment. */
	  }
	if (op == BC_KSHORT) {
	  int32_t k = (int32_t)(int16_t)bc_d(ins);
	  return t == IRT_INT ? lj_ir_kint(J, k) : lj_ir_knum(J, (lua_Number)k);
	} else {
	  cTValue *tv = proto_knumtv(J->pt, bc_d(ins));
	  if (t == IRT_INT) {
	    int32_t k = numberVint(tv);
	    if (tvisint(tv) || numV(tv) == (lua_Number)k)  /* -0 is ok here. */
	      return lj_ir_kint(J, k);
	    return 0;  /* Type mismatch. */
	  } else {
	    return lj_ir_knum(J, numberVnum(tv));
	  }
	}
      }
      return 0;  /* Non-constant initializer. */
    }
  }
  return 0;  /* No assignment to this slot found? */
}

/* Load and optionally convert a FORI argument from a slot. */
static TRef fori_load(jit_State *J, BCReg slot, IRType t, int mode)
{
  int conv = (tvisint(&J->L->base[slot]) != (t==IRT_INT)) ? IRSLOAD_CONVERT : 0;
  return sloadt(J, (int32_t)slot,
		t + (((mode & IRSLOAD_TYPECHECK) ||
		      (conv && t == IRT_INT && !(mode >> 16))) ?
		     IRT_GUARD : 0),
		mode + conv);
}

/* Peek before FORI to find a const initializer. Otherwise load from slot. */
static TRef fori_arg(jit_State *J, const BCIns *fori, BCReg slot,
		     IRType t, int mode)
{
  TRef tr = J->base[slot];
  if (!tr) {
    tr = find_kinit(J, fori, slot, t);
    if (!tr)
      tr = fori_load(J, slot, t, mode);
  }
  return tr;
}

/* Return the direction of the FOR loop iterator.
** It's important to exactly reproduce the semantics of the interpreter.
*/
static int rec_for_direction(cTValue *o)
{
  return (tvisint(o) ? intV(o) : (int32_t)o->u32.hi) >= 0;
}

/* Simulate the runtime behavior of the FOR loop iterator. */
static LoopEvent rec_for_iter(IROp *op, cTValue *o, int isforl)
{
  lua_Number stopv = numberVnum(&o[FORL_STOP]);
  lua_Number idxv = numberVnum(&o[FORL_IDX]);
  lua_Number stepv = numberVnum(&o[FORL_STEP]);
  if (isforl)
    idxv += stepv;
  if (rec_for_direction(&o[FORL_STEP])) {
    if (idxv <= stopv) {
      *op = IR_LE;
      return idxv + 2*stepv > stopv ? LOOPEV_ENTERLO : LOOPEV_ENTER;
    }
    *op = IR_GT; return LOOPEV_LEAVE;
  } else {
    if (stopv <= idxv) {
      *op = IR_GE;
      return idxv + 2*stepv < stopv ? LOOPEV_ENTERLO : LOOPEV_ENTER;
    }
    *op = IR_LT; return LOOPEV_LEAVE;
  }
}

/* Record checks for FOR loop overflow and step direction. */
static void rec_for_check(jit_State *J, IRType t, int dir,
			  TRef stop, TRef step, int init)
{
  if (!tref_isk(step)) {
    /* Non-constant step: need a guard for the direction. */
    TRef zero = (t == IRT_INT) ? lj_ir_kint(J, 0) : lj_ir_knum_zero(J);
    emitir(IRTG(dir ? IR_GE : IR_LT, t), step, zero);
    /* Add hoistable overflow checks for a narrowed FORL index. */
    if (init && t == IRT_INT) {
      if (tref_isk(stop)) {
	/* Constant stop: optimize check away or to a range check for step. */
	int32_t k = IR(tref_ref(stop))->i;
	if (dir) {
	  if (k > 0)
	    emitir(IRTGI(IR_LE), step, lj_ir_kint(J, (int32_t)0x7fffffff-k));
	} else {
	  if (k < 0)
	    emitir(IRTGI(IR_GE), step, lj_ir_kint(J, (int32_t)0x80000000-k));
	}
      } else {
	/* Stop+step variable: need full overflow check. */
	TRef tr = emitir(IRTGI(IR_ADDOV), step, stop);
	emitir(IRTI(IR_USE), tr, 0);  /* ADDOV is weak. Avoid dead result. */
      }
    }
  } else if (init && t == IRT_INT && !tref_isk(stop)) {
    /* Constant step: optimize overflow check to a range check for stop. */
    int32_t k = IR(tref_ref(step))->i;
    k = (int32_t)(dir ? 0x7fffffff : 0x80000000) - k;
    emitir(IRTGI(dir ? IR_LE : IR_GE), stop, lj_ir_kint(J, k));
  }
}

/* Record a FORL instruction. */
static void rec_for_loop(jit_State *J, const BCIns *fori, ScEvEntry *scev,
			 int init)
{
  BCReg ra = bc_a(*fori);
  cTValue *tv = &J->L->base[ra];
  TRef idx = J->base[ra+FORL_IDX];
  IRType t = idx ? tref_type(idx) :
	     (init || LJ_DUALNUM) ? lj_opt_narrow_forl(J, tv) : IRT_NUM;
  int mode = IRSLOAD_INHERIT +
    ((!LJ_DUALNUM || tvisint(tv) == (t == IRT_INT)) ? IRSLOAD_READONLY : 0);
  TRef stop = fori_arg(J, fori, ra+FORL_STOP, t, mode);
  TRef step = fori_arg(J, fori, ra+FORL_STEP, t, mode);
  int tc, dir = rec_for_direction(&tv[FORL_STEP]);
  lua_assert(bc_op(*fori) == BC_FORI || bc_op(*fori) == BC_JFORI);
  scev->t.irt = t;
  scev->dir = dir;
  scev->stop = tref_ref(stop);
  scev->step = tref_ref(step);
  rec_for_check(J, t, dir, stop, step, init);
  scev->start = tref_ref(find_kinit(J, fori, ra+FORL_IDX, IRT_INT));
  tc = (LJ_DUALNUM &&
	!(scev->start && irref_isk(scev->stop) && irref_isk(scev->step) &&
	  tvisint(&tv[FORL_IDX]) == (t == IRT_INT))) ?
	IRSLOAD_TYPECHECK : 0;
  if (tc) {
    J->base[ra+FORL_STOP] = stop;
    J->base[ra+FORL_STEP] = step;
  }
  if (!idx)
    idx = fori_load(J, ra+FORL_IDX, t,
		    IRSLOAD_INHERIT + tc + (J->scev.start << 16));
  if (!init)
    J->base[ra+FORL_IDX] = idx = emitir(IRT(IR_ADD, t), idx, step);
  J->base[ra+FORL_EXT] = idx;
  scev->idx = tref_ref(idx);
  J->maxslot = ra+FORL_EXT+1;
}

/* Record FORL/JFORL or FORI/JFORI. */
static LoopEvent rec_for(jit_State *J, const BCIns *fori, int isforl)
{
  BCReg ra = bc_a(*fori);
  TValue *tv = &J->L->base[ra];
  TRef *tr = &J->base[ra];
  IROp op;
  LoopEvent ev;
  TRef stop;
  IRType t;
  if (isforl) {  /* Handle FORL/JFORL opcodes. */
    TRef idx = tr[FORL_IDX];
    if (tref_ref(idx) == J->scev.idx) {
      t = J->scev.t.irt;
      stop = J->scev.stop;
      idx = emitir(IRT(IR_ADD, t), idx, J->scev.step);
      tr[FORL_EXT] = tr[FORL_IDX] = idx;
    } else {
      ScEvEntry scev;
      rec_for_loop(J, fori, &scev, 0);
      t = scev.t.irt;
      stop = scev.stop;
    }
  } else {  /* Handle FORI/JFORI opcodes. */
    BCReg i;
    lj_meta_for(J->L, tv);
    t = (LJ_DUALNUM || tref_isint(tr[FORL_IDX])) ? lj_opt_narrow_forl(J, tv) :
						   IRT_NUM;
    for (i = FORL_IDX; i <= FORL_STEP; i++) {
      if (!tr[i]) sload(J, ra+i);
      lua_assert(tref_isnumber_str(tr[i]));
      if (tref_isstr(tr[i]))
	tr[i] = emitir(IRTG(IR_STRTO, IRT_NUM), tr[i], 0);
      if (t == IRT_INT) {
	if (!tref_isinteger(tr[i]))
	  tr[i] = emitir(IRTGI(IR_CONV), tr[i], IRCONV_INT_NUM|IRCONV_CHECK);
      } else {
	if (!tref_isnum(tr[i]))
	  tr[i] = emitir(IRTN(IR_CONV), tr[i], IRCONV_NUM_INT);
      }
    }
    tr[FORL_EXT] = tr[FORL_IDX];
    stop = tr[FORL_STOP];
    rec_for_check(J, t, rec_for_direction(&tv[FORL_STEP]),
		  stop, tr[FORL_STEP], 1);
  }

  ev = rec_for_iter(&op, tv, isforl);
  if (ev == LOOPEV_LEAVE) {
    J->maxslot = ra+FORL_EXT+1;
    J->pc = fori+1;
  } else {
    J->maxslot = ra;
    J->pc = fori+bc_j(*fori)+1;
  }
  lj_snap_add(J);

  emitir(IRTG(op, t), tr[FORL_IDX], stop);

  if (ev == LOOPEV_LEAVE) {
    J->maxslot = ra;
    J->pc = fori+bc_j(*fori)+1;
  } else {
    J->maxslot = ra+FORL_EXT+1;
    J->pc = fori+1;
  }
  J->needsnap = 1;
  return ev;
}

/* Record ITERL/JITERL. */
static LoopEvent rec_iterl(jit_State *J, const BCIns iterins)
{
  BCReg ra = bc_a(iterins);
  lua_assert(J->base[ra] != 0);
  if (!tref_isnil(J->base[ra])) {  /* Looping back? */
    J->base[ra-1] = J->base[ra];  /* Copy result of ITERC to control var. */
    J->maxslot = ra-1+bc_b(J->pc[-1]);
    J->pc += bc_j(iterins)+1;
    return LOOPEV_ENTER;
  } else {
    J->maxslot = ra-3;
    J->pc++;
    return LOOPEV_LEAVE;
  }
}

/* Record LOOP/JLOOP. Now, that was easy. */
static LoopEvent rec_loop(jit_State *J, BCReg ra)
{
  if (ra < J->maxslot) J->maxslot = ra;
  J->pc++;
  return LOOPEV_ENTER;
}

/* Check if a loop repeatedly failed to trace because it didn't loop back. */
static int innerloopleft(jit_State *J, const BCIns *pc)
{
  ptrdiff_t i;
  for (i = 0; i < PENALTY_SLOTS; i++)
    if (mref(J->penalty[i].pc, const BCIns) == pc) {
      if ((J->penalty[i].reason == LJ_TRERR_LLEAVE ||
	   J->penalty[i].reason == LJ_TRERR_LINNER) &&
	  J->penalty[i].val >= 2*PENALTY_MIN)
	return 1;
      break;
    }
  return 0;
}

/* Handle the case when an interpreted loop op is hit. */
static void rec_loop_interp(jit_State *J, const BCIns *pc, LoopEvent ev)
{
  if (J->parent == 0) {
    if (pc == J->startpc && J->framedepth + J->retdepth == 0) {
      /* Same loop? */
      if (ev == LOOPEV_LEAVE)  /* Must loop back to form a root trace. */
	lj_trace_err(J, LJ_TRERR_LLEAVE);
      rec_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Looping root trace. */
    } else if (ev != LOOPEV_LEAVE) {  /* Entering inner loop? */
      /* It's usually better to abort here and wait until the inner loop
      ** is traced. But if the inner loop repeatedly didn't loop back,
      ** this indicates a low trip count. In this case try unrolling
      ** an inner loop even in a root trace. But it's better to be a bit
      ** more conservative here and only do it for very short loops.
      */
      if (bc_j(*pc) != -1 && !innerloopleft(J, pc))
	lj_trace_err(J, LJ_TRERR_LINNER);  /* Root trace hit an inner loop. */
      if ((ev != LOOPEV_ENTERLO &&
	   J->loopref && J->cur.nins - J->loopref > 24) || --J->loopunroll < 0)
	lj_trace_err(J, LJ_TRERR_LUNROLL);  /* Limit loop unrolling. */
      J->loopref = J->cur.nins;
    }
  } else if (ev != LOOPEV_LEAVE) {  /* Side trace enters an inner loop. */
    J->loopref = J->cur.nins;
    if (--J->loopunroll < 0)
      lj_trace_err(J, LJ_TRERR_LUNROLL);  /* Limit loop unrolling. */
  }  /* Side trace continues across a loop that's left or not entered. */
}

/* Handle the case when an already compiled loop op is hit. */
static void rec_loop_jit(jit_State *J, TraceNo lnk, LoopEvent ev)
{
  if (J->parent == 0) {  /* Root trace hit an inner loop. */
    /* Better let the inner loop spawn a side trace back here. */
    lj_trace_err(J, LJ_TRERR_LINNER);
  } else if (ev != LOOPEV_LEAVE) {  /* Side trace enters a compiled loop. */
    J->instunroll = 0;  /* Cannot continue across a compiled loop op. */
    if (J->pc == J->startpc && J->framedepth + J->retdepth == 0)
      rec_stop(J, LJ_TRLINK_LOOP, J->cur.traceno);  /* Form an extra loop. */
    else
      rec_stop(J, LJ_TRLINK_ROOT, lnk);  /* Link to the loop. */
  }  /* Side trace continues across a loop that's left or not entered. */
}

/* -- Record calls and returns -------------------------------------------- */

/* Specialize to the runtime value of the called function or its prototype. */
static TRef rec_call_specialize(jit_State *J, GCfunc *fn, TRef tr)
{
  TRef kfunc;
  if (isluafunc(fn)) {
    GCproto *pt = funcproto(fn);
    /* Too many closures created? Probably not a monomorphic function. */
    if (pt->flags >= PROTO_CLC_POLY) {  /* Specialize to prototype instead. */
      TRef trpt = emitir(IRT(IR_FLOAD, IRT_P32), tr, IRFL_FUNC_PC);
      emitir(IRTG(IR_EQ, IRT_P32), trpt, lj_ir_kptr(J, proto_bc(pt)));
      (void)lj_ir_kgc(J, obj2gco(pt), IRT_PROTO);  /* Prevent GC of proto. */
      return tr;
    }
  }
  /* Otherwise specialize to the function (closure) value itself. */
  kfunc = lj_ir_kfunc(J, fn);
  emitir(IRTG(IR_EQ, IRT_FUNC), tr, kfunc);
  return kfunc;
}

/* Record call setup. */
static void rec_call_setup(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  RecordIndex ix;
  TValue *functv = &J->L->base[func];
  TRef *fbase = &J->base[func];
  ptrdiff_t i;
  for (i = 0; i <= nargs; i++)
    (void)getslot(J, func+i);  /* Ensure func and all args have a reference. */
  if (!tref_isfunc(fbase[0])) {  /* Resolve __call metamethod. */
    ix.tab = fbase[0];
    copyTV(J->L, &ix.tabv, functv);
    if (!lj_record_mm_lookup(J, &ix, MM_call) || !tref_isfunc(ix.mobj))
      lj_trace_err(J, LJ_TRERR_NOMM);
    for (i = ++nargs; i > 0; i--)  /* Shift arguments up. */
      fbase[i] = fbase[i-1];
    fbase[0] = ix.mobj;  /* Replace function. */
    functv = &ix.mobjv;
  }
  fbase[0] = TREF_FRAME | rec_call_specialize(J, funcV(functv), fbase[0]);
  J->maxslot = (BCReg)nargs;
}

/* Record call. */
void lj_record_call(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  rec_call_setup(J, func, nargs);
  /* Bump frame. */
  J->framedepth++;
  J->base += func+1;
  J->baseslot += func+1;
}

/* Record tail call. */
void lj_record_tailcall(jit_State *J, BCReg func, ptrdiff_t nargs)
{
  rec_call_setup(J, func, nargs);
  if (frame_isvarg(J->L->base - 1)) {
    BCReg cbase = (BCReg)frame_delta(J->L->base - 1);
    if (--J->framedepth < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    func += cbase;
  }
  /* Move func + args down. */
  memmove(&J->base[-1], &J->base[func], sizeof(TRef)*(J->maxslot+1));
  /* Note: the new TREF_FRAME is now at J->base[-1] (even for slot #0). */
  /* Tailcalls can form a loop, so count towards the loop unroll limit. */
  if (++J->tailcalled > J->loopunroll)
    lj_trace_err(J, LJ_TRERR_LUNROLL);
}

/* Check unroll limits for down-recursion. */
static int check_downrec_unroll(jit_State *J, GCproto *pt)
{
  IRRef ptref;
  for (ptref = J->chain[IR_KGC]; ptref; ptref = IR(ptref)->prev)
    if (ir_kgc(IR(ptref)) == obj2gco(pt)) {
      int count = 0;
      IRRef ref;
      for (ref = J->chain[IR_RETF]; ref; ref = IR(ref)->prev)
	if (IR(ref)->op1 == ptref)
	  count++;
      if (count) {
	if (J->pc == J->startpc) {
	  if (count + J->tailcalled > J->param[JIT_P_recunroll])
	    return 1;
	} else {
	  lj_trace_err(J, LJ_TRERR_DOWNREC);
	}
      }
    }
  return 0;
}

/* Record return. */
void lj_record_ret(jit_State *J, BCReg rbase, ptrdiff_t gotresults)
{
  TValue *frame = J->L->base - 1;
  ptrdiff_t i;
  for (i = 0; i < gotresults; i++)
    (void)getslot(J, rbase+i);  /* Ensure all results have a reference. */
  while (frame_ispcall(frame)) {  /* Immediately resolve pcall() returns. */
    BCReg cbase = (BCReg)frame_delta(frame);
    if (--J->framedepth < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    lua_assert(J->baseslot > 1);
    gotresults++;
    rbase += cbase;
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    J->base[--rbase] = TREF_TRUE;  /* Prepend true to results. */
    frame = frame_prevd(frame);
  }
  /* Return to lower frame via interpreter for unhandled cases. */
  if (J->framedepth == 0 && J->pt && bc_isret(bc_op(*J->pc)) &&
       (!frame_islua(frame) ||
	(J->parent == 0 && !bc_isret(bc_op(J->cur.startins))))) {
    /* NYI: specialize to frame type and return directly, not via RET*. */
    for (i = -1; i < (ptrdiff_t)rbase; i++)
      J->base[i] = 0;  /* Purge dead slots. */
    J->maxslot = rbase + (BCReg)gotresults;
    rec_stop(J, LJ_TRLINK_RETURN, 0);  /* Return to interpreter. */
    return;
  }
  if (frame_isvarg(frame)) {
    BCReg cbase = (BCReg)frame_delta(frame);
    if (--J->framedepth < 0)  /* NYI: return of vararg func to lower frame. */
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    lua_assert(J->baseslot > 1);
    rbase += cbase;
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    frame = frame_prevd(frame);
  }
  if (frame_islua(frame)) {  /* Return to Lua frame. */
    BCIns callins = *(frame_pc(frame)-1);
    ptrdiff_t nresults = bc_b(callins) ? (ptrdiff_t)bc_b(callins)-1 :gotresults;
    BCReg cbase = bc_a(callins);
    GCproto *pt = funcproto(frame_func(frame - (cbase+1)));
    if (J->framedepth == 0 && J->pt && frame == J->L->base - 1) {
      if (check_downrec_unroll(J, pt)) {
	J->maxslot = (BCReg)(rbase + gotresults);
	lj_snap_purge(J);
	rec_stop(J, LJ_TRLINK_DOWNREC, J->cur.traceno);  /* Down-recursion. */
	return;
      }
      lj_snap_add(J);
    }
    for (i = 0; i < nresults; i++)  /* Adjust results. */
      J->base[i-1] = i < gotresults ? J->base[rbase+i] : TREF_NIL;
    J->maxslot = cbase+(BCReg)nresults;
    if (J->framedepth > 0) {  /* Return to a frame that is part of the trace. */
      J->framedepth--;
      lua_assert(J->baseslot > cbase+1);
      J->baseslot -= cbase+1;
      J->base -= cbase+1;
    } else if (J->parent == 0 && !bc_isret(bc_op(J->cur.startins))) {
      /* Return to lower frame would leave the loop in a root trace. */
      lj_trace_err(J, LJ_TRERR_LLEAVE);
    } else {  /* Return to lower frame. Guard for the target we return to. */
      TRef trpt = lj_ir_kgc(J, obj2gco(pt), IRT_PROTO);
      TRef trpc = lj_ir_kptr(J, (void *)frame_pc(frame));
      emitir(IRTG(IR_RETF, IRT_P32), trpt, trpc);
      J->retdepth++;
      J->needsnap = 1;
      lua_assert(J->baseslot == 1);
      /* Shift result slots up and clear the slots of the new frame below. */
      memmove(J->base + cbase, J->base-1, sizeof(TRef)*nresults);
      memset(J->base-1, 0, sizeof(TRef)*(cbase+1));
    }
  } else if (frame_iscont(frame)) {  /* Return to continuation frame. */
    ASMFunction cont = frame_contf(frame);
    BCReg cbase = (BCReg)frame_delta(frame);
    if ((J->framedepth -= 2) < 0)
      lj_trace_err(J, LJ_TRERR_NYIRETL);
    J->baseslot -= (BCReg)cbase;
    J->base -= cbase;
    J->maxslot = cbase-2;
    if (cont == lj_cont_ra) {
      /* Copy result to destination slot. */
      BCReg dst = bc_a(*(frame_contpc(frame)-1));
      J->base[dst] = gotresults ? J->base[cbase+rbase] : TREF_NIL;
      if (dst >= J->maxslot) J->maxslot = dst+1;
    } else if (cont == lj_cont_nop) {
      /* Nothing to do here. */
    } else if (cont == lj_cont_cat) {
      lua_assert(0);
    } else {
      /* Result type already specialized. */
      lua_assert(cont == lj_cont_condf || cont == lj_cont_condt);
    }
  } else {
    lj_trace_err(J, LJ_TRERR_NYIRETL);  /* NYI: handle return to C frame. */
  }
  lua_assert(J->baseslot >= 1);
}

/* -- Metamethod handling ------------------------------------------------- */

/* Prepare to record call to metamethod. */
static BCReg rec_mm_prep(jit_State *J, ASMFunction cont)
{
  BCReg s, top = curr_proto(J->L)->framesize;
  TRef trcont;
  setcont(&J->L->base[top], cont);
#if LJ_64
  trcont = lj_ir_kptr(J, (void *)((int64_t)cont - (int64_t)lj_vm_asm_begin));
#else
  trcont = lj_ir_kptr(J, (void *)cont);
#endif
  J->base[top] = trcont | TREF_CONT;
  J->framedepth++;
  for (s = J->maxslot; s < top; s++)
    J->base[s] = 0;  /* Clear frame gap to avoid resurrecting previous refs. */
  return top+1;
}

/* Record metamethod lookup. */
int lj_record_mm_lookup(jit_State *J, RecordIndex *ix, MMS mm)
{
  RecordIndex mix;
  GCtab *mt;
  if (tref_istab(ix->tab)) {
    mt = tabref(tabV(&ix->tabv)->metatable);
    mix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_TAB_META);
  } else if (tref_isudata(ix->tab)) {
    int udtype = udataV(&ix->tabv)->udtype;
    mt = tabref(udataV(&ix->tabv)->metatable);
    /* The metatables of special userdata objects are treated as immutable. */
    if (udtype != UDTYPE_USERDATA) {
      cTValue *mo;
      if (LJ_HASFFI && udtype == UDTYPE_FFI_CLIB) {
	/* Specialize to the C library namespace object. */
	emitir(IRTG(IR_EQ, IRT_P32), ix->tab, lj_ir_kptr(J, udataV(&ix->tabv)));
      } else {
	/* Specialize to the type of userdata. */
	TRef tr = emitir(IRT(IR_FLOAD, IRT_U8), ix->tab, IRFL_UDATA_UDTYPE);
	emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, udtype));
      }
  immutable_mt:
      mo = lj_tab_getstr(mt, mmname_str(J2G(J), mm));
      if (!mo || tvisnil(mo))
	return 0;  /* No metamethod. */
      /* Treat metamethod or index table as immutable, too. */
      if (!(tvisfunc(mo) || tvistab(mo)))
	lj_trace_err(J, LJ_TRERR_BADTYPE);
      copyTV(J->L, &ix->mobjv, mo);
      ix->mobj = lj_ir_kgc(J, gcV(mo), tvisfunc(mo) ? IRT_FUNC : IRT_TAB);
      ix->mtv = mt;
      ix->mt = TREF_NIL;  /* Dummy value for comparison semantics. */
      return 1;  /* Got metamethod or index table. */
    }
    mix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_UDATA_META);
  } else {
    /* Specialize to base metatable. Must flush mcode in lua_setmetatable(). */
    mt = tabref(basemt_obj(J2G(J), &ix->tabv));
    if (mt == NULL) {
      ix->mt = TREF_NIL;
      return 0;  /* No metamethod. */
    }
    /* The cdata metatable is treated as immutable. */
    if (LJ_HASFFI && tref_iscdata(ix->tab)) goto immutable_mt;
    ix->mt = mix.tab = lj_ir_ktab(J, mt);
    goto nocheck;
  }
  ix->mt = mt ? mix.tab : TREF_NIL;
  emitir(IRTG(mt ? IR_NE : IR_EQ, IRT_TAB), mix.tab, lj_ir_knull(J, IRT_TAB));
nocheck:
  if (mt) {
    GCstr *mmstr = mmname_str(J2G(J), mm);
    cTValue *mo = lj_tab_getstr(mt, mmstr);
    if (mo && !tvisnil(mo))
      copyTV(J->L, &ix->mobjv, mo);
    ix->mtv = mt;
    settabV(J->L, &mix.tabv, mt);
    setstrV(J->L, &mix.keyv, mmstr);
    mix.key = lj_ir_kstr(J, mmstr);
    mix.val = 0;
    mix.idxchain = 0;
    ix->mobj = lj_record_idx(J, &mix);
    return !tref_isnil(ix->mobj);  /* 1 if metamethod found, 0 if not. */
  }
  return 0;  /* No metamethod. */
}

/* Record call to arithmetic metamethod. */
static TRef rec_mm_arith(jit_State *J, RecordIndex *ix, MMS mm)
{
  /* Set up metamethod call first to save ix->tab and ix->tabv. */
  BCReg func = rec_mm_prep(J, lj_cont_ra);
  TRef *base = J->base + func;
  TValue *basev = J->L->base + func;
  base[1] = ix->tab; base[2] = ix->key;
  copyTV(J->L, basev+1, &ix->tabv);
  copyTV(J->L, basev+2, &ix->keyv);
  if (!lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
    if (mm != MM_unm) {
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, &ix->keyv);
      if (lj_record_mm_lookup(J, ix, mm))  /* Lookup mm on 2nd operand. */
	goto ok;
    }
    lj_trace_err(J, LJ_TRERR_NOMM);
  }
ok:
  base[0] = ix->mobj;
  copyTV(J->L, basev+0, &ix->mobjv);
  lj_record_call(J, func, 2);
  return 0;  /* No result yet. */
}

/* Record call to __len metamethod. */
static TRef rec_mm_len(jit_State *J, TRef tr, TValue *tv)
{
  RecordIndex ix;
  ix.tab = tr;
  copyTV(J->L, &ix.tabv, tv);
  if (lj_record_mm_lookup(J, &ix, MM_len)) {
    BCReg func = rec_mm_prep(J, lj_cont_ra);
    TRef *base = J->base + func;
    TValue *basev = J->L->base + func;
    base[0] = ix.mobj; copyTV(J->L, basev+0, &ix.mobjv);
    base[1] = tr; copyTV(J->L, basev+1, tv);
#if LJ_52
    base[2] = tr; copyTV(J->L, basev+2, tv);
#else
    base[2] = TREF_NIL; setnilV(basev+2);
#endif
    lj_record_call(J, func, 2);
  } else {
    if (LJ_52 && tref_istab(tr))
      return lj_ir_call(J, IRCALL_lj_tab_len, tr);
    lj_trace_err(J, LJ_TRERR_NOMM);
  }
  return 0;  /* No result yet. */
}

/* Call a comparison metamethod. */
static void rec_mm_callcomp(jit_State *J, RecordIndex *ix, int op)
{
  BCReg func = rec_mm_prep(J, (op&1) ? lj_cont_condf : lj_cont_condt);
  TRef *base = J->base + func;
  TValue *tv = J->L->base + func;
  base[0] = ix->mobj; base[1] = ix->val; base[2] = ix->key;
  copyTV(J->L, tv+0, &ix->mobjv);
  copyTV(J->L, tv+1, &ix->valv);
  copyTV(J->L, tv+2, &ix->keyv);
  lj_record_call(J, func, 2);
}

/* Record call to equality comparison metamethod (for tab and udata only). */
static void rec_mm_equal(jit_State *J, RecordIndex *ix, int op)
{
  ix->tab = ix->val;
  copyTV(J->L, &ix->tabv, &ix->valv);
  if (lj_record_mm_lookup(J, ix, MM_eq)) {  /* Lookup mm on 1st operand. */
    cTValue *bv;
    TRef mo1 = ix->mobj;
    TValue mo1v;
    copyTV(J->L, &mo1v, &ix->mobjv);
    /* Avoid the 2nd lookup and the objcmp if the metatables are equal. */
    bv = &ix->keyv;
    if (tvistab(bv) && tabref(tabV(bv)->metatable) == ix->mtv) {
      TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_TAB_META);
      emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
    } else if (tvisudata(bv) && tabref(udataV(bv)->metatable) == ix->mtv) {
      TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_UDATA_META);
      emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
    } else {  /* Lookup metamethod on 2nd operand and compare both. */
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, bv);
      if (!lj_record_mm_lookup(J, ix, MM_eq) ||
	  lj_record_objcmp(J, mo1, ix->mobj, &mo1v, &ix->mobjv))
	return;
    }
    rec_mm_callcomp(J, ix, op);
  }
}

/* Record call to ordered comparison metamethods (for arbitrary objects). */
static void rec_mm_comp(jit_State *J, RecordIndex *ix, int op)
{
  ix->tab = ix->val;
  copyTV(J->L, &ix->tabv, &ix->valv);
  while (1) {
    MMS mm = (op & 2) ? MM_le : MM_lt;  /* Try __le + __lt or only __lt. */
#if LJ_52
    if (!lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
      ix->tab = ix->key;
      copyTV(J->L, &ix->tabv, &ix->keyv);
      if (!lj_record_mm_lookup(J, ix, mm))  /* Lookup mm on 2nd operand. */
	goto nomatch;
    }
    rec_mm_callcomp(J, ix, op);
    return;
#else
    if (lj_record_mm_lookup(J, ix, mm)) {  /* Lookup mm on 1st operand. */
      cTValue *bv;
      TRef mo1 = ix->mobj;
      TValue mo1v;
      copyTV(J->L, &mo1v, &ix->mobjv);
      /* Avoid the 2nd lookup and the objcmp if the metatables are equal. */
      bv = &ix->keyv;
      if (tvistab(bv) && tabref(tabV(bv)->metatable) == ix->mtv) {
	TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_TAB_META);
	emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
      } else if (tvisudata(bv) && tabref(udataV(bv)->metatable) == ix->mtv) {
	TRef mt2 = emitir(IRT(IR_FLOAD, IRT_TAB), ix->key, IRFL_UDATA_META);
	emitir(IRTG(IR_EQ, IRT_TAB), mt2, ix->mt);
      } else {  /* Lookup metamethod on 2nd operand and compare both. */
	ix->tab = ix->key;
	copyTV(J->L, &ix->tabv, bv);
	if (!lj_record_mm_lookup(J, ix, mm) ||
	    lj_record_objcmp(J, mo1, ix->mobj, &mo1v, &ix->mobjv))
	  goto nomatch;
      }
      rec_mm_callcomp(J, ix, op);
      return;
    }
#endif
  nomatch:
    /* Lookup failed. Retry with  __lt and swapped operands. */
    if (!(op & 2)) break;  /* Already at __lt. Interpreter will throw. */
    ix->tab = ix->key; ix->key = ix->val; ix->val = ix->tab;
    copyTV(J->L, &ix->tabv, &ix->keyv);
    copyTV(J->L, &ix->keyv, &ix->valv);
    copyTV(J->L, &ix->valv, &ix->tabv);
    op ^= 3;
  }
}

#if LJ_HASFFI
/* Setup call to cdata comparison metamethod. */
static void rec_mm_comp_cdata(jit_State *J, RecordIndex *ix, int op, MMS mm)
{
  lj_snap_add(J);
  if (tref_iscdata(ix->val)) {
    ix->tab = ix->val;
    copyTV(J->L, &ix->tabv, &ix->valv);
  } else {
    lua_assert(tref_iscdata(ix->key));
    ix->tab = ix->key;
    copyTV(J->L, &ix->tabv, &ix->keyv);
  }
  lj_record_mm_lookup(J, ix, mm);
  rec_mm_callcomp(J, ix, op);
}
#endif

/* -- Indexed access ------------------------------------------------------ */

/* Record bounds-check. */
static void rec_idx_abc(jit_State *J, TRef asizeref, TRef ikey, uint32_t asize)
{
  /* Try to emit invariant bounds checks. */
  if ((J->flags & (JIT_F_OPT_LOOP|JIT_F_OPT_ABC)) ==
      (JIT_F_OPT_LOOP|JIT_F_OPT_ABC)) {
    IRRef ref = tref_ref(ikey);
    IRIns *ir = IR(ref);
    int32_t ofs = 0;
    IRRef ofsref = 0;
    /* Handle constant offsets. */
    if (ir->o == IR_ADD && irref_isk(ir->op2)) {
      ofsref = ir->op2;
      ofs = IR(ofsref)->i;
      ref = ir->op1;
      ir = IR(ref);
    }
    /* Got scalar evolution analysis results for this reference? */
    if (ref == J->scev.idx) {
      int32_t stop;
      lua_assert(irt_isint(J->scev.t) && ir->o == IR_SLOAD);
      stop = numberVint(&(J->L->base - J->baseslot)[ir->op1 + FORL_STOP]);
      /* Runtime value for stop of loop is within bounds? */
      if ((int64_t)stop + ofs < (int64_t)asize) {
	/* Emit invariant bounds check for stop. */
	emitir(IRTG(IR_ABC, IRT_P32), asizeref, ofs == 0 ? J->scev.stop :
	       emitir(IRTI(IR_ADD), J->scev.stop, ofsref));
	/* Emit invariant bounds check for start, if not const or negative. */
	if (!(J->scev.dir && J->scev.start &&
	      (int64_t)IR(J->scev.start)->i + ofs >= 0))
	  emitir(IRTG(IR_ABC, IRT_P32), asizeref, ikey);
	return;
      }
    }
  }
  emitir(IRTGI(IR_ABC), asizeref, ikey);  /* Emit regular bounds check. */
}

/* Record indexed key lookup. */
static TRef rec_idx_key(jit_State *J, RecordIndex *ix)
{
  TRef key;
  GCtab *t = tabV(&ix->tabv);
  ix->oldv = lj_tab_get(J->L, t, &ix->keyv);  /* Lookup previous value. */

  /* Integer keys are looked up in the array part first. */
  key = ix->key;
  if (tref_isnumber(key)) {
    int32_t k = numberVint(&ix->keyv);
    if (!tvisint(&ix->keyv) && numV(&ix->keyv) != (lua_Number)k)
      k = LJ_MAX_ASIZE;
    if ((MSize)k < LJ_MAX_ASIZE) {  /* Potential array key? */
      TRef ikey = lj_opt_narrow_index(J, key);
      TRef asizeref = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_ASIZE);
      if ((MSize)k < t->asize) {  /* Currently an array key? */
	TRef arrayref;
	rec_idx_abc(J, asizeref, ikey, t->asize);
	arrayref = emitir(IRT(IR_FLOAD, IRT_P32), ix->tab, IRFL_TAB_ARRAY);
	return emitir(IRT(IR_AREF, IRT_P32), arrayref, ikey);
      } else {  /* Currently not in array (may be an array extension)? */
	emitir(IRTGI(IR_ULE), asizeref, ikey);  /* Inv. bounds check. */
	if (k == 0 && tref_isk(key))
	  key = lj_ir_knum_zero(J);  /* Canonicalize 0 or +-0.0 to +0.0. */
	/* And continue with the hash lookup. */
      }
    } else if (!tref_isk(key)) {
      /* We can rule out const numbers which failed the integerness test
      ** above. But all other numbers are potential array keys.
      */
      if (t->asize == 0) {  /* True sparse tables have an empty array part. */
	/* Guard that the array part stays empty. */
	TRef tmp = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_ASIZE);
	emitir(IRTGI(IR_EQ), tmp, lj_ir_kint(J, 0));
      } else {
	lj_trace_err(J, LJ_TRERR_NYITMIX);
      }
    }
  }

  /* Otherwise the key is located in the hash part. */
  if (t->hmask == 0) {  /* Shortcut for empty hash part. */
    /* Guard that the hash part stays empty. */
    TRef tmp = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_HMASK);
    emitir(IRTGI(IR_EQ), tmp, lj_ir_kint(J, 0));
    return lj_ir_kkptr(J, niltvg(J2G(J)));
  }
  if (tref_isinteger(key))  /* Hash keys are based on numbers, not ints. */
    key = emitir(IRTN(IR_CONV), key, IRCONV_NUM_INT);
  if (tref_isk(key)) {
    /* Optimize lookup of constant hash keys. */
    MSize hslot = (MSize)((char *)ix->oldv - (char *)&noderef(t->node)[0].val);
    if (t->hmask > 0 && hslot <= t->hmask*(MSize)sizeof(Node) &&
	hslot <= 65535*(MSize)sizeof(Node)) {
      TRef node, kslot;
      TRef hm = emitir(IRTI(IR_FLOAD), ix->tab, IRFL_TAB_HMASK);
      emitir(IRTGI(IR_EQ), hm, lj_ir_kint(J, (int32_t)t->hmask));
      node = emitir(IRT(IR_FLOAD, IRT_P32), ix->tab, IRFL_TAB_NODE);
      kslot = lj_ir_kslot(J, key, hslot / sizeof(Node));
      return emitir(IRTG(IR_HREFK, IRT_P32), node, kslot);
    }
  }
  /* Fall back to a regular hash lookup. */
  return emitir(IRT(IR_HREF, IRT_P32), ix->tab, key);
}

/* Determine whether a key is NOT one of the fast metamethod names. */
static int nommstr(jit_State *J, TRef key)
{
  if (tref_isstr(key)) {
    if (tref_isk(key)) {
      GCstr *str = ir_kstr(IR(tref_ref(key)));
      uint32_t mm;
      for (mm = 0; mm <= MM_FAST; mm++)
	if (mmname_str(J2G(J), mm) == str)
	  return 0;  /* MUST be one the fast metamethod names. */
    } else {
      return 0;  /* Variable string key MAY be a metamethod name. */
    }
  }
  return 1;  /* CANNOT be a metamethod name. */
}

/* Record indexed load/store. */
TRef lj_record_idx(jit_State *J, RecordIndex *ix)
{
  TRef xref;
  IROp xrefop, loadop;
  cTValue *oldv;

  while (!tref_istab(ix->tab)) { /* Handle non-table lookup. */
    /* Never call raw lj_record_idx() on non-table. */
    lua_assert(ix->idxchain != 0);
    if (!lj_record_mm_lookup(J, ix, ix->val ? MM_newindex : MM_index))
      lj_trace_err(J, LJ_TRERR_NOMM);
  handlemm:
    if (tref_isfunc(ix->mobj)) {  /* Handle metamethod call. */
      BCReg func = rec_mm_prep(J, ix->val ? lj_cont_nop : lj_cont_ra);
      TRef *base = J->base + func;
      TValue *tv = J->L->base + func;
      base[0] = ix->mobj; base[1] = ix->tab; base[2] = ix->key;
      setfuncV(J->L, tv+0, funcV(&ix->mobjv));
      copyTV(J->L, tv+1, &ix->tabv);
      copyTV(J->L, tv+2, &ix->keyv);
      if (ix->val) {
	base[3] = ix->val;
	copyTV(J->L, tv+3, &ix->valv);
	lj_record_call(J, func, 3);  /* mobj(tab, key, val) */
	return 0;
      } else {
	lj_record_call(J, func, 2);  /* res = mobj(tab, key) */
	return 0;  /* No result yet. */
      }
    }
    /* Otherwise retry lookup with metaobject. */
    ix->tab = ix->mobj;
    copyTV(J->L, &ix->tabv, &ix->mobjv);
    if (--ix->idxchain == 0)
      lj_trace_err(J, LJ_TRERR_IDXLOOP);
  }

  /* First catch nil and NaN keys for tables. */
  if (tvisnil(&ix->keyv) || (tvisnum(&ix->keyv) && tvisnan(&ix->keyv))) {
    if (ix->val)  /* Better fail early. */
      lj_trace_err(J, LJ_TRERR_STORENN);
    if (tref_isk(ix->key)) {
      if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_index))
	goto handlemm;
      return TREF_NIL;
    }
  }

  /* Record the key lookup. */
  xref = rec_idx_key(J, ix);
  xrefop = IR(tref_ref(xref))->o;
  loadop = xrefop == IR_AREF ? IR_ALOAD : IR_HLOAD;
  /* The lj_meta_tset() inconsistency is gone, but better play safe. */
  oldv = xrefop == IR_KKPTR ? (cTValue *)ir_kptr(IR(tref_ref(xref))) : ix->oldv;

  if (ix->val == 0) {  /* Indexed load */
    IRType t = itype2irt(oldv);
    TRef res;
    if (oldv == niltvg(J2G(J))) {
      emitir(IRTG(IR_EQ, IRT_P32), xref, lj_ir_kkptr(J, niltvg(J2G(J))));
      res = TREF_NIL;
    } else {
      res = emitir(IRTG(loadop, t), xref, 0);
    }
    if (t == IRT_NIL && ix->idxchain && lj_record_mm_lookup(J, ix, MM_index))
      goto handlemm;
    if (irtype_ispri(t)) res = TREF_PRI(t);  /* Canonicalize primitives. */
    return res;
  } else {  /* Indexed store. */
    GCtab *mt = tabref(tabV(&ix->tabv)->metatable);
    int keybarrier = tref_isgcv(ix->key) && !tref_isnil(ix->val);
    if (tvisnil(oldv)) {  /* Previous value was nil? */
      /* Need to duplicate the hasmm check for the early guards. */
      int hasmm = 0;
      if (ix->idxchain && mt) {
	cTValue *mo = lj_tab_getstr(mt, mmname_str(J2G(J), MM_newindex));
	hasmm = mo && !tvisnil(mo);
      }
      if (hasmm)
	emitir(IRTG(loadop, IRT_NIL), xref, 0);  /* Guard for nil value. */
      else if (xrefop == IR_HREF)
	emitir(IRTG(oldv == niltvg(J2G(J)) ? IR_EQ : IR_NE, IRT_P32),
	       xref, lj_ir_kkptr(J, niltvg(J2G(J))));
      if (ix->idxchain && lj_record_mm_lookup(J, ix, MM_newindex)) {
	lua_assert(hasmm);
	goto handlemm;
      }
      lua_assert(!hasmm);
      if (oldv == niltvg(J2G(J))) {  /* Need to insert a new key. */
	TRef key = ix->key;
	if (tref_isinteger(key))  /* NEWREF needs a TValue as a key. */
	  key = emitir(IRTN(IR_CONV), key, IRCONV_NUM_INT);
	xref = emitir(IRT(IR_NEWREF, IRT_P32), ix->tab, key);
	keybarrier = 0;  /* NEWREF already takes care of the key barrier. */
      }
    } else if (!lj_opt_fwd_wasnonnil(J, loadop, tref_ref(xref))) {
      /* Cannot derive that the previous value was non-nil, must do checks. */
      if (xrefop == IR_HREF)  /* Guard against store to niltv. */
	emitir(IRTG(IR_NE, IRT_P32), xref, lj_ir_kkptr(J, niltvg(J2G(J))));
      if (ix->idxchain) {  /* Metamethod lookup required? */
	/* A check for NULL metatable is cheaper (hoistable) than a load. */
	if (!mt) {
	  TRef mtref = emitir(IRT(IR_FLOAD, IRT_TAB), ix->tab, IRFL_TAB_META);
	  emitir(IRTG(IR_EQ, IRT_TAB), mtref, lj_ir_knull(J, IRT_TAB));
	} else {
	  IRType t = itype2irt(oldv);
	  emitir(IRTG(loadop, t), xref, 0);  /* Guard for non-nil value. */
	}
      }
    } else {
      keybarrier = 0;  /* Previous non-nil value kept the key alive. */
    }
    /* Convert int to number before storing. */
    if (!LJ_DUALNUM && tref_isinteger(ix->val))
      ix->val = emitir(IRTN(IR_CONV), ix->val, IRCONV_NUM_INT);
    emitir(IRT(loadop+IRDELTA_L2S, tref_type(ix->val)), xref, ix->val);
    if (keybarrier || tref_isgcv(ix->val))
      emitir(IRT(IR_TBAR, IRT_NIL), ix->tab, 0);
    /* Invalidate neg. metamethod cache for stores with certain string keys. */
    if (!nommstr(J, ix->key)) {
      TRef fref = emitir(IRT(IR_FREF, IRT_P32), ix->tab, IRFL_TAB_NOMM);
      emitir(IRT(IR_FSTORE, IRT_U8), fref, lj_ir_kint(J, 0));
    }
    J->needsnap = 1;
    return 0;
  }
}

/* -- Upvalue access ------------------------------------------------------ */

/* Check whether upvalue is immutable and ok to constify. */
static int rec_upvalue_constify(jit_State *J, GCupval *uvp)
{
  if (uvp->immutable) {
    cTValue *o = uvval(uvp);
    /* Don't constify objects that may retain large amounts of memory. */
#if LJ_HASFFI
    if (tviscdata(o)) {
      GCcdata *cd = cdataV(o);
      if (!cdataisv(cd) && !(cd->marked & LJ_GC_CDATA_FIN)) {
	CType *ct = ctype_raw(ctype_ctsG(J2G(J)), cd->ctypeid);
	if (!ctype_hassize(ct->info) || ct->size <= 16)
	  return 1;
      }
      return 0;
    }
#else
    UNUSED(J);
#endif
    if (!(tvistab(o) || tvisudata(o) || tvisthread(o)))
      return 1;
  }
  return 0;
}

/* Record upvalue load/store. */
static TRef rec_upvalue(jit_State *J, uint32_t uv, TRef val)
{
  GCupval *uvp = &gcref(J->fn->l.uvptr[uv])->uv;
  TRef fn = getcurrf(J);
  IRRef uref;
  int needbarrier = 0;
  if (rec_upvalue_constify(J, uvp)) {  /* Try to constify immutable upvalue. */
    TRef tr, kfunc;
    lua_assert(val == 0);
    if (!tref_isk(fn)) {  /* Late specialization of current function. */
      if (J->pt->flags >= PROTO_CLC_POLY)
	goto noconstify;
      kfunc = lj_ir_kfunc(J, J->fn);
      emitir(IRTG(IR_EQ, IRT_FUNC), fn, kfunc);
      J->base[-1] = TREF_FRAME | kfunc;
      fn = kfunc;
    }
    tr = lj_record_constify(J, uvval(uvp));
    if (tr)
      return tr;
  }
noconstify:
  /* Note: this effectively limits LJ_MAX_UPVAL to 127. */
  uv = (uv << 8) | (hashrot(uvp->dhash, uvp->dhash + HASH_BIAS) & 0xff);
  if (!uvp->closed) {
    /* In current stack? */
    if (uvval(uvp) >= tvref(J->L->stack) &&
	uvval(uvp) < tvref(J->L->maxstack)) {
      int32_t slot = (int32_t)(uvval(uvp) - (J->L->base - J->baseslot));
      if (slot >= 0) {  /* Aliases an SSA slot? */
	slot -= (int32_t)J->baseslot;  /* Note: slot number may be negative! */
	/* NYI: add IR to guard that it's still aliasing the same slot. */
	if (val == 0) {
	  return getslot(J, slot);
	} else {
	  J->base[slot] = val;
	  if (slot >= (int32_t)J->maxslot) J->maxslot = (BCReg)(slot+1);
	  return 0;
	}
      }
    }
    uref = tref_ref(emitir(IRTG(IR_UREFO, IRT_P32), fn, uv));
  } else {
    needbarrier = 1;
    uref = tref_ref(emitir(IRTG(IR_UREFC, IRT_P32), fn, uv));
  }
  if (val == 0) {  /* Upvalue load */
    IRType t = itype2irt(uvval(uvp));
    TRef res = emitir(IRTG(IR_ULOAD, t), uref, 0);
    if (irtype_ispri(t)) res = TREF_PRI(t);  /* Canonicalize primitive refs. */
    return res;
  } else {  /* Upvalue store. */
    /* Convert int to number before storing. */
    if (!LJ_DUALNUM && tref_isinteger(val))
      val = emitir(IRTN(IR_CONV), val, IRCONV_NUM_INT);
    emitir(IRT(IR_USTORE, tref_type(val)), uref, val);
    if (needbarrier && tref_isgcv(val))
      emitir(IRT(IR_OBAR, IRT_NIL), uref, val);
    J->needsnap = 1;
    return 0;
  }
}

/* -- Record calls to Lua functions --------------------------------------- */

/* Check unroll limits for calls. */
static void check_call_unroll(jit_State *J, TraceNo lnk)
{
  cTValue *frame = J->L->base - 1;
  void *pc = mref(frame_func(frame)->l.pc, void);
  int32_t depth = J->framedepth;
  int32_t count = 0;
  if ((J->pt->flags & PROTO_VARARG)) depth--;  /* Vararg frame still missing. */
  for (; depth > 0; depth--) {  /* Count frames with same prototype. */
    frame = frame_prev(frame);
    if (mref(frame_func(frame)->l.pc, void) == pc)
      count++;
  }
  if (J->pc == J->startpc) {
    if (count + J->tailcalled > J->param[JIT_P_recunroll]) {
      J->pc++;
      if (J->framedepth + J->retdepth == 0)
	rec_stop(J, LJ_TRLINK_TAILREC, J->cur.traceno);  /* Tail-recursion. */
      else
	rec_stop(J, LJ_TRLINK_UPREC, J->cur.traceno);  /* Up-recursion. */
    }
  } else {
    if (count > J->param[JIT_P_callunroll]) {
      if (lnk) {  /* Possible tail- or up-recursion. */
	lj_trace_flush(J, lnk);  /* Flush trace that only returns. */
	/* Set a small, pseudo-random hotcount for a quick retry of JFUNC*. */
	hotcount_set(J2GG(J), J->pc+1, LJ_PRNG_BITS(J, 4));
      }
      lj_trace_err(J, LJ_TRERR_CUNROLL);
    }
  }
}

/* Record Lua function setup. */
static void rec_func_setup(jit_State *J)
{
  GCproto *pt = J->pt;
  BCReg s, numparams = pt->numparams;
  if ((pt->flags & PROTO_NOJIT))
    lj_trace_err(J, LJ_TRERR_CJITOFF);
  if (J->baseslot + pt->framesize >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
  /* Fill up missing parameters with nil. */
  for (s = J->maxslot; s < numparams; s++)
    J->base[s] = TREF_NIL;
  /* The remaining slots should never be read before they are written. */
  J->maxslot = numparams;
}

/* Record Lua vararg function setup. */
static void rec_func_vararg(jit_State *J)
{
  GCproto *pt = J->pt;
  BCReg s, fixargs, vframe = J->maxslot+1;
  lua_assert((pt->flags & PROTO_VARARG));
  if (J->baseslot + vframe + pt->framesize >= LJ_MAX_JSLOTS)
    lj_trace_err(J, LJ_TRERR_STACKOV);
  J->base[vframe-1] = J->base[-1];  /* Copy function up. */
  /* Copy fixarg slots up and set their original slots to nil. */
  fixargs = pt->numparams < J->maxslot ? pt->numparams : J->maxslot;
  for (s = 0; s < fixargs; s++) {
    J->base[vframe+s] = J->base[s];
    J->base[s] = TREF_NIL;
  }
  J->maxslot = fixargs;
  J->framedepth++;
  J->base += vframe;
  J->baseslot += vframe;
}

/* Record entry to a Lua function. */
static void rec_func_lua(jit_State *J)
{
  rec_func_setup(J);
  check_call_unroll(J, 0);
}

/* Record entry to an already compiled function. */
static void rec_func_jit(jit_State *J, TraceNo lnk)
{
  GCtrace *T;
  rec_func_setup(J);
  T = traceref(J, lnk);
  if (T->linktype == LJ_TRLINK_RETURN) {  /* Trace returns to interpreter? */
    check_call_unroll(J, lnk);
    /* Temporarily unpatch JFUNC* to continue recording across function. */
    J->patchins = *J->pc;
    J->patchpc = (BCIns *)J->pc;
    *J->patchpc = T->startins;
    return;
  }
  J->instunroll = 0;  /* Cannot continue across a compiled function. */
  if (J->pc == J->startpc && J->framedepth + J->retdepth == 0)
    rec_stop(J, LJ_TRLINK_TAILREC, J->cur.traceno);  /* Extra tail-recursion. */
  else
    rec_stop(J, LJ_TRLINK_ROOT, lnk);  /* Link to the function. */
}

/* -- Vararg handling ----------------------------------------------------- */

/* Detect y = select(x, ...) idiom. */
static int select_detect(jit_State *J)
{
  BCIns ins = J->pc[1];
  if (bc_op(ins) == BC_CALLM && bc_b(ins) == 2 && bc_c(ins) == 1) {
    cTValue *func = &J->L->base[bc_a(ins)];
    if (tvisfunc(func) && funcV(func)->c.ffid == FF_select)
      return 1;
  }
  return 0;
}

/* Record vararg instruction. */
static void rec_varg(jit_State *J, BCReg dst, ptrdiff_t nresults)
{
  int32_t numparams = J->pt->numparams;
  ptrdiff_t nvararg = frame_delta(J->L->base-1) - numparams - 1;
  lua_assert(frame_isvarg(J->L->base-1));
  if (J->framedepth > 0) {  /* Simple case: varargs defined on-trace. */
    ptrdiff_t i;
    if (nvararg < 0) nvararg = 0;
    if (nresults == -1) {
      nresults = nvararg;
      J->maxslot = dst + (BCReg)nvararg;
    } else if (dst + nresults > J->maxslot) {
      J->maxslot = dst + (BCReg)nresults;
    }
    for (i = 0; i < nresults; i++) {
      J->base[dst+i] = i < nvararg ? J->base[i - nvararg - 1] : TREF_NIL;
      lua_assert(J->base[dst+i] != 0);
    }
  } else {  /* Unknown number of varargs passed to trace. */
    TRef fr = emitir(IRTI(IR_SLOAD), 0, IRSLOAD_READONLY|IRSLOAD_FRAME);
    int32_t frofs = 8*(1+numparams)+FRAME_VARG;
    if (nresults >= 0) {  /* Known fixed number of results. */
      ptrdiff_t i;
      if (nvararg > 0) {
	ptrdiff_t nload = nvararg >= nresults ? nresults : nvararg;
	TRef vbase;
	if (nvararg >= nresults)
	  emitir(IRTGI(IR_GE), fr, lj_ir_kint(J, frofs+8*(int32_t)nresults));
	else
	  emitir(IRTGI(IR_EQ), fr, lj_ir_kint(J, frame_ftsz(J->L->base-1)));
	vbase = emitir(IRTI(IR_SUB), REF_BASE, fr);
	vbase = emitir(IRT(IR_ADD, IRT_P32), vbase, lj_ir_kint(J, frofs-8));
	for (i = 0; i < nload; i++) {
	  IRType t = itype2irt(&J->L->base[i-1-nvararg]);
	  TRef aref = emitir(IRT(IR_AREF, IRT_P32),
			     vbase, lj_ir_kint(J, (int32_t)i));
	  TRef tr = emitir(IRTG(IR_VLOAD, t), aref, 0);
	  if (irtype_ispri(t)) tr = TREF_PRI(t);  /* Canonicalize primitives. */
	  J->base[dst+i] = tr;
	}
      } else {
	emitir(IRTGI(IR_LE), fr, lj_ir_kint(J, frofs));
	nvararg = 0;
      }
      for (i = nvararg; i < nresults; i++)
	J->base[dst+i] = TREF_NIL;
      if (dst + (BCReg)nresults > J->maxslot)
	J->maxslot = dst + (BCReg)nresults;
    } else if (select_detect(J)) {  /* y = select(x, ...) */
      TRef tridx = J->base[dst-1];
      TRef tr = TREF_NIL;
      ptrdiff_t idx = lj_ffrecord_select_mode(J, tridx, &J->L->base[dst-1]);
      if (idx < 0) goto nyivarg;
      if (idx != 0 && !tref_isinteger(tridx))
	tridx = emitir(IRTGI(IR_CONV), tridx, IRCONV_INT_NUM|IRCONV_INDEX);
      if (idx != 0 && tref_isk(tridx)) {
	emitir(IRTGI(idx <= nvararg ? IR_GE : IR_LT),
	       fr, lj_ir_kint(J, frofs+8*(int32_t)idx));
	frofs -= 8;  /* Bias for 1-based index. */
      } else if (idx <= nvararg) {  /* Compute size. */
	TRef tmp = emitir(IRTI(IR_ADD), fr, lj_ir_kint(J, -frofs));
	if (numparams)
	  emitir(IRTGI(IR_GE), tmp, lj_ir_kint(J, 0));
	tr = emitir(IRTI(IR_BSHR), tmp, lj_ir_kint(J, 3));
	if (idx != 0) {
	  tridx = emitir(IRTI(IR_ADD), tridx, lj_ir_kint(J, -1));
	  rec_idx_abc(J, tr, tridx, (uint32_t)nvararg);
	}
      } else {
	TRef tmp = lj_ir_kint(J, frofs);
	if (idx != 0) {
	  TRef tmp2 = emitir(IRTI(IR_BSHL), tridx, lj_ir_kint(J, 3));
	  tmp = emitir(IRTI(IR_ADD), tmp2, tmp);
	} else {
	  tr = lj_ir_kint(J, 0);
	}
	emitir(IRTGI(IR_LT), fr, tmp);
      }
      if (idx != 0 && idx <= nvararg) {
	IRType t;
	TRef aref, vbase = emitir(IRTI(IR_SUB), REF_BASE, fr);
	vbase = emitir(IRT(IR_ADD, IRT_P32), vbase, lj_ir_kint(J, frofs-8));
	t = itype2irt(&J->L->base[idx-2-nvararg]);
	aref = emitir(IRT(IR_AREF, IRT_P32), vbase, tridx);
	tr = emitir(IRTG(IR_VLOAD, t), aref, 0);
	if (irtype_ispri(t)) tr = TREF_PRI(t);  /* Canonicalize primitives. */
      }
      J->base[dst-2] = tr;
      J->maxslot = dst-1;
      J->bcskip = 2;  /* Skip CALLM + select. */
    } else {
    nyivarg:
      setintV(&J->errinfo, BC_VARG);
      lj_trace_err_info(J, LJ_TRERR_NYIBC);
    }
  }
}

/* -- Record allocations -------------------------------------------------- */

static TRef rec_tnew(jit_State *J, uint32_t ah)
{
  uint32_t asize = ah & 0x7ff;
  uint32_t hbits = ah >> 11;
  if (asize == 0x7ff) asize = 0x801;
  return emitir(IRTG(IR_TNEW, IRT_TAB), asize, hbits);
}

/* -- Record bytecode ops ------------------------------------------------- */

/* Prepare for comparison. */
static void rec_comp_prep(jit_State *J)
{
  /* Prevent merging with snapshot #0 (GC exit) since we fixup the PC. */
  if (J->cur.nsnap == 1 && J->cur.snap[0].ref == J->cur.nins)
    emitir_raw(IRT(IR_NOP, IRT_NIL), 0, 0);
  lj_snap_add(J);
}

/* Fixup comparison. */
static void rec_comp_fixup(jit_State *J, const BCIns *pc, int cond)
{
  BCIns jmpins = pc[1];
  const BCIns *npc = pc + 2 + (cond ? bc_j(jmpins) : 0);
  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
  /* Set PC to opposite target to avoid re-recording the comp. in side trace. */
  J->cur.snapmap[snap->mapofs + snap->nent] = SNAP_MKPC(npc);
  J->needsnap = 1;
  if (bc_a(jmpins) < J->maxslot) J->maxslot = bc_a(jmpins);
  lj_snap_shrink(J);  /* Shrink last snapshot if possible. */
}

/* Record the next bytecode instruction (_before_ it's executed). */
void lj_record_ins(jit_State *J)
{
  cTValue *lbase;
  RecordIndex ix;
  const BCIns *pc;
  BCIns ins;
  BCOp op;
  TRef ra, rb, rc;

  /* Perform post-processing action before recording the next instruction. */
  if (LJ_UNLIKELY(J->postproc != LJ_POST_NONE)) {
    switch (J->postproc) {
    case LJ_POST_FIXCOMP:  /* Fixup comparison. */
      pc = frame_pc(&J2G(J)->tmptv);
      rec_comp_fixup(J, pc, (!tvistruecond(&J2G(J)->tmptv2) ^ (bc_op(*pc)&1)));
      /* fallthrough */
    case LJ_POST_FIXGUARD:  /* Fixup and emit pending guard. */
    case LJ_POST_FIXGUARDSNAP:  /* Fixup and emit pending guard and snapshot. */
      if (!tvistruecond(&J2G(J)->tmptv2)) {
	J->fold.ins.o ^= 1;  /* Flip guard to opposite. */
	if (J->postproc == LJ_POST_FIXGUARDSNAP) {
	  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
	  J->cur.snapmap[snap->mapofs+snap->nent-1]--;  /* False -> true. */
	}
      }
      lj_opt_fold(J);  /* Emit pending guard. */
      /* fallthrough */
    case LJ_POST_FIXBOOL:
      if (!tvistruecond(&J2G(J)->tmptv2)) {
	BCReg s;
	TValue *tv = J->L->base;
	for (s = 0; s < J->maxslot; s++)  /* Fixup stack slot (if any). */
	  if (J->base[s] == TREF_TRUE && tvisfalse(&tv[s])) {
	    J->base[s] = TREF_FALSE;
	    break;
	  }
      }
      break;
    case LJ_POST_FIXCONST:
      {
	BCReg s;
	TValue *tv = J->L->base;
	for (s = 0; s < J->maxslot; s++)  /* Constify stack slots (if any). */
	  if (J->base[s] == TREF_NIL && !tvisnil(&tv[s]))
	    J->base[s] = lj_record_constify(J, &tv[s]);
      }
      break;
    case LJ_POST_FFRETRY:  /* Suppress recording of retried fast function. */
      if (bc_op(*J->pc) >= BC__MAX)
	return;
      break;
    default: lua_assert(0); break;
    }
    J->postproc = LJ_POST_NONE;
  }

  /* Need snapshot before recording next bytecode (e.g. after a store). */
  if (J->needsnap) {
    J->needsnap = 0;
    lj_snap_purge(J);
    lj_snap_add(J);
    J->mergesnap = 1;
  }

  /* Skip some bytecodes. */
  if (LJ_UNLIKELY(J->bcskip > 0)) {
    J->bcskip--;
    return;
  }

  /* Record only closed loops for root traces. */
  pc = J->pc;
  if (J->framedepth == 0 &&
     (MSize)((char *)pc - (char *)J->bc_min) >= J->bc_extent)
    lj_trace_err(J, LJ_TRERR_LLEAVE);

#ifdef LUA_USE_ASSERT
  rec_check_slots(J);
  rec_check_ir(J);
#endif

  /* Keep a copy of the runtime values of var/num/str operands. */
#define rav	(&ix.valv)
#define rbv	(&ix.tabv)
#define rcv	(&ix.keyv)

  lbase = J->L->base;
  ins = *pc;
  op = bc_op(ins);
  ra = bc_a(ins);
  ix.val = 0;
  switch (bcmode_a(op)) {
  case BCMvar:
    copyTV(J->L, rav, &lbase[ra]); ix.val = ra = getslot(J, ra); break;
  default: break;  /* Handled later. */
  }
  rb = bc_b(ins);
  rc = bc_c(ins);
  switch (bcmode_b(op)) {
  case BCMnone: rb = 0; rc = bc_d(ins); break;  /* Upgrade rc to 'rd'. */
  case BCMvar:
    copyTV(J->L, rbv, &lbase[rb]); ix.tab = rb = getslot(J, rb); break;
  default: break;  /* Handled later. */
  }
  switch (bcmode_c(op)) {
  case BCMvar:
    copyTV(J->L, rcv, &lbase[rc]); ix.key = rc = getslot(J, rc); break;
  case BCMpri: setitype(rcv, ~rc); ix.key = rc = TREF_PRI(IRT_NIL+rc); break;
  case BCMnum: { cTValue *tv = proto_knumtv(J->pt, rc);
    copyTV(J->L, rcv, tv); ix.key = rc = tvisint(tv) ? lj_ir_kint(J, intV(tv)) :
    lj_ir_knumint(J, numV(tv)); } break;
  case BCMstr: { GCstr *s = gco2str(proto_kgc(J->pt, ~(ptrdiff_t)rc));
    setstrV(J->L, rcv, s); ix.key = rc = lj_ir_kstr(J, s); } break;
  default: break;  /* Handled later. */
  }

  switch (op) {

  /* -- Comparison ops ---------------------------------------------------- */

  case BC_ISLT: case BC_ISGE: case BC_ISLE: case BC_ISGT:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, ((int)op & 2) ? MM_le : MM_lt);
      break;
    }
#endif
    /* Emit nothing for two numeric or string consts. */
    if (!(tref_isk2(ra,rc) && tref_isnumber_str(ra) && tref_isnumber_str(rc))) {
      IRType ta = tref_isinteger(ra) ? IRT_INT : tref_type(ra);
      IRType tc = tref_isinteger(rc) ? IRT_INT : tref_type(rc);
      int irop;
      if (ta != tc) {
	/* Widen mixed number/int comparisons to number/number comparison. */
	if (ta == IRT_INT && tc == IRT_NUM) {
	  ra = emitir(IRTN(IR_CONV), ra, IRCONV_NUM_INT);
	  ta = IRT_NUM;
	} else if (ta == IRT_NUM && tc == IRT_INT) {
	  rc = emitir(IRTN(IR_CONV), rc, IRCONV_NUM_INT);
	} else if (LJ_52) {
	  ta = IRT_NIL;  /* Force metamethod for different types. */
	} else if (!((ta == IRT_FALSE || ta == IRT_TRUE) &&
		     (tc == IRT_FALSE || tc == IRT_TRUE))) {
	  break;  /* Interpreter will throw for two different types. */
	}
      }
      rec_comp_prep(J);
      irop = (int)op - (int)BC_ISLT + (int)IR_LT;
      if (ta == IRT_NUM) {
	if ((irop & 1)) irop ^= 4;  /* ISGE/ISGT are unordered. */
	if (!lj_ir_numcmp(numberVnum(rav), numberVnum(rcv), (IROp)irop))
	  irop ^= 5;
      } else if (ta == IRT_INT) {
	if (!lj_ir_numcmp(numberVnum(rav), numberVnum(rcv), (IROp)irop))
	  irop ^= 1;
      } else if (ta == IRT_STR) {
	if (!lj_ir_strcmp(strV(rav), strV(rcv), (IROp)irop)) irop ^= 1;
	ra = lj_ir_call(J, IRCALL_lj_str_cmp, ra, rc);
	rc = lj_ir_kint(J, 0);
	ta = IRT_INT;
      } else {
	rec_mm_comp(J, &ix, (int)op);
	break;
      }
      emitir(IRTG(irop, ta), ra, rc);
      rec_comp_fixup(J, J->pc, ((int)op ^ irop) & 1);
    }
    break;

  case BC_ISEQV: case BC_ISNEV:
  case BC_ISEQS: case BC_ISNES:
  case BC_ISEQN: case BC_ISNEN:
  case BC_ISEQP: case BC_ISNEP:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, MM_eq);
      break;
    }
#endif
    /* Emit nothing for two non-table, non-udata consts. */
    if (!(tref_isk2(ra, rc) && !(tref_istab(ra) || tref_isudata(ra)))) {
      int diff;
      rec_comp_prep(J);
      diff = lj_record_objcmp(J, ra, rc, rav, rcv);
      if (diff == 2 || !(tref_istab(ra) || tref_isudata(ra)))
	rec_comp_fixup(J, J->pc, ((int)op & 1) == !diff);
      else if (diff == 1)  /* Only check __eq if different, but same type. */
	rec_mm_equal(J, &ix, (int)op);
    }
    break;

  /* -- Unary test and copy ops ------------------------------------------- */

  case BC_ISTC: case BC_ISFC:
    if ((op & 1) == tref_istruecond(rc))
      rc = 0;  /* Don't store if condition is not true. */
    /* fallthrough */
  case BC_IST: case BC_ISF:  /* Type specialization suffices. */
    if (bc_a(pc[1]) < J->maxslot)
      J->maxslot = bc_a(pc[1]);  /* Shrink used slots. */
    break;

  /* -- Unary ops --------------------------------------------------------- */

  case BC_NOT:
    /* Type specialization already forces const result. */
    rc = tref_istruecond(rc) ? TREF_FALSE : TREF_TRUE;
    break;

  case BC_LEN:
    if (tref_isstr(rc))
      rc = emitir(IRTI(IR_FLOAD), rc, IRFL_STR_LEN);
    else if (!LJ_52 && tref_istab(rc))
      rc = lj_ir_call(J, IRCALL_lj_tab_len, rc);
    else
      rc = rec_mm_len(J, rc, rcv);
    break;

  /* -- Arithmetic ops ---------------------------------------------------- */

  case BC_UNM:
    if (tref_isnumber_str(rc)) {
      rc = lj_opt_narrow_unm(J, rc, rcv);
    } else {
      ix.tab = rc;
      copyTV(J->L, &ix.tabv, rcv);
      rc = rec_mm_arith(J, &ix, MM_unm);
    }
    break;

  case BC_ADDNV: case BC_SUBNV: case BC_MULNV: case BC_DIVNV: case BC_MODNV:
    /* Swap rb/rc and rbv/rcv. rav is temp. */
    ix.tab = rc; ix.key = rc = rb; rb = ix.tab;
    copyTV(J->L, rav, rbv);
    copyTV(J->L, rbv, rcv);
    copyTV(J->L, rcv, rav);
    if (op == BC_MODNV)
      goto recmod;
    /* fallthrough */
  case BC_ADDVN: case BC_SUBVN: case BC_MULVN: case BC_DIVVN:
  case BC_ADDVV: case BC_SUBVV: case BC_MULVV: case BC_DIVVV: {
    MMS mm = bcmode_mm(op);
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_arith(J, rb, rc, rbv, rcv,
			       (int)mm - (int)MM_add + (int)IR_ADD);
    else
      rc = rec_mm_arith(J, &ix, mm);
    break;
    }

  case BC_MODVN: case BC_MODVV:
  recmod:
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_mod(J, rb, rc, rcv);
    else
      rc = rec_mm_arith(J, &ix, MM_mod);
    break;

  case BC_POW:
    if (tref_isnumber_str(rb) && tref_isnumber_str(rc))
      rc = lj_opt_narrow_pow(J, lj_ir_tonum(J, rb), rc, rcv);
    else
      rc = rec_mm_arith(J, &ix, MM_pow);
    break;

  /* -- Constant and move ops --------------------------------------------- */

  case BC_MOV:
    /* Clear gap of method call to avoid resurrecting previous refs. */
    if (ra > J->maxslot) J->base[ra-1] = 0;
    break;
  case BC_KSTR: case BC_KNUM: case BC_KPRI:
    break;
  case BC_KSHORT:
    rc = lj_ir_kint(J, (int32_t)(int16_t)rc);
    break;
  case BC_KNIL:
    while (ra <= rc)
      J->base[ra++] = TREF_NIL;
    if (rc >= J->maxslot) J->maxslot = rc+1;
    break;
#if LJ_HASFFI
  case BC_KCDATA:
    rc = lj_ir_kgc(J, proto_kgc(J->pt, ~(ptrdiff_t)rc), IRT_CDATA);
    break;
#endif

  /* -- Upvalue and function ops ------------------------------------------ */

  case BC_UGET:
    rc = rec_upvalue(J, rc, 0);
    break;
  case BC_USETV: case BC_USETS: case BC_USETN: case BC_USETP:
    rec_upvalue(J, ra, rc);
    break;

  /* -- Table ops --------------------------------------------------------- */

  case BC_GGET: case BC_GSET:
    settabV(J->L, &ix.tabv, tabref(J->fn->l.env));
    ix.tab = emitir(IRT(IR_FLOAD, IRT_TAB), getcurrf(J), IRFL_FUNC_ENV);
    ix.idxchain = LJ_MAX_IDXCHAIN;
    rc = lj_record_idx(J, &ix);
    break;

  case BC_TGETB: case BC_TSETB:
    setintV(&ix.keyv, (int32_t)rc);
    ix.key = lj_ir_kint(J, (int32_t)rc);
    /* fallthrough */
  case BC_TGETV: case BC_TGETS: case BC_TSETV: case BC_TSETS:
    ix.idxchain = LJ_MAX_IDXCHAIN;
    rc = lj_record_idx(J, &ix);
    break;

  case BC_TNEW:
    rc = rec_tnew(J, rc);
    break;
  case BC_TDUP:
    rc = emitir(IRTG(IR_TDUP, IRT_TAB),
		lj_ir_ktab(J, gco2tab(proto_kgc(J->pt, ~(ptrdiff_t)rc))), 0);
    break;

  /* -- Calls and vararg handling ----------------------------------------- */

  case BC_ITERC:
    J->base[ra] = getslot(J, ra-3);
    J->base[ra+1] = getslot(J, ra-2);
    J->base[ra+2] = getslot(J, ra-1);
    { /* Do the actual copy now because lj_record_call needs the values. */
      TValue *b = &J->L->base[ra];
      copyTV(J->L, b, b-3);
      copyTV(J->L, b+1, b-2);
      copyTV(J->L, b+2, b-1);
    }
    lj_record_call(J, ra, (ptrdiff_t)rc-1);
    break;

  /* L->top is set to L->base+ra+rc+NARGS-1+1. See lj_dispatch_ins(). */
  case BC_CALLM:
    rc = (BCReg)(J->L->top - J->L->base) - ra;
    /* fallthrough */
  case BC_CALL:
    lj_record_call(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_CALLMT:
    rc = (BCReg)(J->L->top - J->L->base) - ra;
    /* fallthrough */
  case BC_CALLT:
    lj_record_tailcall(J, ra, (ptrdiff_t)rc-1);
    break;

  case BC_VARG:
    rec_varg(J, ra, (ptrdiff_t)rb-1);
    break;

  /* -- Returns ----------------------------------------------------------- */

  case BC_RETM:
    /* L->top is set to L->base+ra+rc+NRESULTS-1, see lj_dispatch_ins(). */
    rc = (BCReg)(J->L->top - J->L->base) - ra + 1;
    /* fallthrough */
  case BC_RET: case BC_RET0: case BC_RET1:
    lj_record_ret(J, ra, (ptrdiff_t)rc-1);
    break;

  /* -- Loops and branches ------------------------------------------------ */

  case BC_FORI:
    if (rec_for(J, pc, 0) != LOOPEV_LEAVE)
      J->loopref = J->cur.nins;
    break;
  case BC_JFORI:
    lua_assert(bc_op(pc[(ptrdiff_t)rc-BCBIAS_J]) == BC_JFORL);
    if (rec_for(J, pc, 0) != LOOPEV_LEAVE)  /* Link to existing loop. */
      rec_stop(J, LJ_TRLINK_ROOT, bc_d(pc[(ptrdiff_t)rc-BCBIAS_J]));
    /* Continue tracing if the loop is not entered. */
    break;

  case BC_FORL:
    rec_loop_interp(J, pc, rec_for(J, pc+((ptrdiff_t)rc-BCBIAS_J), 1));
    break;
  case BC_ITERL:
    rec_loop_interp(J, pc, rec_iterl(J, *pc));
    break;
  case BC_LOOP:
    rec_loop_interp(J, pc, rec_loop(J, ra));
    break;

  case BC_JFORL:
    rec_loop_jit(J, rc, rec_for(J, pc+bc_j(traceref(J, rc)->startins), 1));
    break;
  case BC_JITERL:
    rec_loop_jit(J, rc, rec_iterl(J, traceref(J, rc)->startins));
    break;
  case BC_JLOOP:
    rec_loop_jit(J, rc, rec_loop(J, ra));
    break;

  case BC_IFORL:
  case BC_IITERL:
  case BC_ILOOP:
  case BC_IFUNCF:
  case BC_IFUNCV:
    lj_trace_err(J, LJ_TRERR_BLACKL);
    break;

  case BC_JMP:
    if (ra < J->maxslot)
      J->maxslot = ra;  /* Shrink used slots. */
    break;

  /* -- Function headers -------------------------------------------------- */

  case BC_FUNCF:
    rec_func_lua(J);
    break;
  case BC_JFUNCF:
    rec_func_jit(J, rc);
    break;

  case BC_FUNCV:
    rec_func_vararg(J);
    rec_func_lua(J);
    break;
  case BC_JFUNCV:
    lua_assert(0);  /* Cannot happen. No hotcall counting for varag funcs. */
    break;

  case BC_FUNCC:
  case BC_FUNCCW:
    lj_ffrecord_func(J);
    break;

  default:
    if (op >= BC__MAX) {
      lj_ffrecord_func(J);
      break;
    }
    /* fallthrough */
  case BC_ITERN:
  case BC_ISNEXT:
  case BC_CAT:
  case BC_UCLO:
  case BC_FNEW:
  case BC_TSETM:
    setintV(&J->errinfo, (int32_t)op);
    lj_trace_err_info(J, LJ_TRERR_NYIBC);
    break;
  }

  /* rc == 0 if we have no result yet, e.g. pending __index metamethod call. */
  if (bcmode_a(op) == BCMdst && rc) {
    J->base[ra] = rc;
    if (ra >= J->maxslot) J->maxslot = ra+1;
  }

#undef rav
#undef rbv
#undef rcv

  /* Limit the number of recorded IR instructions. */
  if (J->cur.nins > REF_FIRST+(IRRef)J->param[JIT_P_maxrecord])
    lj_trace_err(J, LJ_TRERR_TRACEOV);
}

/* -- Recording setup ----------------------------------------------------- */

/* Setup recording for a root trace started by a hot loop. */
static const BCIns *rec_setup_root(jit_State *J)
{
  /* Determine the next PC and the bytecode range for the loop. */
  const BCIns *pcj, *pc = J->pc;
  BCIns ins = *pc;
  BCReg ra = bc_a(ins);
  switch (bc_op(ins)) {
  case BC_FORL:
    J->bc_extent = (MSize)(-bc_j(ins))*sizeof(BCIns);
    pc += 1+bc_j(ins);
    J->bc_min = pc;
    break;
  case BC_ITERL:
    lua_assert(bc_op(pc[-1]) == BC_ITERC);
    J->maxslot = ra + bc_b(pc[-1]) - 1;
    J->bc_extent = (MSize)(-bc_j(ins))*sizeof(BCIns);
    pc += 1+bc_j(ins);
    lua_assert(bc_op(pc[-1]) == BC_JMP);
    J->bc_min = pc;
    break;
  case BC_LOOP:
    /* Only check BC range for real loops, but not for "repeat until true". */
    pcj = pc + bc_j(ins);
    ins = *pcj;
    if (bc_op(ins) == BC_JMP && bc_j(ins) < 0) {
      J->bc_min = pcj+1 + bc_j(ins);
      J->bc_extent = (MSize)(-bc_j(ins))*sizeof(BCIns);
    }
    J->maxslot = ra;
    pc++;
    break;
  case BC_RET:
  case BC_RET0:
  case BC_RET1:
    /* No bytecode range check for down-recursive root traces. */
    J->maxslot = ra + bc_d(ins);
    break;
  case BC_FUNCF:
    /* No bytecode range check for root traces started by a hot call. */
    J->maxslot = J->pt->numparams;
    pc++;
    break;
  default:
    lua_assert(0);
    break;
  }
  return pc;
}

/* Setup for recording a new trace. */
void lj_record_setup(jit_State *J)
{
  uint32_t i;

  /* Initialize state related to current trace. */
  memset(J->slot, 0, sizeof(J->slot));
  memset(J->chain, 0, sizeof(J->chain));
  memset(J->bpropcache, 0, sizeof(J->bpropcache));
  J->scev.idx = REF_NIL;

  J->baseslot = 1;  /* Invoking function is at base[-1]. */
  J->base = J->slot + J->baseslot;
  J->maxslot = 0;
  J->framedepth = 0;
  J->retdepth = 0;

  J->instunroll = J->param[JIT_P_instunroll];
  J->loopunroll = J->param[JIT_P_loopunroll];
  J->tailcalled = 0;
  J->loopref = 0;

  J->bc_min = NULL;  /* Means no limit. */
  J->bc_extent = ~(MSize)0;

  /* Emit instructions for fixed references. Also triggers initial IR alloc. */
  emitir_raw(IRT(IR_BASE, IRT_P32), J->parent, J->exitno);
  for (i = 0; i <= 2; i++) {
    IRIns *ir = IR(REF_NIL-i);
    ir->i = 0;
    ir->t.irt = (uint8_t)(IRT_NIL+i);
    ir->o = IR_KPRI;
    ir->prev = 0;
  }
  J->cur.nk = REF_TRUE;

  J->startpc = J->pc;
  setmref(J->cur.startpc, J->pc);
  if (J->parent) {  /* Side trace. */
    GCtrace *T = traceref(J, J->parent);
    TraceNo root = T->root ? T->root : J->parent;
    J->cur.root = (uint16_t)root;
    J->cur.startins = BCINS_AD(BC_JMP, 0, 0);
    /* Check whether we could at least potentially form an extra loop. */
    if (J->exitno == 0 && T->snap[0].nent == 0) {
      /* We can narrow a FORL for some side traces, too. */
      if (J->pc > proto_bc(J->pt) && bc_op(J->pc[-1]) == BC_JFORI &&
	  bc_d(J->pc[bc_j(J->pc[-1])-1]) == root) {
	lj_snap_add(J);
	rec_for_loop(J, J->pc-1, &J->scev, 1);
	goto sidecheck;
      }
    } else {
      J->startpc = NULL;  /* Prevent forming an extra loop. */
    }
    lj_snap_replay(J, T);
  sidecheck:
    if (traceref(J, J->cur.root)->nchild >= J->param[JIT_P_maxside] ||
	T->snap[J->exitno].count >= J->param[JIT_P_hotexit] +
				    J->param[JIT_P_tryside]) {
      rec_stop(J, LJ_TRLINK_INTERP, 0);
    }
  } else {  /* Root trace. */
    J->cur.root = 0;
    J->cur.startins = *J->pc;
    J->pc = rec_setup_root(J);
    /* Note: the loop instruction itself is recorded at the end and not
    ** at the start! So snapshot #0 needs to point to the *next* instruction.
    */
    lj_snap_add(J);
    if (bc_op(J->cur.startins) == BC_FORL)
      rec_for_loop(J, J->pc-1, &J->scev, 1);
    if (1 + J->pt->framesize >= LJ_MAX_JSLOTS)
      lj_trace_err(J, LJ_TRERR_STACKOV);
  }
#ifdef LUAJIT_ENABLE_CHECKHOOK
  /* Regularly check for instruction/line hooks from compiled code and
  ** exit to the interpreter if the hooks are set.
  **
  ** This is a compile-time option and disabled by default, since the
  ** hook checks may be quite expensive in tight loops.
  **
  ** Note this is only useful if hooks are *not* set most of the time.
  ** Use this only if you want to *asynchronously* interrupt the execution.
  **
  ** You can set the instruction hook via lua_sethook() with a count of 1
  ** from a signal handler or another native thread. Please have a look
  ** at the first few functions in luajit.c for an example (Ctrl-C handler).
  */
  {
    TRef tr = emitir(IRT(IR_XLOAD, IRT_U8),
		     lj_ir_kptr(J, &J2G(J)->hookmask), IRXLOAD_VOLATILE);
    tr = emitir(IRTI(IR_BAND), tr, lj_ir_kint(J, (LUA_MASKLINE|LUA_MASKCOUNT)));
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, 0));
  }
#endif
}

#undef IR
#undef emitir_raw
#undef emitir

#endif
