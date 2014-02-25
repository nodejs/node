/*
** IR assembler (SSA IR -> machine code).
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_asm_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_gc.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_frame.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_mcode.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_snap.h"
#include "lj_asm.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_target.h"

#ifdef LUA_USE_ASSERT
#include <stdio.h>
#endif

/* -- Assembler state and common macros ----------------------------------- */

/* Assembler state. */
typedef struct ASMState {
  RegCost cost[RID_MAX];  /* Reference and blended allocation cost for regs. */

  MCode *mcp;		/* Current MCode pointer (grows down). */
  MCode *mclim;		/* Lower limit for MCode memory + red zone. */
#ifdef LUA_USE_ASSERT
  MCode *mcp_prev;	/* Red zone overflow check. */
#endif

  IRIns *ir;		/* Copy of pointer to IR instructions/constants. */
  jit_State *J;		/* JIT compiler state. */

#if LJ_TARGET_X86ORX64
  x86ModRM mrm;		/* Fused x86 address operand. */
#endif

  RegSet freeset;	/* Set of free registers. */
  RegSet modset;	/* Set of registers modified inside the loop. */
  RegSet weakset;	/* Set of weakly referenced registers. */
  RegSet phiset;	/* Set of PHI registers. */

  uint32_t flags;	/* Copy of JIT compiler flags. */
  int loopinv;		/* Loop branch inversion (0:no, 1:yes, 2:yes+CC_P). */

  int32_t evenspill;	/* Next even spill slot. */
  int32_t oddspill;	/* Next odd spill slot (or 0). */

  IRRef curins;		/* Reference of current instruction. */
  IRRef stopins;	/* Stop assembly before hitting this instruction. */
  IRRef orignins;	/* Original T->nins. */

  IRRef snapref;	/* Current snapshot is active after this reference. */
  IRRef snaprename;	/* Rename highwater mark for snapshot check. */
  SnapNo snapno;	/* Current snapshot number. */
  SnapNo loopsnapno;	/* Loop snapshot number. */

  IRRef fuseref;	/* Fusion limit (loopref, 0 or FUSE_DISABLED). */
  IRRef sectref;	/* Section base reference (loopref or 0). */
  IRRef loopref;	/* Reference of LOOP instruction (or 0). */

  BCReg topslot;	/* Number of slots for stack check (unless 0). */
  int32_t gcsteps;	/* Accumulated number of GC steps (per section). */

  GCtrace *T;		/* Trace to assemble. */
  GCtrace *parent;	/* Parent trace (or NULL). */

  MCode *mcbot;		/* Bottom of reserved MCode. */
  MCode *mctop;		/* Top of generated MCode. */
  MCode *mcloop;	/* Pointer to loop MCode (or NULL). */
  MCode *invmcp;	/* Points to invertible loop branch (or NULL). */
  MCode *flagmcp;	/* Pending opportunity to merge flag setting ins. */
  MCode *realign;	/* Realign loop if not NULL. */

#ifdef RID_NUM_KREF
  int32_t krefk[RID_NUM_KREF];
#endif
  IRRef1 phireg[RID_MAX];  /* PHI register references. */
  uint16_t parentmap[LJ_MAX_JSLOTS];  /* Parent instruction to RegSP map. */
} ASMState;

#define IR(ref)			(&as->ir[(ref)])

#define ASMREF_TMP1		REF_TRUE	/* Temp. register. */
#define ASMREF_TMP2		REF_FALSE	/* Temp. register. */
#define ASMREF_L		REF_NIL		/* Stores register for L. */

/* Check for variant to invariant references. */
#define iscrossref(as, ref)	((ref) < as->sectref)

/* Inhibit memory op fusion from variant to invariant references. */
#define FUSE_DISABLED		(~(IRRef)0)
#define mayfuse(as, ref)	((ref) > as->fuseref)
#define neverfuse(as)		(as->fuseref == FUSE_DISABLED)
#define canfuse(as, ir)		(!neverfuse(as) && !irt_isphi((ir)->t))
#define opisfusableload(o) \
  ((o) == IR_ALOAD || (o) == IR_HLOAD || (o) == IR_ULOAD || \
   (o) == IR_FLOAD || (o) == IR_XLOAD || (o) == IR_SLOAD || (o) == IR_VLOAD)

/* Sparse limit checks using a red zone before the actual limit. */
#define MCLIM_REDZONE	64

static LJ_NORET LJ_NOINLINE void asm_mclimit(ASMState *as)
{
  lj_mcode_limiterr(as->J, (size_t)(as->mctop - as->mcp + 4*MCLIM_REDZONE));
}

static LJ_AINLINE void checkmclim(ASMState *as)
{
#ifdef LUA_USE_ASSERT
  if (as->mcp + MCLIM_REDZONE < as->mcp_prev) {
    IRIns *ir = IR(as->curins+1);
    fprintf(stderr, "RED ZONE OVERFLOW: %p IR %04d  %02d %04d %04d\n", as->mcp,
	    as->curins+1-REF_BIAS, ir->o, ir->op1-REF_BIAS, ir->op2-REF_BIAS);
    lua_assert(0);
  }
#endif
  if (LJ_UNLIKELY(as->mcp < as->mclim)) asm_mclimit(as);
#ifdef LUA_USE_ASSERT
  as->mcp_prev = as->mcp;
#endif
}

#ifdef RID_NUM_KREF
#define ra_iskref(ref)		((ref) < RID_NUM_KREF)
#define ra_krefreg(ref)		((Reg)(RID_MIN_KREF + (Reg)(ref)))
#define ra_krefk(as, ref)	(as->krefk[(ref)])

static LJ_AINLINE void ra_setkref(ASMState *as, Reg r, int32_t k)
{
  IRRef ref = (IRRef)(r - RID_MIN_KREF);
  as->krefk[ref] = k;
  as->cost[r] = REGCOST(ref, ref);
}

#else
#define ra_iskref(ref)		0
#define ra_krefreg(ref)		RID_MIN_GPR
#define ra_krefk(as, ref)	0
#endif

/* Arch-specific field offsets. */
static const uint8_t field_ofs[IRFL__MAX+1] = {
#define FLOFS(name, ofs)	(uint8_t)(ofs),
IRFLDEF(FLOFS)
#undef FLOFS
  0
};

/* -- Target-specific instruction emitter --------------------------------- */

#if LJ_TARGET_X86ORX64
#include "lj_emit_x86.h"
#elif LJ_TARGET_ARM
#include "lj_emit_arm.h"
#elif LJ_TARGET_PPC
#include "lj_emit_ppc.h"
#elif LJ_TARGET_MIPS
#include "lj_emit_mips.h"
#else
#error "Missing instruction emitter for target CPU"
#endif

/* -- Register allocator debugging ---------------------------------------- */

/* #define LUAJIT_DEBUG_RA */

#ifdef LUAJIT_DEBUG_RA

#include <stdio.h>
#include <stdarg.h>

#define RIDNAME(name)	#name,
static const char *const ra_regname[] = {
  GPRDEF(RIDNAME)
  FPRDEF(RIDNAME)
  VRIDDEF(RIDNAME)
  NULL
};
#undef RIDNAME

static char ra_dbg_buf[65536];
static char *ra_dbg_p;
static char *ra_dbg_merge;
static MCode *ra_dbg_mcp;

static void ra_dstart(void)
{
  ra_dbg_p = ra_dbg_buf;
  ra_dbg_merge = NULL;
  ra_dbg_mcp = NULL;
}

static void ra_dflush(void)
{
  fwrite(ra_dbg_buf, 1, (size_t)(ra_dbg_p-ra_dbg_buf), stdout);
  ra_dstart();
}

static void ra_dprintf(ASMState *as, const char *fmt, ...)
{
  char *p;
  va_list argp;
  va_start(argp, fmt);
  p = ra_dbg_mcp == as->mcp ? ra_dbg_merge : ra_dbg_p;
  ra_dbg_mcp = NULL;
  p += sprintf(p, "%08x  \e[36m%04d ", (uintptr_t)as->mcp, as->curins-REF_BIAS);
  for (;;) {
    const char *e = strchr(fmt, '$');
    if (e == NULL) break;
    memcpy(p, fmt, (size_t)(e-fmt));
    p += e-fmt;
    if (e[1] == 'r') {
      Reg r = va_arg(argp, Reg) & RID_MASK;
      if (r <= RID_MAX) {
	const char *q;
	for (q = ra_regname[r]; *q; q++)
	  *p++ = *q >= 'A' && *q <= 'Z' ? *q + 0x20 : *q;
      } else {
	*p++ = '?';
	lua_assert(0);
      }
    } else if (e[1] == 'f' || e[1] == 'i') {
      IRRef ref;
      if (e[1] == 'f')
	ref = va_arg(argp, IRRef);
      else
	ref = va_arg(argp, IRIns *) - as->ir;
      if (ref >= REF_BIAS)
	p += sprintf(p, "%04d", ref - REF_BIAS);
      else
	p += sprintf(p, "K%03d", REF_BIAS - ref);
    } else if (e[1] == 's') {
      uint32_t slot = va_arg(argp, uint32_t);
      p += sprintf(p, "[sp+0x%x]", sps_scale(slot));
    } else if (e[1] == 'x') {
      p += sprintf(p, "%08x", va_arg(argp, int32_t));
    } else {
      lua_assert(0);
    }
    fmt = e+2;
  }
  va_end(argp);
  while (*fmt)
    *p++ = *fmt++;
  *p++ = '\e'; *p++ = '['; *p++ = 'm'; *p++ = '\n';
  if (p > ra_dbg_buf+sizeof(ra_dbg_buf)-256) {
    fwrite(ra_dbg_buf, 1, (size_t)(p-ra_dbg_buf), stdout);
    p = ra_dbg_buf;
  }
  ra_dbg_p = p;
}

#define RA_DBG_START()	ra_dstart()
#define RA_DBG_FLUSH()	ra_dflush()
#define RA_DBG_REF() \
  do { char *_p = ra_dbg_p; ra_dprintf(as, ""); \
       ra_dbg_merge = _p; ra_dbg_mcp = as->mcp; } while (0)
#define RA_DBGX(x)	ra_dprintf x

#else
#define RA_DBG_START()	((void)0)
#define RA_DBG_FLUSH()	((void)0)
#define RA_DBG_REF()	((void)0)
#define RA_DBGX(x)	((void)0)
#endif

/* -- Register allocator -------------------------------------------------- */

#define ra_free(as, r)		rset_set(as->freeset, (r))
#define ra_modified(as, r)	rset_set(as->modset, (r))
#define ra_weak(as, r)		rset_set(as->weakset, (r))
#define ra_noweak(as, r)	rset_clear(as->weakset, (r))

#define ra_used(ir)		(ra_hasreg((ir)->r) || ra_hasspill((ir)->s))

/* Setup register allocator. */
static void ra_setup(ASMState *as)
{
  Reg r;
  /* Initially all regs (except the stack pointer) are free for use. */
  as->freeset = RSET_INIT;
  as->modset = RSET_EMPTY;
  as->weakset = RSET_EMPTY;
  as->phiset = RSET_EMPTY;
  memset(as->phireg, 0, sizeof(as->phireg));
  for (r = RID_MIN_GPR; r < RID_MAX; r++)
    as->cost[r] = REGCOST(~0u, 0u);
}

/* Rematerialize constants. */
static Reg ra_rematk(ASMState *as, IRRef ref)
{
  IRIns *ir;
  Reg r;
  if (ra_iskref(ref)) {
    r = ra_krefreg(ref);
    lua_assert(!rset_test(as->freeset, r));
    ra_free(as, r);
    ra_modified(as, r);
    emit_loadi(as, r, ra_krefk(as, ref));
    return r;
  }
  ir = IR(ref);
  r = ir->r;
  lua_assert(ra_hasreg(r) && !ra_hasspill(ir->s));
  ra_free(as, r);
  ra_modified(as, r);
  ir->r = RID_INIT;  /* Do not keep any hint. */
  RA_DBGX((as, "remat     $i $r", ir, r));
#if !LJ_SOFTFP
  if (ir->o == IR_KNUM) {
    emit_loadn(as, r, ir_knum(ir));
  } else
#endif
  if (emit_canremat(REF_BASE) && ir->o == IR_BASE) {
    ra_sethint(ir->r, RID_BASE);  /* Restore BASE register hint. */
    emit_getgl(as, r, jit_base);
  } else if (emit_canremat(ASMREF_L) && ir->o == IR_KPRI) {
    lua_assert(irt_isnil(ir->t));  /* REF_NIL stores ASMREF_L register. */
    emit_getgl(as, r, jit_L);
#if LJ_64
  } else if (ir->o == IR_KINT64) {
    emit_loadu64(as, r, ir_kint64(ir)->u64);
#endif
  } else {
    lua_assert(ir->o == IR_KINT || ir->o == IR_KGC ||
	       ir->o == IR_KPTR || ir->o == IR_KKPTR || ir->o == IR_KNULL);
    emit_loadi(as, r, ir->i);
  }
  return r;
}

/* Force a spill. Allocate a new spill slot if needed. */
static int32_t ra_spill(ASMState *as, IRIns *ir)
{
  int32_t slot = ir->s;
  if (!ra_hasspill(slot)) {
    if (irt_is64(ir->t)) {
      slot = as->evenspill;
      as->evenspill += 2;
    } else if (as->oddspill) {
      slot = as->oddspill;
      as->oddspill = 0;
    } else {
      slot = as->evenspill;
      as->oddspill = slot+1;
      as->evenspill += 2;
    }
    if (as->evenspill > 256)
      lj_trace_err(as->J, LJ_TRERR_SPILLOV);
    ir->s = (uint8_t)slot;
  }
  return sps_scale(slot);
}

/* Release the temporarily allocated register in ASMREF_TMP1/ASMREF_TMP2. */
static Reg ra_releasetmp(ASMState *as, IRRef ref)
{
  IRIns *ir = IR(ref);
  Reg r = ir->r;
  lua_assert(ra_hasreg(r) && !ra_hasspill(ir->s));
  ra_free(as, r);
  ra_modified(as, r);
  ir->r = RID_INIT;
  return r;
}

/* Restore a register (marked as free). Rematerialize or force a spill. */
static Reg ra_restore(ASMState *as, IRRef ref)
{
  if (emit_canremat(ref)) {
    return ra_rematk(as, ref);
  } else {
    IRIns *ir = IR(ref);
    int32_t ofs = ra_spill(as, ir);  /* Force a spill slot. */
    Reg r = ir->r;
    lua_assert(ra_hasreg(r));
    ra_sethint(ir->r, r);  /* Keep hint. */
    ra_free(as, r);
    if (!rset_test(as->weakset, r)) {  /* Only restore non-weak references. */
      ra_modified(as, r);
      RA_DBGX((as, "restore   $i $r", ir, r));
      emit_spload(as, ir, r, ofs);
    }
    return r;
  }
}

/* Save a register to a spill slot. */
static void ra_save(ASMState *as, IRIns *ir, Reg r)
{
  RA_DBGX((as, "save      $i $r", ir, r));
  emit_spstore(as, ir, r, sps_scale(ir->s));
}

#define MINCOST(name) \
  if (rset_test(RSET_ALL, RID_##name) && \
      LJ_LIKELY(allow&RID2RSET(RID_##name)) && as->cost[RID_##name] < cost) \
    cost = as->cost[RID_##name];

/* Evict the register with the lowest cost, forcing a restore. */
static Reg ra_evict(ASMState *as, RegSet allow)
{
  IRRef ref;
  RegCost cost = ~(RegCost)0;
  lua_assert(allow != RSET_EMPTY);
  if (RID_NUM_FPR == 0 || allow < RID2RSET(RID_MAX_GPR)) {
    GPRDEF(MINCOST)
  } else {
    FPRDEF(MINCOST)
  }
  ref = regcost_ref(cost);
  lua_assert(ra_iskref(ref) || (ref >= as->T->nk && ref < as->T->nins));
  /* Preferably pick any weak ref instead of a non-weak, non-const ref. */
  if (!irref_isk(ref) && (as->weakset & allow)) {
    IRIns *ir = IR(ref);
    if (!rset_test(as->weakset, ir->r))
      ref = regcost_ref(as->cost[rset_pickbot((as->weakset & allow))]);
  }
  return ra_restore(as, ref);
}

/* Pick any register (marked as free). Evict on-demand. */
static Reg ra_pick(ASMState *as, RegSet allow)
{
  RegSet pick = as->freeset & allow;
  if (!pick)
    return ra_evict(as, allow);
  else
    return rset_picktop(pick);
}

/* Get a scratch register (marked as free). */
static Reg ra_scratch(ASMState *as, RegSet allow)
{
  Reg r = ra_pick(as, allow);
  ra_modified(as, r);
  RA_DBGX((as, "scratch        $r", r));
  return r;
}

/* Evict all registers from a set (if not free). */
static void ra_evictset(ASMState *as, RegSet drop)
{
  RegSet work;
  as->modset |= drop;
#if !LJ_SOFTFP
  work = (drop & ~as->freeset) & RSET_FPR;
  while (work) {
    Reg r = rset_pickbot(work);
    ra_restore(as, regcost_ref(as->cost[r]));
    rset_clear(work, r);
    checkmclim(as);
  }
#endif
  work = (drop & ~as->freeset);
  while (work) {
    Reg r = rset_pickbot(work);
    ra_restore(as, regcost_ref(as->cost[r]));
    rset_clear(work, r);
    checkmclim(as);
  }
}

/* Evict (rematerialize) all registers allocated to constants. */
static void ra_evictk(ASMState *as)
{
  RegSet work;
#if !LJ_SOFTFP
  work = ~as->freeset & RSET_FPR;
  while (work) {
    Reg r = rset_pickbot(work);
    IRRef ref = regcost_ref(as->cost[r]);
    if (emit_canremat(ref) && irref_isk(ref)) {
      ra_rematk(as, ref);
      checkmclim(as);
    }
    rset_clear(work, r);
  }
#endif
  work = ~as->freeset & RSET_GPR;
  while (work) {
    Reg r = rset_pickbot(work);
    IRRef ref = regcost_ref(as->cost[r]);
    if (emit_canremat(ref) && irref_isk(ref)) {
      ra_rematk(as, ref);
      checkmclim(as);
    }
    rset_clear(work, r);
  }
}

#ifdef RID_NUM_KREF
/* Allocate a register for a constant. */
static Reg ra_allock(ASMState *as, int32_t k, RegSet allow)
{
  /* First try to find a register which already holds the same constant. */
  RegSet pick, work = ~as->freeset & RSET_GPR;
  Reg r;
  while (work) {
    IRRef ref;
    r = rset_pickbot(work);
    ref = regcost_ref(as->cost[r]);
    if (ref < ASMREF_L &&
	k == (ra_iskref(ref) ? ra_krefk(as, ref) : IR(ref)->i))
      return r;
    rset_clear(work, r);
  }
  pick = as->freeset & allow;
  if (pick) {
    /* Constants should preferably get unmodified registers. */
    if ((pick & ~as->modset))
      pick &= ~as->modset;
    r = rset_pickbot(pick);  /* Reduce conflicts with inverse allocation. */
  } else {
    r = ra_evict(as, allow);
  }
  RA_DBGX((as, "allock    $x $r", k, r));
  ra_setkref(as, r, k);
  rset_clear(as->freeset, r);
  ra_noweak(as, r);
  return r;
}

/* Allocate a specific register for a constant. */
static void ra_allockreg(ASMState *as, int32_t k, Reg r)
{
  Reg kr = ra_allock(as, k, RID2RSET(r));
  if (kr != r) {
    IRIns irdummy;
    irdummy.t.irt = IRT_INT;
    ra_scratch(as, RID2RSET(r));
    emit_movrr(as, &irdummy, r, kr);
  }
}
#else
#define ra_allockreg(as, k, r)		emit_loadi(as, (r), (k))
#endif

/* Allocate a register for ref from the allowed set of registers.
** Note: this function assumes the ref does NOT have a register yet!
** Picks an optimal register, sets the cost and marks the register as non-free.
*/
static Reg ra_allocref(ASMState *as, IRRef ref, RegSet allow)
{
  IRIns *ir = IR(ref);
  RegSet pick = as->freeset & allow;
  Reg r;
  lua_assert(ra_noreg(ir->r));
  if (pick) {
    /* First check register hint from propagation or PHI. */
    if (ra_hashint(ir->r)) {
      r = ra_gethint(ir->r);
      if (rset_test(pick, r))  /* Use hint register if possible. */
	goto found;
      /* Rematerialization is cheaper than missing a hint. */
      if (rset_test(allow, r) && emit_canremat(regcost_ref(as->cost[r]))) {
	ra_rematk(as, regcost_ref(as->cost[r]));
	goto found;
      }
      RA_DBGX((as, "hintmiss  $f $r", ref, r));
    }
    /* Invariants should preferably get unmodified registers. */
    if (ref < as->loopref && !irt_isphi(ir->t)) {
      if ((pick & ~as->modset))
	pick &= ~as->modset;
      r = rset_pickbot(pick);  /* Reduce conflicts with inverse allocation. */
    } else {
      /* We've got plenty of regs, so get callee-save regs if possible. */
      if (RID_NUM_GPR > 8 && (pick & ~RSET_SCRATCH))
	pick &= ~RSET_SCRATCH;
      r = rset_picktop(pick);
    }
  } else {
    r = ra_evict(as, allow);
  }
found:
  RA_DBGX((as, "alloc     $f $r", ref, r));
  ir->r = (uint8_t)r;
  rset_clear(as->freeset, r);
  ra_noweak(as, r);
  as->cost[r] = REGCOST_REF_T(ref, irt_t(ir->t));
  return r;
}

/* Allocate a register on-demand. */
static Reg ra_alloc1(ASMState *as, IRRef ref, RegSet allow)
{
  Reg r = IR(ref)->r;
  /* Note: allow is ignored if the register is already allocated. */
  if (ra_noreg(r)) r = ra_allocref(as, ref, allow);
  ra_noweak(as, r);
  return r;
}

/* Rename register allocation and emit move. */
static void ra_rename(ASMState *as, Reg down, Reg up)
{
  IRRef ren, ref = regcost_ref(as->cost[up] = as->cost[down]);
  IRIns *ir = IR(ref);
  ir->r = (uint8_t)up;
  as->cost[down] = 0;
  lua_assert((down < RID_MAX_GPR) == (up < RID_MAX_GPR));
  lua_assert(!rset_test(as->freeset, down) && rset_test(as->freeset, up));
  ra_free(as, down);  /* 'down' is free ... */
  ra_modified(as, down);
  rset_clear(as->freeset, up);  /* ... and 'up' is now allocated. */
  ra_noweak(as, up);
  RA_DBGX((as, "rename    $f $r $r", regcost_ref(as->cost[up]), down, up));
  emit_movrr(as, ir, down, up);  /* Backwards codegen needs inverse move. */
  if (!ra_hasspill(IR(ref)->s)) {  /* Add the rename to the IR. */
    lj_ir_set(as->J, IRT(IR_RENAME, IRT_NIL), ref, as->snapno);
    ren = tref_ref(lj_ir_emit(as->J));
    as->ir = as->T->ir;  /* The IR may have been reallocated. */
    IR(ren)->r = (uint8_t)down;
    IR(ren)->s = SPS_NONE;
  }
}

/* Pick a destination register (marked as free).
** Caveat: allow is ignored if there's already a destination register.
** Use ra_destreg() to get a specific register.
*/
static Reg ra_dest(ASMState *as, IRIns *ir, RegSet allow)
{
  Reg dest = ir->r;
  if (ra_hasreg(dest)) {
    ra_free(as, dest);
    ra_modified(as, dest);
  } else {
    if (ra_hashint(dest) && rset_test((as->freeset&allow), ra_gethint(dest))) {
      dest = ra_gethint(dest);
      ra_modified(as, dest);
      RA_DBGX((as, "dest           $r", dest));
    } else {
      dest = ra_scratch(as, allow);
    }
    ir->r = dest;
  }
  if (LJ_UNLIKELY(ra_hasspill(ir->s))) ra_save(as, ir, dest);
  return dest;
}

/* Force a specific destination register (marked as free). */
static void ra_destreg(ASMState *as, IRIns *ir, Reg r)
{
  Reg dest = ra_dest(as, ir, RID2RSET(r));
  if (dest != r) {
    lua_assert(rset_test(as->freeset, r));
    ra_modified(as, r);
    emit_movrr(as, ir, dest, r);
  }
}

#if LJ_TARGET_X86ORX64
/* Propagate dest register to left reference. Emit moves as needed.
** This is a required fixup step for all 2-operand machine instructions.
*/
static void ra_left(ASMState *as, Reg dest, IRRef lref)
{
  IRIns *ir = IR(lref);
  Reg left = ir->r;
  if (ra_noreg(left)) {
    if (irref_isk(lref)) {
      if (ir->o == IR_KNUM) {
	cTValue *tv = ir_knum(ir);
	/* FP remat needs a load except for +0. Still better than eviction. */
	if (tvispzero(tv) || !(as->freeset & RSET_FPR)) {
	  emit_loadn(as, dest, tv);
	  return;
	}
#if LJ_64
      } else if (ir->o == IR_KINT64) {
	emit_loadu64(as, dest, ir_kint64(ir)->u64);
	return;
#endif
      } else {
	lua_assert(ir->o == IR_KINT || ir->o == IR_KGC ||
		   ir->o == IR_KPTR || ir->o == IR_KKPTR || ir->o == IR_KNULL);
	emit_loadi(as, dest, ir->i);
	return;
      }
    }
    if (!ra_hashint(left) && !iscrossref(as, lref))
      ra_sethint(ir->r, dest);  /* Propagate register hint. */
    left = ra_allocref(as, lref, dest < RID_MAX_GPR ? RSET_GPR : RSET_FPR);
  }
  ra_noweak(as, left);
  /* Move needed for true 3-operand instruction: y=a+b ==> y=a; y+=b. */
  if (dest != left) {
    /* Use register renaming if dest is the PHI reg. */
    if (irt_isphi(ir->t) && as->phireg[dest] == lref) {
      ra_modified(as, left);
      ra_rename(as, left, dest);
    } else {
      emit_movrr(as, ir, dest, left);
    }
  }
}
#else
/* Similar to ra_left, except we override any hints. */
static void ra_leftov(ASMState *as, Reg dest, IRRef lref)
{
  IRIns *ir = IR(lref);
  Reg left = ir->r;
  if (ra_noreg(left)) {
    ra_sethint(ir->r, dest);  /* Propagate register hint. */
    left = ra_allocref(as, lref,
		       (LJ_SOFTFP || dest < RID_MAX_GPR) ? RSET_GPR : RSET_FPR);
  }
  ra_noweak(as, left);
  if (dest != left) {
    /* Use register renaming if dest is the PHI reg. */
    if (irt_isphi(ir->t) && as->phireg[dest] == lref) {
      ra_modified(as, left);
      ra_rename(as, left, dest);
    } else {
      emit_movrr(as, ir, dest, left);
    }
  }
}
#endif

#if !LJ_64
/* Force a RID_RETLO/RID_RETHI destination register pair (marked as free). */
static void ra_destpair(ASMState *as, IRIns *ir)
{
  Reg destlo = ir->r, desthi = (ir+1)->r;
  /* First spill unrelated refs blocking the destination registers. */
  if (!rset_test(as->freeset, RID_RETLO) &&
      destlo != RID_RETLO && desthi != RID_RETLO)
    ra_restore(as, regcost_ref(as->cost[RID_RETLO]));
  if (!rset_test(as->freeset, RID_RETHI) &&
      destlo != RID_RETHI && desthi != RID_RETHI)
    ra_restore(as, regcost_ref(as->cost[RID_RETHI]));
  /* Next free the destination registers (if any). */
  if (ra_hasreg(destlo)) {
    ra_free(as, destlo);
    ra_modified(as, destlo);
  } else {
    destlo = RID_RETLO;
  }
  if (ra_hasreg(desthi)) {
    ra_free(as, desthi);
    ra_modified(as, desthi);
  } else {
    desthi = RID_RETHI;
  }
  /* Check for conflicts and shuffle the registers as needed. */
  if (destlo == RID_RETHI) {
    if (desthi == RID_RETLO) {
#if LJ_TARGET_X86
      *--as->mcp = XI_XCHGa + RID_RETHI;
#else
      emit_movrr(as, ir, RID_RETHI, RID_TMP);
      emit_movrr(as, ir, RID_RETLO, RID_RETHI);
      emit_movrr(as, ir, RID_TMP, RID_RETLO);
#endif
    } else {
      emit_movrr(as, ir, RID_RETHI, RID_RETLO);
      if (desthi != RID_RETHI) emit_movrr(as, ir, desthi, RID_RETHI);
    }
  } else if (desthi == RID_RETLO) {
    emit_movrr(as, ir, RID_RETLO, RID_RETHI);
    if (destlo != RID_RETLO) emit_movrr(as, ir, destlo, RID_RETLO);
  } else {
    if (desthi != RID_RETHI) emit_movrr(as, ir, desthi, RID_RETHI);
    if (destlo != RID_RETLO) emit_movrr(as, ir, destlo, RID_RETLO);
  }
  /* Restore spill slots (if any). */
  if (ra_hasspill((ir+1)->s)) ra_save(as, ir+1, RID_RETHI);
  if (ra_hasspill(ir->s)) ra_save(as, ir, RID_RETLO);
}
#endif

/* -- Snapshot handling --------- ----------------------------------------- */

/* Can we rematerialize a KNUM instead of forcing a spill? */
static int asm_snap_canremat(ASMState *as)
{
  Reg r;
  for (r = RID_MIN_FPR; r < RID_MAX_FPR; r++)
    if (irref_isk(regcost_ref(as->cost[r])))
      return 1;
  return 0;
}

/* Check whether a sunk store corresponds to an allocation. */
static int asm_sunk_store(ASMState *as, IRIns *ira, IRIns *irs)
{
  if (irs->s == 255) {
    if (irs->o == IR_ASTORE || irs->o == IR_HSTORE ||
	irs->o == IR_FSTORE || irs->o == IR_XSTORE) {
      IRIns *irk = IR(irs->op1);
      if (irk->o == IR_AREF || irk->o == IR_HREFK)
	irk = IR(irk->op1);
      return (IR(irk->op1) == ira);
    }
    return 0;
  } else {
    return (ira + irs->s == irs);  /* Quick check. */
  }
}

/* Allocate register or spill slot for a ref that escapes to a snapshot. */
static void asm_snap_alloc1(ASMState *as, IRRef ref)
{
  IRIns *ir = IR(ref);
  if (!irref_isk(ref) && (!(ra_used(ir) || ir->r == RID_SUNK))) {
    if (ir->r == RID_SINK) {
      ir->r = RID_SUNK;
#if LJ_HASFFI
      if (ir->o == IR_CNEWI) {  /* Allocate CNEWI value. */
	asm_snap_alloc1(as, ir->op2);
	if (LJ_32 && (ir+1)->o == IR_HIOP)
	  asm_snap_alloc1(as, (ir+1)->op2);
      } else
#endif
      {  /* Allocate stored values for TNEW, TDUP and CNEW. */
	IRIns *irs;
	lua_assert(ir->o == IR_TNEW || ir->o == IR_TDUP || ir->o == IR_CNEW);
	for (irs = IR(as->snapref-1); irs > ir; irs--)
	  if (irs->r == RID_SINK && asm_sunk_store(as, ir, irs)) {
	    lua_assert(irs->o == IR_ASTORE || irs->o == IR_HSTORE ||
		       irs->o == IR_FSTORE || irs->o == IR_XSTORE);
	    asm_snap_alloc1(as, irs->op2);
	    if (LJ_32 && (irs+1)->o == IR_HIOP)
	      asm_snap_alloc1(as, (irs+1)->op2);
	  }
      }
    } else {
      RegSet allow;
      if (ir->o == IR_CONV && ir->op2 == IRCONV_NUM_INT) {
	IRIns *irc;
	for (irc = IR(as->curins); irc > ir; irc--)
	  if ((irc->op1 == ref || irc->op2 == ref) &&
	      !(irc->r == RID_SINK || irc->r == RID_SUNK))
	    goto nosink;  /* Don't sink conversion if result is used. */
	asm_snap_alloc1(as, ir->op1);
	return;
      }
    nosink:
      allow = (!LJ_SOFTFP && irt_isfp(ir->t)) ? RSET_FPR : RSET_GPR;
      if ((as->freeset & allow) ||
	       (allow == RSET_FPR && asm_snap_canremat(as))) {
	/* Get a weak register if we have a free one or can rematerialize. */
	Reg r = ra_allocref(as, ref, allow);  /* Allocate a register. */
	if (!irt_isphi(ir->t))
	  ra_weak(as, r);  /* But mark it as weakly referenced. */
	checkmclim(as);
	RA_DBGX((as, "snapreg   $f $r", ref, ir->r));
      } else {
	ra_spill(as, ir);  /* Otherwise force a spill slot. */
	RA_DBGX((as, "snapspill $f $s", ref, ir->s));
      }
    }
  }
}

/* Allocate refs escaping to a snapshot. */
static void asm_snap_alloc(ASMState *as)
{
  SnapShot *snap = &as->T->snap[as->snapno];
  SnapEntry *map = &as->T->snapmap[snap->mapofs];
  MSize n, nent = snap->nent;
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    IRRef ref = snap_ref(sn);
    if (!irref_isk(ref)) {
      asm_snap_alloc1(as, ref);
      if (LJ_SOFTFP && (sn & SNAP_SOFTFPNUM)) {
	lua_assert(irt_type(IR(ref+1)->t) == IRT_SOFTFP);
	asm_snap_alloc1(as, ref+1);
      }
    }
  }
}

/* All guards for a snapshot use the same exitno. This is currently the
** same as the snapshot number. Since the exact origin of the exit cannot
** be determined, all guards for the same snapshot must exit with the same
** RegSP mapping.
** A renamed ref which has been used in a prior guard for the same snapshot
** would cause an inconsistency. The easy way out is to force a spill slot.
*/
static int asm_snap_checkrename(ASMState *as, IRRef ren)
{
  SnapShot *snap = &as->T->snap[as->snapno];
  SnapEntry *map = &as->T->snapmap[snap->mapofs];
  MSize n, nent = snap->nent;
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    IRRef ref = snap_ref(sn);
    if (ref == ren || (LJ_SOFTFP && (sn & SNAP_SOFTFPNUM) && ++ref == ren)) {
      IRIns *ir = IR(ref);
      ra_spill(as, ir);  /* Register renamed, so force a spill slot. */
      RA_DBGX((as, "snaprensp $f $s", ref, ir->s));
      return 1;  /* Found. */
    }
  }
  return 0;  /* Not found. */
}

/* Prepare snapshot for next guard instruction. */
static void asm_snap_prep(ASMState *as)
{
  if (as->curins < as->snapref) {
    do {
      if (as->snapno == 0) return;  /* Called by sunk stores before snap #0. */
      as->snapno--;
      as->snapref = as->T->snap[as->snapno].ref;
    } while (as->curins < as->snapref);
    asm_snap_alloc(as);
    as->snaprename = as->T->nins;
  } else {
    /* Process any renames above the highwater mark. */
    for (; as->snaprename < as->T->nins; as->snaprename++) {
      IRIns *ir = IR(as->snaprename);
      if (asm_snap_checkrename(as, ir->op1))
	ir->op2 = REF_BIAS-1;  /* Kill rename. */
    }
  }
}

/* -- Miscellaneous helpers ----------------------------------------------- */

/* Collect arguments from CALL* and CARG instructions. */
static void asm_collectargs(ASMState *as, IRIns *ir,
			    const CCallInfo *ci, IRRef *args)
{
  uint32_t n = CCI_NARGS(ci);
  lua_assert(n <= CCI_NARGS_MAX*2);  /* Account for split args. */
  if ((ci->flags & CCI_L)) { *args++ = ASMREF_L; n--; }
  while (n-- > 1) {
    ir = IR(ir->op1);
    lua_assert(ir->o == IR_CARG);
    args[n] = ir->op2 == REF_NIL ? 0 : ir->op2;
  }
  args[0] = ir->op1 == REF_NIL ? 0 : ir->op1;
  lua_assert(IR(ir->op1)->o != IR_CARG);
}

/* Reconstruct CCallInfo flags for CALLX*. */
static uint32_t asm_callx_flags(ASMState *as, IRIns *ir)
{
  uint32_t nargs = 0;
  if (ir->op1 != REF_NIL) {  /* Count number of arguments first. */
    IRIns *ira = IR(ir->op1);
    nargs++;
    while (ira->o == IR_CARG) { nargs++; ira = IR(ira->op1); }
  }
#if LJ_HASFFI
  if (IR(ir->op2)->o == IR_CARG) {  /* Copy calling convention info. */
    CTypeID id = (CTypeID)IR(IR(ir->op2)->op2)->i;
    CType *ct = ctype_get(ctype_ctsG(J2G(as->J)), id);
    nargs |= ((ct->info & CTF_VARARG) ? CCI_VARARG : 0);
#if LJ_TARGET_X86
    nargs |= (ctype_cconv(ct->info) << CCI_CC_SHIFT);
#endif
  }
#endif
  return (nargs | (ir->t.irt << CCI_OTSHIFT));
}

/* Calculate stack adjustment. */
static int32_t asm_stack_adjust(ASMState *as)
{
  if (as->evenspill <= SPS_FIXED)
    return 0;
  return sps_scale(sps_align(as->evenspill));
}

/* Must match with hash*() in lj_tab.c. */
static uint32_t ir_khash(IRIns *ir)
{
  uint32_t lo, hi;
  if (irt_isstr(ir->t)) {
    return ir_kstr(ir)->hash;
  } else if (irt_isnum(ir->t)) {
    lo = ir_knum(ir)->u32.lo;
    hi = ir_knum(ir)->u32.hi << 1;
  } else if (irt_ispri(ir->t)) {
    lua_assert(!irt_isnil(ir->t));
    return irt_type(ir->t)-IRT_FALSE;
  } else {
    lua_assert(irt_isgcv(ir->t));
    lo = u32ptr(ir_kgc(ir));
    hi = lo + HASH_BIAS;
  }
  return hashrot(lo, hi);
}

/* -- Allocations --------------------------------------------------------- */

static void asm_gencall(ASMState *as, const CCallInfo *ci, IRRef *args);
static void asm_setupresult(ASMState *as, IRIns *ir, const CCallInfo *ci);

static void asm_snew(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_str_new];
  IRRef args[3];
  args[0] = ASMREF_L;  /* lua_State *L    */
  args[1] = ir->op1;   /* const char *str */
  args[2] = ir->op2;   /* size_t len      */
  as->gcsteps++;
  asm_setupresult(as, ir, ci);  /* GCstr * */
  asm_gencall(as, ci, args);
}

static void asm_tnew(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_tab_new1];
  IRRef args[2];
  args[0] = ASMREF_L;     /* lua_State *L    */
  args[1] = ASMREF_TMP1;  /* uint32_t ahsize */
  as->gcsteps++;
  asm_setupresult(as, ir, ci);  /* GCtab * */
  asm_gencall(as, ci, args);
  ra_allockreg(as, ir->op1 | (ir->op2 << 24), ra_releasetmp(as, ASMREF_TMP1));
}

static void asm_tdup(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_tab_dup];
  IRRef args[2];
  args[0] = ASMREF_L;  /* lua_State *L    */
  args[1] = ir->op1;   /* const GCtab *kt */
  as->gcsteps++;
  asm_setupresult(as, ir, ci);  /* GCtab * */
  asm_gencall(as, ci, args);
}

static void asm_gc_check(ASMState *as);

/* Explicit GC step. */
static void asm_gcstep(ASMState *as, IRIns *ir)
{
  IRIns *ira;
  for (ira = IR(as->stopins+1); ira < ir; ira++)
    if ((ira->o == IR_TNEW || ira->o == IR_TDUP ||
	 (LJ_HASFFI && (ira->o == IR_CNEW || ira->o == IR_CNEWI))) &&
	ra_used(ira))
      as->gcsteps++;
  if (as->gcsteps)
    asm_gc_check(as);
  as->gcsteps = 0x80000000;  /* Prevent implicit GC check further up. */
}

/* -- PHI and loop handling ----------------------------------------------- */

/* Break a PHI cycle by renaming to a free register (evict if needed). */
static void asm_phi_break(ASMState *as, RegSet blocked, RegSet blockedby,
			  RegSet allow)
{
  RegSet candidates = blocked & allow;
  if (candidates) {  /* If this register file has candidates. */
    /* Note: the set for ra_pick cannot be empty, since each register file
    ** has some registers never allocated to PHIs.
    */
    Reg down, up = ra_pick(as, ~blocked & allow);  /* Get a free register. */
    if (candidates & ~blockedby)  /* Optimize shifts, else it's a cycle. */
      candidates = candidates & ~blockedby;
    down = rset_picktop(candidates);  /* Pick candidate PHI register. */
    ra_rename(as, down, up);  /* And rename it to the free register. */
  }
}

/* PHI register shuffling.
**
** The allocator tries hard to preserve PHI register assignments across
** the loop body. Most of the time this loop does nothing, since there
** are no register mismatches.
**
** If a register mismatch is detected and ...
** - the register is currently free: rename it.
** - the register is blocked by an invariant: restore/remat and rename it.
** - Otherwise the register is used by another PHI, so mark it as blocked.
**
** The renames are order-sensitive, so just retry the loop if a register
** is marked as blocked, but has been freed in the meantime. A cycle is
** detected if all of the blocked registers are allocated. To break the
** cycle rename one of them to a free register and retry.
**
** Note that PHI spill slots are kept in sync and don't need to be shuffled.
*/
static void asm_phi_shuffle(ASMState *as)
{
  RegSet work;

  /* Find and resolve PHI register mismatches. */
  for (;;) {
    RegSet blocked = RSET_EMPTY;
    RegSet blockedby = RSET_EMPTY;
    RegSet phiset = as->phiset;
    while (phiset) {  /* Check all left PHI operand registers. */
      Reg r = rset_pickbot(phiset);
      IRIns *irl = IR(as->phireg[r]);
      Reg left = irl->r;
      if (r != left) {  /* Mismatch? */
	if (!rset_test(as->freeset, r)) {  /* PHI register blocked? */
	  IRRef ref = regcost_ref(as->cost[r]);
	  /* Blocked by other PHI (w/reg)? */
	  if (!ra_iskref(ref) && irt_ismarked(IR(ref)->t)) {
	    rset_set(blocked, r);
	    if (ra_hasreg(left))
	      rset_set(blockedby, left);
	    left = RID_NONE;
	  } else {  /* Otherwise grab register from invariant. */
	    ra_restore(as, ref);
	    checkmclim(as);
	  }
	}
	if (ra_hasreg(left)) {
	  ra_rename(as, left, r);
	  checkmclim(as);
	}
      }
      rset_clear(phiset, r);
    }
    if (!blocked) break;  /* Finished. */
    if (!(as->freeset & blocked)) {  /* Break cycles if none are free. */
      asm_phi_break(as, blocked, blockedby, RSET_GPR);
      if (!LJ_SOFTFP) asm_phi_break(as, blocked, blockedby, RSET_FPR);
      checkmclim(as);
    }  /* Else retry some more renames. */
  }

  /* Restore/remat invariants whose registers are modified inside the loop. */
#if !LJ_SOFTFP
  work = as->modset & ~(as->freeset | as->phiset) & RSET_FPR;
  while (work) {
    Reg r = rset_pickbot(work);
    ra_restore(as, regcost_ref(as->cost[r]));
    rset_clear(work, r);
    checkmclim(as);
  }
#endif
  work = as->modset & ~(as->freeset | as->phiset);
  while (work) {
    Reg r = rset_pickbot(work);
    ra_restore(as, regcost_ref(as->cost[r]));
    rset_clear(work, r);
    checkmclim(as);
  }

  /* Allocate and save all unsaved PHI regs and clear marks. */
  work = as->phiset;
  while (work) {
    Reg r = rset_picktop(work);
    IRRef lref = as->phireg[r];
    IRIns *ir = IR(lref);
    if (ra_hasspill(ir->s)) {  /* Left PHI gained a spill slot? */
      irt_clearmark(ir->t);  /* Handled here, so clear marker now. */
      ra_alloc1(as, lref, RID2RSET(r));
      ra_save(as, ir, r);  /* Save to spill slot inside the loop. */
      checkmclim(as);
    }
    rset_clear(work, r);
  }
}

/* Copy unsynced left/right PHI spill slots. Rarely needed. */
static void asm_phi_copyspill(ASMState *as)
{
  int need = 0;
  IRIns *ir;
  for (ir = IR(as->orignins-1); ir->o == IR_PHI; ir--)
    if (ra_hasspill(ir->s) && ra_hasspill(IR(ir->op1)->s))
      need |= irt_isfp(ir->t) ? 2 : 1;  /* Unsynced spill slot? */
  if ((need & 1)) {  /* Copy integer spill slots. */
#if !LJ_TARGET_X86ORX64
    Reg r = RID_TMP;
#else
    Reg r = RID_RET;
    if ((as->freeset & RSET_GPR))
      r = rset_pickbot((as->freeset & RSET_GPR));
    else
      emit_spload(as, IR(regcost_ref(as->cost[r])), r, SPOFS_TMP);
#endif
    for (ir = IR(as->orignins-1); ir->o == IR_PHI; ir--) {
      if (ra_hasspill(ir->s)) {
	IRIns *irl = IR(ir->op1);
	if (ra_hasspill(irl->s) && !irt_isfp(ir->t)) {
	  emit_spstore(as, irl, r, sps_scale(irl->s));
	  emit_spload(as, ir, r, sps_scale(ir->s));
	  checkmclim(as);
	}
      }
    }
#if LJ_TARGET_X86ORX64
    if (!rset_test(as->freeset, r))
      emit_spstore(as, IR(regcost_ref(as->cost[r])), r, SPOFS_TMP);
#endif
  }
#if !LJ_SOFTFP
  if ((need & 2)) {  /* Copy FP spill slots. */
#if LJ_TARGET_X86
    Reg r = RID_XMM0;
#else
    Reg r = RID_FPRET;
#endif
    if ((as->freeset & RSET_FPR))
      r = rset_pickbot((as->freeset & RSET_FPR));
    if (!rset_test(as->freeset, r))
      emit_spload(as, IR(regcost_ref(as->cost[r])), r, SPOFS_TMP);
    for (ir = IR(as->orignins-1); ir->o == IR_PHI; ir--) {
      if (ra_hasspill(ir->s)) {
	IRIns *irl = IR(ir->op1);
	if (ra_hasspill(irl->s) && irt_isfp(ir->t)) {
	  emit_spstore(as, irl, r, sps_scale(irl->s));
	  emit_spload(as, ir, r, sps_scale(ir->s));
	  checkmclim(as);
	}
      }
    }
    if (!rset_test(as->freeset, r))
      emit_spstore(as, IR(regcost_ref(as->cost[r])), r, SPOFS_TMP);
  }
#endif
}

/* Emit renames for left PHIs which are only spilled outside the loop. */
static void asm_phi_fixup(ASMState *as)
{
  RegSet work = as->phiset;
  while (work) {
    Reg r = rset_picktop(work);
    IRRef lref = as->phireg[r];
    IRIns *ir = IR(lref);
    /* Left PHI gained a spill slot before the loop? */
    if (irt_ismarked(ir->t) && ra_hasspill(ir->s)) {
      IRRef ren;
      lj_ir_set(as->J, IRT(IR_RENAME, IRT_NIL), lref, as->loopsnapno);
      ren = tref_ref(lj_ir_emit(as->J));
      as->ir = as->T->ir;  /* The IR may have been reallocated. */
      IR(ren)->r = (uint8_t)r;
      IR(ren)->s = SPS_NONE;
    }
    irt_clearmark(ir->t);  /* Always clear marker. */
    rset_clear(work, r);
  }
}

/* Setup right PHI reference. */
static void asm_phi(ASMState *as, IRIns *ir)
{
  RegSet allow = ((!LJ_SOFTFP && irt_isfp(ir->t)) ? RSET_FPR : RSET_GPR) &
		 ~as->phiset;
  RegSet afree = (as->freeset & allow);
  IRIns *irl = IR(ir->op1);
  IRIns *irr = IR(ir->op2);
  if (ir->r == RID_SINK)  /* Sink PHI. */
    return;
  /* Spill slot shuffling is not implemented yet (but rarely needed). */
  if (ra_hasspill(irl->s) || ra_hasspill(irr->s))
    lj_trace_err(as->J, LJ_TRERR_NYIPHI);
  /* Leave at least one register free for non-PHIs (and PHI cycle breaking). */
  if ((afree & (afree-1))) {  /* Two or more free registers? */
    Reg r;
    if (ra_noreg(irr->r)) {  /* Get a register for the right PHI. */
      r = ra_allocref(as, ir->op2, allow);
    } else {  /* Duplicate right PHI, need a copy (rare). */
      r = ra_scratch(as, allow);
      emit_movrr(as, irr, r, irr->r);
    }
    ir->r = (uint8_t)r;
    rset_set(as->phiset, r);
    as->phireg[r] = (IRRef1)ir->op1;
    irt_setmark(irl->t);  /* Marks left PHIs _with_ register. */
    if (ra_noreg(irl->r))
      ra_sethint(irl->r, r); /* Set register hint for left PHI. */
  } else {  /* Otherwise allocate a spill slot. */
    /* This is overly restrictive, but it triggers only on synthetic code. */
    if (ra_hasreg(irl->r) || ra_hasreg(irr->r))
      lj_trace_err(as->J, LJ_TRERR_NYIPHI);
    ra_spill(as, ir);
    irr->s = ir->s;  /* Set right PHI spill slot. Sync left slot later. */
  }
}

static void asm_loop_fixup(ASMState *as);

/* Middle part of a loop. */
static void asm_loop(ASMState *as)
{
  MCode *mcspill;
  /* LOOP is a guard, so the snapno is up to date. */
  as->loopsnapno = as->snapno;
  if (as->gcsteps)
    asm_gc_check(as);
  /* LOOP marks the transition from the variant to the invariant part. */
  as->flagmcp = as->invmcp = NULL;
  as->sectref = 0;
  if (!neverfuse(as)) as->fuseref = 0;
  asm_phi_shuffle(as);
  mcspill = as->mcp;
  asm_phi_copyspill(as);
  asm_loop_fixup(as);
  as->mcloop = as->mcp;
  RA_DBGX((as, "===== LOOP ====="));
  if (!as->realign) RA_DBG_FLUSH();
  if (as->mcp != mcspill)
    emit_jmp(as, mcspill);
}

/* -- Target-specific assembler ------------------------------------------- */

#if LJ_TARGET_X86ORX64
#include "lj_asm_x86.h"
#elif LJ_TARGET_ARM
#include "lj_asm_arm.h"
#elif LJ_TARGET_PPC
#include "lj_asm_ppc.h"
#elif LJ_TARGET_MIPS
#include "lj_asm_mips.h"
#else
#error "Missing assembler for target CPU"
#endif

/* -- Head of trace ------------------------------------------------------- */

/* Head of a root trace. */
static void asm_head_root(ASMState *as)
{
  int32_t spadj;
  asm_head_root_base(as);
  emit_setvmstate(as, (int32_t)as->T->traceno);
  spadj = asm_stack_adjust(as);
  as->T->spadjust = (uint16_t)spadj;
  emit_spsub(as, spadj);
  /* Root traces assume a checked stack for the starting proto. */
  as->T->topslot = gcref(as->T->startpt)->pt.framesize;
}

/* Head of a side trace.
**
** The current simplistic algorithm requires that all slots inherited
** from the parent are live in a register between pass 2 and pass 3. This
** avoids the complexity of stack slot shuffling. But of course this may
** overflow the register set in some cases and cause the dreaded error:
** "NYI: register coalescing too complex". A refined algorithm is needed.
*/
static void asm_head_side(ASMState *as)
{
  IRRef1 sloadins[RID_MAX];
  RegSet allow = RSET_ALL;  /* Inverse of all coalesced registers. */
  RegSet live = RSET_EMPTY;  /* Live parent registers. */
  IRIns *irp = &as->parent->ir[REF_BASE];  /* Parent base. */
  int32_t spadj, spdelta;
  int pass2 = 0;
  int pass3 = 0;
  IRRef i;

  allow = asm_head_side_base(as, irp, allow);

  /* Scan all parent SLOADs and collect register dependencies. */
  for (i = as->stopins; i > REF_BASE; i--) {
    IRIns *ir = IR(i);
    RegSP rs;
    lua_assert((ir->o == IR_SLOAD && (ir->op2 & IRSLOAD_PARENT)) ||
	       (LJ_SOFTFP && ir->o == IR_HIOP) || ir->o == IR_PVAL);
    rs = as->parentmap[i - REF_FIRST];
    if (ra_hasreg(ir->r)) {
      rset_clear(allow, ir->r);
      if (ra_hasspill(ir->s)) {
	ra_save(as, ir, ir->r);
	checkmclim(as);
      }
    } else if (ra_hasspill(ir->s)) {
      irt_setmark(ir->t);
      pass2 = 1;
    }
    if (ir->r == rs) {  /* Coalesce matching registers right now. */
      ra_free(as, ir->r);
    } else if (ra_hasspill(regsp_spill(rs))) {
      if (ra_hasreg(ir->r))
	pass3 = 1;
    } else if (ra_used(ir)) {
      sloadins[rs] = (IRRef1)i;
      rset_set(live, rs);  /* Block live parent register. */
    }
  }

  /* Calculate stack frame adjustment. */
  spadj = asm_stack_adjust(as);
  spdelta = spadj - (int32_t)as->parent->spadjust;
  if (spdelta < 0) {  /* Don't shrink the stack frame. */
    spadj = (int32_t)as->parent->spadjust;
    spdelta = 0;
  }
  as->T->spadjust = (uint16_t)spadj;

  /* Reload spilled target registers. */
  if (pass2) {
    for (i = as->stopins; i > REF_BASE; i--) {
      IRIns *ir = IR(i);
      if (irt_ismarked(ir->t)) {
	RegSet mask;
	Reg r;
	RegSP rs;
	irt_clearmark(ir->t);
	rs = as->parentmap[i - REF_FIRST];
	if (!ra_hasspill(regsp_spill(rs)))
	  ra_sethint(ir->r, rs);  /* Hint may be gone, set it again. */
	else if (sps_scale(regsp_spill(rs))+spdelta == sps_scale(ir->s))
	  continue;  /* Same spill slot, do nothing. */
	mask = ((!LJ_SOFTFP && irt_isfp(ir->t)) ? RSET_FPR : RSET_GPR) & allow;
	if (mask == RSET_EMPTY)
	  lj_trace_err(as->J, LJ_TRERR_NYICOAL);
	r = ra_allocref(as, i, mask);
	ra_save(as, ir, r);
	rset_clear(allow, r);
	if (r == rs) {  /* Coalesce matching registers right now. */
	  ra_free(as, r);
	  rset_clear(live, r);
	} else if (ra_hasspill(regsp_spill(rs))) {
	  pass3 = 1;
	}
	checkmclim(as);
      }
    }
  }

  /* Store trace number and adjust stack frame relative to the parent. */
  emit_setvmstate(as, (int32_t)as->T->traceno);
  emit_spsub(as, spdelta);

#if !LJ_TARGET_X86ORX64
  /* Restore BASE register from parent spill slot. */
  if (ra_hasspill(irp->s))
    emit_spload(as, IR(REF_BASE), IR(REF_BASE)->r, sps_scale(irp->s));
#endif

  /* Restore target registers from parent spill slots. */
  if (pass3) {
    RegSet work = ~as->freeset & RSET_ALL;
    while (work) {
      Reg r = rset_pickbot(work);
      IRRef ref = regcost_ref(as->cost[r]);
      RegSP rs = as->parentmap[ref - REF_FIRST];
      rset_clear(work, r);
      if (ra_hasspill(regsp_spill(rs))) {
	int32_t ofs = sps_scale(regsp_spill(rs));
	ra_free(as, r);
	emit_spload(as, IR(ref), r, ofs);
	checkmclim(as);
      }
    }
  }

  /* Shuffle registers to match up target regs with parent regs. */
  for (;;) {
    RegSet work;

    /* Repeatedly coalesce free live registers by moving to their target. */
    while ((work = as->freeset & live) != RSET_EMPTY) {
      Reg rp = rset_pickbot(work);
      IRIns *ir = IR(sloadins[rp]);
      rset_clear(live, rp);
      rset_clear(allow, rp);
      ra_free(as, ir->r);
      emit_movrr(as, ir, ir->r, rp);
      checkmclim(as);
    }

    /* We're done if no live registers remain. */
    if (live == RSET_EMPTY)
      break;

    /* Break cycles by renaming one target to a temp. register. */
    if (live & RSET_GPR) {
      RegSet tmpset = as->freeset & ~live & allow & RSET_GPR;
      if (tmpset == RSET_EMPTY)
	lj_trace_err(as->J, LJ_TRERR_NYICOAL);
      ra_rename(as, rset_pickbot(live & RSET_GPR), rset_pickbot(tmpset));
    }
    if (!LJ_SOFTFP && (live & RSET_FPR)) {
      RegSet tmpset = as->freeset & ~live & allow & RSET_FPR;
      if (tmpset == RSET_EMPTY)
	lj_trace_err(as->J, LJ_TRERR_NYICOAL);
      ra_rename(as, rset_pickbot(live & RSET_FPR), rset_pickbot(tmpset));
    }
    checkmclim(as);
    /* Continue with coalescing to fix up the broken cycle(s). */
  }

  /* Inherit top stack slot already checked by parent trace. */
  as->T->topslot = as->parent->topslot;
  if (as->topslot > as->T->topslot) {  /* Need to check for higher slot? */
#ifdef EXITSTATE_CHECKEXIT
    /* Highest exit + 1 indicates stack check. */
    ExitNo exitno = as->T->nsnap;
#else
    /* Reuse the parent exit in the context of the parent trace. */
    ExitNo exitno = as->J->exitno;
#endif
    as->T->topslot = (uint8_t)as->topslot;  /* Remember for child traces. */
    asm_stack_check(as, as->topslot, irp, allow & RSET_GPR, exitno);
  }
}

/* -- Tail of trace ------------------------------------------------------- */

/* Get base slot for a snapshot. */
static BCReg asm_baseslot(ASMState *as, SnapShot *snap, int *gotframe)
{
  SnapEntry *map = &as->T->snapmap[snap->mapofs];
  MSize n;
  for (n = snap->nent; n > 0; n--) {
    SnapEntry sn = map[n-1];
    if ((sn & SNAP_FRAME)) {
      *gotframe = 1;
      return snap_slot(sn);
    }
  }
  return 0;
}

/* Link to another trace. */
static void asm_tail_link(ASMState *as)
{
  SnapNo snapno = as->T->nsnap-1;  /* Last snapshot. */
  SnapShot *snap = &as->T->snap[snapno];
  int gotframe = 0;
  BCReg baseslot = asm_baseslot(as, snap, &gotframe);

  as->topslot = snap->topslot;
  checkmclim(as);
  ra_allocref(as, REF_BASE, RID2RSET(RID_BASE));

  if (as->T->link == 0) {
    /* Setup fixed registers for exit to interpreter. */
    const BCIns *pc = snap_pc(as->T->snapmap[snap->mapofs + snap->nent]);
    int32_t mres;
    if (bc_op(*pc) == BC_JLOOP) {  /* NYI: find a better way to do this. */
      BCIns *retpc = &traceref(as->J, bc_d(*pc))->startins;
      if (bc_isret(bc_op(*retpc)))
	pc = retpc;
    }
    ra_allockreg(as, i32ptr(J2GG(as->J)->dispatch), RID_DISPATCH);
    ra_allockreg(as, i32ptr(pc), RID_LPC);
    mres = (int32_t)(snap->nslots - baseslot);
    switch (bc_op(*pc)) {
    case BC_CALLM: case BC_CALLMT:
      mres -= (int32_t)(1 + bc_a(*pc) + bc_c(*pc)); break;
    case BC_RETM: mres -= (int32_t)(bc_a(*pc) + bc_d(*pc)); break;
    case BC_TSETM: mres -= (int32_t)bc_a(*pc); break;
    default: if (bc_op(*pc) < BC_FUNCF) mres = 0; break;
    }
    ra_allockreg(as, mres, RID_RET);  /* Return MULTRES or 0. */
  } else if (baseslot) {
    /* Save modified BASE for linking to trace with higher start frame. */
    emit_setgl(as, RID_BASE, jit_base);
  }
  emit_addptr(as, RID_BASE, 8*(int32_t)baseslot);

  /* Sync the interpreter state with the on-trace state. */
  asm_stack_restore(as, snap);

  /* Root traces that add frames need to check the stack at the end. */
  if (!as->parent && gotframe)
    asm_stack_check(as, as->topslot, NULL, as->freeset & RSET_GPR, snapno);
}

/* -- Trace setup --------------------------------------------------------- */

/* Clear reg/sp for all instructions and add register hints. */
static void asm_setup_regsp(ASMState *as)
{
  GCtrace *T = as->T;
  int sink = T->sinktags;
  IRRef nins = T->nins;
  IRIns *ir, *lastir;
  int inloop;
#if LJ_TARGET_ARM
  uint32_t rload = 0xa6402a64;
#endif

  ra_setup(as);

  /* Clear reg/sp for constants. */
  for (ir = IR(T->nk), lastir = IR(REF_BASE); ir < lastir; ir++)
    ir->prev = REGSP_INIT;

  /* REF_BASE is used for implicit references to the BASE register. */
  lastir->prev = REGSP_HINT(RID_BASE);

  ir = IR(nins-1);
  if (ir->o == IR_RENAME) {
    do { ir--; nins--; } while (ir->o == IR_RENAME);
    T->nins = nins;  /* Remove any renames left over from ASM restart. */
  }
  as->snaprename = nins;
  as->snapref = nins;
  as->snapno = T->nsnap;

  as->stopins = REF_BASE;
  as->orignins = nins;
  as->curins = nins;

  /* Setup register hints for parent link instructions. */
  ir = IR(REF_FIRST);
  if (as->parent) {
    uint16_t *p;
    lastir = lj_snap_regspmap(as->parent, as->J->exitno, ir);
    if (lastir - ir > LJ_MAX_JSLOTS)
      lj_trace_err(as->J, LJ_TRERR_NYICOAL);
    as->stopins = (IRRef)((lastir-1) - as->ir);
    for (p = as->parentmap; ir < lastir; ir++) {
      RegSP rs = ir->prev;
      *p++ = (uint16_t)rs;  /* Copy original parent RegSP to parentmap. */
      if (!ra_hasspill(regsp_spill(rs)))
	ir->prev = (uint16_t)REGSP_HINT(regsp_reg(rs));
      else
	ir->prev = REGSP_INIT;
    }
  }

  inloop = 0;
  as->evenspill = SPS_FIRST;
  for (lastir = IR(nins); ir < lastir; ir++) {
    if (sink) {
      if (ir->r == RID_SINK)
	continue;
      if (ir->r == RID_SUNK) {  /* Revert after ASM restart. */
	ir->r = RID_SINK;
	continue;
      }
    }
    switch (ir->o) {
    case IR_LOOP:
      inloop = 1;
      break;
#if LJ_TARGET_ARM
    case IR_SLOAD:
      if (!((ir->op2 & IRSLOAD_TYPECHECK) || (ir+1)->o == IR_HIOP))
	break;
      /* fallthrough */
    case IR_ALOAD: case IR_HLOAD: case IR_ULOAD: case IR_VLOAD:
      if (!LJ_SOFTFP && irt_isnum(ir->t)) break;
      ir->prev = (uint16_t)REGSP_HINT((rload & 15));
      rload = lj_ror(rload, 4);
      continue;
#endif
    case IR_CALLXS: {
      CCallInfo ci;
      ci.flags = asm_callx_flags(as, ir);
      ir->prev = asm_setup_call_slots(as, ir, &ci);
      if (inloop)
	as->modset |= RSET_SCRATCH;
      continue;
      }
    case IR_CALLN: case IR_CALLL: case IR_CALLS: {
      const CCallInfo *ci = &lj_ir_callinfo[ir->op2];
      ir->prev = asm_setup_call_slots(as, ir, ci);
      if (inloop)
	as->modset |= (ci->flags & CCI_NOFPRCLOBBER) ?
		      (RSET_SCRATCH & ~RSET_FPR) : RSET_SCRATCH;
      continue;
      }
#if LJ_SOFTFP || (LJ_32 && LJ_HASFFI)
    case IR_HIOP:
      switch ((ir-1)->o) {
#if LJ_SOFTFP && LJ_TARGET_ARM
      case IR_SLOAD: case IR_ALOAD: case IR_HLOAD: case IR_ULOAD: case IR_VLOAD:
	if (ra_hashint((ir-1)->r)) {
	  ir->prev = (ir-1)->prev + 1;
	  continue;
	}
	break;
#endif
#if !LJ_SOFTFP && LJ_NEED_FP64
      case IR_CONV:
	if (irt_isfp((ir-1)->t)) {
	  ir->prev = REGSP_HINT(RID_FPRET);
	  continue;
	}
	/* fallthrough */
#endif
      case IR_CALLN: case IR_CALLXS:
#if LJ_SOFTFP
      case IR_MIN: case IR_MAX:
#endif
	(ir-1)->prev = REGSP_HINT(RID_RETLO);
	ir->prev = REGSP_HINT(RID_RETHI);
	continue;
      default:
	break;
      }
      break;
#endif
#if LJ_SOFTFP
    case IR_MIN: case IR_MAX:
      if ((ir+1)->o != IR_HIOP) break;
      /* fallthrough */
#endif
    /* C calls evict all scratch regs and return results in RID_RET. */
    case IR_SNEW: case IR_XSNEW: case IR_NEWREF:
      if (REGARG_NUMGPR < 3 && as->evenspill < 3)
	as->evenspill = 3;  /* lj_str_new and lj_tab_newkey need 3 args. */
    case IR_TNEW: case IR_TDUP: case IR_CNEW: case IR_CNEWI: case IR_TOSTR:
      ir->prev = REGSP_HINT(RID_RET);
      if (inloop)
	as->modset = RSET_SCRATCH;
      continue;
    case IR_STRTO: case IR_OBAR:
      if (inloop)
	as->modset = RSET_SCRATCH;
      break;
#if !LJ_TARGET_X86ORX64 && !LJ_SOFTFP
    case IR_ATAN2: case IR_LDEXP:
#endif
    case IR_POW:
      if (!LJ_SOFTFP && irt_isnum(ir->t)) {
#if LJ_TARGET_X86ORX64
	ir->prev = REGSP_HINT(RID_XMM0);
	if (inloop)
	  as->modset |= RSET_RANGE(RID_XMM0, RID_XMM1+1)|RID2RSET(RID_EAX);
#else
	ir->prev = REGSP_HINT(RID_FPRET);
	if (inloop)
	  as->modset |= RSET_SCRATCH;
#endif
	continue;
      }
      /* fallthrough for integer POW */
    case IR_DIV: case IR_MOD:
      if (!irt_isnum(ir->t)) {
	ir->prev = REGSP_HINT(RID_RET);
	if (inloop)
	  as->modset |= (RSET_SCRATCH & RSET_GPR);
	continue;
      }
      break;
    case IR_FPMATH:
#if LJ_TARGET_X86ORX64
      if (ir->op2 == IRFPM_EXP2) {  /* May be joined to lj_vm_pow_sse. */
	ir->prev = REGSP_HINT(RID_XMM0);
#if !LJ_64
	if (as->evenspill < 4)  /* Leave room for 16 byte scratch area. */
	  as->evenspill = 4;
#endif
	if (inloop)
	  as->modset |= RSET_RANGE(RID_XMM0, RID_XMM2+1)|RID2RSET(RID_EAX);
	continue;
      } else if (ir->op2 <= IRFPM_TRUNC && !(as->flags & JIT_F_SSE4_1)) {
	ir->prev = REGSP_HINT(RID_XMM0);
	if (inloop)
	  as->modset |= RSET_RANGE(RID_XMM0, RID_XMM3+1)|RID2RSET(RID_EAX);
	continue;
      }
      break;
#else
      ir->prev = REGSP_HINT(RID_FPRET);
      if (inloop)
	as->modset |= RSET_SCRATCH;
      continue;
#endif
#if LJ_TARGET_X86ORX64
    /* Non-constant shift counts need to be in RID_ECX on x86/x64. */
    case IR_BSHL: case IR_BSHR: case IR_BSAR: case IR_BROL: case IR_BROR:
      if (!irref_isk(ir->op2) && !ra_hashint(IR(ir->op2)->r)) {
	IR(ir->op2)->r = REGSP_HINT(RID_ECX);
	if (inloop)
	  rset_set(as->modset, RID_ECX);
      }
      break;
#endif
    /* Do not propagate hints across type conversions or loads. */
    case IR_TOBIT:
    case IR_XLOAD:
#if !LJ_TARGET_ARM
    case IR_ALOAD: case IR_HLOAD: case IR_ULOAD: case IR_VLOAD:
#endif
      break;
    case IR_CONV:
      if (irt_isfp(ir->t) || (ir->op2 & IRCONV_SRCMASK) == IRT_NUM ||
	  (ir->op2 & IRCONV_SRCMASK) == IRT_FLOAT)
	break;
      /* fallthrough */
    default:
      /* Propagate hints across likely 'op reg, imm' or 'op reg'. */
      if (irref_isk(ir->op2) && !irref_isk(ir->op1) &&
	  ra_hashint(regsp_reg(IR(ir->op1)->prev))) {
	ir->prev = IR(ir->op1)->prev;
	continue;
      }
      break;
    }
    ir->prev = REGSP_INIT;
  }
  if ((as->evenspill & 1))
    as->oddspill = as->evenspill++;
  else
    as->oddspill = 0;
}

/* -- Assembler core ------------------------------------------------------ */

/* Assemble a trace. */
void lj_asm_trace(jit_State *J, GCtrace *T)
{
  ASMState as_;
  ASMState *as = &as_;
  MCode *origtop;

  /* Ensure an initialized instruction beyond the last one for HIOP checks. */
  J->cur.nins = lj_ir_nextins(J);
  J->cur.ir[J->cur.nins].o = IR_NOP;

  /* Setup initial state. Copy some fields to reduce indirections. */
  as->J = J;
  as->T = T;
  as->ir = T->ir;
  as->flags = J->flags;
  as->loopref = J->loopref;
  as->realign = NULL;
  as->loopinv = 0;
  as->parent = J->parent ? traceref(J, J->parent) : NULL;

  /* Reserve MCode memory. */
  as->mctop = origtop = lj_mcode_reserve(J, &as->mcbot);
  as->mcp = as->mctop;
  as->mclim = as->mcbot + MCLIM_REDZONE;
  asm_setup_target(as);

  do {
    as->mcp = as->mctop;
#ifdef LUA_USE_ASSERT
    as->mcp_prev = as->mcp;
#endif
    as->curins = T->nins;
    RA_DBG_START();
    RA_DBGX((as, "===== STOP ====="));

    /* General trace setup. Emit tail of trace. */
    asm_tail_prep(as);
    as->mcloop = NULL;
    as->flagmcp = NULL;
    as->topslot = 0;
    as->gcsteps = 0;
    as->sectref = as->loopref;
    as->fuseref = (as->flags & JIT_F_OPT_FUSE) ? as->loopref : FUSE_DISABLED;
    asm_setup_regsp(as);
    if (!as->loopref)
      asm_tail_link(as);

    /* Assemble a trace in linear backwards order. */
    for (as->curins--; as->curins > as->stopins; as->curins--) {
      IRIns *ir = IR(as->curins);
      lua_assert(!(LJ_32 && irt_isint64(ir->t)));  /* Handled by SPLIT. */
      if (!ra_used(ir) && !ir_sideeff(ir) && (as->flags & JIT_F_OPT_DCE))
	continue;  /* Dead-code elimination can be soooo easy. */
      if (irt_isguard(ir->t))
	asm_snap_prep(as);
      RA_DBG_REF();
      checkmclim(as);
      asm_ir(as, ir);
    }
  } while (as->realign);  /* Retry in case the MCode needs to be realigned. */

  /* Emit head of trace. */
  RA_DBG_REF();
  checkmclim(as);
  if (as->gcsteps > 0) {
    as->curins = as->T->snap[0].ref;
    asm_snap_prep(as);  /* The GC check is a guard. */
    asm_gc_check(as);
  }
  ra_evictk(as);
  if (as->parent)
    asm_head_side(as);
  else
    asm_head_root(as);
  asm_phi_fixup(as);

  RA_DBGX((as, "===== START ===="));
  RA_DBG_FLUSH();
  if (as->freeset != RSET_ALL)
    lj_trace_err(as->J, LJ_TRERR_BADRA);  /* Ouch! Should never happen. */

  /* Set trace entry point before fixing up tail to allow link to self. */
  T->mcode = as->mcp;
  T->mcloop = as->mcloop ? (MSize)((char *)as->mcloop - (char *)as->mcp) : 0;
  if (!as->loopref)
    asm_tail_fixup(as, T->link);  /* Note: this may change as->mctop! */
  T->szmcode = (MSize)((char *)as->mctop - (char *)as->mcp);
  lj_mcode_sync(T->mcode, origtop);
}

#undef IR

#endif
