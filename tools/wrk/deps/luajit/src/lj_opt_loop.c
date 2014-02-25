/*
** LOOP: Loop Optimizations.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_opt_loop_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_err.h"
#include "lj_str.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_snap.h"
#include "lj_vm.h"

/* Loop optimization:
**
** Traditional Loop-Invariant Code Motion (LICM) splits the instructions
** of a loop into invariant and variant instructions. The invariant
** instructions are hoisted out of the loop and only the variant
** instructions remain inside the loop body.
**
** Unfortunately LICM is mostly useless for compiling dynamic languages.
** The IR has many guards and most of the subsequent instructions are
** control-dependent on them. The first non-hoistable guard would
** effectively prevent hoisting of all subsequent instructions.
**
** That's why we use a special form of unrolling using copy-substitution,
** combined with redundancy elimination:
**
** The recorded instruction stream is re-emitted to the compiler pipeline
** with substituted operands. The substitution table is filled with the
** refs returned by re-emitting each instruction. This can be done
** on-the-fly, because the IR is in strict SSA form, where every ref is
** defined before its use.
**
** This aproach generates two code sections, separated by the LOOP
** instruction:
**
** 1. The recorded instructions form a kind of pre-roll for the loop. It
** contains a mix of invariant and variant instructions and performs
** exactly one loop iteration (but not necessarily the 1st iteration).
**
** 2. The loop body contains only the variant instructions and performs
** all remaining loop iterations.
**
** On first sight that looks like a waste of space, because the variant
** instructions are present twice. But the key insight is that the
** pre-roll honors the control-dependencies for *both* the pre-roll itself
** *and* the loop body!
**
** It also means one doesn't have to explicitly model control-dependencies
** (which, BTW, wouldn't help LICM much). And it's much easier to
** integrate sparse snapshotting with this approach.
**
** One of the nicest aspects of this approach is that all of the
** optimizations of the compiler pipeline (FOLD, CSE, FWD, etc.) can be
** reused with only minor restrictions (e.g. one should not fold
** instructions across loop-carried dependencies).
**
** But in general all optimizations can be applied which only need to look
** backwards into the generated instruction stream. At any point in time
** during the copy-substitution process this contains both a static loop
** iteration (the pre-roll) and a dynamic one (from the to-be-copied
** instruction up to the end of the partial loop body).
**
** Since control-dependencies are implicitly kept, CSE also applies to all
** kinds of guards. The major advantage is that all invariant guards can
** be hoisted, too.
**
** Load/store forwarding works across loop iterations, too. This is
** important if loop-carried dependencies are kept in upvalues or tables.
** E.g. 'self.idx = self.idx + 1' deep down in some OO-style method may
** become a forwarded loop-recurrence after inlining.
**
** Since the IR is in SSA form, loop-carried dependencies have to be
** modeled with PHI instructions. The potential candidates for PHIs are
** collected on-the-fly during copy-substitution. After eliminating the
** redundant ones, PHI instructions are emitted *below* the loop body.
**
** Note that this departure from traditional SSA form doesn't change the
** semantics of the PHI instructions themselves. But it greatly simplifies
** on-the-fly generation of the IR and the machine code.
*/

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)		(&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Emit raw IR without passing through optimizations. */
#define emitir_raw(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

/* -- PHI elimination ----------------------------------------------------- */

/* Emit or eliminate collected PHIs. */
static void loop_emit_phi(jit_State *J, IRRef1 *subst, IRRef1 *phi, IRRef nphi,
			  SnapNo onsnap)
{
  int passx = 0;
  IRRef i, nslots;
  IRRef invar = J->chain[IR_LOOP];
  /* Pass #1: mark redundant and potentially redundant PHIs. */
  for (i = 0; i < nphi; i++) {
    IRRef lref = phi[i];
    IRRef rref = subst[lref];
    if (lref == rref || rref == REF_DROP) {  /* Invariants are redundant. */
      irt_setmark(IR(lref)->t);
    } else if (!(IR(rref)->op1 == lref || IR(rref)->op2 == lref)) {
      /* Quick check for simple recurrences failed, need pass2. */
      irt_setmark(IR(lref)->t);
      passx = 1;
    }
  }
  /* Pass #2: traverse variant part and clear marks of non-redundant PHIs. */
  if (passx) {
    SnapNo s;
    for (i = J->cur.nins-1; i > invar; i--) {
      IRIns *ir = IR(i);
      if (!irref_isk(ir->op2)) irt_clearmark(IR(ir->op2)->t);
      if (!irref_isk(ir->op1)) {
	irt_clearmark(IR(ir->op1)->t);
	if (ir->op1 < invar &&
	    ir->o >= IR_CALLN && ir->o <= IR_CARG) {  /* ORDER IR */
	  ir = IR(ir->op1);
	  while (ir->o == IR_CARG) {
	    if (!irref_isk(ir->op2)) irt_clearmark(IR(ir->op2)->t);
	    if (irref_isk(ir->op1)) break;
	    ir = IR(ir->op1);
	    irt_clearmark(ir->t);
	  }
	}
      }
    }
    for (s = J->cur.nsnap-1; s >= onsnap; s--) {
      SnapShot *snap = &J->cur.snap[s];
      SnapEntry *map = &J->cur.snapmap[snap->mapofs];
      MSize n, nent = snap->nent;
      for (n = 0; n < nent; n++) {
	IRRef ref = snap_ref(map[n]);
	if (!irref_isk(ref)) irt_clearmark(IR(ref)->t);
      }
    }
  }
  /* Pass #3: add PHIs for variant slots without a corresponding SLOAD. */
  nslots = J->baseslot+J->maxslot;
  for (i = 1; i < nslots; i++) {
    IRRef ref = tref_ref(J->slot[i]);
    while (!irref_isk(ref) && ref != subst[ref]) {
      IRIns *ir = IR(ref);
      irt_clearmark(ir->t);  /* Unmark potential uses, too. */
      if (irt_isphi(ir->t) || irt_ispri(ir->t))
	break;
      irt_setphi(ir->t);
      if (nphi >= LJ_MAX_PHI)
	lj_trace_err(J, LJ_TRERR_PHIOV);
      phi[nphi++] = (IRRef1)ref;
      ref = subst[ref];
      if (ref > invar)
	break;
    }
  }
  /* Pass #4: propagate non-redundant PHIs. */
  while (passx) {
    passx = 0;
    for (i = 0; i < nphi; i++) {
      IRRef lref = phi[i];
      IRIns *ir = IR(lref);
      if (!irt_ismarked(ir->t)) {  /* Propagate only from unmarked PHIs. */
	IRRef rref = subst[lref];
	if (lref == rref) {  /* Mark redundant PHI. */
	  irt_setmark(ir->t);
	} else {
	  IRIns *irr = IR(rref);
	  if (irt_ismarked(irr->t)) {  /* Right ref points to other PHI? */
	    irt_clearmark(irr->t);  /* Mark that PHI as non-redundant. */
	    passx = 1;  /* Retry. */
	  }
	}
      }
    }
  }
  /* Pass #5: emit PHI instructions or eliminate PHIs. */
  for (i = 0; i < nphi; i++) {
    IRRef lref = phi[i];
    IRIns *ir = IR(lref);
    if (!irt_ismarked(ir->t)) {  /* Emit PHI if not marked. */
      IRRef rref = subst[lref];
      if (rref > invar)
	irt_setphi(IR(rref)->t);
      emitir_raw(IRT(IR_PHI, irt_type(ir->t)), lref, rref);
    } else {  /* Otherwise eliminate PHI. */
      irt_clearmark(ir->t);
      irt_clearphi(ir->t);
    }
  }
}

/* -- Loop unrolling using copy-substitution ------------------------------ */

/* Copy-substitute snapshot. */
static void loop_subst_snap(jit_State *J, SnapShot *osnap,
			    SnapEntry *loopmap, IRRef1 *subst)
{
  SnapEntry *nmap, *omap = &J->cur.snapmap[osnap->mapofs];
  SnapEntry *nextmap = &J->cur.snapmap[snap_nextofs(&J->cur, osnap)];
  MSize nmapofs;
  MSize on, ln, nn, onent = osnap->nent;
  BCReg nslots = osnap->nslots;
  SnapShot *snap = &J->cur.snap[J->cur.nsnap];
  if (irt_isguard(J->guardemit)) {  /* Guard inbetween? */
    nmapofs = J->cur.nsnapmap;
    J->cur.nsnap++;  /* Add new snapshot. */
  } else {  /* Otherwise overwrite previous snapshot. */
    snap--;
    nmapofs = snap->mapofs;
  }
  J->guardemit.irt = 0;
  /* Setup new snapshot. */
  snap->mapofs = (uint16_t)nmapofs;
  snap->ref = (IRRef1)J->cur.nins;
  snap->nslots = nslots;
  snap->topslot = osnap->topslot;
  snap->count = 0;
  nmap = &J->cur.snapmap[nmapofs];
  /* Substitute snapshot slots. */
  on = ln = nn = 0;
  while (on < onent) {
    SnapEntry osn = omap[on], lsn = loopmap[ln];
    if (snap_slot(lsn) < snap_slot(osn)) {  /* Copy slot from loop map. */
      nmap[nn++] = lsn;
      ln++;
    } else {  /* Copy substituted slot from snapshot map. */
      if (snap_slot(lsn) == snap_slot(osn)) ln++;  /* Shadowed loop slot. */
      if (!irref_isk(snap_ref(osn)))
	osn = snap_setref(osn, subst[snap_ref(osn)]);
      nmap[nn++] = osn;
      on++;
    }
  }
  while (snap_slot(loopmap[ln]) < nslots)  /* Copy remaining loop slots. */
    nmap[nn++] = loopmap[ln++];
  snap->nent = (uint8_t)nn;
  omap += onent;
  nmap += nn;
  while (omap < nextmap)  /* Copy PC + frame links. */
    *nmap++ = *omap++;
  J->cur.nsnapmap = (uint16_t)(nmap - J->cur.snapmap);
}

/* Unroll loop. */
static void loop_unroll(jit_State *J)
{
  IRRef1 phi[LJ_MAX_PHI];
  uint32_t nphi = 0;
  IRRef1 *subst;
  SnapNo onsnap;
  SnapShot *osnap, *loopsnap;
  SnapEntry *loopmap, *psentinel;
  IRRef ins, invar;

  /* Use temp buffer for substitution table.
  ** Only non-constant refs in [REF_BIAS,invar) are valid indexes.
  ** Caveat: don't call into the VM or run the GC or the buffer may be gone.
  */
  invar = J->cur.nins;
  subst = (IRRef1 *)lj_str_needbuf(J->L, &G(J->L)->tmpbuf,
				   (invar-REF_BIAS)*sizeof(IRRef1)) - REF_BIAS;
  subst[REF_BASE] = REF_BASE;

  /* LOOP separates the pre-roll from the loop body. */
  emitir_raw(IRTG(IR_LOOP, IRT_NIL), 0, 0);

  /* Grow snapshot buffer and map for copy-substituted snapshots.
  ** Need up to twice the number of snapshots minus #0 and loop snapshot.
  ** Need up to twice the number of entries plus fallback substitutions
  ** from the loop snapshot entries for each new snapshot.
  ** Caveat: both calls may reallocate J->cur.snap and J->cur.snapmap!
  */
  onsnap = J->cur.nsnap;
  lj_snap_grow_buf(J, 2*onsnap-2);
  lj_snap_grow_map(J, J->cur.nsnapmap*2+(onsnap-2)*J->cur.snap[onsnap-1].nent);

  /* The loop snapshot is used for fallback substitutions. */
  loopsnap = &J->cur.snap[onsnap-1];
  loopmap = &J->cur.snapmap[loopsnap->mapofs];
  /* The PC of snapshot #0 and the loop snapshot must match. */
  psentinel = &loopmap[loopsnap->nent];
  lua_assert(*psentinel == J->cur.snapmap[J->cur.snap[0].nent]);
  *psentinel = SNAP(255, 0, 0);  /* Replace PC with temporary sentinel. */

  /* Start substitution with snapshot #1 (#0 is empty for root traces). */
  osnap = &J->cur.snap[1];

  /* Copy and substitute all recorded instructions and snapshots. */
  for (ins = REF_FIRST; ins < invar; ins++) {
    IRIns *ir;
    IRRef op1, op2;

    if (ins >= osnap->ref)  /* Instruction belongs to next snapshot? */
      loop_subst_snap(J, osnap++, loopmap, subst);  /* Copy-substitute it. */

    /* Substitute instruction operands. */
    ir = IR(ins);
    op1 = ir->op1;
    if (!irref_isk(op1)) op1 = subst[op1];
    op2 = ir->op2;
    if (!irref_isk(op2)) op2 = subst[op2];
    if (irm_kind(lj_ir_mode[ir->o]) == IRM_N &&
	op1 == ir->op1 && op2 == ir->op2) {  /* Regular invariant ins? */
      subst[ins] = (IRRef1)ins;  /* Shortcut. */
    } else {
      /* Re-emit substituted instruction to the FOLD/CSE/etc. pipeline. */
      IRType1 t = ir->t;  /* Get this first, since emitir may invalidate ir. */
      IRRef ref = tref_ref(emitir(ir->ot & ~IRT_ISPHI, op1, op2));
      subst[ins] = (IRRef1)ref;
      if (ref != ins) {
	IRIns *irr = IR(ref);
	if (ref < invar) {  /* Loop-carried dependency? */
	  /* Potential PHI? */
	  if (!irref_isk(ref) && !irt_isphi(irr->t) && !irt_ispri(irr->t)) {
	    irt_setphi(irr->t);
	    if (nphi >= LJ_MAX_PHI)
	      lj_trace_err(J, LJ_TRERR_PHIOV);
	    phi[nphi++] = (IRRef1)ref;
	  }
	  /* Check all loop-carried dependencies for type instability. */
	  if (!irt_sametype(t, irr->t)) {
	    if (irt_isinteger(t) && irt_isinteger(irr->t))
	      continue;
	    else if (irt_isnum(t) && irt_isinteger(irr->t))  /* Fix int->num. */
	      ref = tref_ref(emitir(IRTN(IR_CONV), ref, IRCONV_NUM_INT));
	    else if (irt_isnum(irr->t) && irt_isinteger(t))  /* Fix num->int. */
	      ref = tref_ref(emitir(IRTGI(IR_CONV), ref,
				    IRCONV_INT_NUM|IRCONV_CHECK));
	    else
	      lj_trace_err(J, LJ_TRERR_TYPEINS);
	    subst[ins] = (IRRef1)ref;
	    irr = IR(ref);
	    goto phiconv;
	  }
	} else if (ref != REF_DROP && irr->o == IR_CONV &&
		   ref > invar && irr->op1 < invar) {
	  /* May need an extra PHI for a CONV. */
	  ref = irr->op1;
	  irr = IR(ref);
	phiconv:
	  if (ref < invar && !irref_isk(ref) && !irt_isphi(irr->t)) {
	    irt_setphi(irr->t);
	    if (nphi >= LJ_MAX_PHI)
	      lj_trace_err(J, LJ_TRERR_PHIOV);
	    phi[nphi++] = (IRRef1)ref;
	  }
	}
      }
    }
  }
  if (!irt_isguard(J->guardemit))  /* Drop redundant snapshot. */
    J->cur.nsnapmap = (uint16_t)J->cur.snap[--J->cur.nsnap].mapofs;
  lua_assert(J->cur.nsnapmap <= J->sizesnapmap);
  *psentinel = J->cur.snapmap[J->cur.snap[0].nent];  /* Restore PC. */

  loop_emit_phi(J, subst, phi, nphi, onsnap);
}

/* Undo any partial changes made by the loop optimization. */
static void loop_undo(jit_State *J, IRRef ins, SnapNo nsnap, MSize nsnapmap)
{
  ptrdiff_t i;
  SnapShot *snap = &J->cur.snap[nsnap-1];
  SnapEntry *map = J->cur.snapmap;
  map[snap->mapofs + snap->nent] = map[J->cur.snap[0].nent];  /* Restore PC. */
  J->cur.nsnapmap = (uint16_t)nsnapmap;
  J->cur.nsnap = nsnap;
  J->guardemit.irt = 0;
  lj_ir_rollback(J, ins);
  for (i = 0; i < BPROP_SLOTS; i++) {  /* Remove backprop. cache entries. */
    BPropEntry *bp = &J->bpropcache[i];
    if (bp->val >= ins)
      bp->key = 0;
  }
  for (ins--; ins >= REF_FIRST; ins--) {  /* Remove flags. */
    IRIns *ir = IR(ins);
    irt_clearphi(ir->t);
    irt_clearmark(ir->t);
  }
}

/* Protected callback for loop optimization. */
static TValue *cploop_opt(lua_State *L, lua_CFunction dummy, void *ud)
{
  UNUSED(L); UNUSED(dummy);
  loop_unroll((jit_State *)ud);
  return NULL;
}

/* Loop optimization. */
int lj_opt_loop(jit_State *J)
{
  IRRef nins = J->cur.nins;
  SnapNo nsnap = J->cur.nsnap;
  MSize nsnapmap = J->cur.nsnapmap;
  int errcode = lj_vm_cpcall(J->L, NULL, J, cploop_opt);
  if (LJ_UNLIKELY(errcode)) {
    lua_State *L = J->L;
    if (errcode == LUA_ERRRUN && tvisnumber(L->top-1)) {  /* Trace error? */
      int32_t e = numberVint(L->top-1);
      switch ((TraceError)e) {
      case LJ_TRERR_TYPEINS:  /* Type instability. */
      case LJ_TRERR_GFAIL:  /* Guard would always fail. */
	/* Unrolling via recording fixes many cases, e.g. a flipped boolean. */
	if (--J->instunroll < 0)  /* But do not unroll forever. */
	  break;
	L->top--;  /* Remove error object. */
	loop_undo(J, nins, nsnap, nsnapmap);
	return 1;  /* Loop optimization failed, continue recording. */
      default:
	break;
      }
    }
    lj_err_throw(L, errcode);  /* Propagate all other errors. */
  }
  return 0;  /* Loop optimization is ok. */
}

#undef IR
#undef emitir
#undef emitir_raw

#endif
