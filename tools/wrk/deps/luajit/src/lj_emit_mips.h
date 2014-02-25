/*
** MIPS instruction emitter.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

/* -- Emit basic instructions --------------------------------------------- */

static void emit_dst(ASMState *as, MIPSIns mi, Reg rd, Reg rs, Reg rt)
{
  *--as->mcp = mi | MIPSF_D(rd) | MIPSF_S(rs) | MIPSF_T(rt);
}

static void emit_dta(ASMState *as, MIPSIns mi, Reg rd, Reg rt, uint32_t a)
{
  *--as->mcp = mi | MIPSF_D(rd) | MIPSF_T(rt) | MIPSF_A(a);
}

#define emit_ds(as, mi, rd, rs)		emit_dst(as, (mi), (rd), (rs), 0)
#define emit_tg(as, mi, rt, rg)		emit_dst(as, (mi), (rg)&31, 0, (rt))

static void emit_tsi(ASMState *as, MIPSIns mi, Reg rt, Reg rs, int32_t i)
{
  *--as->mcp = mi | MIPSF_T(rt) | MIPSF_S(rs) | (i & 0xffff);
}

#define emit_ti(as, mi, rt, i)		emit_tsi(as, (mi), (rt), 0, (i))
#define emit_hsi(as, mi, rh, rs, i)	emit_tsi(as, (mi), (rh) & 31, (rs), (i))

static void emit_fgh(ASMState *as, MIPSIns mi, Reg rf, Reg rg, Reg rh)
{
  *--as->mcp = mi | MIPSF_F(rf&31) | MIPSF_G(rg&31) | MIPSF_H(rh&31);
}

#define emit_fg(as, mi, rf, rg)		emit_fgh(as, (mi), (rf), (rg), 0)

static void emit_rotr(ASMState *as, Reg dest, Reg src, Reg tmp, uint32_t shift)
{
  if ((as->flags & JIT_F_MIPS32R2)) {
    emit_dta(as, MIPSI_ROTR, dest, src, shift);
  } else {
    emit_dst(as, MIPSI_OR, dest, dest, tmp);
    emit_dta(as, MIPSI_SLL, dest, src, (-shift)&31);
    emit_dta(as, MIPSI_SRL, tmp, src, shift);
  }
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
	emit_tsi(as, MIPSI_ADDIU, t, r, delta);
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
    emit_ti(as, MIPSI_LI, r, i);
  } else {
    if ((i & 0xffff)) {
      int32_t jgl = i32ptr(J2G(as->J));
      if ((uint32_t)(i-jgl) < 65536) {
	emit_tsi(as, MIPSI_ADDIU, r, RID_JGL, i-jgl-32768);
	return;
      } else if (emit_kdelta1(as, r, i)) {
	return;
      } else if ((i >> 16) == 0) {
	emit_tsi(as, MIPSI_ORI, r, RID_ZERO, i);
	return;
      }
      emit_tsi(as, MIPSI_ORI, r, r, i);
    }
    emit_ti(as, MIPSI_LUI, r, (i >> 16));
  }
}

#define emit_loada(as, r, addr)		emit_loadi(as, (r), i32ptr((addr)))

static Reg ra_allock(ASMState *as, int32_t k, RegSet allow);
static void ra_allockreg(ASMState *as, int32_t k, Reg r);

/* Get/set from constant pointer. */
static void emit_lsptr(ASMState *as, MIPSIns mi, Reg r, void *p, RegSet allow)
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
  emit_tsi(as, mi, r, base, i);
}

#define emit_loadn(as, r, tv) \
  emit_lsptr(as, MIPSI_LDC1, ((r) & 31), (void *)(tv), RSET_GPR)

/* Get/set global_State fields. */
static void emit_lsglptr(ASMState *as, MIPSIns mi, Reg r, int32_t ofs)
{
  emit_tsi(as, mi, r, RID_JGL, ofs-32768);
}

#define emit_getgl(as, r, field) \
  emit_lsglptr(as, MIPSI_LW, (r), (int32_t)offsetof(global_State, field))
#define emit_setgl(as, r, field) \
  emit_lsglptr(as, MIPSI_SW, (r), (int32_t)offsetof(global_State, field))

/* Trace number is determined from per-trace exit stubs. */
#define emit_setvmstate(as, i)		UNUSED(i)

/* -- Emit control-flow instructions -------------------------------------- */

/* Label for internal jumps. */
typedef MCode *MCLabel;

/* Return label pointing to current PC. */
#define emit_label(as)		((as)->mcp)

static void emit_branch(ASMState *as, MIPSIns mi, Reg rs, Reg rt, MCode *target)
{
  MCode *p = as->mcp;
  ptrdiff_t delta = target - p;
  lua_assert(((delta + 0x8000) >> 16) == 0);
  *--p = mi | MIPSF_S(rs) | MIPSF_T(rt) | ((uint32_t)delta & 0xffffu);
  as->mcp = p;
}

static void emit_jmp(ASMState *as, MCode *target)
{
  *--as->mcp = MIPSI_NOP;
  emit_branch(as, MIPSI_B, RID_ZERO, RID_ZERO, (target));
}

static void emit_call(ASMState *as, void *target)
{
  MCode *p = as->mcp;
  *--p = MIPSI_NOP;
  if ((((uintptr_t)target ^ (uintptr_t)p) >> 28) == 0)
    *--p = MIPSI_JAL | (((uintptr_t)target >>2) & 0x03ffffffu);
  else  /* Target out of range: need indirect call. */
    *--p = MIPSI_JALR | MIPSF_S(RID_CFUNCADDR);
  as->mcp = p;
  ra_allockreg(as, i32ptr(target), RID_CFUNCADDR);
}

/* -- Emit generic operations --------------------------------------------- */

#define emit_move(as, dst, src) \
  emit_ds(as, MIPSI_MOVE, (dst), (src))

/* Generic move between two regs. */
static void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src)
{
  if (dst < RID_MAX_GPR)
    emit_move(as, dst, src);
  else
    emit_fg(as, irt_isnum(ir->t) ? MIPSI_MOV_D : MIPSI_MOV_S, dst, src);
}

/* Generic load of register from stack slot. */
static void emit_spload(ASMState *as, IRIns *ir, Reg r, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_tsi(as, MIPSI_LW, r, RID_SP, ofs);
  else
    emit_tsi(as, irt_isnum(ir->t) ? MIPSI_LDC1 : MIPSI_LWC1,
	     (r & 31), RID_SP, ofs);
}

/* Generic store of register to stack slot. */
static void emit_spstore(ASMState *as, IRIns *ir, Reg r, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_tsi(as, MIPSI_SW, r, RID_SP, ofs);
  else
    emit_tsi(as, irt_isnum(ir->t) ? MIPSI_SDC1 : MIPSI_SWC1,
	     (r&31), RID_SP, ofs);
}

/* Add offset to pointer. */
static void emit_addptr(ASMState *as, Reg r, int32_t ofs)
{
  if (ofs) {
    lua_assert(checki16(ofs));
    emit_tsi(as, MIPSI_ADDIU, r, r, ofs);
  }
}

#define emit_spsub(as, ofs)	emit_addptr(as, RID_SP, -(ofs))

