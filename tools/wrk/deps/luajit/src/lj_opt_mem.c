/*
** Memory access optimizations.
** AA: Alias Analysis using high-level semantic disambiguation.
** FWD: Load Forwarding (L2L) + Store Forwarding (S2L).
** DSE: Dead-Store Elimination.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_opt_mem_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_tab.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_iropt.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)		(&J->cur.ir[(ref)])
#define fins		(&J->fold.ins)
#define fleft		(&J->fold.left)
#define fright		(&J->fold.right)

/*
** Caveat #1: return value is not always a TRef -- only use with tref_ref().
** Caveat #2: FWD relies on active CSE for xREF operands -- see lj_opt_fold().
*/

/* Return values from alias analysis. */
typedef enum {
  ALIAS_NO,	/* The two refs CANNOT alias (exact). */
  ALIAS_MAY,	/* The two refs MAY alias (inexact). */
  ALIAS_MUST	/* The two refs MUST alias (exact). */
} AliasRet;

/* -- ALOAD/HLOAD forwarding and ASTORE/HSTORE elimination ---------------- */

/* Simplified escape analysis: check for intervening stores. */
static AliasRet aa_escape(jit_State *J, IRIns *ir, IRIns *stop)
{
  IRRef ref = (IRRef)(ir - J->cur.ir);  /* The ref that might be stored. */
  for (ir++; ir < stop; ir++)
    if (ir->op2 == ref &&
	(ir->o == IR_ASTORE || ir->o == IR_HSTORE ||
	 ir->o == IR_USTORE || ir->o == IR_FSTORE))
      return ALIAS_MAY;  /* Reference was stored and might alias. */
  return ALIAS_NO;  /* Reference was not stored. */
}

/* Alias analysis for two different table references. */
static AliasRet aa_table(jit_State *J, IRRef ta, IRRef tb)
{
  IRIns *taba = IR(ta), *tabb = IR(tb);
  int newa, newb;
  lua_assert(ta != tb);
  lua_assert(irt_istab(taba->t) && irt_istab(tabb->t));
  /* Disambiguate new allocations. */
  newa = (taba->o == IR_TNEW || taba->o == IR_TDUP);
  newb = (tabb->o == IR_TNEW || tabb->o == IR_TDUP);
  if (newa && newb)
    return ALIAS_NO;  /* Two different allocations never alias. */
  if (newb) {  /* At least one allocation? */
    IRIns *tmp = taba; taba = tabb; tabb = tmp;
  } else if (!newa) {
    return ALIAS_MAY;  /* Anything else: we just don't know. */
  }
  return aa_escape(J, taba, tabb);
}

/* Alias analysis for array and hash access using key-based disambiguation. */
static AliasRet aa_ahref(jit_State *J, IRIns *refa, IRIns *refb)
{
  IRRef ka = refa->op2;
  IRRef kb = refb->op2;
  IRIns *keya, *keyb;
  IRRef ta, tb;
  if (refa == refb)
    return ALIAS_MUST;  /* Shortcut for same refs. */
  keya = IR(ka);
  if (keya->o == IR_KSLOT) { ka = keya->op1; keya = IR(ka); }
  keyb = IR(kb);
  if (keyb->o == IR_KSLOT) { kb = keyb->op1; keyb = IR(kb); }
  ta = (refa->o==IR_HREFK || refa->o==IR_AREF) ? IR(refa->op1)->op1 : refa->op1;
  tb = (refb->o==IR_HREFK || refb->o==IR_AREF) ? IR(refb->op1)->op1 : refb->op1;
  if (ka == kb) {
    /* Same key. Check for same table with different ref (NEWREF vs. HREF). */
    if (ta == tb)
      return ALIAS_MUST;  /* Same key, same table. */
    else
      return aa_table(J, ta, tb);  /* Same key, possibly different table. */
  }
  if (irref_isk(ka) && irref_isk(kb))
    return ALIAS_NO;  /* Different constant keys. */
  if (refa->o == IR_AREF) {
    /* Disambiguate array references based on index arithmetic. */
    int32_t ofsa = 0, ofsb = 0;
    IRRef basea = ka, baseb = kb;
    lua_assert(refb->o == IR_AREF);
    /* Gather base and offset from t[base] or t[base+-ofs]. */
    if (keya->o == IR_ADD && irref_isk(keya->op2)) {
      basea = keya->op1;
      ofsa = IR(keya->op2)->i;
      if (basea == kb && ofsa != 0)
	return ALIAS_NO;  /* t[base+-ofs] vs. t[base]. */
    }
    if (keyb->o == IR_ADD && irref_isk(keyb->op2)) {
      baseb = keyb->op1;
      ofsb = IR(keyb->op2)->i;
      if (ka == baseb && ofsb != 0)
	return ALIAS_NO;  /* t[base] vs. t[base+-ofs]. */
    }
    if (basea == baseb && ofsa != ofsb)
      return ALIAS_NO;  /* t[base+-o1] vs. t[base+-o2] and o1 != o2. */
  } else {
    /* Disambiguate hash references based on the type of their keys. */
    lua_assert((refa->o==IR_HREF || refa->o==IR_HREFK || refa->o==IR_NEWREF) &&
	       (refb->o==IR_HREF || refb->o==IR_HREFK || refb->o==IR_NEWREF));
    if (!irt_sametype(keya->t, keyb->t))
      return ALIAS_NO;  /* Different key types. */
  }
  if (ta == tb)
    return ALIAS_MAY;  /* Same table, cannot disambiguate keys. */
  else
    return aa_table(J, ta, tb);  /* Try to disambiguate tables. */
}

/* Array and hash load forwarding. */
static TRef fwd_ahload(jit_State *J, IRRef xref)
{
  IRIns *xr = IR(xref);
  IRRef lim = xref;  /* Search limit. */
  IRRef ref;

  /* Search for conflicting stores. */
  ref = J->chain[fins->o+IRDELTA_L2S];
  while (ref > xref) {
    IRIns *store = IR(ref);
    switch (aa_ahref(J, xr, IR(store->op1))) {
    case ALIAS_NO:   break;  /* Continue searching. */
    case ALIAS_MAY:  lim = ref; goto cselim;  /* Limit search for load. */
    case ALIAS_MUST: return store->op2;  /* Store forwarding. */
    }
    ref = store->prev;
  }

  /* No conflicting store (yet): const-fold loads from allocations. */
  {
    IRIns *ir = (xr->o == IR_HREFK || xr->o == IR_AREF) ? IR(xr->op1) : xr;
    IRRef tab = ir->op1;
    ir = IR(tab);
    if (ir->o == IR_TNEW || (ir->o == IR_TDUP && irref_isk(xr->op2))) {
      /* A NEWREF with a number key may end up pointing to the array part.
      ** But it's referenced from HSTORE and not found in the ASTORE chain.
      ** For now simply consider this a conflict without forwarding anything.
      */
      if (xr->o == IR_AREF) {
	IRRef ref2 = J->chain[IR_NEWREF];
	while (ref2 > tab) {
	  IRIns *newref = IR(ref2);
	  if (irt_isnum(IR(newref->op2)->t))
	    goto cselim;
	  ref2 = newref->prev;
	}
      }
      /* NEWREF inhibits CSE for HREF, and dependent FLOADs from HREFK/AREF.
      ** But the above search for conflicting stores was limited by xref.
      ** So continue searching, limited by the TNEW/TDUP. Store forwarding
      ** is ok, too. A conflict does NOT limit the search for a matching load.
      */
      while (ref > tab) {
	IRIns *store = IR(ref);
	switch (aa_ahref(J, xr, IR(store->op1))) {
	case ALIAS_NO:   break;  /* Continue searching. */
	case ALIAS_MAY:  goto cselim;  /* Conflicting store. */
	case ALIAS_MUST: return store->op2;  /* Store forwarding. */
	}
	ref = store->prev;
      }
      lua_assert(ir->o != IR_TNEW || irt_isnil(fins->t));
      if (irt_ispri(fins->t)) {
	return TREF_PRI(irt_type(fins->t));
      } else if (irt_isnum(fins->t) || (LJ_DUALNUM && irt_isint(fins->t)) ||
		 irt_isstr(fins->t)) {
	TValue keyv;
	cTValue *tv;
	IRIns *key = IR(xr->op2);
	if (key->o == IR_KSLOT) key = IR(key->op1);
	lj_ir_kvalue(J->L, &keyv, key);
	tv = lj_tab_get(J->L, ir_ktab(IR(ir->op1)), &keyv);
	lua_assert(itype2irt(tv) == irt_type(fins->t));
	if (irt_isnum(fins->t))
	  return lj_ir_knum_u64(J, tv->u64);
	else if (LJ_DUALNUM && irt_isint(fins->t))
	  return lj_ir_kint(J, intV(tv));
	else
	  return lj_ir_kstr(J, strV(tv));
      }
      /* Othwerwise: don't intern as a constant. */
    }
  }

cselim:
  /* Try to find a matching load. Below the conflicting store, if any. */
  ref = J->chain[fins->o];
  while (ref > lim) {
    IRIns *load = IR(ref);
    if (load->op1 == xref)
      return ref;  /* Load forwarding. */
    ref = load->prev;
  }
  return 0;  /* Conflict or no match. */
}

/* Reassociate ALOAD across PHIs to handle t[i-1] forwarding case. */
static TRef fwd_aload_reassoc(jit_State *J)
{
  IRIns *irx = IR(fins->op1);
  IRIns *key = IR(irx->op2);
  if (key->o == IR_ADD && irref_isk(key->op2)) {
    IRIns *add2 = IR(key->op1);
    if (add2->o == IR_ADD && irref_isk(add2->op2) &&
	IR(key->op2)->i == -IR(add2->op2)->i) {
      IRRef ref = J->chain[IR_AREF];
      IRRef lim = add2->op1;
      if (irx->op1 > lim) lim = irx->op1;
      while (ref > lim) {
	IRIns *ir = IR(ref);
	if (ir->op1 == irx->op1 && ir->op2 == add2->op1)
	  return fwd_ahload(J, ref);
	ref = ir->prev;
      }
    }
  }
  return 0;
}

/* ALOAD forwarding. */
TRef LJ_FASTCALL lj_opt_fwd_aload(jit_State *J)
{
  IRRef ref;
  if ((ref = fwd_ahload(J, fins->op1)) ||
      (ref = fwd_aload_reassoc(J)))
    return ref;
  return EMITFOLD;
}

/* HLOAD forwarding. */
TRef LJ_FASTCALL lj_opt_fwd_hload(jit_State *J)
{
  IRRef ref = fwd_ahload(J, fins->op1);
  if (ref)
    return ref;
  return EMITFOLD;
}

/* HREFK forwarding. */
TRef LJ_FASTCALL lj_opt_fwd_hrefk(jit_State *J)
{
  IRRef tab = fleft->op1;
  IRRef ref = J->chain[IR_NEWREF];
  while (ref > tab) {
    IRIns *newref = IR(ref);
    if (tab == newref->op1) {
      if (fright->op1 == newref->op2)
	return ref;  /* Forward from NEWREF. */
      else
	goto docse;
    } else if (aa_table(J, tab, newref->op1) != ALIAS_NO) {
      goto docse;
    }
    ref = newref->prev;
  }
  /* No conflicting NEWREF: key location unchanged for HREFK of TDUP. */
  if (IR(tab)->o == IR_TDUP)
    fins->t.irt &= ~IRT_GUARD;  /* Drop HREFK guard. */
docse:
  return CSEFOLD;
}

/* Check whether HREF of TNEW/TDUP can be folded to niltv. */
int LJ_FASTCALL lj_opt_fwd_href_nokey(jit_State *J)
{
  IRRef lim = fins->op1;  /* Search limit. */
  IRRef ref;

  /* The key for an ASTORE may end up in the hash part after a NEWREF. */
  if (irt_isnum(fright->t) && J->chain[IR_NEWREF] > lim) {
    ref = J->chain[IR_ASTORE];
    while (ref > lim) {
      if (ref < J->chain[IR_NEWREF])
	return 0;  /* Conflict. */
      ref = IR(ref)->prev;
    }
  }

  /* Search for conflicting stores. */
  ref = J->chain[IR_HSTORE];
  while (ref > lim) {
    IRIns *store = IR(ref);
    if (aa_ahref(J, fins, IR(store->op1)) != ALIAS_NO)
      return 0;  /* Conflict. */
    ref = store->prev;
  }

  return 1;  /* No conflict. Can fold to niltv. */
}

/* Check whether there's no aliasing NEWREF for the left operand. */
int LJ_FASTCALL lj_opt_fwd_tptr(jit_State *J, IRRef lim)
{
  IRRef ta = fins->op1;
  IRRef ref = J->chain[IR_NEWREF];
  while (ref > lim) {
    IRIns *newref = IR(ref);
    if (ta == newref->op1 || aa_table(J, ta, newref->op1) != ALIAS_NO)
      return 0;  /* Conflict. */
    ref = newref->prev;
  }
  return 1;  /* No conflict. Can safely FOLD/CSE. */
}

/* ASTORE/HSTORE elimination. */
TRef LJ_FASTCALL lj_opt_dse_ahstore(jit_State *J)
{
  IRRef xref = fins->op1;  /* xREF reference. */
  IRRef val = fins->op2;  /* Stored value reference. */
  IRIns *xr = IR(xref);
  IRRef1 *refp = &J->chain[fins->o];
  IRRef ref = *refp;
  while (ref > xref) {  /* Search for redundant or conflicting stores. */
    IRIns *store = IR(ref);
    switch (aa_ahref(J, xr, IR(store->op1))) {
    case ALIAS_NO:
      break;  /* Continue searching. */
    case ALIAS_MAY:	/* Store to MAYBE the same location. */
      if (store->op2 != val)  /* Conflict if the value is different. */
	goto doemit;
      break;  /* Otherwise continue searching. */
    case ALIAS_MUST:	/* Store to the same location. */
      if (store->op2 == val)  /* Same value: drop the new store. */
	return DROPFOLD;
      /* Different value: try to eliminate the redundant store. */
      if (ref > J->chain[IR_LOOP]) {  /* Quick check to avoid crossing LOOP. */
	IRIns *ir;
	/* Check for any intervening guards (includes conflicting loads). */
	for (ir = IR(J->cur.nins-1); ir > store; ir--)
	  if (irt_isguard(ir->t) || ir->o == IR_CALLL)
	    goto doemit;  /* No elimination possible. */
	/* Remove redundant store from chain and replace with NOP. */
	*refp = store->prev;
	store->o = IR_NOP;
	store->t.irt = IRT_NIL;
	store->op1 = store->op2 = 0;
	store->prev = 0;
	/* Now emit the new store instead. */
      }
      goto doemit;
    }
    ref = *(refp = &store->prev);
  }
doemit:
  return EMITFOLD;  /* Otherwise we have a conflict or simply no match. */
}

/* -- ULOAD forwarding ---------------------------------------------------- */

/* The current alias analysis for upvalues is very simplistic. It only
** disambiguates between the unique upvalues of the same function.
** This is good enough for now, since most upvalues are read-only.
**
** A more precise analysis would be feasible with the help of the parser:
** generate a unique key for every upvalue, even across all prototypes.
** Lacking a realistic use-case, it's unclear whether this is beneficial.
*/
static AliasRet aa_uref(IRIns *refa, IRIns *refb)
{
  if (refa->o != refb->o)
    return ALIAS_NO;  /* Different UREFx type. */
  if (refa->op1 == refb->op1) {  /* Same function. */
    if (refa->op2 == refb->op2)
      return ALIAS_MUST;  /* Same function, same upvalue idx. */
    else
      return ALIAS_NO;  /* Same function, different upvalue idx. */
  } else {  /* Different functions, check disambiguation hash values. */
    if (((refa->op2 ^ refb->op2) & 0xff))
      return ALIAS_NO;  /* Upvalues with different hash values cannot alias. */
    else
      return ALIAS_MAY;  /* No conclusion can be drawn for same hash value. */
  }
}

/* ULOAD forwarding. */
TRef LJ_FASTCALL lj_opt_fwd_uload(jit_State *J)
{
  IRRef uref = fins->op1;
  IRRef lim = uref;  /* Search limit. */
  IRIns *xr = IR(uref);
  IRRef ref;

  /* Search for conflicting stores. */
  ref = J->chain[IR_USTORE];
  while (ref > uref) {
    IRIns *store = IR(ref);
    switch (aa_uref(xr, IR(store->op1))) {
    case ALIAS_NO:   break;  /* Continue searching. */
    case ALIAS_MAY:  lim = ref; goto cselim;  /* Limit search for load. */
    case ALIAS_MUST: return store->op2;  /* Store forwarding. */
    }
    ref = store->prev;
  }

cselim:
  /* Try to find a matching load. Below the conflicting store, if any. */
  return lj_opt_cselim(J, lim);
}

/* USTORE elimination. */
TRef LJ_FASTCALL lj_opt_dse_ustore(jit_State *J)
{
  IRRef xref = fins->op1;  /* xREF reference. */
  IRRef val = fins->op2;  /* Stored value reference. */
  IRIns *xr = IR(xref);
  IRRef1 *refp = &J->chain[IR_USTORE];
  IRRef ref = *refp;
  while (ref > xref) {  /* Search for redundant or conflicting stores. */
    IRIns *store = IR(ref);
    switch (aa_uref(xr, IR(store->op1))) {
    case ALIAS_NO:
      break;  /* Continue searching. */
    case ALIAS_MAY:	/* Store to MAYBE the same location. */
      if (store->op2 != val)  /* Conflict if the value is different. */
	goto doemit;
      break;  /* Otherwise continue searching. */
    case ALIAS_MUST:	/* Store to the same location. */
      if (store->op2 == val)  /* Same value: drop the new store. */
	return DROPFOLD;
      /* Different value: try to eliminate the redundant store. */
      if (ref > J->chain[IR_LOOP]) {  /* Quick check to avoid crossing LOOP. */
	IRIns *ir;
	/* Check for any intervening guards (includes conflicting loads). */
	for (ir = IR(J->cur.nins-1); ir > store; ir--)
	  if (irt_isguard(ir->t))
	    goto doemit;  /* No elimination possible. */
	/* Remove redundant store from chain and replace with NOP. */
	*refp = store->prev;
	store->o = IR_NOP;
	store->t.irt = IRT_NIL;
	store->op1 = store->op2 = 0;
	store->prev = 0;
	if (ref+1 < J->cur.nins &&
	    store[1].o == IR_OBAR && store[1].op1 == xref) {
	  IRRef1 *bp = &J->chain[IR_OBAR];
	  IRIns *obar;
	  for (obar = IR(*bp); *bp > ref+1; obar = IR(*bp))
	    bp = &obar->prev;
	  /* Remove OBAR, too. */
	  *bp = obar->prev;
	  obar->o = IR_NOP;
	  obar->t.irt = IRT_NIL;
	  obar->op1 = obar->op2 = 0;
	  obar->prev = 0;
	}
	/* Now emit the new store instead. */
      }
      goto doemit;
    }
    ref = *(refp = &store->prev);
  }
doemit:
  return EMITFOLD;  /* Otherwise we have a conflict or simply no match. */
}

/* -- FLOAD forwarding and FSTORE elimination ----------------------------- */

/* Alias analysis for field access.
** Field loads are cheap and field stores are rare.
** Simple disambiguation based on field types is good enough.
*/
static AliasRet aa_fref(jit_State *J, IRIns *refa, IRIns *refb)
{
  if (refa->op2 != refb->op2)
    return ALIAS_NO;  /* Different fields. */
  if (refa->op1 == refb->op1)
    return ALIAS_MUST;  /* Same field, same object. */
  else if (refa->op2 >= IRFL_TAB_META && refa->op2 <= IRFL_TAB_NOMM)
    return aa_table(J, refa->op1, refb->op1);  /* Disambiguate tables. */
  else
    return ALIAS_MAY;  /* Same field, possibly different object. */
}

/* Only the loads for mutable fields end up here (see FOLD). */
TRef LJ_FASTCALL lj_opt_fwd_fload(jit_State *J)
{
  IRRef oref = fins->op1;  /* Object reference. */
  IRRef fid = fins->op2;  /* Field ID. */
  IRRef lim = oref;  /* Search limit. */
  IRRef ref;

  /* Search for conflicting stores. */
  ref = J->chain[IR_FSTORE];
  while (ref > oref) {
    IRIns *store = IR(ref);
    switch (aa_fref(J, fins, IR(store->op1))) {
    case ALIAS_NO:   break;  /* Continue searching. */
    case ALIAS_MAY:  lim = ref; goto cselim;  /* Limit search for load. */
    case ALIAS_MUST: return store->op2;  /* Store forwarding. */
    }
    ref = store->prev;
  }

  /* No conflicting store: const-fold field loads from allocations. */
  if (fid == IRFL_TAB_META) {
    IRIns *ir = IR(oref);
    if (ir->o == IR_TNEW || ir->o == IR_TDUP)
      return lj_ir_knull(J, IRT_TAB);
  }

cselim:
  /* Try to find a matching load. Below the conflicting store, if any. */
  return lj_opt_cselim(J, lim);
}

/* FSTORE elimination. */
TRef LJ_FASTCALL lj_opt_dse_fstore(jit_State *J)
{
  IRRef fref = fins->op1;  /* FREF reference. */
  IRRef val = fins->op2;  /* Stored value reference. */
  IRIns *xr = IR(fref);
  IRRef1 *refp = &J->chain[IR_FSTORE];
  IRRef ref = *refp;
  while (ref > fref) {  /* Search for redundant or conflicting stores. */
    IRIns *store = IR(ref);
    switch (aa_fref(J, xr, IR(store->op1))) {
    case ALIAS_NO:
      break;  /* Continue searching. */
    case ALIAS_MAY:
      if (store->op2 != val)  /* Conflict if the value is different. */
	goto doemit;
      break;  /* Otherwise continue searching. */
    case ALIAS_MUST:
      if (store->op2 == val)  /* Same value: drop the new store. */
	return DROPFOLD;
      /* Different value: try to eliminate the redundant store. */
      if (ref > J->chain[IR_LOOP]) {  /* Quick check to avoid crossing LOOP. */
	IRIns *ir;
	/* Check for any intervening guards or conflicting loads. */
	for (ir = IR(J->cur.nins-1); ir > store; ir--)
	  if (irt_isguard(ir->t) || (ir->o == IR_FLOAD && ir->op2 == xr->op2))
	    goto doemit;  /* No elimination possible. */
	/* Remove redundant store from chain and replace with NOP. */
	*refp = store->prev;
	store->o = IR_NOP;
	store->t.irt = IRT_NIL;
	store->op1 = store->op2 = 0;
	store->prev = 0;
	/* Now emit the new store instead. */
      }
      goto doemit;
    }
    ref = *(refp = &store->prev);
  }
doemit:
  return EMITFOLD;  /* Otherwise we have a conflict or simply no match. */
}

/* -- XLOAD forwarding and XSTORE elimination ----------------------------- */

/* Find cdata allocation for a reference (if any). */
static IRIns *aa_findcnew(jit_State *J, IRIns *ir)
{
  while (ir->o == IR_ADD) {
    if (!irref_isk(ir->op1)) {
      IRIns *ir1 = aa_findcnew(J, IR(ir->op1));  /* Left-recursion. */
      if (ir1) return ir1;
    }
    if (irref_isk(ir->op2)) return NULL;
    ir = IR(ir->op2);  /* Flatten right-recursion. */
  }
  return ir->o == IR_CNEW ? ir : NULL;
}

/* Alias analysis for two cdata allocations. */
static AliasRet aa_cnew(jit_State *J, IRIns *refa, IRIns *refb)
{
  IRIns *cnewa = aa_findcnew(J, refa);
  IRIns *cnewb = aa_findcnew(J, refb);
  if (cnewa == cnewb)
    return ALIAS_MAY;  /* Same allocation or neither is an allocation. */
  if (cnewa && cnewb)
    return ALIAS_NO;  /* Two different allocations never alias. */
  if (cnewb) { cnewa = cnewb; refb = refa; }
  return aa_escape(J, cnewa, refb);
}

/* Alias analysis for XLOAD/XSTORE. */
static AliasRet aa_xref(jit_State *J, IRIns *refa, IRIns *xa, IRIns *xb)
{
  ptrdiff_t ofsa = 0, ofsb = 0;
  IRIns *refb = IR(xb->op1);
  IRIns *basea = refa, *baseb = refb;
  if (refa == refb && irt_sametype(xa->t, xb->t))
    return ALIAS_MUST;  /* Shortcut for same refs with identical type. */
  /* Offset-based disambiguation. */
  if (refa->o == IR_ADD && irref_isk(refa->op2)) {
    IRIns *irk = IR(refa->op2);
    basea = IR(refa->op1);
    ofsa = (LJ_64 && irk->o == IR_KINT64) ? (ptrdiff_t)ir_k64(irk)->u64 :
					    (ptrdiff_t)irk->i;
    if (basea == refb && ofsa != 0)
      return ALIAS_NO;  /* base+-ofs vs. base. */
  }
  if (refb->o == IR_ADD && irref_isk(refb->op2)) {
    IRIns *irk = IR(refb->op2);
    baseb = IR(refb->op1);
    ofsb = (LJ_64 && irk->o == IR_KINT64) ? (ptrdiff_t)ir_k64(irk)->u64 :
					    (ptrdiff_t)irk->i;
    if (refa == baseb && ofsb != 0)
      return ALIAS_NO;  /* base vs. base+-ofs. */
  }
  /* This implements (very) strict aliasing rules.
  ** Different types do NOT alias, except for differences in signedness.
  ** Type punning through unions is allowed (but forces a reload).
  */
  if (basea == baseb) {
    ptrdiff_t sza = irt_size(xa->t), szb = irt_size(xb->t);
    if (ofsa == ofsb) {
      if (sza == szb && irt_isfp(xa->t) == irt_isfp(xb->t))
	return ALIAS_MUST;  /* Same-sized, same-kind. May need to convert. */
    } else if (ofsa + sza <= ofsb || ofsb + szb <= ofsa) {
      return ALIAS_NO;  /* Non-overlapping base+-o1 vs. base+-o2. */
    }
    /* NYI: extract, extend or reinterpret bits (int <-> fp). */
    return ALIAS_MAY;  /* Overlapping or type punning: force reload. */
  }
  if (!irt_sametype(xa->t, xb->t) &&
      !(irt_typerange(xa->t, IRT_I8, IRT_U64) &&
	((xa->t.irt - IRT_I8) ^ (xb->t.irt - IRT_I8)) == 1))
    return ALIAS_NO;
  /* NYI: structural disambiguation. */
  return aa_cnew(J, basea, baseb);  /* Try to disambiguate allocations. */
}

/* Return CSEd reference or 0. Caveat: swaps lower ref to the right! */
static IRRef reassoc_trycse(jit_State *J, IROp op, IRRef op1, IRRef op2)
{
  IRRef ref = J->chain[op];
  IRRef lim = op1;
  if (op2 > lim) { lim = op2; op2 = op1; op1 = lim; }
  while (ref > lim) {
    IRIns *ir = IR(ref);
    if (ir->op1 == op1 && ir->op2 == op2)
      return ref;
    ref = ir->prev;
  }
  return 0;
}

/* Reassociate index references. */
static IRRef reassoc_xref(jit_State *J, IRIns *ir)
{
  ptrdiff_t ofs = 0;
  if (ir->o == IR_ADD && irref_isk(ir->op2)) {  /* Get constant offset. */
    IRIns *irk = IR(ir->op2);
    ofs = (LJ_64 && irk->o == IR_KINT64) ? (ptrdiff_t)ir_k64(irk)->u64 :
					   (ptrdiff_t)irk->i;
    ir = IR(ir->op1);
  }
  if (ir->o == IR_ADD) {  /* Add of base + index. */
    /* Index ref > base ref for loop-carried dependences. Only check op1. */
    IRIns *ir2, *ir1 = IR(ir->op1);
    int32_t shift = 0;
    IRRef idxref;
    /* Determine index shifts. Don't bother with IR_MUL here. */
    if (ir1->o == IR_BSHL && irref_isk(ir1->op2))
      shift = IR(ir1->op2)->i;
    else if (ir1->o == IR_ADD && ir1->op1 == ir1->op2)
      shift = 1;
    else
      ir1 = ir;
    ir2 = IR(ir1->op1);
    /* A non-reassociated add. Must be a loop-carried dependence. */
    if (ir2->o == IR_ADD && irt_isint(ir2->t) && irref_isk(ir2->op2))
      ofs += (ptrdiff_t)IR(ir2->op2)->i << shift;
    else
      return 0;
    idxref = ir2->op1;
    /* Try to CSE the reassociated chain. Give up if not found. */
    if (ir1 != ir &&
	!(idxref = reassoc_trycse(J, ir1->o, idxref,
				  ir1->o == IR_BSHL ? ir1->op2 : idxref)))
      return 0;
    if (!(idxref = reassoc_trycse(J, IR_ADD, idxref, ir->op2)))
      return 0;
    if (ofs != 0) {
      IRRef refk = tref_ref(lj_ir_kintp(J, ofs));
      if (!(idxref = reassoc_trycse(J, IR_ADD, idxref, refk)))
	return 0;
    }
    return idxref;  /* Success, found a reassociated index reference. Phew. */
  }
  return 0;  /* Failure. */
}

/* XLOAD forwarding. */
TRef LJ_FASTCALL lj_opt_fwd_xload(jit_State *J)
{
  IRRef xref = fins->op1;
  IRIns *xr = IR(xref);
  IRRef lim = xref;  /* Search limit. */
  IRRef ref;

  if ((fins->op2 & IRXLOAD_READONLY))
    goto cselim;
  if ((fins->op2 & IRXLOAD_VOLATILE))
    goto doemit;

  /* Search for conflicting stores. */
  ref = J->chain[IR_XSTORE];
retry:
  if (J->chain[IR_CALLXS] > lim) lim = J->chain[IR_CALLXS];
  if (J->chain[IR_XBAR] > lim) lim = J->chain[IR_XBAR];
  while (ref > lim) {
    IRIns *store = IR(ref);
    switch (aa_xref(J, xr, fins, store)) {
    case ALIAS_NO:   break;  /* Continue searching. */
    case ALIAS_MAY:  lim = ref; goto cselim;  /* Limit search for load. */
    case ALIAS_MUST:
      /* Emit conversion if the loaded type doesn't match the forwarded type. */
      if (!irt_sametype(fins->t, IR(store->op2)->t)) {
	IRType st = irt_type(fins->t);
	if (st == IRT_I8 || st == IRT_I16) {  /* Trunc + sign-extend. */
	  st |= IRCONV_SEXT;
	} else if (st == IRT_U8 || st == IRT_U16) {  /* Trunc + zero-extend. */
	} else if (st == IRT_INT) {
	  st = irt_type(IR(store->op2)->t);  /* Needs dummy CONV.int.*. */
	} else {  /* I64/U64 are boxed, U32 is hidden behind a CONV.num.u32. */
	  goto store_fwd;
	}
	fins->ot = IRTI(IR_CONV);
	fins->op1 = store->op2;
	fins->op2 = (IRT_INT<<5)|st;
	return RETRYFOLD;
      }
    store_fwd:
      return store->op2;  /* Store forwarding. */
    }
    ref = store->prev;
  }

cselim:
  /* Try to find a matching load. Below the conflicting store, if any. */
  ref = J->chain[IR_XLOAD];
  while (ref > lim) {
    /* CSE for XLOAD depends on the type, but not on the IRXLOAD_* flags. */
    if (IR(ref)->op1 == xref && irt_sametype(IR(ref)->t, fins->t))
      return ref;
    ref = IR(ref)->prev;
  }

  /* Reassociate XLOAD across PHIs to handle a[i-1] forwarding case. */
  if (!(fins->op2 & IRXLOAD_READONLY) && J->chain[IR_LOOP] &&
      xref == fins->op1 && (xref = reassoc_xref(J, xr)) != 0) {
    ref = J->chain[IR_XSTORE];
    while (ref > lim)  /* Skip stores that have already been checked. */
      ref = IR(ref)->prev;
    lim = xref;
    xr = IR(xref);
    goto retry;  /* Retry with the reassociated reference. */
  }
doemit:
  return EMITFOLD;
}

/* XSTORE elimination. */
TRef LJ_FASTCALL lj_opt_dse_xstore(jit_State *J)
{
  IRRef xref = fins->op1;
  IRIns *xr = IR(xref);
  IRRef lim = xref;  /* Search limit. */
  IRRef val = fins->op2;  /* Stored value reference. */
  IRRef1 *refp = &J->chain[IR_XSTORE];
  IRRef ref = *refp;
  if (J->chain[IR_CALLXS] > lim) lim = J->chain[IR_CALLXS];
  if (J->chain[IR_XBAR] > lim) lim = J->chain[IR_XBAR];
  while (ref > lim) {  /* Search for redundant or conflicting stores. */
    IRIns *store = IR(ref);
    switch (aa_xref(J, xr, fins, store)) {
    case ALIAS_NO:
      break;  /* Continue searching. */
    case ALIAS_MAY:
      if (store->op2 != val)  /* Conflict if the value is different. */
	goto doemit;
      break;  /* Otherwise continue searching. */
    case ALIAS_MUST:
      if (store->op2 == val)  /* Same value: drop the new store. */
	return DROPFOLD;
      /* Different value: try to eliminate the redundant store. */
      if (ref > J->chain[IR_LOOP]) {  /* Quick check to avoid crossing LOOP. */
	IRIns *ir;
	/* Check for any intervening guards or any XLOADs (no AA performed). */
	for (ir = IR(J->cur.nins-1); ir > store; ir--)
	  if (irt_isguard(ir->t) || ir->o == IR_XLOAD)
	    goto doemit;  /* No elimination possible. */
	/* Remove redundant store from chain and replace with NOP. */
	*refp = store->prev;
	store->o = IR_NOP;
	store->t.irt = IRT_NIL;
	store->op1 = store->op2 = 0;
	store->prev = 0;
	/* Now emit the new store instead. */
      }
      goto doemit;
    }
    ref = *(refp = &store->prev);
  }
doemit:
  return EMITFOLD;  /* Otherwise we have a conflict or simply no match. */
}

/* -- Forwarding of lj_tab_len -------------------------------------------- */

/* This is rather simplistic right now, but better than nothing. */
TRef LJ_FASTCALL lj_opt_fwd_tab_len(jit_State *J)
{
  IRRef tab = fins->op1;  /* Table reference. */
  IRRef lim = tab;  /* Search limit. */
  IRRef ref;

  /* Any ASTORE is a conflict and limits the search. */
  if (J->chain[IR_ASTORE] > lim) lim = J->chain[IR_ASTORE];

  /* Search for conflicting HSTORE with numeric key. */
  ref = J->chain[IR_HSTORE];
  while (ref > lim) {
    IRIns *store = IR(ref);
    IRIns *href = IR(store->op1);
    IRIns *key = IR(href->op2);
    if (irt_isnum(key->o == IR_KSLOT ? IR(key->op1)->t : key->t)) {
      lim = ref;  /* Conflicting store found, limits search for TLEN. */
      break;
    }
    ref = store->prev;
  }

  /* Try to find a matching load. Below the conflicting store, if any. */
  return lj_opt_cselim(J, lim);
}

/* -- ASTORE/HSTORE previous type analysis -------------------------------- */

/* Check whether the previous value for a table store is non-nil.
** This can be derived either from a previous store or from a previous
** load (because all loads from tables perform a type check).
**
** The result of the analysis can be used to avoid the metatable check
** and the guard against HREF returning niltv. Both of these are cheap,
** so let's not spend too much effort on the analysis.
**
** A result of 1 is exact: previous value CANNOT be nil.
** A result of 0 is inexact: previous value MAY be nil.
*/
int lj_opt_fwd_wasnonnil(jit_State *J, IROpT loadop, IRRef xref)
{
  /* First check stores. */
  IRRef ref = J->chain[loadop+IRDELTA_L2S];
  while (ref > xref) {
    IRIns *store = IR(ref);
    if (store->op1 == xref) {  /* Same xREF. */
      /* A nil store MAY alias, but a non-nil store MUST alias. */
      return !irt_isnil(store->t);
    } else if (irt_isnil(store->t)) {  /* Must check any nil store. */
      IRRef skref = IR(store->op1)->op2;
      IRRef xkref = IR(xref)->op2;
      /* Same key type MAY alias. Need ALOAD check due to multiple int types. */
      if (loadop == IR_ALOAD || irt_sametype(IR(skref)->t, IR(xkref)->t)) {
	if (skref == xkref || !irref_isk(skref) || !irref_isk(xkref))
	  return 0;  /* A nil store with same const key or var key MAY alias. */
	/* Different const keys CANNOT alias. */
      }  /* Different key types CANNOT alias. */
    }  /* Other non-nil stores MAY alias. */
    ref = store->prev;
  }

  /* Check loads since nothing could be derived from stores. */
  ref = J->chain[loadop];
  while (ref > xref) {
    IRIns *load = IR(ref);
    if (load->op1 == xref) {  /* Same xREF. */
      /* A nil load MAY alias, but a non-nil load MUST alias. */
      return !irt_isnil(load->t);
    }  /* Other non-nil loads MAY alias. */
    ref = load->prev;
  }
  return 0;  /* Nothing derived at all, previous value MAY be nil. */
}

/* ------------------------------------------------------------------------ */

#undef IR
#undef fins
#undef fleft
#undef fright

#endif
