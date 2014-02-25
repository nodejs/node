/*
** SINK: Allocation Sinking and Store Sinking.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_opt_sink_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_iropt.h"
#include "lj_target.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)		(&J->cur.ir[(ref)])

/* Check whether the store ref points to an eligible allocation. */
static IRIns *sink_checkalloc(jit_State *J, IRIns *irs)
{
  IRIns *ir = IR(irs->op1);
  if (!irref_isk(ir->op2))
    return NULL;  /* Non-constant key. */
  if (ir->o == IR_HREFK || ir->o == IR_AREF)
    ir = IR(ir->op1);
  else if (!(ir->o == IR_HREF || ir->o == IR_NEWREF ||
	     ir->o == IR_FREF || ir->o == IR_ADD))
    return NULL;  /* Unhandled reference type (for XSTORE). */
  ir = IR(ir->op1);
  if (!(ir->o == IR_TNEW || ir->o == IR_TDUP || ir->o == IR_CNEW))
    return NULL;  /* Not an allocation. */
  return ir;  /* Return allocation. */
}

/* Recursively check whether a value depends on a PHI. */
static int sink_phidep(jit_State *J, IRRef ref)
{
  IRIns *ir = IR(ref);
  if (irt_isphi(ir->t)) return 1;
  if (ir->op1 >= REF_FIRST && sink_phidep(J, ir->op1)) return 1;
  if (ir->op2 >= REF_FIRST && sink_phidep(J, ir->op2)) return 1;
  return 0;
}

/* Check whether a value is a sinkable PHI or loop-invariant. */
static int sink_checkphi(jit_State *J, IRIns *ira, IRRef ref)
{
  if (ref >= REF_FIRST) {
    IRIns *ir = IR(ref);
    if (irt_isphi(ir->t) || (ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT &&
			     irt_isphi(IR(ir->op1)->t))) {
      ira->prev++;
      return 1;  /* Sinkable PHI. */
    }
    /* Otherwise the value must be loop-invariant. */
    return ref < J->loopref && !sink_phidep(J, ref);
  }
  return 1;  /* Constant (non-PHI). */
}

/* Mark non-sinkable allocations using single-pass backward propagation.
**
** Roots for the marking process are:
** - Some PHIs or snapshots (see below).
** - Non-PHI, non-constant values stored to PHI allocations.
** - All guards.
** - Any remaining loads not eliminated by store-to-load forwarding.
** - Stores with non-constant keys.
** - All stored values.
*/
static void sink_mark_ins(jit_State *J)
{
  IRIns *ir, *irlast = IR(J->cur.nins-1);
  for (ir = irlast ; ; ir--) {
    switch (ir->o) {
    case IR_BASE:
      return;  /* Finished. */
    case IR_CALLL:  /* IRCALL_lj_tab_len */
    case IR_ALOAD: case IR_HLOAD: case IR_XLOAD: case IR_TBAR:
      irt_setmark(IR(ir->op1)->t);  /* Mark ref for remaining loads. */
      break;
    case IR_FLOAD:
      if (irt_ismarked(ir->t) || ir->op2 == IRFL_TAB_META)
	irt_setmark(IR(ir->op1)->t);  /* Mark table for remaining loads. */
      break;
    case IR_ASTORE: case IR_HSTORE: case IR_FSTORE: case IR_XSTORE: {
      IRIns *ira = sink_checkalloc(J, ir);
      if (!ira || (irt_isphi(ira->t) && !sink_checkphi(J, ira, ir->op2)))
	irt_setmark(IR(ir->op1)->t);  /* Mark ineligible ref. */
      irt_setmark(IR(ir->op2)->t);  /* Mark stored value. */
      break;
      }
#if LJ_HASFFI
    case IR_CNEWI:
      if (irt_isphi(ir->t) &&
	  (!sink_checkphi(J, ir, ir->op2) ||
	   (LJ_32 && ir+1 < irlast && (ir+1)->o == IR_HIOP &&
	    !sink_checkphi(J, ir, (ir+1)->op2))))
	irt_setmark(ir->t);  /* Mark ineligible allocation. */
      /* fallthrough */
#endif
    case IR_USTORE:
      irt_setmark(IR(ir->op2)->t);  /* Mark stored value. */
      break;
#if LJ_HASFFI
    case IR_CALLXS:
#endif
    case IR_CALLS:
      irt_setmark(IR(ir->op1)->t);  /* Mark (potentially) stored values. */
      break;
    case IR_PHI: {
      IRIns *irl = IR(ir->op1), *irr = IR(ir->op2);
      irl->prev = irr->prev = 0;  /* Clear PHI value counts. */
      if (irl->o == irr->o &&
	  (irl->o == IR_TNEW || irl->o == IR_TDUP ||
	   (LJ_HASFFI && (irl->o == IR_CNEW || irl->o == IR_CNEWI))))
	break;
      irt_setmark(irl->t);
      irt_setmark(irr->t);
      break;
      }
    default:
      if (irt_ismarked(ir->t) || irt_isguard(ir->t)) {  /* Propagate mark. */
	if (ir->op1 >= REF_FIRST) irt_setmark(IR(ir->op1)->t);
	if (ir->op2 >= REF_FIRST) irt_setmark(IR(ir->op2)->t);
      }
      break;
    }
  }
}

/* Mark all instructions referenced by a snapshot. */
static void sink_mark_snap(jit_State *J, SnapShot *snap)
{
  SnapEntry *map = &J->cur.snapmap[snap->mapofs];
  MSize n, nent = snap->nent;
  for (n = 0; n < nent; n++) {
    IRRef ref = snap_ref(map[n]);
    if (!irref_isk(ref))
      irt_setmark(IR(ref)->t);
  }
}

/* Iteratively remark PHI refs with differing marks or PHI value counts. */
static void sink_remark_phi(jit_State *J)
{
  IRIns *ir;
  int remark;
  do {
    remark = 0;
    for (ir = IR(J->cur.nins-1); ir->o == IR_PHI; ir--) {
      IRIns *irl = IR(ir->op1), *irr = IR(ir->op2);
      if (((irl->t.irt ^ irr->t.irt) & IRT_MARK))
	remark = 1;
      else if (irl->prev == irr->prev)
	continue;
      irt_setmark(IR(ir->op1)->t);
      irt_setmark(IR(ir->op2)->t);
    }
  } while (remark);
}

/* Sweep instructions and tag sunken allocations and stores. */
static void sink_sweep_ins(jit_State *J)
{
  IRIns *ir, *irfirst = IR(J->cur.nk);
  for (ir = IR(J->cur.nins-1) ; ir >= irfirst; ir--) {
    switch (ir->o) {
    case IR_ASTORE: case IR_HSTORE: case IR_FSTORE: case IR_XSTORE: {
      IRIns *ira = sink_checkalloc(J, ir);
      if (ira && !irt_ismarked(ira->t)) {
	int delta = (int)(ir - ira);
	ir->prev = REGSP(RID_SINK, delta > 255 ? 255 : delta);
      } else {
	ir->prev = REGSP_INIT;
      }
      break;
      }
    case IR_NEWREF:
      if (!irt_ismarked(IR(ir->op1)->t)) {
	ir->prev = REGSP(RID_SINK, 0);
      } else {
	irt_clearmark(ir->t);
	ir->prev = REGSP_INIT;
      }
      break;
#if LJ_HASFFI
    case IR_CNEW: case IR_CNEWI:
#endif
    case IR_TNEW: case IR_TDUP:
      if (!irt_ismarked(ir->t)) {
	ir->t.irt &= ~IRT_GUARD;
	ir->prev = REGSP(RID_SINK, 0);
	J->cur.sinktags = 1;  /* Signal present SINK tags to assembler. */
      } else {
	irt_clearmark(ir->t);
	ir->prev = REGSP_INIT;
      }
      break;
    case IR_PHI: {
      IRIns *ira = IR(ir->op2);
      if (!irt_ismarked(ira->t) &&
	  (ira->o == IR_TNEW || ira->o == IR_TDUP ||
	   (LJ_HASFFI && (ira->o == IR_CNEW || ira->o == IR_CNEWI)))) {
	ir->prev = REGSP(RID_SINK, 0);
      } else {
	ir->prev = REGSP_INIT;
      }
      break;
      }
    default:
      irt_clearmark(ir->t);
      ir->prev = REGSP_INIT;
      break;
    }
  }
}

/* Allocation sinking and store sinking.
**
** 1. Mark all non-sinkable allocations.
** 2. Then sink all remaining allocations and the related stores.
*/
void lj_opt_sink(jit_State *J)
{
  const uint32_t need = (JIT_F_OPT_SINK|JIT_F_OPT_FWD|
			 JIT_F_OPT_DCE|JIT_F_OPT_CSE|JIT_F_OPT_FOLD);
  if ((J->flags & need) == need &&
      (J->chain[IR_TNEW] || J->chain[IR_TDUP] ||
       (LJ_HASFFI && (J->chain[IR_CNEW] || J->chain[IR_CNEWI])))) {
    if (!J->loopref)
      sink_mark_snap(J, &J->cur.snap[J->cur.nsnap-1]);
    sink_mark_ins(J);
    if (J->loopref)
      sink_remark_phi(J);
    sink_sweep_ins(J);
  }
}

#undef IR

#endif
