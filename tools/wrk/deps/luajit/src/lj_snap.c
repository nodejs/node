/*
** Snapshot handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_snap_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_gc.h"
#include "lj_tab.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_snap.h"
#include "lj_target.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#include "lj_cdata.h"
#endif

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)		(&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* Emit raw IR without passing through optimizations. */
#define emitir_raw(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_ir_emit(J))

/* -- Snapshot buffer allocation ------------------------------------------ */

/* Grow snapshot buffer. */
void lj_snap_grow_buf_(jit_State *J, MSize need)
{
  MSize maxsnap = (MSize)J->param[JIT_P_maxsnap];
  if (need > maxsnap)
    lj_trace_err(J, LJ_TRERR_SNAPOV);
  lj_mem_growvec(J->L, J->snapbuf, J->sizesnap, maxsnap, SnapShot);
  J->cur.snap = J->snapbuf;
}

/* Grow snapshot map buffer. */
void lj_snap_grow_map_(jit_State *J, MSize need)
{
  if (need < 2*J->sizesnapmap)
    need = 2*J->sizesnapmap;
  else if (need < 64)
    need = 64;
  J->snapmapbuf = (SnapEntry *)lj_mem_realloc(J->L, J->snapmapbuf,
		    J->sizesnapmap*sizeof(SnapEntry), need*sizeof(SnapEntry));
  J->cur.snapmap = J->snapmapbuf;
  J->sizesnapmap = need;
}

/* -- Snapshot generation ------------------------------------------------- */

/* Add all modified slots to the snapshot. */
static MSize snapshot_slots(jit_State *J, SnapEntry *map, BCReg nslots)
{
  IRRef retf = J->chain[IR_RETF];  /* Limits SLOAD restore elimination. */
  BCReg s;
  MSize n = 0;
  for (s = 0; s < nslots; s++) {
    TRef tr = J->slot[s];
    IRRef ref = tref_ref(tr);
    if (ref) {
      SnapEntry sn = SNAP_TR(s, tr);
      IRIns *ir = IR(ref);
      if (!(sn & (SNAP_CONT|SNAP_FRAME)) &&
	  ir->o == IR_SLOAD && ir->op1 == s && ref > retf) {
	/* No need to snapshot unmodified non-inherited slots. */
	if (!(ir->op2 & IRSLOAD_INHERIT))
	  continue;
	/* No need to restore readonly slots and unmodified non-parent slots. */
	if (!(LJ_DUALNUM && (ir->op2 & IRSLOAD_CONVERT)) &&
	    (ir->op2 & (IRSLOAD_READONLY|IRSLOAD_PARENT)) != IRSLOAD_PARENT)
	  sn |= SNAP_NORESTORE;
      }
      if (LJ_SOFTFP && irt_isnum(ir->t))
	sn |= SNAP_SOFTFPNUM;
      map[n++] = sn;
    }
  }
  return n;
}

/* Add frame links at the end of the snapshot. */
static BCReg snapshot_framelinks(jit_State *J, SnapEntry *map)
{
  cTValue *frame = J->L->base - 1;
  cTValue *lim = J->L->base - J->baseslot;
  cTValue *ftop = frame + funcproto(frame_func(frame))->framesize;
  MSize f = 0;
  map[f++] = SNAP_MKPC(J->pc);  /* The current PC is always the first entry. */
  while (frame > lim) {  /* Backwards traversal of all frames above base. */
    if (frame_islua(frame)) {
      map[f++] = SNAP_MKPC(frame_pc(frame));
      frame = frame_prevl(frame);
      if (frame + funcproto(frame_func(frame))->framesize > ftop)
	ftop = frame + funcproto(frame_func(frame))->framesize;
    } else if (frame_iscont(frame)) {
      map[f++] = SNAP_MKFTSZ(frame_ftsz(frame));
      map[f++] = SNAP_MKPC(frame_contpc(frame));
      frame = frame_prevd(frame);
    } else {
      lua_assert(!frame_isc(frame));
      map[f++] = SNAP_MKFTSZ(frame_ftsz(frame));
      frame = frame_prevd(frame);
    }
  }
  lua_assert(f == (MSize)(1 + J->framedepth));
  return (BCReg)(ftop - lim);
}

/* Take a snapshot of the current stack. */
static void snapshot_stack(jit_State *J, SnapShot *snap, MSize nsnapmap)
{
  BCReg nslots = J->baseslot + J->maxslot;
  MSize nent;
  SnapEntry *p;
  /* Conservative estimate. */
  lj_snap_grow_map(J, nsnapmap + nslots + (MSize)J->framedepth+1);
  p = &J->cur.snapmap[nsnapmap];
  nent = snapshot_slots(J, p, nslots);
  snap->topslot = (uint8_t)snapshot_framelinks(J, p + nent);
  snap->mapofs = (uint16_t)nsnapmap;
  snap->ref = (IRRef1)J->cur.nins;
  snap->nent = (uint8_t)nent;
  snap->nslots = (uint8_t)nslots;
  snap->count = 0;
  J->cur.nsnapmap = (uint16_t)(nsnapmap + nent + 1 + J->framedepth);
}

/* Add or merge a snapshot. */
void lj_snap_add(jit_State *J)
{
  MSize nsnap = J->cur.nsnap;
  MSize nsnapmap = J->cur.nsnapmap;
  /* Merge if no ins. inbetween or if requested and no guard inbetween. */
  if (J->mergesnap ? !irt_isguard(J->guardemit) :
      (nsnap > 0 && J->cur.snap[nsnap-1].ref == J->cur.nins)) {
    if (nsnap == 1) {  /* But preserve snap #0 PC. */
      emitir_raw(IRT(IR_NOP, IRT_NIL), 0, 0);
      goto nomerge;
    }
    nsnapmap = J->cur.snap[--nsnap].mapofs;
  } else {
  nomerge:
    lj_snap_grow_buf(J, nsnap+1);
    J->cur.nsnap = (uint16_t)(nsnap+1);
  }
  J->mergesnap = 0;
  J->guardemit.irt = 0;
  snapshot_stack(J, &J->cur.snap[nsnap], nsnapmap);
}

/* -- Snapshot modification ----------------------------------------------- */

#define SNAP_USEDEF_SLOTS	(LJ_MAX_JSLOTS+LJ_STACK_EXTRA)

/* Find unused slots with reaching-definitions bytecode data-flow analysis. */
static BCReg snap_usedef(jit_State *J, uint8_t *udf,
			 const BCIns *pc, BCReg maxslot)
{
  BCReg s;
  GCobj *o;

  if (maxslot == 0) return 0;
#ifdef LUAJIT_USE_VALGRIND
  /* Avoid errors for harmless reads beyond maxslot. */
  memset(udf, 1, SNAP_USEDEF_SLOTS);
#else
  memset(udf, 1, maxslot);
#endif

  /* Treat open upvalues as used. */
  o = gcref(J->L->openupval);
  while (o) {
    if (uvval(gco2uv(o)) < J->L->base) break;
    udf[uvval(gco2uv(o)) - J->L->base] = 0;
    o = gcref(o->gch.nextgc);
  }

#define USE_SLOT(s)		udf[(s)] &= ~1
#define DEF_SLOT(s)		udf[(s)] *= 3

  /* Scan through following bytecode and check for uses/defs. */
  lua_assert(pc >= proto_bc(J->pt) && pc < proto_bc(J->pt) + J->pt->sizebc);
  for (;;) {
    BCIns ins = *pc++;
    BCOp op = bc_op(ins);
    switch (bcmode_b(op)) {
    case BCMvar: USE_SLOT(bc_b(ins)); break;
    default: break;
    }
    switch (bcmode_c(op)) {
    case BCMvar: USE_SLOT(bc_c(ins)); break;
    case BCMrbase:
      lua_assert(op == BC_CAT);
      for (s = bc_b(ins); s <= bc_c(ins); s++) USE_SLOT(s);
      for (; s < maxslot; s++) DEF_SLOT(s);
      break;
    case BCMjump:
    handle_jump: {
      BCReg minslot = bc_a(ins);
      if (op >= BC_FORI && op <= BC_JFORL) minslot += FORL_EXT;
      else if (op >= BC_ITERL && op <= BC_JITERL) minslot += bc_b(pc[-2])-1;
      else if (op == BC_UCLO) { pc += bc_j(ins); break; }
      for (s = minslot; s < maxslot; s++) DEF_SLOT(s);
      return minslot < maxslot ? minslot : maxslot;
      }
    case BCMlit:
      if (op == BC_JFORL || op == BC_JITERL || op == BC_JLOOP) {
	goto handle_jump;
      } else if (bc_isret(op)) {
	BCReg top = op == BC_RETM ? maxslot : (bc_a(ins) + bc_d(ins)-1);
	for (s = 0; s < bc_a(ins); s++) DEF_SLOT(s);
	for (; s < top; s++) USE_SLOT(s);
	for (; s < maxslot; s++) DEF_SLOT(s);
	return 0;
      }
      break;
    case BCMfunc: return maxslot;  /* NYI: will abort, anyway. */
    default: break;
    }
    switch (bcmode_a(op)) {
    case BCMvar: USE_SLOT(bc_a(ins)); break;
    case BCMdst:
       if (!(op == BC_ISTC || op == BC_ISFC)) DEF_SLOT(bc_a(ins));
       break;
    case BCMbase:
      if (op >= BC_CALLM && op <= BC_VARG) {
	BCReg top = (op == BC_CALLM || op == BC_CALLMT || bc_c(ins) == 0) ?
		    maxslot : (bc_a(ins) + bc_c(ins));
	s = bc_a(ins) - ((op == BC_ITERC || op == BC_ITERN) ? 3 : 0);
	for (; s < top; s++) USE_SLOT(s);
	for (; s < maxslot; s++) DEF_SLOT(s);
	if (op == BC_CALLT || op == BC_CALLMT) {
	  for (s = 0; s < bc_a(ins); s++) DEF_SLOT(s);
	  return 0;
	}
      } else if (op == BC_KNIL) {
	for (s = bc_a(ins); s <= bc_d(ins); s++) DEF_SLOT(s);
      } else if (op == BC_TSETM) {
	for (s = bc_a(ins)-1; s < maxslot; s++) USE_SLOT(s);
      }
      break;
    default: break;
    }
    lua_assert(pc >= proto_bc(J->pt) && pc < proto_bc(J->pt) + J->pt->sizebc);
  }

#undef USE_SLOT
#undef DEF_SLOT

  return 0;  /* unreachable */
}

/* Purge dead slots before the next snapshot. */
void lj_snap_purge(jit_State *J)
{
  uint8_t udf[SNAP_USEDEF_SLOTS];
  BCReg maxslot = J->maxslot;
  BCReg s = snap_usedef(J, udf, J->pc, maxslot);
  for (; s < maxslot; s++)
    if (udf[s] != 0)
      J->base[s] = 0;  /* Purge dead slots. */
}

/* Shrink last snapshot. */
void lj_snap_shrink(jit_State *J)
{
  SnapShot *snap = &J->cur.snap[J->cur.nsnap-1];
  SnapEntry *map = &J->cur.snapmap[snap->mapofs];
  MSize n, m, nlim, nent = snap->nent;
  uint8_t udf[SNAP_USEDEF_SLOTS];
  BCReg maxslot = J->maxslot;
  BCReg minslot = snap_usedef(J, udf, snap_pc(map[nent]), maxslot);
  BCReg baseslot = J->baseslot;
  maxslot += baseslot;
  minslot += baseslot;
  snap->nslots = (uint8_t)maxslot;
  for (n = m = 0; n < nent; n++) {  /* Remove unused slots from snapshot. */
    BCReg s = snap_slot(map[n]);
    if (s < minslot || (s < maxslot && udf[s-baseslot] == 0))
      map[m++] = map[n];  /* Only copy used slots. */
  }
  snap->nent = (uint8_t)m;
  nlim = J->cur.nsnapmap - snap->mapofs - 1;
  while (n <= nlim) map[m++] = map[n++];  /* Move PC + frame links down. */
  J->cur.nsnapmap = (uint16_t)(snap->mapofs + m);  /* Free up space in map. */
}

/* -- Snapshot access ----------------------------------------------------- */

/* Initialize a Bloom Filter with all renamed refs.
** There are very few renames (often none), so the filter has
** very few bits set. This makes it suitable for negative filtering.
*/
static BloomFilter snap_renamefilter(GCtrace *T, SnapNo lim)
{
  BloomFilter rfilt = 0;
  IRIns *ir;
  for (ir = &T->ir[T->nins-1]; ir->o == IR_RENAME; ir--)
    if (ir->op2 <= lim)
      bloomset(rfilt, ir->op1);
  return rfilt;
}

/* Process matching renames to find the original RegSP. */
static RegSP snap_renameref(GCtrace *T, SnapNo lim, IRRef ref, RegSP rs)
{
  IRIns *ir;
  for (ir = &T->ir[T->nins-1]; ir->o == IR_RENAME; ir--)
    if (ir->op1 == ref && ir->op2 <= lim)
      rs = ir->prev;
  return rs;
}

/* Copy RegSP from parent snapshot to the parent links of the IR. */
IRIns *lj_snap_regspmap(GCtrace *T, SnapNo snapno, IRIns *ir)
{
  SnapShot *snap = &T->snap[snapno];
  SnapEntry *map = &T->snapmap[snap->mapofs];
  BloomFilter rfilt = snap_renamefilter(T, snapno);
  MSize n = 0;
  IRRef ref = 0;
  for ( ; ; ir++) {
    uint32_t rs;
    if (ir->o == IR_SLOAD) {
      if (!(ir->op2 & IRSLOAD_PARENT)) break;
      for ( ; ; n++) {
	lua_assert(n < snap->nent);
	if (snap_slot(map[n]) == ir->op1) {
	  ref = snap_ref(map[n++]);
	  break;
	}
      }
    } else if (LJ_SOFTFP && ir->o == IR_HIOP) {
      ref++;
    } else if (ir->o == IR_PVAL) {
      ref = ir->op1 + REF_BIAS;
    } else {
      break;
    }
    rs = T->ir[ref].prev;
    if (bloomtest(rfilt, ref))
      rs = snap_renameref(T, snapno, ref, rs);
    ir->prev = (uint16_t)rs;
    lua_assert(regsp_used(rs));
  }
  return ir;
}

/* -- Snapshot replay ----------------------------------------------------- */

/* Replay constant from parent trace. */
static TRef snap_replay_const(jit_State *J, IRIns *ir)
{
  /* Only have to deal with constants that can occur in stack slots. */
  switch ((IROp)ir->o) {
  case IR_KPRI: return TREF_PRI(irt_type(ir->t));
  case IR_KINT: return lj_ir_kint(J, ir->i);
  case IR_KGC: return lj_ir_kgc(J, ir_kgc(ir), irt_t(ir->t));
  case IR_KNUM: return lj_ir_k64(J, IR_KNUM, ir_knum(ir));
  case IR_KINT64: return lj_ir_k64(J, IR_KINT64, ir_kint64(ir));
  case IR_KPTR: return lj_ir_kptr(J, ir_kptr(ir));  /* Continuation. */
  default: lua_assert(0); return TREF_NIL; break;
  }
}

/* De-duplicate parent reference. */
static TRef snap_dedup(jit_State *J, SnapEntry *map, MSize nmax, IRRef ref)
{
  MSize j;
  for (j = 0; j < nmax; j++)
    if (snap_ref(map[j]) == ref)
      return J->slot[snap_slot(map[j])] & ~(SNAP_CONT|SNAP_FRAME);
  return 0;
}

/* Emit parent reference with de-duplication. */
static TRef snap_pref(jit_State *J, GCtrace *T, SnapEntry *map, MSize nmax,
		      BloomFilter seen, IRRef ref)
{
  IRIns *ir = &T->ir[ref];
  TRef tr;
  if (irref_isk(ref))
    tr = snap_replay_const(J, ir);
  else if (!regsp_used(ir->prev))
    tr = 0;
  else if (!bloomtest(seen, ref) || (tr = snap_dedup(J, map, nmax, ref)) == 0)
    tr = emitir(IRT(IR_PVAL, irt_type(ir->t)), ref - REF_BIAS, 0);
  return tr;
}

/* Check whether a sunk store corresponds to an allocation. Slow path. */
static int snap_sunk_store2(jit_State *J, IRIns *ira, IRIns *irs)
{
  if (irs->o == IR_ASTORE || irs->o == IR_HSTORE ||
      irs->o == IR_FSTORE || irs->o == IR_XSTORE) {
    IRIns *irk = IR(irs->op1);
    if (irk->o == IR_AREF || irk->o == IR_HREFK)
      irk = IR(irk->op1);
    return (IR(irk->op1) == ira);
  }
  return 0;
}

/* Check whether a sunk store corresponds to an allocation. Fast path. */
static LJ_AINLINE int snap_sunk_store(jit_State *J, IRIns *ira, IRIns *irs)
{
  if (irs->s != 255)
    return (ira + irs->s == irs);  /* Fast check. */
  return snap_sunk_store2(J, ira, irs);
}

/* Replay snapshot state to setup side trace. */
void lj_snap_replay(jit_State *J, GCtrace *T)
{
  SnapShot *snap = &T->snap[J->exitno];
  SnapEntry *map = &T->snapmap[snap->mapofs];
  MSize n, nent = snap->nent;
  BloomFilter seen = 0;
  int pass23 = 0;
  J->framedepth = 0;
  /* Emit IR for slots inherited from parent snapshot. */
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    BCReg s = snap_slot(sn);
    IRRef ref = snap_ref(sn);
    IRIns *ir = &T->ir[ref];
    TRef tr;
    /* The bloom filter avoids O(nent^2) overhead for de-duping slots. */
    if (bloomtest(seen, ref) && (tr = snap_dedup(J, map, n, ref)) != 0)
      goto setslot;
    bloomset(seen, ref);
    if (irref_isk(ref)) {
      tr = snap_replay_const(J, ir);
    } else if (!regsp_used(ir->prev)) {
      pass23 = 1;
      lua_assert(s != 0);
      tr = s;
    } else {
      IRType t = irt_type(ir->t);
      uint32_t mode = IRSLOAD_INHERIT|IRSLOAD_PARENT;
      if (LJ_SOFTFP && (sn & SNAP_SOFTFPNUM)) t = IRT_NUM;
      if (ir->o == IR_SLOAD) mode |= (ir->op2 & IRSLOAD_READONLY);
      tr = emitir_raw(IRT(IR_SLOAD, t), s, mode);
    }
  setslot:
    J->slot[s] = tr | (sn&(SNAP_CONT|SNAP_FRAME));  /* Same as TREF_* flags. */
    J->framedepth += ((sn & (SNAP_CONT|SNAP_FRAME)) && s);
    if ((sn & SNAP_FRAME))
      J->baseslot = s+1;
  }
  if (pass23) {
    IRIns *irlast = &T->ir[snap->ref];
    pass23 = 0;
    /* Emit dependent PVALs. */
    for (n = 0; n < nent; n++) {
      SnapEntry sn = map[n];
      IRRef refp = snap_ref(sn);
      IRIns *ir = &T->ir[refp];
      if (regsp_reg(ir->r) == RID_SUNK) {
	if (J->slot[snap_slot(sn)] != snap_slot(sn)) continue;
	pass23 = 1;
	lua_assert(ir->o == IR_TNEW || ir->o == IR_TDUP ||
		   ir->o == IR_CNEW || ir->o == IR_CNEWI);
	if (ir->op1 >= T->nk) snap_pref(J, T, map, nent, seen, ir->op1);
	if (ir->op2 >= T->nk) snap_pref(J, T, map, nent, seen, ir->op2);
	if (LJ_HASFFI && ir->o == IR_CNEWI) {
	  if (LJ_32 && refp+1 < T->nins && (ir+1)->o == IR_HIOP)
	    snap_pref(J, T, map, nent, seen, (ir+1)->op2);
	} else {
	  IRIns *irs;
	  for (irs = ir+1; irs < irlast; irs++)
	    if (irs->r == RID_SINK && snap_sunk_store(J, ir, irs)) {
	      if (snap_pref(J, T, map, nent, seen, irs->op2) == 0)
		snap_pref(J, T, map, nent, seen, T->ir[irs->op2].op1);
	      else if ((LJ_SOFTFP || (LJ_32 && LJ_HASFFI)) &&
		       irs+1 < irlast && (irs+1)->o == IR_HIOP)
		snap_pref(J, T, map, nent, seen, (irs+1)->op2);
	    }
	}
      } else if (!irref_isk(refp) && !regsp_used(ir->prev)) {
	lua_assert(ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT);
	J->slot[snap_slot(sn)] = snap_pref(J, T, map, nent, seen, ir->op1);
      }
    }
    /* Replay sunk instructions. */
    for (n = 0; pass23 && n < nent; n++) {
      SnapEntry sn = map[n];
      IRRef refp = snap_ref(sn);
      IRIns *ir = &T->ir[refp];
      if (regsp_reg(ir->r) == RID_SUNK) {
	TRef op1, op2;
	if (J->slot[snap_slot(sn)] != snap_slot(sn)) {  /* De-dup allocs. */
	  J->slot[snap_slot(sn)] = J->slot[J->slot[snap_slot(sn)]];
	  continue;
	}
	op1 = ir->op1;
	if (op1 >= T->nk) op1 = snap_pref(J, T, map, nent, seen, op1);
	op2 = ir->op2;
	if (op2 >= T->nk) op2 = snap_pref(J, T, map, nent, seen, op2);
	if (LJ_HASFFI && ir->o == IR_CNEWI) {
	  if (LJ_32 && refp+1 < T->nins && (ir+1)->o == IR_HIOP) {
	    lj_needsplit(J);  /* Emit joining HIOP. */
	    op2 = emitir_raw(IRT(IR_HIOP, IRT_I64), op2,
			     snap_pref(J, T, map, nent, seen, (ir+1)->op2));
	  }
	  J->slot[snap_slot(sn)] = emitir(ir->ot, op1, op2);
	} else {
	  IRIns *irs;
	  TRef tr = emitir(ir->ot, op1, op2);
	  J->slot[snap_slot(sn)] = tr;
	  for (irs = ir+1; irs < irlast; irs++)
	    if (irs->r == RID_SINK && snap_sunk_store(J, ir, irs)) {
	      IRIns *irr = &T->ir[irs->op1];
	      TRef val, key = irr->op2, tmp = tr;
	      if (irr->o != IR_FREF) {
		IRIns *irk = &T->ir[key];
		if (irr->o == IR_HREFK)
		  key = lj_ir_kslot(J, snap_replay_const(J, &T->ir[irk->op1]),
				    irk->op2);
		else
		  key = snap_replay_const(J, irk);
		if (irr->o == IR_HREFK || irr->o == IR_AREF) {
		  IRIns *irf = &T->ir[irr->op1];
		  tmp = emitir(irf->ot, tmp, irf->op2);
		}
	      }
	      tmp = emitir(irr->ot, tmp, key);
	      val = snap_pref(J, T, map, nent, seen, irs->op2);
	      if (val == 0) {
		IRIns *irc = &T->ir[irs->op2];
		lua_assert(irc->o == IR_CONV && irc->op2 == IRCONV_NUM_INT);
		val = snap_pref(J, T, map, nent, seen, irc->op1);
		val = emitir(IRTN(IR_CONV), val, IRCONV_NUM_INT);
	      } else if ((LJ_SOFTFP || (LJ_32 && LJ_HASFFI)) &&
			 irs+1 < irlast && (irs+1)->o == IR_HIOP) {
		IRType t = IRT_I64;
		if (LJ_SOFTFP && irt_type((irs+1)->t) == IRT_SOFTFP)
		  t = IRT_NUM;
		lj_needsplit(J);
		if (irref_isk(irs->op2) && irref_isk((irs+1)->op2)) {
		  uint64_t k = (uint32_t)T->ir[irs->op2].i +
			       ((uint64_t)T->ir[(irs+1)->op2].i << 32);
		  val = lj_ir_k64(J, t == IRT_I64 ? IR_KINT64 : IR_KNUM,
				  lj_ir_k64_find(J, k));
		} else {
		  val = emitir_raw(IRT(IR_HIOP, t), val,
			  snap_pref(J, T, map, nent, seen, (irs+1)->op2));
		}
		tmp = emitir(IRT(irs->o, t), tmp, val);
		continue;
	      }
	      tmp = emitir(irs->ot, tmp, val);
	    } else if (LJ_HASFFI && irs->o == IR_XBAR && ir->o == IR_CNEW) {
	      emitir(IRT(IR_XBAR, IRT_NIL), 0, 0);
	    }
	}
      }
    }
  }
  J->base = J->slot + J->baseslot;
  J->maxslot = snap->nslots - J->baseslot;
  lj_snap_add(J);
  if (pass23)  /* Need explicit GC step _after_ initial snapshot. */
    emitir_raw(IRTG(IR_GCSTEP, IRT_NIL), 0, 0);
}

/* -- Snapshot restore ---------------------------------------------------- */

static void snap_unsink(jit_State *J, GCtrace *T, ExitState *ex,
			SnapNo snapno, BloomFilter rfilt,
			IRIns *ir, TValue *o);

/* Restore a value from the trace exit state. */
static void snap_restoreval(jit_State *J, GCtrace *T, ExitState *ex,
			    SnapNo snapno, BloomFilter rfilt,
			    IRRef ref, TValue *o)
{
  IRIns *ir = &T->ir[ref];
  IRType1 t = ir->t;
  RegSP rs = ir->prev;
  if (irref_isk(ref)) {  /* Restore constant slot. */
    lj_ir_kvalue(J->L, o, ir);
    return;
  }
  if (LJ_UNLIKELY(bloomtest(rfilt, ref)))
    rs = snap_renameref(T, snapno, ref, rs);
  if (ra_hasspill(regsp_spill(rs))) {  /* Restore from spill slot. */
    int32_t *sps = &ex->spill[regsp_spill(rs)];
    if (irt_isinteger(t)) {
      setintV(o, *sps);
#if !LJ_SOFTFP
    } else if (irt_isnum(t)) {
      o->u64 = *(uint64_t *)sps;
#endif
    } else if (LJ_64 && irt_islightud(t)) {
      /* 64 bit lightuserdata which may escape already has the tag bits. */
      o->u64 = *(uint64_t *)sps;
    } else {
      lua_assert(!irt_ispri(t));  /* PRI refs never have a spill slot. */
      setgcrefi(o->gcr, *sps);
      setitype(o, irt_toitype(t));
    }
  } else {  /* Restore from register. */
    Reg r = regsp_reg(rs);
    if (ra_noreg(r)) {
      lua_assert(ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT);
      snap_restoreval(J, T, ex, snapno, rfilt, ir->op1, o);
      if (LJ_DUALNUM) setnumV(o, (lua_Number)intV(o));
      return;
    } else if (irt_isinteger(t)) {
      setintV(o, (int32_t)ex->gpr[r-RID_MIN_GPR]);
#if !LJ_SOFTFP
    } else if (irt_isnum(t)) {
      setnumV(o, ex->fpr[r-RID_MIN_FPR]);
#endif
    } else if (LJ_64 && irt_islightud(t)) {
      /* 64 bit lightuserdata which may escape already has the tag bits. */
      o->u64 = ex->gpr[r-RID_MIN_GPR];
    } else {
      if (!irt_ispri(t))
	setgcrefi(o->gcr, ex->gpr[r-RID_MIN_GPR]);
      setitype(o, irt_toitype(t));
    }
  }
}

#if LJ_HASFFI
/* Restore raw data from the trace exit state. */
static void snap_restoredata(GCtrace *T, ExitState *ex,
			     SnapNo snapno, BloomFilter rfilt,
			     IRRef ref, void *dst, CTSize sz)
{
  IRIns *ir = &T->ir[ref];
  RegSP rs = ir->prev;
  int32_t *src;
  uint64_t tmp;
  if (irref_isk(ref)) {
    if (ir->o == IR_KNUM || ir->o == IR_KINT64) {
      src = mref(ir->ptr, int32_t);
    } else if (sz == 8) {
      tmp = (uint64_t)(uint32_t)ir->i;
      src = (int32_t *)&tmp;
    } else {
      src = &ir->i;
    }
  } else {
    if (LJ_UNLIKELY(bloomtest(rfilt, ref)))
      rs = snap_renameref(T, snapno, ref, rs);
    if (ra_hasspill(regsp_spill(rs))) {
      src = &ex->spill[regsp_spill(rs)];
      if (sz == 8 && !irt_is64(ir->t)) {
	tmp = (uint64_t)(uint32_t)*src;
	src = (int32_t *)&tmp;
      }
    } else {
      Reg r = regsp_reg(rs);
      if (ra_noreg(r)) {
	/* Note: this assumes CNEWI is never used for SOFTFP split numbers. */
	lua_assert(sz == 8 && ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT);
	snap_restoredata(T, ex, snapno, rfilt, ir->op1, dst, 4);
	*(lua_Number *)dst = (lua_Number)*(int32_t *)dst;
	return;
      }
      src = (int32_t *)&ex->gpr[r-RID_MIN_GPR];
#if !LJ_SOFTFP
      if (r >= RID_MAX_GPR) {
	src = (int32_t *)&ex->fpr[r-RID_MIN_FPR];
#if LJ_TARGET_PPC
	if (sz == 4) {  /* PPC FPRs are always doubles. */
	  *(float *)dst = (float)*(double *)src;
	  return;
	}
#else
	if (LJ_BE && sz == 4) src++;
#endif
      }
#endif
    }
  }
  lua_assert(sz == 1 || sz == 2 || sz == 4 || sz == 8);
  if (sz == 4) *(int32_t *)dst = *src;
  else if (sz == 8) *(int64_t *)dst = *(int64_t *)src;
  else if (sz == 1) *(int8_t *)dst = (int8_t)*src;
  else *(int16_t *)dst = (int16_t)*src;
}
#endif

/* Unsink allocation from the trace exit state. Unsink sunk stores. */
static void snap_unsink(jit_State *J, GCtrace *T, ExitState *ex,
			SnapNo snapno, BloomFilter rfilt,
			IRIns *ir, TValue *o)
{
  lua_assert(ir->o == IR_TNEW || ir->o == IR_TDUP ||
	     ir->o == IR_CNEW || ir->o == IR_CNEWI);
#if LJ_HASFFI
  if (ir->o == IR_CNEW || ir->o == IR_CNEWI) {
    CTState *cts = ctype_ctsG(J2G(J));
    CTypeID id = (CTypeID)T->ir[ir->op1].i;
    CTSize sz = lj_ctype_size(cts, id);
    GCcdata *cd = lj_cdata_new(cts, id, sz);
    setcdataV(J->L, o, cd);
    if (ir->o == IR_CNEWI) {
      uint8_t *p = (uint8_t *)cdataptr(cd);
      lua_assert(sz == 4 || sz == 8);
      if (LJ_32 && sz == 8 && ir+1 < T->ir + T->nins && (ir+1)->o == IR_HIOP) {
	snap_restoredata(T, ex, snapno, rfilt, (ir+1)->op2, LJ_LE?p+4:p, 4);
	if (LJ_BE) p += 4;
	sz = 4;
      }
      snap_restoredata(T, ex, snapno, rfilt, ir->op2, p, sz);
    } else {
      IRIns *irs, *irlast = &T->ir[T->snap[snapno].ref];
      for (irs = ir+1; irs < irlast; irs++)
	if (irs->r == RID_SINK && snap_sunk_store(J, ir, irs)) {
	  IRIns *iro = &T->ir[T->ir[irs->op1].op2];
	  uint8_t *p = (uint8_t *)cd;
	  CTSize szs;
	  lua_assert(irs->o == IR_XSTORE && T->ir[irs->op1].o == IR_ADD);
	  lua_assert(iro->o == IR_KINT || iro->o == IR_KINT64);
	  if (irt_is64(irs->t)) szs = 8;
	  else if (irt_isi8(irs->t) || irt_isu8(irs->t)) szs = 1;
	  else if (irt_isi16(irs->t) || irt_isu16(irs->t)) szs = 2;
	  else szs = 4;
	  if (LJ_64 && iro->o == IR_KINT64)
	    p += (int64_t)ir_k64(iro)->u64;
	  else
	    p += iro->i;
	  lua_assert(p >= (uint8_t *)cdataptr(cd) &&
		     p + szs <= (uint8_t *)cdataptr(cd) + sz);
	  if (LJ_32 && irs+1 < T->ir + T->nins && (irs+1)->o == IR_HIOP) {
	    lua_assert(szs == 4);
	    snap_restoredata(T, ex, snapno, rfilt, (irs+1)->op2, LJ_LE?p+4:p,4);
	    if (LJ_BE) p += 4;
	  }
	  snap_restoredata(T, ex, snapno, rfilt, irs->op2, p, szs);
	}
    }
  } else
#endif
  {
    IRIns *irs, *irlast;
    GCtab *t = ir->o == IR_TNEW ? lj_tab_new(J->L, ir->op1, ir->op2) :
				  lj_tab_dup(J->L, ir_ktab(&T->ir[ir->op1]));
    settabV(J->L, o, t);
    irlast = &T->ir[T->snap[snapno].ref];
    for (irs = ir+1; irs < irlast; irs++)
      if (irs->r == RID_SINK && snap_sunk_store(J, ir, irs)) {
	IRIns *irk = &T->ir[irs->op1];
	TValue tmp, *val;
	lua_assert(irs->o == IR_ASTORE || irs->o == IR_HSTORE ||
		   irs->o == IR_FSTORE);
	if (irk->o == IR_FREF) {
	  lua_assert(irk->op2 == IRFL_TAB_META);
	  snap_restoreval(J, T, ex, snapno, rfilt, irs->op2, &tmp);
	  /* NOBARRIER: The table is new (marked white). */
	  setgcref(t->metatable, obj2gco(tabV(&tmp)));
	} else {
	  irk = &T->ir[irk->op2];
	  if (irk->o == IR_KSLOT) irk = &T->ir[irk->op1];
	  lj_ir_kvalue(J->L, &tmp, irk);
	  val = lj_tab_set(J->L, t, &tmp);
	  /* NOBARRIER: The table is new (marked white). */
	  snap_restoreval(J, T, ex, snapno, rfilt, irs->op2, val);
	  if (LJ_SOFTFP && irs+1 < T->ir + T->nins && (irs+1)->o == IR_HIOP) {
	    snap_restoreval(J, T, ex, snapno, rfilt, (irs+1)->op2, &tmp);
	    val->u32.hi = tmp.u32.lo;
	  }
	}
      }
  }
}

/* Restore interpreter state from exit state with the help of a snapshot. */
const BCIns *lj_snap_restore(jit_State *J, void *exptr)
{
  ExitState *ex = (ExitState *)exptr;
  SnapNo snapno = J->exitno;  /* For now, snapno == exitno. */
  GCtrace *T = traceref(J, J->parent);
  SnapShot *snap = &T->snap[snapno];
  MSize n, nent = snap->nent;
  SnapEntry *map = &T->snapmap[snap->mapofs];
  SnapEntry *flinks = &T->snapmap[snap_nextofs(T, snap)-1];
  int32_t ftsz0;
  TValue *frame;
  BloomFilter rfilt = snap_renamefilter(T, snapno);
  const BCIns *pc = snap_pc(map[nent]);
  lua_State *L = J->L;

  /* Set interpreter PC to the next PC to get correct error messages. */
  setcframe_pc(cframe_raw(L->cframe), pc+1);

  /* Make sure the stack is big enough for the slots from the snapshot. */
  if (LJ_UNLIKELY(L->base + snap->topslot >= tvref(L->maxstack))) {
    L->top = curr_topL(L);
    lj_state_growstack(L, snap->topslot - curr_proto(L)->framesize);
  }

  /* Fill stack slots with data from the registers and spill slots. */
  frame = L->base-1;
  ftsz0 = frame_ftsz(frame);  /* Preserve link to previous frame in slot #0. */
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    if (!(sn & SNAP_NORESTORE)) {
      TValue *o = &frame[snap_slot(sn)];
      IRRef ref = snap_ref(sn);
      IRIns *ir = &T->ir[ref];
      if (ir->r == RID_SUNK) {
	MSize j;
	for (j = 0; j < n; j++)
	  if (snap_ref(map[j]) == ref) {  /* De-duplicate sunk allocations. */
	    copyTV(L, o, &frame[snap_slot(map[j])]);
	    goto dupslot;
	  }
	snap_unsink(J, T, ex, snapno, rfilt, ir, o);
      dupslot:
	continue;
      }
      snap_restoreval(J, T, ex, snapno, rfilt, ref, o);
      if (LJ_SOFTFP && (sn & SNAP_SOFTFPNUM) && tvisint(o)) {
	TValue tmp;
	snap_restoreval(J, T, ex, snapno, rfilt, ref+1, &tmp);
	o->u32.hi = tmp.u32.lo;
      } else if ((sn & (SNAP_CONT|SNAP_FRAME))) {
	/* Overwrite tag with frame link. */
	o->fr.tp.ftsz = snap_slot(sn) != 0 ? (int32_t)*flinks-- : ftsz0;
	L->base = o+1;
      }
    }
  }
  lua_assert(map + nent == flinks);

  /* Compute current stack top. */
  switch (bc_op(*pc)) {
  case BC_CALLM: case BC_CALLMT: case BC_RETM: case BC_TSETM:
    L->top = frame + snap->nslots;
    break;
  default:
    L->top = curr_topL(L);
    break;
  }
  return pc;
}

#undef IR
#undef emitir_raw
#undef emitir

#endif
