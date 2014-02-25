/*
** PPC instruction emitter.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

/* -- Emit basic instructions --------------------------------------------- */

static void emit_tab(ASMState *as, PPCIns pi, Reg rt, Reg ra, Reg rb)
{
  *--as->mcp = pi | PPCF_T(rt) | PPCF_A(ra) | PPCF_B(rb);
}

#define emit_asb(as, pi, ra, rs, rb)	emit_tab(as, (pi), (rs), (ra), (rb))
#define emit_as(as, pi, ra, rs)		emit_tab(as, (pi), (rs), (ra), 0)
#define emit_ab(as, pi, ra, rb)		emit_tab(as, (pi), 0, (ra), (rb))

static void emit_tai(ASMState *as, PPCIns pi, Reg rt, Reg ra, int32_t i)
{
  *--as->mcp = pi | PPCF_T(rt) | PPCF_A(ra) | (i & 0xffff);
}

#define emit_ti(as, pi, rt, i)		emit_tai(as, (pi), (rt), 0, (i))
#define emit_ai(as, pi, ra, i)		emit_tai(as, (pi), 0, (ra), (i))
#define emit_asi(as, pi, ra, rs, i)	emit_tai(as, (pi), (rs), (ra), (i))

#define emit_fab(as, pi, rf, ra, rb) \
  emit_tab(as, (pi), (rf)&31, (ra)&31, (rb)&31)
#define emit_fb(as, pi, rf, rb)		emit_tab(as, (pi), (rf)&31, 0, (rb)&31)
#define emit_fac(as, pi, rf, ra, rc) \
  emit_tab(as, (pi) | PPCF_C((rc) & 31), (rf)&31, (ra)&31, 0)
#define emit_facb(as, pi, rf, ra, rc, rb) \
  emit_tab(as, (pi) | PPCF_C((rc) & 31), (rf)&31, (ra)&31, (rb)&31)
#define emit_fai(as, pi, rf, ra, i)	emit_tai(as, (pi), (rf)&31, (ra), (i))

static void emit_rot(ASMState *as, PPCIns pi, Reg ra, Reg rs,
		     int32_t n, int32_t b, int32_t e)
{
  *--as->mcp = pi | PPCF_T(rs) | PPCF_A(ra) | PPCF_B(n) |
	       PPCF_MB(b) | PPCF_ME(e);
}

static void emit_slwi(ASMState *as, Reg ra, Reg rs, int32_t n)
{
  lua_assert(n >= 0 && n < 32);
  emit_rot(as, PPCI_RLWINM, ra, rs, n, 0, 31-n);
}

static void emit_rotlwi(ASMState *as, Reg ra, Reg rs, int32_t n)
{
  lua_assert(n >= 0 && n < 32);
  emit_rot(as, PPCI_RLWINM, ra, rs, n, 0, 31);
}

/* -- Emit loads/stores --------------------------------------------------- */

/* Prefer rematerialization of BASE/L from global_State over spills. */
#define emit_canremat(ref)	((ref) <= REF_BASE)

/* Try to find a one step delta relative to another constant. */
static int emit_kdelta1(ASMState *as, Reg t, int32_t i)
{
  RegSet work = ~as->freeset & RSET_GPR;
  while (work) {
    Reg r = rset_picktop(work);
    IRRef ref = regcost_ref(as->cost[r]);
    lua_assert(r != t);
    if (ref < ASMREF_L) {
      int32_t delta = i - (ra_iskref(ref) ? ra_krefk(as, ref) : IR(ref)->i);
      if (checki16(delta)) {
	emit_tai(as, PPCI_ADDI, t, r, delta);
	return 1;
      }
    }
    rset_clear(work, r);
  }
  return 0;  /* Failed. */
}

/* Load a 32 bit constant into a GPR. */
static void emit_loadi(ASMState *as, Reg r, int32_t i)
{
  if (checki16(i)) {
    emit_ti(as, PPCI_LI, r, i);
  } else {
    if ((i & 0xffff)) {
      int32_t jgl = i32ptr(J2G(as->J));
      if ((uint32_t)(i-jgl) < 65536) {
	emit_tai(as, PPCI_ADDI, r, RID_JGL, i-jgl-32768);
	return;
      } else if (emit_kdelta1(as, r, i)) {
	return;
      }
      emit_asi(as, PPCI_ORI, r, r, i);
    }
    emit_ti(as, PPCI_LIS, r, (i >> 16));
  }
}

#define emit_loada(as, r, addr)		emit_loadi(as, (r), i32ptr((addr)))

static Reg ra_allock(ASMState *as, int32_t k, RegSet allow);

/* Get/set from constant pointer. */
static void emit_lsptr(ASMState *as, PPCIns pi, Reg r, void *p, RegSet allow)
{
  int32_t jgl = i32ptr(J2G(as->J));
  int32_t i = i32ptr(p);
  Reg base;
  if ((uint32_t)(i-jgl) < 65536) {
    i = i-jgl-32768;
    base = RID_JGL;
  } else {
    base = ra_allock(as, i-(int16_t)i, allow);
  }
  emit_tai(as, pi, r, base, i);
}

#define emit_loadn(as, r, tv) \
  emit_lsptr(as, PPCI_LFD, ((r) & 31), (void *)(tv), RSET_GPR)

/* Get/set global_State fields. */
static void emit_lsglptr(ASMState *as, PPCIns pi, Reg r, int32_t ofs)
{
  emit_tai(as, pi, r, RID_JGL, ofs-32768);
}

#define emit_getgl(as, r, field) \
  emit_lsglptr(as, PPCI_LWZ, (r), (int32_t)offsetof(global_State, field))
#define emit_setgl(as, r, field) \
  emit_lsglptr(as, PPCI_STW, (r), (int32_t)offsetof(global_State, field))

/* Trace number is determined from per-trace exit stubs. */
#define emit_setvmstate(as, i)		UNUSED(i)

/* -- Emit control-flow instructions -------------------------------------- */

/* Label for internal jumps. */
typedef MCode *MCLabel;

/* Return label pointing to current PC. */
#define emit_label(as)		((as)->mcp)

static void emit_condbranch(ASMState *as, PPCIns pi, PPCCC cc, MCode *target)
{
  MCode *p = --as->mcp;
  ptrdiff_t delta = (char *)target - (char *)p;
  lua_assert(((delta + 0x8000) >> 16) == 0);
  pi ^= (delta & 0x8000) * (PPCF_Y/0x8000);
  *p = pi | PPCF_CC(cc) | ((uint32_t)delta & 0xffffu);
}

static void emit_jmp(ASMState *as, MCode *target)
{
  MCode *p = --as->mcp;
  ptrdiff_t delta = (char *)target - (char *)p;
  *p = PPCI_B | (delta & 0x03fffffcu);
}

static void emit_call(ASMState *as, void *target)
{
  MCode *p = --as->mcp;
  ptrdiff_t delta = (char *)target - (char *)p;
  if ((((delta>>2) + 0x00800000) >> 24) == 0) {
    *p = PPCI_BL | (delta & 0x03fffffcu);
  } else {  /* Target out of range: need indirect call. Don't use arg reg. */
    RegSet allow = RSET_GPR & ~RSET_RANGE(RID_R0, REGARG_LASTGPR+1);
    Reg r = ra_allock(as, i32ptr(target), allow);
    *p = PPCI_BCTRL;
    p[-1] = PPCI_MTCTR | PPCF_T(r);
    as->mcp = p-1;
  }
}

/* -- Emit generic operations --------------------------------------------- */

#define emit_mr(as, dst, src) \
  emit_asb(as, PPCI_MR, (dst), (src), (src))

/* Generic move between two regs. */
static void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src)
{
  UNUSED(ir);
  if (dst < RID_MAX_GPR)
    emit_mr(as, dst, src);
  else
    emit_fb(as, PPCI_FMR, dst, src);
}

/* Generic load of register from stack slot. */
static void emit_spload(ASMState *as, IRIns *ir, Reg r, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_tai(as, PPCI_LWZ, r, RID_SP, ofs);
  else
    emit_fai(as, irt_isnum(ir->t) ? PPCI_LFD : PPCI_LFS, r, RID_SP, ofs);
}

/* Generic store of register to stack slot. */
static void emit_spstore(ASMState *as, IRIns *ir, Reg r, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_tai(as, PPCI_STW, r, RID_SP, ofs);
  else
    emit_fai(as, irt_isnum(ir->t) ? PPCI_STFD : PPCI_STFS, r, RID_SP, ofs);
}

/* Emit a compare (for equality) with a constant operand. */
static void emit_cmpi(ASMState *as, Reg r, int32_t k)
{
  if (checki16(k)) {
    emit_ai(as, PPCI_CMPWI, r, k);
  } else if (checku16(k)) {
    emit_ai(as, PPCI_CMPLWI, r, k);
  } else {
    emit_ai(as, PPCI_CMPLWI, RID_TMP, k);
    emit_asi(as, PPCI_XORIS, RID_TMP, r, (k >> 16));
  }
}

/* Add offset to pointer. */
static void emit_addptr(ASMState *as, Reg r, int32_t ofs)
{
  if (ofs) {
    emit_tai(as, PPCI_ADDI, r, r, ofs);
    if (!checki16(ofs))
      emit_tai(as, PPCI_ADDIS, r, r, (ofs + 32768) >> 16);
  }
}

static void emit_spsub(ASMState *as, int32_t ofs)
{
  if (ofs) {
    emit_tai(as, PPCI_STWU, RID_TMP, RID_SP, -ofs);
    emit_tai(as, PPCI_ADDI, RID_TMP, RID_SP,
	     CFRAME_SIZE + (as->parent ? as->parent->spadjust : 0));
  }
}

