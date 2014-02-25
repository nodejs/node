/*
** x86/x64 instruction emitter.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

/* -- Emit basic instructions --------------------------------------------- */

#define MODRM(mode, r1, r2)	((MCode)((mode)+(((r1)&7)<<3)+((r2)&7)))

#if LJ_64
#define REXRB(p, rr, rb) \
    { MCode rex = 0x40 + (((rr)>>1)&4) + (((rb)>>3)&1); \
      if (rex != 0x40) *--(p) = rex; }
#define FORCE_REX		0x200
#define REX_64			(FORCE_REX|0x080000)
#else
#define REXRB(p, rr, rb)	((void)0)
#define FORCE_REX		0
#define REX_64			0
#endif

#define emit_i8(as, i)		(*--as->mcp = (MCode)(i))
#define emit_i32(as, i)		(*(int32_t *)(as->mcp-4) = (i), as->mcp -= 4)
#define emit_u32(as, u)		(*(uint32_t *)(as->mcp-4) = (u), as->mcp -= 4)

#define emit_x87op(as, xo) \
  (*(uint16_t *)(as->mcp-2) = (uint16_t)(xo), as->mcp -= 2)

/* op */
static LJ_AINLINE MCode *emit_op(x86Op xo, Reg rr, Reg rb, Reg rx,
				 MCode *p, int delta)
{
  int n = (int8_t)xo;
#if defined(__GNUC__)
  if (__builtin_constant_p(xo) && n == -2)
    p[delta-2] = (MCode)(xo >> 24);
  else if (__builtin_constant_p(xo) && n == -3)
    *(uint16_t *)(p+delta-3) = (uint16_t)(xo >> 16);
  else
#endif
    *(uint32_t *)(p+delta-5) = (uint32_t)xo;
  p += n + delta;
#if LJ_64
  {
    uint32_t rex = 0x40 + ((rr>>1)&(4+(FORCE_REX>>1)))+((rx>>2)&2)+((rb>>3)&1);
    if (rex != 0x40) {
      rex |= (rr >> 16);
      if (n == -4) { *p = (MCode)rex; rex = (MCode)(xo >> 8); }
      else if ((xo & 0xffffff) == 0x6600fd) { *p = (MCode)rex; rex = 0x66; }
      *--p = (MCode)rex;
    }
  }
#else
  UNUSED(rr); UNUSED(rb); UNUSED(rx);
#endif
  return p;
}

/* op + modrm */
#define emit_opm(xo, mode, rr, rb, p, delta) \
  (p[(delta)-1] = MODRM((mode), (rr), (rb)), \
   emit_op((xo), (rr), (rb), 0, (p), (delta)))

/* op + modrm + sib */
#define emit_opmx(xo, mode, scale, rr, rb, rx, p) \
  (p[-1] = MODRM((scale), (rx), (rb)), \
   p[-2] = MODRM((mode), (rr), RID_ESP), \
   emit_op((xo), (rr), (rb), (rx), (p), -1))

/* op r1, r2 */
static void emit_rr(ASMState *as, x86Op xo, Reg r1, Reg r2)
{
  MCode *p = as->mcp;
  as->mcp = emit_opm(xo, XM_REG, r1, r2, p, 0);
}

#if LJ_64 && defined(LUA_USE_ASSERT)
/* [addr] is sign-extended in x64 and must be in lower 2G (not 4G). */
static int32_t ptr2addr(const void *p)
{
  lua_assert((uintptr_t)p < (uintptr_t)0x80000000);
  return i32ptr(p);
}
#else
#define ptr2addr(p)	(i32ptr((p)))
#endif

/* op r, [addr] */
static void emit_rma(ASMState *as, x86Op xo, Reg rr, const void *addr)
{
  MCode *p = as->mcp;
  *(int32_t *)(p-4) = ptr2addr(addr);
#if LJ_64
  p[-5] = MODRM(XM_SCALE1, RID_ESP, RID_EBP);
  as->mcp = emit_opm(xo, XM_OFS0, rr, RID_ESP, p, -5);
#else
  as->mcp = emit_opm(xo, XM_OFS0, rr, RID_EBP, p, -4);
#endif
}

/* op r, [base+ofs] */
static void emit_rmro(ASMState *as, x86Op xo, Reg rr, Reg rb, int32_t ofs)
{
  MCode *p = as->mcp;
  x86Mode mode;
  if (ra_hasreg(rb)) {
    if (ofs == 0 && (rb&7) != RID_EBP) {
      mode = XM_OFS0;
    } else if (checki8(ofs)) {
      *--p = (MCode)ofs;
      mode = XM_OFS8;
    } else {
      p -= 4;
      *(int32_t *)p = ofs;
      mode = XM_OFS32;
    }
    if ((rb&7) == RID_ESP)
      *--p = MODRM(XM_SCALE1, RID_ESP, RID_ESP);
  } else {
    *(int32_t *)(p-4) = ofs;
#if LJ_64
    p[-5] = MODRM(XM_SCALE1, RID_ESP, RID_EBP);
    p -= 5;
    rb = RID_ESP;
#else
    p -= 4;
    rb = RID_EBP;
#endif
    mode = XM_OFS0;
  }
  as->mcp = emit_opm(xo, mode, rr, rb, p, 0);
}

/* op r, [base+idx*scale+ofs] */
static void emit_rmrxo(ASMState *as, x86Op xo, Reg rr, Reg rb, Reg rx,
		       x86Mode scale, int32_t ofs)
{
  MCode *p = as->mcp;
  x86Mode mode;
  if (ofs == 0 && (rb&7) != RID_EBP) {
    mode = XM_OFS0;
  } else if (checki8(ofs)) {
    mode = XM_OFS8;
    *--p = (MCode)ofs;
  } else {
    mode = XM_OFS32;
    p -= 4;
    *(int32_t *)p = ofs;
  }
  as->mcp = emit_opmx(xo, mode, scale, rr, rb, rx, p);
}

/* op r, i */
static void emit_gri(ASMState *as, x86Group xg, Reg rb, int32_t i)
{
  MCode *p = as->mcp;
  x86Op xo;
  if (checki8(i)) {
    *--p = (MCode)i;
    xo = XG_TOXOi8(xg);
  } else {
    p -= 4;
    *(int32_t *)p = i;
    xo = XG_TOXOi(xg);
  }
  as->mcp = emit_opm(xo, XM_REG, (Reg)(xg & 7) | (rb & REX_64), rb, p, 0);
}

/* op [base+ofs], i */
static void emit_gmroi(ASMState *as, x86Group xg, Reg rb, int32_t ofs,
		       int32_t i)
{
  x86Op xo;
  if (checki8(i)) {
    emit_i8(as, i);
    xo = XG_TOXOi8(xg);
  } else {
    emit_i32(as, i);
    xo = XG_TOXOi(xg);
  }
  emit_rmro(as, xo, (Reg)(xg & 7), rb, ofs);
}

#define emit_shifti(as, xg, r, i) \
  (emit_i8(as, (i)), emit_rr(as, XO_SHIFTi, (Reg)(xg), (r)))

/* op r, rm/mrm */
static void emit_mrm(ASMState *as, x86Op xo, Reg rr, Reg rb)
{
  MCode *p = as->mcp;
  x86Mode mode = XM_REG;
  if (rb == RID_MRM) {
    rb = as->mrm.base;
    if (rb == RID_NONE) {
      rb = RID_EBP;
      mode = XM_OFS0;
      p -= 4;
      *(int32_t *)p = as->mrm.ofs;
      if (as->mrm.idx != RID_NONE)
	goto mrmidx;
#if LJ_64
      *--p = MODRM(XM_SCALE1, RID_ESP, RID_EBP);
      rb = RID_ESP;
#endif
    } else {
      if (as->mrm.ofs == 0 && (rb&7) != RID_EBP) {
	mode = XM_OFS0;
      } else if (checki8(as->mrm.ofs)) {
	*--p = (MCode)as->mrm.ofs;
	mode = XM_OFS8;
      } else {
	p -= 4;
	*(int32_t *)p = as->mrm.ofs;
	mode = XM_OFS32;
      }
      if (as->mrm.idx != RID_NONE) {
      mrmidx:
	as->mcp = emit_opmx(xo, mode, as->mrm.scale, rr, rb, as->mrm.idx, p);
	return;
      }
      if ((rb&7) == RID_ESP)
	*--p = MODRM(XM_SCALE1, RID_ESP, RID_ESP);
    }
  }
  as->mcp = emit_opm(xo, mode, rr, rb, p, 0);
}

/* op rm/mrm, i */
static void emit_gmrmi(ASMState *as, x86Group xg, Reg rb, int32_t i)
{
  x86Op xo;
  if (checki8(i)) {
    emit_i8(as, i);
    xo = XG_TOXOi8(xg);
  } else {
    emit_i32(as, i);
    xo = XG_TOXOi(xg);
  }
  emit_mrm(as, xo, (Reg)(xg & 7) | (rb & REX_64), (rb & ~REX_64));
}

/* -- Emit loads/stores --------------------------------------------------- */

/* Instruction selection for XMM moves. */
#define XMM_MOVRR(as)	((as->flags & JIT_F_SPLIT_XMM) ? XO_MOVSD : XO_MOVAPS)
#define XMM_MOVRM(as)	((as->flags & JIT_F_SPLIT_XMM) ? XO_MOVLPD : XO_MOVSD)

/* mov [base+ofs], i */
static void emit_movmroi(ASMState *as, Reg base, int32_t ofs, int32_t i)
{
  emit_i32(as, i);
  emit_rmro(as, XO_MOVmi, 0, base, ofs);
}

/* mov [base+ofs], r */
#define emit_movtomro(as, r, base, ofs) \
  emit_rmro(as, XO_MOVto, (r), (base), (ofs))

/* Get/set global_State fields. */
#define emit_opgl(as, xo, r, field) \
  emit_rma(as, (xo), (r), (void *)&J2G(as->J)->field)
#define emit_getgl(as, r, field)	emit_opgl(as, XO_MOV, (r), field)
#define emit_setgl(as, r, field)	emit_opgl(as, XO_MOVto, (r), field)

#define emit_setvmstate(as, i) \
  (emit_i32(as, i), emit_opgl(as, XO_MOVmi, 0, vmstate))

/* mov r, i / xor r, r */
static void emit_loadi(ASMState *as, Reg r, int32_t i)
{
  /* XOR r,r is shorter, but modifies the flags. This is bad for HIOP. */
  if (i == 0 && !(LJ_32 && (IR(as->curins)->o == IR_HIOP ||
			    (as->curins+1 < as->T->nins &&
			     IR(as->curins+1)->o == IR_HIOP)))) {
    emit_rr(as, XO_ARITH(XOg_XOR), r, r);
  } else {
    MCode *p = as->mcp;
    *(int32_t *)(p-4) = i;
    p[-5] = (MCode)(XI_MOVri+(r&7));
    p -= 5;
    REXRB(p, 0, r);
    as->mcp = p;
  }
}

/* mov r, addr */
#define emit_loada(as, r, addr) \
  emit_loadi(as, (r), ptr2addr((addr)))

#if LJ_64
/* mov r, imm64 or shorter 32 bit extended load. */
static void emit_loadu64(ASMState *as, Reg r, uint64_t u64)
{
  if (checku32(u64)) {  /* 32 bit load clears upper 32 bits. */
    emit_loadi(as, r, (int32_t)u64);
  } else if (checki32((int64_t)u64)) {  /* Sign-extended 32 bit load. */
    MCode *p = as->mcp;
    *(int32_t *)(p-4) = (int32_t)u64;
    as->mcp = emit_opm(XO_MOVmi, XM_REG, REX_64, r, p, -4);
  } else {  /* Full-size 64 bit load. */
    MCode *p = as->mcp;
    *(uint64_t *)(p-8) = u64;
    p[-9] = (MCode)(XI_MOVri+(r&7));
    p[-10] = 0x48 + ((r>>3)&1);
    p -= 10;
    as->mcp = p;
  }
}
#endif

/* movsd r, [&tv->n] / xorps r, r */
static void emit_loadn(ASMState *as, Reg r, cTValue *tv)
{
  if (tvispzero(tv))  /* Use xor only for +0. */
    emit_rr(as, XO_XORPS, r, r);
  else
    emit_rma(as, XMM_MOVRM(as), r, &tv->n);
}

/* -- Emit control-flow instructions -------------------------------------- */

/* Label for short jumps. */
typedef MCode *MCLabel;

#if LJ_32 && LJ_HASFFI
/* jmp short target */
static void emit_sjmp(ASMState *as, MCLabel target)
{
  MCode *p = as->mcp;
  ptrdiff_t delta = target - p;
  lua_assert(delta == (int8_t)delta);
  p[-1] = (MCode)(int8_t)delta;
  p[-2] = XI_JMPs;
  as->mcp = p - 2;
}
#endif

/* jcc short target */
static void emit_sjcc(ASMState *as, int cc, MCLabel target)
{
  MCode *p = as->mcp;
  ptrdiff_t delta = target - p;
  lua_assert(delta == (int8_t)delta);
  p[-1] = (MCode)(int8_t)delta;
  p[-2] = (MCode)(XI_JCCs+(cc&15));
  as->mcp = p - 2;
}

/* jcc short (pending target) */
static MCLabel emit_sjcc_label(ASMState *as, int cc)
{
  MCode *p = as->mcp;
  p[-1] = 0;
  p[-2] = (MCode)(XI_JCCs+(cc&15));
  as->mcp = p - 2;
  return p;
}

/* Fixup jcc short target. */
static void emit_sfixup(ASMState *as, MCLabel source)
{
  source[-1] = (MCode)(as->mcp-source);
}

/* Return label pointing to current PC. */
#define emit_label(as)		((as)->mcp)

/* Compute relative 32 bit offset for jump and call instructions. */
static LJ_AINLINE int32_t jmprel(MCode *p, MCode *target)
{
  ptrdiff_t delta = target - p;
  lua_assert(delta == (int32_t)delta);
  return (int32_t)delta;
}

/* jcc target */
static void emit_jcc(ASMState *as, int cc, MCode *target)
{
  MCode *p = as->mcp;
  *(int32_t *)(p-4) = jmprel(p, target);
  p[-5] = (MCode)(XI_JCCn+(cc&15));
  p[-6] = 0x0f;
  as->mcp = p - 6;
}

/* jmp target */
static void emit_jmp(ASMState *as, MCode *target)
{
  MCode *p = as->mcp;
  *(int32_t *)(p-4) = jmprel(p, target);
  p[-5] = XI_JMP;
  as->mcp = p - 5;
}

/* call target */
static void emit_call_(ASMState *as, MCode *target)
{
  MCode *p = as->mcp;
#if LJ_64
  if (target-p != (int32_t)(target-p)) {
    /* Assumes RID_RET is never an argument to calls and always clobbered. */
    emit_rr(as, XO_GROUP5, XOg_CALL, RID_RET);
    emit_loadu64(as, RID_RET, (uint64_t)target);
    return;
  }
#endif
  *(int32_t *)(p-4) = jmprel(p, target);
  p[-5] = XI_CALL;
  as->mcp = p - 5;
}

#define emit_call(as, f)	emit_call_(as, (MCode *)(void *)(f))

/* -- Emit generic operations --------------------------------------------- */

/* Use 64 bit operations to handle 64 bit IR types. */
#if LJ_64
#define REX_64IR(ir, r)		((r) + (irt_is64((ir)->t) ? REX_64 : 0))
#else
#define REX_64IR(ir, r)		(r)
#endif

/* Generic move between two regs. */
static void emit_movrr(ASMState *as, IRIns *ir, Reg dst, Reg src)
{
  UNUSED(ir);
  if (dst < RID_MAX_GPR)
    emit_rr(as, XO_MOV, REX_64IR(ir, dst), src);
  else
    emit_rr(as, XMM_MOVRR(as), dst, src);
}

/* Generic load of register from stack slot. */
static void emit_spload(ASMState *as, IRIns *ir, Reg r, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_rmro(as, XO_MOV, REX_64IR(ir, r), RID_ESP, ofs);
  else
    emit_rmro(as, irt_isnum(ir->t) ? XMM_MOVRM(as) : XO_MOVSS, r, RID_ESP, ofs);
}

/* Generic store of register to stack slot. */
static void emit_spstore(ASMState *as, IRIns *ir, Reg r, int32_t ofs)
{
  if (r < RID_MAX_GPR)
    emit_rmro(as, XO_MOVto, REX_64IR(ir, r), RID_ESP, ofs);
  else
    emit_rmro(as, irt_isnum(ir->t) ? XO_MOVSDto : XO_MOVSSto, r, RID_ESP, ofs);
}

/* Add offset to pointer. */
static void emit_addptr(ASMState *as, Reg r, int32_t ofs)
{
  if (ofs) {
    if ((as->flags & JIT_F_LEA_AGU))
      emit_rmro(as, XO_LEA, r, r, ofs);
    else
      emit_gri(as, XG_ARITHi(XOg_ADD), r, ofs);
  }
}

#define emit_spsub(as, ofs)	emit_addptr(as, RID_ESP|REX_64, -(ofs))

/* Prefer rematerialization of BASE/L from global_State over spills. */
#define emit_canremat(ref)	((ref) <= REF_BASE)

