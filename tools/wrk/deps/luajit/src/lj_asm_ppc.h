/*
** PPC IR assembler (SSA IR -> machine code).
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

/* -- Register allocator extensions --------------------------------------- */

/* Allocate a register with a hint. */
static Reg ra_hintalloc(ASMState *as, IRRef ref, Reg hint, RegSet allow)
{
  Reg r = IR(ref)->r;
  if (ra_noreg(r)) {
    if (!ra_hashint(r) && !iscrossref(as, ref))
      ra_sethint(IR(ref)->r, hint);  /* Propagate register hint. */
    r = ra_allocref(as, ref, allow);
  }
  ra_noweak(as, r);
  return r;
}

/* Allocate two source registers for three-operand instructions. */
static Reg ra_alloc2(ASMState *as, IRIns *ir, RegSet allow)
{
  IRIns *irl = IR(ir->op1), *irr = IR(ir->op2);
  Reg left = irl->r, right = irr->r;
  if (ra_hasreg(left)) {
    ra_noweak(as, left);
    if (ra_noreg(right))
      right = ra_allocref(as, ir->op2, rset_exclude(allow, left));
    else
      ra_noweak(as, right);
  } else if (ra_hasreg(right)) {
    ra_noweak(as, right);
    left = ra_allocref(as, ir->op1, rset_exclude(allow, right));
  } else if (ra_hashint(right)) {
    right = ra_allocref(as, ir->op2, allow);
    left = ra_alloc1(as, ir->op1, rset_exclude(allow, right));
  } else {
    left = ra_allocref(as, ir->op1, allow);
    right = ra_alloc1(as, ir->op2, rset_exclude(allow, left));
  }
  return left | (right << 8);
}

/* -- Guard handling ------------------------------------------------------ */

/* Setup exit stubs after the end of each trace. */
static void asm_exitstub_setup(ASMState *as, ExitNo nexits)
{
  ExitNo i;
  MCode *mxp = as->mctop;
  /* 1: mflr r0; bl ->vm_exit_handler; li r0, traceno; bl <1; bl <1; ... */
  for (i = nexits-1; (int32_t)i >= 0; i--)
    *--mxp = PPCI_BL|(((-3-i)&0x00ffffffu)<<2);
  *--mxp = PPCI_LI|PPCF_T(RID_TMP)|as->T->traceno;  /* Read by exit handler. */
  mxp--;
  *mxp = PPCI_BL|((((MCode *)(void *)lj_vm_exit_handler-mxp)&0x00ffffffu)<<2);
  *--mxp = PPCI_MFLR|PPCF_T(RID_TMP);
  as->mctop = mxp;
}

static MCode *asm_exitstub_addr(ASMState *as, ExitNo exitno)
{
  /* Keep this in-sync with exitstub_trace_addr(). */
  return as->mctop + exitno + 3;
}

/* Emit conditional branch to exit for guard. */
static void asm_guardcc(ASMState *as, PPCCC cc)
{
  MCode *target = asm_exitstub_addr(as, as->snapno);
  MCode *p = as->mcp;
  if (LJ_UNLIKELY(p == as->invmcp)) {
    as->loopinv = 1;
    *p = PPCI_B | (((target-p) & 0x00ffffffu) << 2);
    emit_condbranch(as, PPCI_BC, cc^4, p);
    return;
  }
  emit_condbranch(as, PPCI_BC, cc, target);
}

/* -- Operand fusion ------------------------------------------------------ */

/* Limit linear search to this distance. Avoids O(n^2) behavior. */
#define CONFLICT_SEARCH_LIM	31

/* Check if there's no conflicting instruction between curins and ref. */
static int noconflict(ASMState *as, IRRef ref, IROp conflict)
{
  IRIns *ir = as->ir;
  IRRef i = as->curins;
  if (i > ref + CONFLICT_SEARCH_LIM)
    return 0;  /* Give up, ref is too far away. */
  while (--i > ref)
    if (ir[i].o == conflict)
      return 0;  /* Conflict found. */
  return 1;  /* Ok, no conflict. */
}

/* Fuse the array base of colocated arrays. */
static int32_t asm_fuseabase(ASMState *as, IRRef ref)
{
  IRIns *ir = IR(ref);
  if (ir->o == IR_TNEW && ir->op1 <= LJ_MAX_COLOSIZE &&
      !neverfuse(as) && noconflict(as, ref, IR_NEWREF))
    return (int32_t)sizeof(GCtab);
  return 0;
}

/* Indicates load/store indexed is ok. */
#define AHUREF_LSX	((int32_t)0x80000000)

/* Fuse array/hash/upvalue reference into register+offset operand. */
static Reg asm_fuseahuref(ASMState *as, IRRef ref, int32_t *ofsp, RegSet allow)
{
  IRIns *ir = IR(ref);
  if (ra_noreg(ir->r)) {
    if (ir->o == IR_AREF) {
      if (mayfuse(as, ref)) {
	if (irref_isk(ir->op2)) {
	  IRRef tab = IR(ir->op1)->op1;
	  int32_t ofs = asm_fuseabase(as, tab);
	  IRRef refa = ofs ? tab : ir->op1;
	  ofs += 8*IR(ir->op2)->i;
	  if (checki16(ofs)) {
	    *ofsp = ofs;
	    return ra_alloc1(as, refa, allow);
	  }
	}
	if (*ofsp == AHUREF_LSX) {
	  Reg base = ra_alloc1(as, ir->op1, allow);
	  Reg idx = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, base));
	  return base | (idx << 8);
	}
      }
    } else if (ir->o == IR_HREFK) {
      if (mayfuse(as, ref)) {
	int32_t ofs = (int32_t)(IR(ir->op2)->op2 * sizeof(Node));
	if (checki16(ofs)) {
	  *ofsp = ofs;
	  return ra_alloc1(as, ir->op1, allow);
	}
      }
    } else if (ir->o == IR_UREFC) {
      if (irref_isk(ir->op1)) {
	GCfunc *fn = ir_kfunc(IR(ir->op1));
	int32_t ofs = i32ptr(&gcref(fn->l.uvptr[(ir->op2 >> 8)])->uv.tv);
	int32_t jgl = (intptr_t)J2G(as->J);
	if ((uint32_t)(ofs-jgl) < 65536) {
	  *ofsp = ofs-jgl-32768;
	  return RID_JGL;
	} else {
	  *ofsp = (int16_t)ofs;
	  return ra_allock(as, ofs-(int16_t)ofs, allow);
	}
      }
    }
  }
  *ofsp = 0;
  return ra_alloc1(as, ref, allow);
}

/* Fuse XLOAD/XSTORE reference into load/store operand. */
static void asm_fusexref(ASMState *as, PPCIns pi, Reg rt, IRRef ref,
			 RegSet allow, int32_t ofs)
{
  IRIns *ir = IR(ref);
  Reg base;
  if (ra_noreg(ir->r) && canfuse(as, ir)) {
    if (ir->o == IR_ADD) {
      int32_t ofs2;
      if (irref_isk(ir->op2) && (ofs2 = ofs + IR(ir->op2)->i, checki16(ofs2))) {
	ofs = ofs2;
	ref = ir->op1;
      } else if (ofs == 0) {
	Reg right, left = ra_alloc2(as, ir, allow);
	right = (left >> 8); left &= 255;
	emit_fab(as, PPCI_LWZX | ((pi >> 20) & 0x780), rt, left, right);
	return;
      }
    } else if (ir->o == IR_STRREF) {
      lua_assert(ofs == 0);
      ofs = (int32_t)sizeof(GCstr);
      if (irref_isk(ir->op2)) {
	ofs += IR(ir->op2)->i;
	ref = ir->op1;
      } else if (irref_isk(ir->op1)) {
	ofs += IR(ir->op1)->i;
	ref = ir->op2;
      } else {
	/* NYI: Fuse ADD with constant. */
	Reg tmp, right, left = ra_alloc2(as, ir, allow);
	right = (left >> 8); left &= 255;
	tmp = ra_scratch(as, rset_exclude(rset_exclude(allow, left), right));
	emit_fai(as, pi, rt, tmp, ofs);
	emit_tab(as, PPCI_ADD, tmp, left, right);
	return;
      }
      if (!checki16(ofs)) {
	Reg left = ra_alloc1(as, ref, allow);
	Reg right = ra_allock(as, ofs, rset_exclude(allow, left));
	emit_fab(as, PPCI_LWZX | ((pi >> 20) & 0x780), rt, left, right);
	return;
      }
    }
  }
  base = ra_alloc1(as, ref, allow);
  emit_fai(as, pi, rt, base, ofs);
}

/* Fuse XLOAD/XSTORE reference into indexed-only load/store operand. */
static void asm_fusexrefx(ASMState *as, PPCIns pi, Reg rt, IRRef ref,
			  RegSet allow)
{
  IRIns *ira = IR(ref);
  Reg right, left;
  if (canfuse(as, ira) && ira->o == IR_ADD && ra_noreg(ira->r)) {
    left = ra_alloc2(as, ira, allow);
    right = (left >> 8); left &= 255;
  } else {
    right = ra_alloc1(as, ref, allow);
    left = RID_R0;
  }
  emit_tab(as, pi, rt, left, right);
}

/* Fuse to multiply-add/sub instruction. */
static int asm_fusemadd(ASMState *as, IRIns *ir, PPCIns pi, PPCIns pir)
{
  IRRef lref = ir->op1, rref = ir->op2;
  IRIns *irm;
  if (lref != rref &&
      ((mayfuse(as, lref) && (irm = IR(lref), irm->o == IR_MUL) &&
	ra_noreg(irm->r)) ||
       (mayfuse(as, rref) && (irm = IR(rref), irm->o == IR_MUL) &&
	(rref = lref, pi = pir, ra_noreg(irm->r))))) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg add = ra_alloc1(as, rref, RSET_FPR);
    Reg right, left = ra_alloc2(as, irm, rset_exclude(RSET_FPR, add));
    right = (left >> 8); left &= 255;
    emit_facb(as, pi, dest, left, right, add);
    return 1;
  }
  return 0;
}

/* -- Calls --------------------------------------------------------------- */

/* Generate a call to a C function. */
static void asm_gencall(ASMState *as, const CCallInfo *ci, IRRef *args)
{
  uint32_t n, nargs = CCI_NARGS(ci);
  int32_t ofs = 8;
  Reg gpr = REGARG_FIRSTGPR, fpr = REGARG_FIRSTFPR;
  if ((void *)ci->func)
    emit_call(as, (void *)ci->func);
  for (n = 0; n < nargs; n++) {  /* Setup args. */
    IRRef ref = args[n];
    if (ref) {
      IRIns *ir = IR(ref);
      if (irt_isfp(ir->t)) {
	if (fpr <= REGARG_LASTFPR) {
	  lua_assert(rset_test(as->freeset, fpr));  /* Already evicted. */
	  ra_leftov(as, fpr, ref);
	  fpr++;
	} else {
	  Reg r = ra_alloc1(as, ref, RSET_FPR);
	  if (irt_isnum(ir->t)) ofs = (ofs + 4) & ~4;
	  emit_spstore(as, ir, r, ofs);
	  ofs += irt_isnum(ir->t) ? 8 : 4;
	}
      } else {
	if (gpr <= REGARG_LASTGPR) {
	  lua_assert(rset_test(as->freeset, gpr));  /* Already evicted. */
	  ra_leftov(as, gpr, ref);
	  gpr++;
	} else {
	  Reg r = ra_alloc1(as, ref, RSET_GPR);
	  emit_spstore(as, ir, r, ofs);
	  ofs += 4;
	}
      }
    } else {
      if (gpr <= REGARG_LASTGPR)
	gpr++;
      else
	ofs += 4;
    }
    checkmclim(as);
  }
  if ((ci->flags & CCI_VARARG))  /* Vararg calls need to know about FPR use. */
    emit_tab(as, fpr == REGARG_FIRSTFPR ? PPCI_CRXOR : PPCI_CREQV, 6, 6, 6);
}

/* Setup result reg/sp for call. Evict scratch regs. */
static void asm_setupresult(ASMState *as, IRIns *ir, const CCallInfo *ci)
{
  RegSet drop = RSET_SCRATCH;
  int hiop = ((ir+1)->o == IR_HIOP);
  if ((ci->flags & CCI_NOFPRCLOBBER))
    drop &= ~RSET_FPR;
  if (ra_hasreg(ir->r))
    rset_clear(drop, ir->r);  /* Dest reg handled below. */
  if (hiop && ra_hasreg((ir+1)->r))
    rset_clear(drop, (ir+1)->r);  /* Dest reg handled below. */
  ra_evictset(as, drop);  /* Evictions must be performed first. */
  if (ra_used(ir)) {
    lua_assert(!irt_ispri(ir->t));
    if (irt_isfp(ir->t)) {
      if ((ci->flags & CCI_CASTU64)) {
	/* Use spill slot or temp slots. */
	int32_t ofs = ir->s ? sps_scale(ir->s) : SPOFS_TMP;
	Reg dest = ir->r;
	if (ra_hasreg(dest)) {
	  ra_free(as, dest);
	  ra_modified(as, dest);
	  emit_fai(as, PPCI_LFD, dest, RID_SP, ofs);
	}
	emit_tai(as, PPCI_STW, RID_RETHI, RID_SP, ofs);
	emit_tai(as, PPCI_STW, RID_RETLO, RID_SP, ofs+4);
      } else {
	ra_destreg(as, ir, RID_FPRET);
      }
    } else if (hiop) {
      ra_destpair(as, ir);
    } else {
      ra_destreg(as, ir, RID_RET);
    }
  }
}

static void asm_call(ASMState *as, IRIns *ir)
{
  IRRef args[CCI_NARGS_MAX];
  const CCallInfo *ci = &lj_ir_callinfo[ir->op2];
  asm_collectargs(as, ir, ci, args);
  asm_setupresult(as, ir, ci);
  asm_gencall(as, ci, args);
}

static void asm_callx(ASMState *as, IRIns *ir)
{
  IRRef args[CCI_NARGS_MAX*2];
  CCallInfo ci;
  IRRef func;
  IRIns *irf;
  ci.flags = asm_callx_flags(as, ir);
  asm_collectargs(as, ir, &ci, args);
  asm_setupresult(as, ir, &ci);
  func = ir->op2; irf = IR(func);
  if (irf->o == IR_CARG) { func = irf->op1; irf = IR(func); }
  if (irref_isk(func)) {  /* Call to constant address. */
    ci.func = (ASMFunction)(void *)(irf->i);
  } else {  /* Need a non-argument register for indirect calls. */
    RegSet allow = RSET_GPR & ~RSET_RANGE(RID_R0, REGARG_LASTGPR+1);
    Reg freg = ra_alloc1(as, func, allow);
    *--as->mcp = PPCI_BCTRL;
    *--as->mcp = PPCI_MTCTR | PPCF_T(freg);
    ci.func = (ASMFunction)(void *)0;
  }
  asm_gencall(as, &ci, args);
}

static void asm_callid(ASMState *as, IRIns *ir, IRCallID id)
{
  const CCallInfo *ci = &lj_ir_callinfo[id];
  IRRef args[2];
  args[0] = ir->op1;
  args[1] = ir->op2;
  asm_setupresult(as, ir, ci);
  asm_gencall(as, ci, args);
}

/* -- Returns ------------------------------------------------------------- */

/* Return to lower frame. Guard that it goes to the right spot. */
static void asm_retf(ASMState *as, IRIns *ir)
{
  Reg base = ra_alloc1(as, REF_BASE, RSET_GPR);
  void *pc = ir_kptr(IR(ir->op2));
  int32_t delta = 1+bc_a(*((const BCIns *)pc - 1));
  as->topslot -= (BCReg)delta;
  if ((int32_t)as->topslot < 0) as->topslot = 0;
  emit_setgl(as, base, jit_base);
  emit_addptr(as, base, -8*delta);
  asm_guardcc(as, CC_NE);
  emit_ab(as, PPCI_CMPW, RID_TMP,
	  ra_allock(as, i32ptr(pc), rset_exclude(RSET_GPR, base)));
  emit_tai(as, PPCI_LWZ, RID_TMP, base, -8);
}

/* -- Type conversions ---------------------------------------------------- */

static void asm_tointg(ASMState *as, IRIns *ir, Reg left)
{
  RegSet allow = RSET_FPR;
  Reg tmp = ra_scratch(as, rset_clear(allow, left));
  Reg fbias = ra_scratch(as, rset_clear(allow, tmp));
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg hibias = ra_allock(as, 0x43300000, rset_exclude(RSET_GPR, dest));
  asm_guardcc(as, CC_NE);
  emit_fab(as, PPCI_FCMPU, 0, tmp, left);
  emit_fab(as, PPCI_FSUB, tmp, tmp, fbias);
  emit_fai(as, PPCI_LFD, tmp, RID_SP, SPOFS_TMP);
  emit_tai(as, PPCI_STW, RID_TMP, RID_SP, SPOFS_TMPLO);
  emit_tai(as, PPCI_STW, hibias, RID_SP, SPOFS_TMPHI);
  emit_asi(as, PPCI_XORIS, RID_TMP, dest, 0x8000);
  emit_tai(as, PPCI_LWZ, dest, RID_SP, SPOFS_TMPLO);
  emit_lsptr(as, PPCI_LFS, (fbias & 31),
	     (void *)lj_ir_k64_find(as->J, U64x(59800004,59800000)),
	     RSET_GPR);
  emit_fai(as, PPCI_STFD, tmp, RID_SP, SPOFS_TMP);
  emit_fb(as, PPCI_FCTIWZ, tmp, left);
}

static void asm_tobit(ASMState *as, IRIns *ir)
{
  RegSet allow = RSET_FPR;
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg left = ra_alloc1(as, ir->op1, allow);
  Reg right = ra_alloc1(as, ir->op2, rset_clear(allow, left));
  Reg tmp = ra_scratch(as, rset_clear(allow, right));
  emit_tai(as, PPCI_LWZ, dest, RID_SP, SPOFS_TMPLO);
  emit_fai(as, PPCI_STFD, tmp, RID_SP, SPOFS_TMP);
  emit_fab(as, PPCI_FADD, tmp, left, right);
}

static void asm_conv(ASMState *as, IRIns *ir)
{
  IRType st = (IRType)(ir->op2 & IRCONV_SRCMASK);
  int stfp = (st == IRT_NUM || st == IRT_FLOAT);
  IRRef lref = ir->op1;
  lua_assert(irt_type(ir->t) != st);
  lua_assert(!(irt_isint64(ir->t) ||
	       (st == IRT_I64 || st == IRT_U64))); /* Handled by SPLIT. */
  if (irt_isfp(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    if (stfp) {  /* FP to FP conversion. */
      if (st == IRT_NUM)  /* double -> float conversion. */
	emit_fb(as, PPCI_FRSP, dest, ra_alloc1(as, lref, RSET_FPR));
      else  /* float -> double conversion is a no-op on PPC. */
	ra_leftov(as, dest, lref);  /* Do nothing, but may need to move regs. */
    } else {  /* Integer to FP conversion. */
      /* IRT_INT: Flip hibit, bias with 2^52, subtract 2^52+2^31. */
      /* IRT_U32: Bias with 2^52, subtract 2^52. */
      RegSet allow = RSET_GPR;
      Reg left = ra_alloc1(as, lref, allow);
      Reg hibias = ra_allock(as, 0x43300000, rset_clear(allow, left));
      Reg fbias = ra_scratch(as, rset_exclude(RSET_FPR, dest));
      const float *kbias;
      if (irt_isfloat(ir->t)) emit_fb(as, PPCI_FRSP, dest, dest);
      emit_fab(as, PPCI_FSUB, dest, dest, fbias);
      emit_fai(as, PPCI_LFD, dest, RID_SP, SPOFS_TMP);
      kbias = (const float *)lj_ir_k64_find(as->J, U64x(59800004,59800000));
      if (st == IRT_U32) kbias++;
      emit_lsptr(as, PPCI_LFS, (fbias & 31), (void *)kbias,
		 rset_clear(allow, hibias));
      emit_tai(as, PPCI_STW, st == IRT_U32 ? left : RID_TMP,
	       RID_SP, SPOFS_TMPLO);
      emit_tai(as, PPCI_STW, hibias, RID_SP, SPOFS_TMPHI);
      if (st != IRT_U32) emit_asi(as, PPCI_XORIS, RID_TMP, left, 0x8000);
    }
  } else if (stfp) {  /* FP to integer conversion. */
    if (irt_isguard(ir->t)) {
      /* Checked conversions are only supported from number to int. */
      lua_assert(irt_isint(ir->t) && st == IRT_NUM);
      asm_tointg(as, ir, ra_alloc1(as, lref, RSET_FPR));
    } else {
      Reg dest = ra_dest(as, ir, RSET_GPR);
      Reg left = ra_alloc1(as, lref, RSET_FPR);
      Reg tmp = ra_scratch(as, rset_exclude(RSET_FPR, left));
      if (irt_isu32(ir->t)) {
	/* Convert both x and x-2^31 to int and merge results. */
	Reg tmpi = ra_scratch(as, rset_exclude(RSET_GPR, dest));
	emit_asb(as, PPCI_OR, dest, dest, tmpi);  /* Select with mask idiom. */
	emit_asb(as, PPCI_AND, tmpi, tmpi, RID_TMP);
	emit_asb(as, PPCI_ANDC, dest, dest, RID_TMP);
	emit_tai(as, PPCI_LWZ, tmpi, RID_SP, SPOFS_TMPLO);  /* tmp = (int)(x) */
	emit_tai(as, PPCI_ADDIS, dest, dest, 0x8000);  /* dest += 2^31 */
	emit_asb(as, PPCI_SRAWI, RID_TMP, dest, 31);  /* mask = -(dest < 0) */
	emit_fai(as, PPCI_STFD, tmp, RID_SP, SPOFS_TMP);
	emit_tai(as, PPCI_LWZ, dest,
		 RID_SP, SPOFS_TMPLO);  /* dest = (int)(x-2^31) */
	emit_fb(as, PPCI_FCTIWZ, tmp, left);
	emit_fai(as, PPCI_STFD, tmp, RID_SP, SPOFS_TMP);
	emit_fb(as, PPCI_FCTIWZ, tmp, tmp);
	emit_fab(as, PPCI_FSUB, tmp, left, tmp);
	emit_lsptr(as, PPCI_LFS, (tmp & 31),
		   (void *)lj_ir_k64_find(as->J, U64x(4f000000,00000000)),
		   RSET_GPR);
      } else {
	emit_tai(as, PPCI_LWZ, dest, RID_SP, SPOFS_TMPLO);
	emit_fai(as, PPCI_STFD, tmp, RID_SP, SPOFS_TMP);
	emit_fb(as, PPCI_FCTIWZ, tmp, left);
      }
    }
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    if (st >= IRT_I8 && st <= IRT_U16) {  /* Extend to 32 bit integer. */
      Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
      lua_assert(irt_isint(ir->t) || irt_isu32(ir->t));
      if ((ir->op2 & IRCONV_SEXT))
	emit_as(as, st == IRT_I8 ? PPCI_EXTSB : PPCI_EXTSH, dest, left);
      else
	emit_rot(as, PPCI_RLWINM, dest, left, 0, st == IRT_U8 ? 24 : 16, 31);
    } else {  /* 32/64 bit integer conversions. */
      /* Only need to handle 32/32 bit no-op (cast) on 32 bit archs. */
      ra_leftov(as, dest, lref);  /* Do nothing, but may need to move regs. */
    }
  }
}

#if LJ_HASFFI
static void asm_conv64(ASMState *as, IRIns *ir)
{
  IRType st = (IRType)((ir-1)->op2 & IRCONV_SRCMASK);
  IRType dt = (((ir-1)->op2 & IRCONV_DSTMASK) >> IRCONV_DSH);
  IRCallID id;
  const CCallInfo *ci;
  IRRef args[2];
  args[0] = ir->op1;
  args[1] = (ir-1)->op1;
  if (st == IRT_NUM || st == IRT_FLOAT) {
    id = IRCALL_fp64_d2l + ((st == IRT_FLOAT) ? 2 : 0) + (dt - IRT_I64);
    ir--;
  } else {
    id = IRCALL_fp64_l2d + ((dt == IRT_FLOAT) ? 2 : 0) + (st - IRT_I64);
  }
  ci = &lj_ir_callinfo[id];
  asm_setupresult(as, ir, ci);
  asm_gencall(as, ci, args);
}
#endif

static void asm_strto(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_strscan_num];
  IRRef args[2];
  int32_t ofs;
  RegSet drop = RSET_SCRATCH;
  if (ra_hasreg(ir->r)) rset_set(drop, ir->r);  /* Spill dest reg (if any). */
  ra_evictset(as, drop);
  asm_guardcc(as, CC_EQ);
  emit_ai(as, PPCI_CMPWI, RID_RET, 0);  /* Test return status. */
  args[0] = ir->op1;      /* GCstr *str */
  args[1] = ASMREF_TMP1;  /* TValue *n  */
  asm_gencall(as, ci, args);
  /* Store the result to the spill slot or temp slots. */
  ofs = ir->s ? sps_scale(ir->s) : SPOFS_TMP;
  emit_tai(as, PPCI_ADDI, ra_releasetmp(as, ASMREF_TMP1), RID_SP, ofs);
}

/* Get pointer to TValue. */
static void asm_tvptr(ASMState *as, Reg dest, IRRef ref)
{
  IRIns *ir = IR(ref);
  if (irt_isnum(ir->t)) {
    if (irref_isk(ref))  /* Use the number constant itself as a TValue. */
      ra_allockreg(as, i32ptr(ir_knum(ir)), dest);
    else  /* Otherwise force a spill and use the spill slot. */
      emit_tai(as, PPCI_ADDI, dest, RID_SP, ra_spill(as, ir));
  } else {
    /* Otherwise use g->tmptv to hold the TValue. */
    RegSet allow = rset_exclude(RSET_GPR, dest);
    Reg type;
    emit_tai(as, PPCI_ADDI, dest, RID_JGL, offsetof(global_State, tmptv)-32768);
    if (!irt_ispri(ir->t)) {
      Reg src = ra_alloc1(as, ref, allow);
      emit_setgl(as, src, tmptv.gcr);
    }
    type = ra_allock(as, irt_toitype(ir->t), allow);
    emit_setgl(as, type, tmptv.it);
  }
}

static void asm_tostr(ASMState *as, IRIns *ir)
{
  IRRef args[2];
  args[0] = ASMREF_L;
  as->gcsteps++;
  if (irt_isnum(IR(ir->op1)->t) || (ir+1)->o == IR_HIOP) {
    const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_str_fromnum];
    args[1] = ASMREF_TMP1;  /* const lua_Number * */
    asm_setupresult(as, ir, ci);  /* GCstr * */
    asm_gencall(as, ci, args);
    asm_tvptr(as, ra_releasetmp(as, ASMREF_TMP1), ir->op1);
  } else {
    const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_str_fromint];
    args[1] = ir->op1;  /* int32_t k */
    asm_setupresult(as, ir, ci);  /* GCstr * */
    asm_gencall(as, ci, args);
  }
}

/* -- Memory references --------------------------------------------------- */

static void asm_aref(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg idx, base;
  if (irref_isk(ir->op2)) {
    IRRef tab = IR(ir->op1)->op1;
    int32_t ofs = asm_fuseabase(as, tab);
    IRRef refa = ofs ? tab : ir->op1;
    ofs += 8*IR(ir->op2)->i;
    if (checki16(ofs)) {
      base = ra_alloc1(as, refa, RSET_GPR);
      emit_tai(as, PPCI_ADDI, dest, base, ofs);
      return;
    }
  }
  base = ra_alloc1(as, ir->op1, RSET_GPR);
  idx = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, base));
  emit_tab(as, PPCI_ADD, dest, RID_TMP, base);
  emit_slwi(as, RID_TMP, idx, 3);
}

/* Inlined hash lookup. Specialized for key type and for const keys.
** The equivalent C code is:
**   Node *n = hashkey(t, key);
**   do {
**     if (lj_obj_equal(&n->key, key)) return &n->val;
**   } while ((n = nextnode(n)));
**   return niltv(L);
*/
static void asm_href(ASMState *as, IRIns *ir, IROp merge)
{
  RegSet allow = RSET_GPR;
  int destused = ra_used(ir);
  Reg dest = ra_dest(as, ir, allow);
  Reg tab = ra_alloc1(as, ir->op1, rset_clear(allow, dest));
  Reg key = RID_NONE, tmp1 = RID_TMP, tmp2;
  Reg tisnum = RID_NONE, tmpnum = RID_NONE;
  IRRef refkey = ir->op2;
  IRIns *irkey = IR(refkey);
  IRType1 kt = irkey->t;
  uint32_t khash;
  MCLabel l_end, l_loop, l_next;

  rset_clear(allow, tab);
  if (irt_isnum(kt)) {
    key = ra_alloc1(as, refkey, RSET_FPR);
    tmpnum = ra_scratch(as, rset_exclude(RSET_FPR, key));
    tisnum = ra_allock(as, (int32_t)LJ_TISNUM, allow);
    rset_clear(allow, tisnum);
  } else if (!irt_ispri(kt)) {
    key = ra_alloc1(as, refkey, allow);
    rset_clear(allow, key);
  }
  tmp2 = ra_scratch(as, allow);
  rset_clear(allow, tmp2);

  /* Key not found in chain: jump to exit (if merged) or load niltv. */
  l_end = emit_label(as);
  as->invmcp = NULL;
  if (merge == IR_NE)
    asm_guardcc(as, CC_EQ);
  else if (destused)
    emit_loada(as, dest, niltvg(J2G(as->J)));

  /* Follow hash chain until the end. */
  l_loop = --as->mcp;
  emit_ai(as, PPCI_CMPWI, dest, 0);
  emit_tai(as, PPCI_LWZ, dest, dest, (int32_t)offsetof(Node, next));
  l_next = emit_label(as);

  /* Type and value comparison. */
  if (merge == IR_EQ)
    asm_guardcc(as, CC_EQ);
  else
    emit_condbranch(as, PPCI_BC|PPCF_Y, CC_EQ, l_end);
  if (irt_isnum(kt)) {
    emit_fab(as, PPCI_FCMPU, 0, tmpnum, key);
    emit_condbranch(as, PPCI_BC, CC_GE, l_next);
    emit_ab(as, PPCI_CMPLW, tmp1, tisnum);
    emit_fai(as, PPCI_LFD, tmpnum, dest, (int32_t)offsetof(Node, key.n));
  } else {
    if (!irt_ispri(kt)) {
      emit_ab(as, PPCI_CMPW, tmp2, key);
      emit_condbranch(as, PPCI_BC, CC_NE, l_next);
    }
    emit_ai(as, PPCI_CMPWI, tmp1, irt_toitype(irkey->t));
    if (!irt_ispri(kt))
      emit_tai(as, PPCI_LWZ, tmp2, dest, (int32_t)offsetof(Node, key.gcr));
  }
  emit_tai(as, PPCI_LWZ, tmp1, dest, (int32_t)offsetof(Node, key.it));
  *l_loop = PPCI_BC | PPCF_Y | PPCF_CC(CC_NE) |
	    (((char *)as->mcp-(char *)l_loop) & 0xffffu);

  /* Load main position relative to tab->node into dest. */
  khash = irref_isk(refkey) ? ir_khash(irkey) : 1;
  if (khash == 0) {
    emit_tai(as, PPCI_LWZ, dest, tab, (int32_t)offsetof(GCtab, node));
  } else {
    Reg tmphash = tmp1;
    if (irref_isk(refkey))
      tmphash = ra_allock(as, khash, allow);
    emit_tab(as, PPCI_ADD, dest, dest, tmp1);
    emit_tai(as, PPCI_MULLI, tmp1, tmp1, sizeof(Node));
    emit_asb(as, PPCI_AND, tmp1, tmp2, tmphash);
    emit_tai(as, PPCI_LWZ, dest, tab, (int32_t)offsetof(GCtab, node));
    emit_tai(as, PPCI_LWZ, tmp2, tab, (int32_t)offsetof(GCtab, hmask));
    if (irref_isk(refkey)) {
      /* Nothing to do. */
    } else if (irt_isstr(kt)) {
      emit_tai(as, PPCI_LWZ, tmp1, key, (int32_t)offsetof(GCstr, hash));
    } else {  /* Must match with hash*() in lj_tab.c. */
      emit_tab(as, PPCI_SUBF, tmp1, tmp2, tmp1);
      emit_rotlwi(as, tmp2, tmp2, HASH_ROT3);
      emit_asb(as, PPCI_XOR, tmp1, tmp1, tmp2);
      emit_rotlwi(as, tmp1, tmp1, (HASH_ROT2+HASH_ROT1)&31);
      emit_tab(as, PPCI_SUBF, tmp2, dest, tmp2);
      if (irt_isnum(kt)) {
	int32_t ofs = ra_spill(as, irkey);
	emit_asb(as, PPCI_XOR, tmp2, tmp2, tmp1);
	emit_rotlwi(as, dest, tmp1, HASH_ROT1);
	emit_tab(as, PPCI_ADD, tmp1, tmp1, tmp1);
	emit_tai(as, PPCI_LWZ, tmp2, RID_SP, ofs+4);
	emit_tai(as, PPCI_LWZ, tmp1, RID_SP, ofs);
      } else {
	emit_asb(as, PPCI_XOR, tmp2, key, tmp1);
	emit_rotlwi(as, dest, tmp1, HASH_ROT1);
	emit_tai(as, PPCI_ADDI, tmp1, tmp2, HASH_BIAS);
	emit_tai(as, PPCI_ADDIS, tmp2, key, (HASH_BIAS + 32768)>>16);
      }
    }
  }
}

static void asm_hrefk(ASMState *as, IRIns *ir)
{
  IRIns *kslot = IR(ir->op2);
  IRIns *irkey = IR(kslot->op1);
  int32_t ofs = (int32_t)(kslot->op2 * sizeof(Node));
  int32_t kofs = ofs + (int32_t)offsetof(Node, key);
  Reg dest = (ra_used(ir)||ofs > 32736) ? ra_dest(as, ir, RSET_GPR) : RID_NONE;
  Reg node = ra_alloc1(as, ir->op1, RSET_GPR);
  Reg key = RID_NONE, type = RID_TMP, idx = node;
  RegSet allow = rset_exclude(RSET_GPR, node);
  lua_assert(ofs % sizeof(Node) == 0);
  if (ofs > 32736) {
    idx = dest;
    rset_clear(allow, dest);
    kofs = (int32_t)offsetof(Node, key);
  } else if (ra_hasreg(dest)) {
    emit_tai(as, PPCI_ADDI, dest, node, ofs);
  }
  asm_guardcc(as, CC_NE);
  if (!irt_ispri(irkey->t)) {
    key = ra_scratch(as, allow);
    rset_clear(allow, key);
  }
  rset_clear(allow, type);
  if (irt_isnum(irkey->t)) {
    emit_cmpi(as, key, (int32_t)ir_knum(irkey)->u32.lo);
    asm_guardcc(as, CC_NE);
    emit_cmpi(as, type, (int32_t)ir_knum(irkey)->u32.hi);
  } else {
    if (ra_hasreg(key)) {
      emit_cmpi(as, key, irkey->i);  /* May use RID_TMP, i.e. type. */
      asm_guardcc(as, CC_NE);
    }
    emit_ai(as, PPCI_CMPWI, type, irt_toitype(irkey->t));
  }
  if (ra_hasreg(key)) emit_tai(as, PPCI_LWZ, key, idx, kofs+4);
  emit_tai(as, PPCI_LWZ, type, idx, kofs);
  if (ofs > 32736) {
    emit_tai(as, PPCI_ADDIS, dest, dest, (ofs + 32768) >> 16);
    emit_tai(as, PPCI_ADDI, dest, node, ofs);
  }
}

static void asm_newref(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_tab_newkey];
  IRRef args[3];
  if (ir->r == RID_SINK)
    return;
  args[0] = ASMREF_L;     /* lua_State *L */
  args[1] = ir->op1;      /* GCtab *t     */
  args[2] = ASMREF_TMP1;  /* cTValue *key */
  asm_setupresult(as, ir, ci);  /* TValue * */
  asm_gencall(as, ci, args);
  asm_tvptr(as, ra_releasetmp(as, ASMREF_TMP1), ir->op2);
}

static void asm_uref(ASMState *as, IRIns *ir)
{
  /* NYI: Check that UREFO is still open and not aliasing a slot. */
  Reg dest = ra_dest(as, ir, RSET_GPR);
  if (irref_isk(ir->op1)) {
    GCfunc *fn = ir_kfunc(IR(ir->op1));
    MRef *v = &gcref(fn->l.uvptr[(ir->op2 >> 8)])->uv.v;
    emit_lsptr(as, PPCI_LWZ, dest, v, RSET_GPR);
  } else {
    Reg uv = ra_scratch(as, RSET_GPR);
    Reg func = ra_alloc1(as, ir->op1, RSET_GPR);
    if (ir->o == IR_UREFC) {
      asm_guardcc(as, CC_NE);
      emit_ai(as, PPCI_CMPWI, RID_TMP, 1);
      emit_tai(as, PPCI_ADDI, dest, uv, (int32_t)offsetof(GCupval, tv));
      emit_tai(as, PPCI_LBZ, RID_TMP, uv, (int32_t)offsetof(GCupval, closed));
    } else {
      emit_tai(as, PPCI_LWZ, dest, uv, (int32_t)offsetof(GCupval, v));
    }
    emit_tai(as, PPCI_LWZ, uv, func,
	     (int32_t)offsetof(GCfuncL, uvptr) + 4*(int32_t)(ir->op2 >> 8));
  }
}

static void asm_fref(ASMState *as, IRIns *ir)
{
  UNUSED(as); UNUSED(ir);
  lua_assert(!ra_used(ir));
}

static void asm_strref(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  IRRef ref = ir->op2, refk = ir->op1;
  int32_t ofs = (int32_t)sizeof(GCstr);
  Reg r;
  if (irref_isk(ref)) {
    IRRef tmp = refk; refk = ref; ref = tmp;
  } else if (!irref_isk(refk)) {
    Reg right, left = ra_alloc1(as, ir->op1, RSET_GPR);
    IRIns *irr = IR(ir->op2);
    if (ra_hasreg(irr->r)) {
      ra_noweak(as, irr->r);
      right = irr->r;
    } else if (mayfuse(as, irr->op2) &&
	       irr->o == IR_ADD && irref_isk(irr->op2) &&
	       checki16(ofs + IR(irr->op2)->i)) {
      ofs += IR(irr->op2)->i;
      right = ra_alloc1(as, irr->op1, rset_exclude(RSET_GPR, left));
    } else {
      right = ra_allocref(as, ir->op2, rset_exclude(RSET_GPR, left));
    }
    emit_tai(as, PPCI_ADDI, dest, dest, ofs);
    emit_tab(as, PPCI_ADD, dest, left, right);
    return;
  }
  r = ra_alloc1(as, ref, RSET_GPR);
  ofs += IR(refk)->i;
  if (checki16(ofs))
    emit_tai(as, PPCI_ADDI, dest, r, ofs);
  else
    emit_tab(as, PPCI_ADD, dest, r,
	     ra_allock(as, ofs, rset_exclude(RSET_GPR, r)));
}

/* -- Loads and stores ---------------------------------------------------- */

static PPCIns asm_fxloadins(IRIns *ir)
{
  switch (irt_type(ir->t)) {
  case IRT_I8: return PPCI_LBZ;  /* Needs sign-extension. */
  case IRT_U8: return PPCI_LBZ;
  case IRT_I16: return PPCI_LHA;
  case IRT_U16: return PPCI_LHZ;
  case IRT_NUM: return PPCI_LFD;
  case IRT_FLOAT: return PPCI_LFS;
  default: return PPCI_LWZ;
  }
}

static PPCIns asm_fxstoreins(IRIns *ir)
{
  switch (irt_type(ir->t)) {
  case IRT_I8: case IRT_U8: return PPCI_STB;
  case IRT_I16: case IRT_U16: return PPCI_STH;
  case IRT_NUM: return PPCI_STFD;
  case IRT_FLOAT: return PPCI_STFS;
  default: return PPCI_STW;
  }
}

static void asm_fload(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg idx = ra_alloc1(as, ir->op1, RSET_GPR);
  PPCIns pi = asm_fxloadins(ir);
  int32_t ofs;
  if (ir->op2 == IRFL_TAB_ARRAY) {
    ofs = asm_fuseabase(as, ir->op1);
    if (ofs) {  /* Turn the t->array load into an add for colocated arrays. */
      emit_tai(as, PPCI_ADDI, dest, idx, ofs);
      return;
    }
  }
  ofs = field_ofs[ir->op2];
  lua_assert(!irt_isi8(ir->t));
  emit_tai(as, pi, dest, idx, ofs);
}

static void asm_fstore(ASMState *as, IRIns *ir)
{
  if (ir->r != RID_SINK) {
    Reg src = ra_alloc1(as, ir->op2, RSET_GPR);
    IRIns *irf = IR(ir->op1);
    Reg idx = ra_alloc1(as, irf->op1, rset_exclude(RSET_GPR, src));
    int32_t ofs = field_ofs[irf->op2];
    PPCIns pi = asm_fxstoreins(ir);
    emit_tai(as, pi, src, idx, ofs);
  }
}

static void asm_xload(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
  lua_assert(!(ir->op2 & IRXLOAD_UNALIGNED));
  if (irt_isi8(ir->t))
    emit_as(as, PPCI_EXTSB, dest, dest);
  asm_fusexref(as, asm_fxloadins(ir), dest, ir->op1, RSET_GPR, 0);
}

static void asm_xstore(ASMState *as, IRIns *ir, int32_t ofs)
{
  IRIns *irb;
  if (ir->r == RID_SINK)
    return;
  if (ofs == 0 && mayfuse(as, ir->op2) && (irb = IR(ir->op2))->o == IR_BSWAP &&
      ra_noreg(irb->r) && (irt_isint(ir->t) || irt_isu32(ir->t))) {
    /* Fuse BSWAP with XSTORE to stwbrx. */
    Reg src = ra_alloc1(as, irb->op1, RSET_GPR);
    asm_fusexrefx(as, PPCI_STWBRX, src, ir->op1, rset_exclude(RSET_GPR, src));
  } else {
    Reg src = ra_alloc1(as, ir->op2, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
    asm_fusexref(as, asm_fxstoreins(ir), src, ir->op1,
		 rset_exclude(RSET_GPR, src), ofs);
  }
}

static void asm_ahuvload(ASMState *as, IRIns *ir)
{
  IRType1 t = ir->t;
  Reg dest = RID_NONE, type = RID_TMP, tmp = RID_TMP, idx;
  RegSet allow = RSET_GPR;
  int32_t ofs = AHUREF_LSX;
  if (ra_used(ir)) {
    lua_assert(irt_isnum(t) || irt_isint(t) || irt_isaddr(t));
    if (!irt_isnum(t)) ofs = 0;
    dest = ra_dest(as, ir, irt_isnum(t) ? RSET_FPR : RSET_GPR);
    rset_clear(allow, dest);
  }
  idx = asm_fuseahuref(as, ir->op1, &ofs, allow);
  if (irt_isnum(t)) {
    Reg tisnum = ra_allock(as, (int32_t)LJ_TISNUM, rset_exclude(allow, idx));
    asm_guardcc(as, CC_GE);
    emit_ab(as, PPCI_CMPLW, type, tisnum);
    if (ra_hasreg(dest)) {
      if (ofs == AHUREF_LSX) {
	tmp = ra_scratch(as, rset_exclude(rset_exclude(RSET_GPR,
						       (idx&255)), (idx>>8)));
	emit_fab(as, PPCI_LFDX, dest, (idx&255), tmp);
      } else {
	emit_fai(as, PPCI_LFD, dest, idx, ofs);
      }
    }
  } else {
    asm_guardcc(as, CC_NE);
    emit_ai(as, PPCI_CMPWI, type, irt_toitype(t));
    if (ra_hasreg(dest)) emit_tai(as, PPCI_LWZ, dest, idx, ofs+4);
  }
  if (ofs == AHUREF_LSX) {
    emit_tab(as, PPCI_LWZX, type, (idx&255), tmp);
    emit_slwi(as, tmp, (idx>>8), 3);
  } else {
    emit_tai(as, PPCI_LWZ, type, idx, ofs);
  }
}

static void asm_ahustore(ASMState *as, IRIns *ir)
{
  RegSet allow = RSET_GPR;
  Reg idx, src = RID_NONE, type = RID_NONE;
  int32_t ofs = AHUREF_LSX;
  if (ir->r == RID_SINK)
    return;
  if (irt_isnum(ir->t)) {
    src = ra_alloc1(as, ir->op2, RSET_FPR);
  } else {
    if (!irt_ispri(ir->t)) {
      src = ra_alloc1(as, ir->op2, allow);
      rset_clear(allow, src);
      ofs = 0;
    }
    type = ra_allock(as, (int32_t)irt_toitype(ir->t), allow);
    rset_clear(allow, type);
  }
  idx = asm_fuseahuref(as, ir->op1, &ofs, allow);
  if (irt_isnum(ir->t)) {
    if (ofs == AHUREF_LSX) {
      emit_fab(as, PPCI_STFDX, src, (idx&255), RID_TMP);
      emit_slwi(as, RID_TMP, (idx>>8), 3);
    } else {
      emit_fai(as, PPCI_STFD, src, idx, ofs);
    }
  } else {
    if (ra_hasreg(src))
      emit_tai(as, PPCI_STW, src, idx, ofs+4);
    if (ofs == AHUREF_LSX) {
      emit_tab(as, PPCI_STWX, type, (idx&255), RID_TMP);
      emit_slwi(as, RID_TMP, (idx>>8), 3);
    } else {
      emit_tai(as, PPCI_STW, type, idx, ofs);
    }
  }
}

static void asm_sload(ASMState *as, IRIns *ir)
{
  int32_t ofs = 8*((int32_t)ir->op1-1) + ((ir->op2 & IRSLOAD_FRAME) ? 0 : 4);
  IRType1 t = ir->t;
  Reg dest = RID_NONE, type = RID_NONE, base;
  RegSet allow = RSET_GPR;
  lua_assert(!(ir->op2 & IRSLOAD_PARENT));  /* Handled by asm_head_side(). */
  lua_assert(irt_isguard(t) || !(ir->op2 & IRSLOAD_TYPECHECK));
  lua_assert(LJ_DUALNUM ||
	     !irt_isint(t) || (ir->op2 & (IRSLOAD_CONVERT|IRSLOAD_FRAME)));
  if ((ir->op2 & IRSLOAD_CONVERT) && irt_isguard(t) && irt_isint(t)) {
    dest = ra_scratch(as, RSET_FPR);
    asm_tointg(as, ir, dest);
    t.irt = IRT_NUM;  /* Continue with a regular number type check. */
  } else if (ra_used(ir)) {
    lua_assert(irt_isnum(t) || irt_isint(t) || irt_isaddr(t));
    dest = ra_dest(as, ir, irt_isnum(t) ? RSET_FPR : RSET_GPR);
    rset_clear(allow, dest);
    base = ra_alloc1(as, REF_BASE, allow);
    rset_clear(allow, base);
    if ((ir->op2 & IRSLOAD_CONVERT)) {
      if (irt_isint(t)) {
	emit_tai(as, PPCI_LWZ, dest, RID_SP, SPOFS_TMPLO);
	dest = ra_scratch(as, RSET_FPR);
	emit_fai(as, PPCI_STFD, dest, RID_SP, SPOFS_TMP);
	emit_fb(as, PPCI_FCTIWZ, dest, dest);
	t.irt = IRT_NUM;  /* Check for original type. */
      } else {
	Reg tmp = ra_scratch(as, allow);
	Reg hibias = ra_allock(as, 0x43300000, rset_clear(allow, tmp));
	Reg fbias = ra_scratch(as, rset_exclude(RSET_FPR, dest));
	emit_fab(as, PPCI_FSUB, dest, dest, fbias);
	emit_fai(as, PPCI_LFD, dest, RID_SP, SPOFS_TMP);
	emit_lsptr(as, PPCI_LFS, (fbias & 31),
		   (void *)lj_ir_k64_find(as->J, U64x(59800004,59800000)),
		   rset_clear(allow, hibias));
	emit_tai(as, PPCI_STW, tmp, RID_SP, SPOFS_TMPLO);
	emit_tai(as, PPCI_STW, hibias, RID_SP, SPOFS_TMPHI);
	emit_asi(as, PPCI_XORIS, tmp, tmp, 0x8000);
	dest = tmp;
	t.irt = IRT_INT;  /* Check for original type. */
      }
    }
    goto dotypecheck;
  }
  base = ra_alloc1(as, REF_BASE, allow);
  rset_clear(allow, base);
dotypecheck:
  if (irt_isnum(t)) {
    if ((ir->op2 & IRSLOAD_TYPECHECK)) {
      Reg tisnum = ra_allock(as, (int32_t)LJ_TISNUM, allow);
      asm_guardcc(as, CC_GE);
      emit_ab(as, PPCI_CMPLW, RID_TMP, tisnum);
      type = RID_TMP;
    }
    if (ra_hasreg(dest)) emit_fai(as, PPCI_LFD, dest, base, ofs-4);
  } else {
    if ((ir->op2 & IRSLOAD_TYPECHECK)) {
      asm_guardcc(as, CC_NE);
      emit_ai(as, PPCI_CMPWI, RID_TMP, irt_toitype(t));
      type = RID_TMP;
    }
    if (ra_hasreg(dest)) emit_tai(as, PPCI_LWZ, dest, base, ofs);
  }
  if (ra_hasreg(type)) emit_tai(as, PPCI_LWZ, type, base, ofs-4);
}

/* -- Allocations --------------------------------------------------------- */

#if LJ_HASFFI
static void asm_cnew(ASMState *as, IRIns *ir)
{
  CTState *cts = ctype_ctsG(J2G(as->J));
  CTypeID ctypeid = (CTypeID)IR(ir->op1)->i;
  CTSize sz = (ir->o == IR_CNEWI || ir->op2 == REF_NIL) ?
	      lj_ctype_size(cts, ctypeid) : (CTSize)IR(ir->op2)->i;
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_mem_newgco];
  IRRef args[2];
  RegSet allow = (RSET_GPR & ~RSET_SCRATCH);
  RegSet drop = RSET_SCRATCH;
  lua_assert(sz != CTSIZE_INVALID);

  args[0] = ASMREF_L;     /* lua_State *L */
  args[1] = ASMREF_TMP1;  /* MSize size   */
  as->gcsteps++;

  if (ra_hasreg(ir->r))
    rset_clear(drop, ir->r);  /* Dest reg handled below. */
  ra_evictset(as, drop);
  if (ra_used(ir))
    ra_destreg(as, ir, RID_RET);  /* GCcdata * */

  /* Initialize immutable cdata object. */
  if (ir->o == IR_CNEWI) {
    int32_t ofs = sizeof(GCcdata);
    lua_assert(sz == 4 || sz == 8);
    if (sz == 8) {
      ofs += 4;
      lua_assert((ir+1)->o == IR_HIOP);
    }
    for (;;) {
      Reg r = ra_alloc1(as, ir->op2, allow);
      emit_tai(as, PPCI_STW, r, RID_RET, ofs);
      rset_clear(allow, r);
      if (ofs == sizeof(GCcdata)) break;
      ofs -= 4; ir++;
    }
  }
  /* Initialize gct and ctypeid. lj_mem_newgco() already sets marked. */
  emit_tai(as, PPCI_STB, RID_RET+1, RID_RET, offsetof(GCcdata, gct));
  emit_tai(as, PPCI_STH, RID_TMP, RID_RET, offsetof(GCcdata, ctypeid));
  emit_ti(as, PPCI_LI, RID_RET+1, ~LJ_TCDATA);
  emit_ti(as, PPCI_LI, RID_TMP, ctypeid);  /* Lower 16 bit used. Sign-ext ok. */
  asm_gencall(as, ci, args);
  ra_allockreg(as, (int32_t)(sz+sizeof(GCcdata)),
	       ra_releasetmp(as, ASMREF_TMP1));
}
#else
#define asm_cnew(as, ir)	((void)0)
#endif

/* -- Write barriers ------------------------------------------------------ */

static void asm_tbar(ASMState *as, IRIns *ir)
{
  Reg tab = ra_alloc1(as, ir->op1, RSET_GPR);
  Reg mark = ra_scratch(as, rset_exclude(RSET_GPR, tab));
  Reg link = RID_TMP;
  MCLabel l_end = emit_label(as);
  emit_tai(as, PPCI_STW, link, tab, (int32_t)offsetof(GCtab, gclist));
  emit_tai(as, PPCI_STB, mark, tab, (int32_t)offsetof(GCtab, marked));
  emit_setgl(as, tab, gc.grayagain);
  lua_assert(LJ_GC_BLACK == 0x04);
  emit_rot(as, PPCI_RLWINM, mark, mark, 0, 30, 28);  /* Clear black bit. */
  emit_getgl(as, link, gc.grayagain);
  emit_condbranch(as, PPCI_BC|PPCF_Y, CC_EQ, l_end);
  emit_asi(as, PPCI_ANDIDOT, RID_TMP, mark, LJ_GC_BLACK);
  emit_tai(as, PPCI_LBZ, mark, tab, (int32_t)offsetof(GCtab, marked));
}

static void asm_obar(ASMState *as, IRIns *ir)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_gc_barrieruv];
  IRRef args[2];
  MCLabel l_end;
  Reg obj, val, tmp;
  /* No need for other object barriers (yet). */
  lua_assert(IR(ir->op1)->o == IR_UREFC);
  ra_evictset(as, RSET_SCRATCH);
  l_end = emit_label(as);
  args[0] = ASMREF_TMP1;  /* global_State *g */
  args[1] = ir->op1;      /* TValue *tv      */
  asm_gencall(as, ci, args);
  emit_tai(as, PPCI_ADDI, ra_releasetmp(as, ASMREF_TMP1), RID_JGL, -32768);
  obj = IR(ir->op1)->r;
  tmp = ra_scratch(as, rset_exclude(RSET_GPR, obj));
  emit_condbranch(as, PPCI_BC|PPCF_Y, CC_EQ, l_end);
  emit_asi(as, PPCI_ANDIDOT, tmp, tmp, LJ_GC_BLACK);
  emit_condbranch(as, PPCI_BC, CC_EQ, l_end);
  emit_asi(as, PPCI_ANDIDOT, RID_TMP, RID_TMP, LJ_GC_WHITES);
  val = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, obj));
  emit_tai(as, PPCI_LBZ, tmp, obj,
	   (int32_t)offsetof(GCupval, marked)-(int32_t)offsetof(GCupval, tv));
  emit_tai(as, PPCI_LBZ, RID_TMP, val, (int32_t)offsetof(GChead, marked));
}

/* -- Arithmetic and logic operations ------------------------------------- */

static void asm_fparith(ASMState *as, IRIns *ir, PPCIns pi)
{
  Reg dest = ra_dest(as, ir, RSET_FPR);
  Reg right, left = ra_alloc2(as, ir, RSET_FPR);
  right = (left >> 8); left &= 255;
  if (pi == PPCI_FMUL)
    emit_fac(as, pi, dest, left, right);
  else
    emit_fab(as, pi, dest, left, right);
}

static void asm_fpunary(ASMState *as, IRIns *ir, PPCIns pi)
{
  Reg dest = ra_dest(as, ir, RSET_FPR);
  Reg left = ra_hintalloc(as, ir->op1, dest, RSET_FPR);
  emit_fb(as, pi, dest, left);
}

static int asm_fpjoin_pow(ASMState *as, IRIns *ir)
{
  IRIns *irp = IR(ir->op1);
  if (irp == ir-1 && irp->o == IR_MUL && !ra_used(irp)) {
    IRIns *irpp = IR(irp->op1);
    if (irpp == ir-2 && irpp->o == IR_FPMATH &&
	irpp->op2 == IRFPM_LOG2 && !ra_used(irpp)) {
      const CCallInfo *ci = &lj_ir_callinfo[IRCALL_pow];
      IRRef args[2];
      args[0] = irpp->op1;
      args[1] = irp->op2;
      asm_setupresult(as, ir, ci);
      asm_gencall(as, ci, args);
      return 1;
    }
  }
  return 0;
}

static void asm_add(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    if (!asm_fusemadd(as, ir, PPCI_FMADD, PPCI_FMADD))
      asm_fparith(as, ir, PPCI_FADD);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg right, left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    PPCIns pi;
    if (irref_isk(ir->op2)) {
      int32_t k = IR(ir->op2)->i;
      if (checki16(k)) {
	pi = PPCI_ADDI;
	/* May fail due to spills/restores above, but simplifies the logic. */
	if (as->flagmcp == as->mcp) {
	  as->flagmcp = NULL;
	  as->mcp++;
	  pi = PPCI_ADDICDOT;
	}
	emit_tai(as, pi, dest, left, k);
	return;
      } else if ((k & 0xffff) == 0) {
	emit_tai(as, PPCI_ADDIS, dest, left, (k >> 16));
	return;
      } else if (!as->sectref) {
	emit_tai(as, PPCI_ADDIS, dest, dest, (k + 32768) >> 16);
	emit_tai(as, PPCI_ADDI, dest, left, k);
	return;
      }
    }
    pi = PPCI_ADD;
    /* May fail due to spills/restores above, but simplifies the logic. */
    if (as->flagmcp == as->mcp) {
      as->flagmcp = NULL;
      as->mcp++;
      pi |= PPCF_DOT;
    }
    right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_tab(as, pi, dest, left, right);
  }
}

static void asm_sub(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    if (!asm_fusemadd(as, ir, PPCI_FMSUB, PPCI_FNMSUB))
      asm_fparith(as, ir, PPCI_FSUB);
  } else {
    PPCIns pi = PPCI_SUBF;
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left, right;
    if (irref_isk(ir->op1)) {
      int32_t k = IR(ir->op1)->i;
      if (checki16(k)) {
	right = ra_alloc1(as, ir->op2, RSET_GPR);
	emit_tai(as, PPCI_SUBFIC, dest, right, k);
	return;
      }
    }
    /* May fail due to spills/restores above, but simplifies the logic. */
    if (as->flagmcp == as->mcp) {
      as->flagmcp = NULL;
      as->mcp++;
      pi |= PPCF_DOT;
    }
    left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_tab(as, pi, dest, right, left);  /* Subtract right _from_ left. */
  }
}

static void asm_mul(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    asm_fparith(as, ir, PPCI_FMUL);
  } else {
    PPCIns pi = PPCI_MULLW;
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg right, left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    if (irref_isk(ir->op2)) {
      int32_t k = IR(ir->op2)->i;
      if (checki16(k)) {
	emit_tai(as, PPCI_MULLI, dest, left, k);
	return;
      }
    }
    /* May fail due to spills/restores above, but simplifies the logic. */
    if (as->flagmcp == as->mcp) {
      as->flagmcp = NULL;
      as->mcp++;
      pi |= PPCF_DOT;
    }
    right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_tab(as, pi, dest, left, right);
  }
}

static void asm_neg(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    asm_fpunary(as, ir, PPCI_FNEG);
  } else {
    Reg dest, left;
    PPCIns pi = PPCI_NEG;
    if (as->flagmcp == as->mcp) {
      as->flagmcp = NULL;
      as->mcp++;
      pi |= PPCF_DOT;
    }
    dest = ra_dest(as, ir, RSET_GPR);
    left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    emit_tab(as, pi, dest, left, 0);
  }
}

static void asm_arithov(ASMState *as, IRIns *ir, PPCIns pi)
{
  Reg dest, left, right;
  if (as->flagmcp == as->mcp) {
    as->flagmcp = NULL;
    as->mcp++;
  }
  asm_guardcc(as, CC_SO);
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc2(as, ir, RSET_GPR);
  right = (left >> 8); left &= 255;
  if (pi == PPCI_SUBFO) { Reg tmp = left; left = right; right = tmp; }
  emit_tab(as, pi|PPCF_DOT, dest, left, right);
}

#if LJ_HASFFI
static void asm_add64(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg right, left = ra_alloc1(as, ir->op1, RSET_GPR);
  PPCIns pi = PPCI_ADDE;
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    if (k == 0)
      pi = PPCI_ADDZE;
    else if (k == -1)
      pi = PPCI_ADDME;
    else
      goto needright;
    right = 0;
  } else {
  needright:
    right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
  }
  emit_tab(as, pi, dest, left, right);
  ir--;
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc1(as, ir->op1, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    if (checki16(k)) {
      emit_tai(as, PPCI_ADDIC, dest, left, k);
      return;
    }
  }
  right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
  emit_tab(as, PPCI_ADDC, dest, left, right);
}

static void asm_sub64(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg left, right = ra_alloc1(as, ir->op2, RSET_GPR);
  PPCIns pi = PPCI_SUBFE;
  if (irref_isk(ir->op1)) {
    int32_t k = IR(ir->op1)->i;
    if (k == 0)
      pi = PPCI_SUBFZE;
    else if (k == -1)
      pi = PPCI_SUBFME;
    else
      goto needleft;
    left = 0;
  } else {
  needleft:
    left = ra_alloc1(as, ir->op1, rset_exclude(RSET_GPR, right));
  }
  emit_tab(as, pi, dest, right, left);  /* Subtract right _from_ left. */
  ir--;
  dest = ra_dest(as, ir, RSET_GPR);
  right = ra_alloc1(as, ir->op2, RSET_GPR);
  if (irref_isk(ir->op1)) {
    int32_t k = IR(ir->op1)->i;
    if (checki16(k)) {
      emit_tai(as, PPCI_SUBFIC, dest, right, k);
      return;
    }
  }
  left = ra_alloc1(as, ir->op1, rset_exclude(RSET_GPR, right));
  emit_tab(as, PPCI_SUBFC, dest, right, left);
}

static void asm_neg64(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
  emit_tab(as, PPCI_SUBFZE, dest, left, 0);
  ir--;
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc1(as, ir->op1, RSET_GPR);
  emit_tai(as, PPCI_SUBFIC, dest, left, 0);
}
#endif

static void asm_bitnot(ASMState *as, IRIns *ir)
{
  Reg dest, left, right;
  PPCIns pi = PPCI_NOR;
  if (as->flagmcp == as->mcp) {
    as->flagmcp = NULL;
    as->mcp++;
    pi |= PPCF_DOT;
  }
  dest = ra_dest(as, ir, RSET_GPR);
  if (mayfuse(as, ir->op1)) {
    IRIns *irl = IR(ir->op1);
    if (irl->o == IR_BAND)
      pi ^= (PPCI_NOR ^ PPCI_NAND);
    else if (irl->o == IR_BXOR)
      pi ^= (PPCI_NOR ^ PPCI_EQV);
    else if (irl->o != IR_BOR)
      goto nofuse;
    left = ra_hintalloc(as, irl->op1, dest, RSET_GPR);
    right = ra_alloc1(as, irl->op2, rset_exclude(RSET_GPR, left));
  } else {
nofuse:
    left = right = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
  }
  emit_asb(as, pi, dest, left, right);
}

static void asm_bitswap(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  IRIns *irx;
  if (mayfuse(as, ir->op1) && (irx = IR(ir->op1))->o == IR_XLOAD &&
      ra_noreg(irx->r) && (irt_isint(irx->t) || irt_isu32(irx->t))) {
    /* Fuse BSWAP with XLOAD to lwbrx. */
    asm_fusexrefx(as, PPCI_LWBRX, dest, irx->op1, RSET_GPR);
  } else {
    Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
    Reg tmp = dest;
    if (tmp == left) {
      tmp = RID_TMP;
      emit_mr(as, dest, RID_TMP);
    }
    emit_rot(as, PPCI_RLWIMI, tmp, left, 24, 16, 23);
    emit_rot(as, PPCI_RLWIMI, tmp, left, 24, 0, 7);
    emit_rotlwi(as, tmp, left, 8);
  }
}

static void asm_bitop(ASMState *as, IRIns *ir, PPCIns pi, PPCIns pik)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg right, left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    Reg tmp = left;
    if ((checku16(k) || (k & 0xffff) == 0) || (tmp = dest, !as->sectref)) {
      if (!checku16(k)) {
	emit_asi(as, pik ^ (PPCI_ORI ^ PPCI_ORIS), dest, tmp, (k >> 16));
	if ((k & 0xffff) == 0) return;
      }
      emit_asi(as, pik, dest, left, k);
      return;
    }
  }
  /* May fail due to spills/restores above, but simplifies the logic. */
  if (as->flagmcp == as->mcp) {
    as->flagmcp = NULL;
    as->mcp++;
    pi |= PPCF_DOT;
  }
  right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
  emit_asb(as, pi, dest, left, right);
}

/* Fuse BAND with contiguous bitmask and a shift to rlwinm. */
static void asm_fuseandsh(ASMState *as, PPCIns pi, int32_t mask, IRRef ref)
{
  IRIns *ir;
  Reg left;
  if (mayfuse(as, ref) && (ir = IR(ref), ra_noreg(ir->r)) &&
      irref_isk(ir->op2) && ir->o >= IR_BSHL && ir->o <= IR_BROR) {
    int32_t sh = (IR(ir->op2)->i & 31);
    switch (ir->o) {
    case IR_BSHL:
      if ((mask & ((1u<<sh)-1))) goto nofuse;
      break;
    case IR_BSHR:
      if ((mask & ~((~0u)>>sh))) goto nofuse;
      sh = ((32-sh)&31);
      break;
    case IR_BROL:
      break;
    default:
      goto nofuse;
    }
    left = ra_alloc1(as, ir->op1, RSET_GPR);
    *--as->mcp = pi | PPCF_T(left) | PPCF_B(sh);
    return;
  }
nofuse:
  left = ra_alloc1(as, ref, RSET_GPR);
  *--as->mcp = pi | PPCF_T(left);
}

static void asm_bitand(ASMState *as, IRIns *ir)
{
  Reg dest, left, right;
  IRRef lref = ir->op1;
  PPCIns dot = 0;
  IRRef op2;
  if (as->flagmcp == as->mcp) {
    as->flagmcp = NULL;
    as->mcp++;
    dot = PPCF_DOT;
  }
  dest = ra_dest(as, ir, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    if (k) {
      /* First check for a contiguous bitmask as used by rlwinm. */
      uint32_t s1 = lj_ffs((uint32_t)k);
      uint32_t k1 = ((uint32_t)k >> s1);
      if ((k1 & (k1+1)) == 0) {
	asm_fuseandsh(as, PPCI_RLWINM|dot | PPCF_A(dest) |
			  PPCF_MB(31-lj_fls((uint32_t)k)) | PPCF_ME(31-s1),
			  k, lref);
	return;
      }
      if (~(uint32_t)k) {
	uint32_t s2 = lj_ffs(~(uint32_t)k);
	uint32_t k2 = (~(uint32_t)k >> s2);
	if ((k2 & (k2+1)) == 0) {
	  asm_fuseandsh(as, PPCI_RLWINM|dot | PPCF_A(dest) |
			    PPCF_MB(32-s2) | PPCF_ME(30-lj_fls(~(uint32_t)k)),
			    k, lref);
	  return;
	}
      }
    }
    if (checku16(k)) {
      left = ra_alloc1(as, lref, RSET_GPR);
      emit_asi(as, PPCI_ANDIDOT, dest, left, k);
      return;
    } else if ((k & 0xffff) == 0) {
      left = ra_alloc1(as, lref, RSET_GPR);
      emit_asi(as, PPCI_ANDISDOT, dest, left, (k >> 16));
      return;
    }
  }
  op2 = ir->op2;
  if (mayfuse(as, op2) && IR(op2)->o == IR_BNOT && ra_noreg(IR(op2)->r)) {
    dot ^= (PPCI_AND ^ PPCI_ANDC);
    op2 = IR(op2)->op1;
  }
  left = ra_hintalloc(as, lref, dest, RSET_GPR);
  right = ra_alloc1(as, op2, rset_exclude(RSET_GPR, left));
  emit_asb(as, PPCI_AND ^ dot, dest, left, right);
}

static void asm_bitshift(ASMState *as, IRIns *ir, PPCIns pi, PPCIns pik)
{
  Reg dest, left;
  Reg dot = 0;
  if (as->flagmcp == as->mcp) {
    as->flagmcp = NULL;
    as->mcp++;
    dot = PPCF_DOT;
  }
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc1(as, ir->op1, RSET_GPR);
  if (irref_isk(ir->op2)) {  /* Constant shifts. */
    int32_t shift = (IR(ir->op2)->i & 31);
    if (pik == 0)  /* SLWI */
      emit_rot(as, PPCI_RLWINM|dot, dest, left, shift, 0, 31-shift);
    else if (pik == 1)  /* SRWI */
      emit_rot(as, PPCI_RLWINM|dot, dest, left, (32-shift)&31, shift, 31);
    else
      emit_asb(as, pik|dot, dest, left, shift);
  } else {
    Reg right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_asb(as, pi|dot, dest, left, right);
  }
}

static void asm_min_max(ASMState *as, IRIns *ir, int ismax)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg tmp = dest;
    Reg right, left = ra_alloc2(as, ir, RSET_FPR);
    right = (left >> 8); left &= 255;
    if (tmp == left || tmp == right)
      tmp = ra_scratch(as, rset_exclude(rset_exclude(rset_exclude(RSET_FPR,
					dest), left), right));
    emit_facb(as, PPCI_FSEL, dest, tmp,
	      ismax ? left : right, ismax ? right : left);
    emit_fab(as, PPCI_FSUB, tmp, left, right);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg tmp1 = RID_TMP, tmp2 = dest;
    Reg right, left = ra_alloc2(as, ir, RSET_GPR);
    right = (left >> 8); left &= 255;
    if (tmp2 == left || tmp2 == right)
      tmp2 = ra_scratch(as, rset_exclude(rset_exclude(rset_exclude(RSET_GPR,
					 dest), left), right));
    emit_tab(as, PPCI_ADD, dest, tmp2, right);
    emit_asb(as, ismax ? PPCI_ANDC : PPCI_AND, tmp2, tmp2, tmp1);
    emit_tab(as, PPCI_SUBFE, tmp1, tmp1, tmp1);
    emit_tab(as, PPCI_SUBFC, tmp2, tmp2, tmp1);
    emit_asi(as, PPCI_XORIS, tmp2, right, 0x8000);
    emit_asi(as, PPCI_XORIS, tmp1, left, 0x8000);
  }
}

/* -- Comparisons --------------------------------------------------------- */

#define CC_UNSIGNED	0x08	/* Unsigned integer comparison. */
#define CC_TWO		0x80	/* Check two flags for FP comparison. */

/* Map of comparisons to flags. ORDER IR. */
static const uint8_t asm_compmap[IR_ABC+1] = {
  /* op     int cc                 FP cc */
  /* LT  */ CC_GE               + (CC_GE<<4),
  /* GE  */ CC_LT               + (CC_LE<<4) + CC_TWO,
  /* LE  */ CC_GT               + (CC_GE<<4) + CC_TWO,
  /* GT  */ CC_LE               + (CC_LE<<4),
  /* ULT */ CC_GE + CC_UNSIGNED + (CC_GT<<4) + CC_TWO,
  /* UGE */ CC_LT + CC_UNSIGNED + (CC_LT<<4),
  /* ULE */ CC_GT + CC_UNSIGNED + (CC_GT<<4),
  /* UGT */ CC_LE + CC_UNSIGNED + (CC_LT<<4) + CC_TWO,
  /* EQ  */ CC_NE               + (CC_NE<<4),
  /* NE  */ CC_EQ               + (CC_EQ<<4),
  /* ABC */ CC_LE + CC_UNSIGNED + (CC_LT<<4) + CC_TWO  /* Same as UGT. */
};

static void asm_intcomp_(ASMState *as, IRRef lref, IRRef rref, Reg cr, PPCCC cc)
{
  Reg right, left = ra_alloc1(as, lref, RSET_GPR);
  if (irref_isk(rref)) {
    int32_t k = IR(rref)->i;
    if ((cc & CC_UNSIGNED) == 0) {  /* Signed comparison with constant. */
      if (checki16(k)) {
	emit_tai(as, PPCI_CMPWI, cr, left, k);
	/* Signed comparison with zero and referencing previous ins? */
	if (k == 0 && lref == as->curins-1)
	  as->flagmcp = as->mcp;  /* Allow elimination of the compare. */
	return;
      } else if ((cc & 3) == (CC_EQ & 3)) {  /* Use CMPLWI for EQ or NE. */
	if (checku16(k)) {
	  emit_tai(as, PPCI_CMPLWI, cr, left, k);
	  return;
	} else if (!as->sectref && ra_noreg(IR(rref)->r)) {
	  emit_tai(as, PPCI_CMPLWI, cr, RID_TMP, k);
	  emit_asi(as, PPCI_XORIS, RID_TMP, left, (k >> 16));
	  return;
	}
      }
    } else {  /* Unsigned comparison with constant. */
      if (checku16(k)) {
	emit_tai(as, PPCI_CMPLWI, cr, left, k);
	return;
      }
    }
  }
  right = ra_alloc1(as, rref, rset_exclude(RSET_GPR, left));
  emit_tab(as, (cc & CC_UNSIGNED) ? PPCI_CMPLW : PPCI_CMPW, cr, left, right);
}

static void asm_comp(ASMState *as, IRIns *ir)
{
  PPCCC cc = asm_compmap[ir->o];
  if (irt_isnum(ir->t)) {
    Reg right, left = ra_alloc2(as, ir, RSET_FPR);
    right = (left >> 8); left &= 255;
    asm_guardcc(as, (cc >> 4));
    if ((cc & CC_TWO))
      emit_tab(as, PPCI_CROR, ((cc>>4)&3), ((cc>>4)&3), (CC_EQ&3));
    emit_fab(as, PPCI_FCMPU, 0, left, right);
  } else {
    IRRef lref = ir->op1, rref = ir->op2;
    if (irref_isk(lref) && !irref_isk(rref)) {
      /* Swap constants to the right (only for ABC). */
      IRRef tmp = lref; lref = rref; rref = tmp;
      if ((cc & 2) == 0) cc ^= 1;  /* LT <-> GT, LE <-> GE */
    }
    asm_guardcc(as, cc);
    asm_intcomp_(as, lref, rref, 0, cc);
  }
}

#if LJ_HASFFI
/* 64 bit integer comparisons. */
static void asm_comp64(ASMState *as, IRIns *ir)
{
  PPCCC cc = asm_compmap[(ir-1)->o];
  if ((cc&3) == (CC_EQ&3)) {
    asm_guardcc(as, cc);
    emit_tab(as, (cc&4) ? PPCI_CRAND : PPCI_CROR,
	     (CC_EQ&3), (CC_EQ&3), 4+(CC_EQ&3));
  } else {
    asm_guardcc(as, CC_EQ);
    emit_tab(as, PPCI_CROR, (CC_EQ&3), (CC_EQ&3), ((cc^~(cc>>2))&1));
    emit_tab(as, (cc&4) ? PPCI_CRAND : PPCI_CRANDC,
	     (CC_EQ&3), (CC_EQ&3), 4+(cc&3));
  }
  /* Loword comparison sets cr1 and is unsigned, except for equality. */
  asm_intcomp_(as, (ir-1)->op1, (ir-1)->op2, 4,
	       cc | ((cc&3) == (CC_EQ&3) ? 0 : CC_UNSIGNED));
  /* Hiword comparison sets cr0. */
  asm_intcomp_(as, ir->op1, ir->op2, 0, cc);
  as->flagmcp = NULL;  /* Doesn't work here. */
}
#endif

/* -- Support for 64 bit ops in 32 bit mode ------------------------------- */

/* Hiword op of a split 64 bit op. Previous op must be the loword op. */
static void asm_hiop(ASMState *as, IRIns *ir)
{
#if LJ_HASFFI
  /* HIOP is marked as a store because it needs its own DCE logic. */
  int uselo = ra_used(ir-1), usehi = ra_used(ir);  /* Loword/hiword used? */
  if (LJ_UNLIKELY(!(as->flags & JIT_F_OPT_DCE))) uselo = usehi = 1;
  if ((ir-1)->o == IR_CONV) {  /* Conversions to/from 64 bit. */
    as->curins--;  /* Always skip the CONV. */
    if (usehi || uselo)
      asm_conv64(as, ir);
    return;
  } else if ((ir-1)->o <= IR_NE) {  /* 64 bit integer comparisons. ORDER IR. */
    as->curins--;  /* Always skip the loword comparison. */
    asm_comp64(as, ir);
    return;
  } else if ((ir-1)->o == IR_XSTORE) {
    as->curins--;  /* Handle both stores here. */
    if ((ir-1)->r != RID_SINK) {
      asm_xstore(as, ir, 0);
      asm_xstore(as, ir-1, 4);
    }
    return;
  }
  if (!usehi) return;  /* Skip unused hiword op for all remaining ops. */
  switch ((ir-1)->o) {
  case IR_ADD: as->curins--; asm_add64(as, ir); break;
  case IR_SUB: as->curins--; asm_sub64(as, ir); break;
  case IR_NEG: as->curins--; asm_neg64(as, ir); break;
  case IR_CALLN:
  case IR_CALLXS:
    if (!uselo)
      ra_allocref(as, ir->op1, RID2RSET(RID_RETLO));  /* Mark lo op as used. */
    break;
  case IR_CNEWI:
    /* Nothing to do here. Handled by lo op itself. */
    break;
  default: lua_assert(0); break;
  }
#else
  UNUSED(as); UNUSED(ir); lua_assert(0);  /* Unused without FFI. */
#endif
}

/* -- Stack handling ------------------------------------------------------ */

/* Check Lua stack size for overflow. Use exit handler as fallback. */
static void asm_stack_check(ASMState *as, BCReg topslot,
			    IRIns *irp, RegSet allow, ExitNo exitno)
{
  /* Try to get an unused temp. register, otherwise spill/restore RID_RET*. */
  Reg tmp, pbase = irp ? (ra_hasreg(irp->r) ? irp->r : RID_TMP) : RID_BASE;
  rset_clear(allow, pbase);
  tmp = allow ? rset_pickbot(allow) :
		(pbase == RID_RETHI ? RID_RETLO : RID_RETHI);
  emit_condbranch(as, PPCI_BC, CC_LT, asm_exitstub_addr(as, exitno));
  if (allow == RSET_EMPTY)  /* Restore temp. register. */
    emit_tai(as, PPCI_LWZ, tmp, RID_SP, SPOFS_TMPW);
  else
    ra_modified(as, tmp);
  emit_ai(as, PPCI_CMPLWI, RID_TMP, (int32_t)(8*topslot));
  emit_tab(as, PPCI_SUBF, RID_TMP, pbase, tmp);
  emit_tai(as, PPCI_LWZ, tmp, tmp, offsetof(lua_State, maxstack));
  if (pbase == RID_TMP)
    emit_getgl(as, RID_TMP, jit_base);
  emit_getgl(as, tmp, jit_L);
  if (allow == RSET_EMPTY)  /* Spill temp. register. */
    emit_tai(as, PPCI_STW, tmp, RID_SP, SPOFS_TMPW);
}

/* Restore Lua stack from on-trace state. */
static void asm_stack_restore(ASMState *as, SnapShot *snap)
{
  SnapEntry *map = &as->T->snapmap[snap->mapofs];
  SnapEntry *flinks = &as->T->snapmap[snap_nextofs(as->T, snap)-1];
  MSize n, nent = snap->nent;
  /* Store the value of all modified slots to the Lua stack. */
  for (n = 0; n < nent; n++) {
    SnapEntry sn = map[n];
    BCReg s = snap_slot(sn);
    int32_t ofs = 8*((int32_t)s-1);
    IRRef ref = snap_ref(sn);
    IRIns *ir = IR(ref);
    if ((sn & SNAP_NORESTORE))
      continue;
    if (irt_isnum(ir->t)) {
      Reg src = ra_alloc1(as, ref, RSET_FPR);
      emit_fai(as, PPCI_STFD, src, RID_BASE, ofs);
    } else {
      Reg type;
      RegSet allow = rset_exclude(RSET_GPR, RID_BASE);
      lua_assert(irt_ispri(ir->t) || irt_isaddr(ir->t) || irt_isinteger(ir->t));
      if (!irt_ispri(ir->t)) {
	Reg src = ra_alloc1(as, ref, allow);
	rset_clear(allow, src);
	emit_tai(as, PPCI_STW, src, RID_BASE, ofs+4);
      }
      if ((sn & (SNAP_CONT|SNAP_FRAME))) {
	if (s == 0) continue;  /* Do not overwrite link to previous frame. */
	type = ra_allock(as, (int32_t)(*flinks--), allow);
      } else {
	type = ra_allock(as, (int32_t)irt_toitype(ir->t), allow);
      }
      emit_tai(as, PPCI_STW, type, RID_BASE, ofs);
    }
    checkmclim(as);
  }
  lua_assert(map + nent == flinks);
}

/* -- GC handling --------------------------------------------------------- */

/* Check GC threshold and do one or more GC steps. */
static void asm_gc_check(ASMState *as)
{
  const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_gc_step_jit];
  IRRef args[2];
  MCLabel l_end;
  Reg tmp;
  ra_evictset(as, RSET_SCRATCH);
  l_end = emit_label(as);
  /* Exit trace if in GCSatomic or GCSfinalize. Avoids syncing GC objects. */
  asm_guardcc(as, CC_NE);  /* Assumes asm_snap_prep() already done. */
  emit_ai(as, PPCI_CMPWI, RID_RET, 0);
  args[0] = ASMREF_TMP1;  /* global_State *g */
  args[1] = ASMREF_TMP2;  /* MSize steps     */
  asm_gencall(as, ci, args);
  emit_tai(as, PPCI_ADDI, ra_releasetmp(as, ASMREF_TMP1), RID_JGL, -32768);
  tmp = ra_releasetmp(as, ASMREF_TMP2);
  emit_loadi(as, tmp, as->gcsteps);
  /* Jump around GC step if GC total < GC threshold. */
  emit_condbranch(as, PPCI_BC|PPCF_Y, CC_LT, l_end);
  emit_ab(as, PPCI_CMPLW, RID_TMP, tmp);
  emit_getgl(as, tmp, gc.threshold);
  emit_getgl(as, RID_TMP, gc.total);
  as->gcsteps = 0;
  checkmclim(as);
}

/* -- Loop handling ------------------------------------------------------- */

/* Fixup the loop branch. */
static void asm_loop_fixup(ASMState *as)
{
  MCode *p = as->mctop;
  MCode *target = as->mcp;
  if (as->loopinv) {  /* Inverted loop branch? */
    /* asm_guardcc already inverted the cond branch and patched the final b. */
    p[-2] = (p[-2] & (0xffff0000u & ~PPCF_Y)) | (((target-p+2) & 0x3fffu) << 2);
  } else {
    p[-1] = PPCI_B|(((target-p+1)&0x00ffffffu)<<2);
  }
}

/* -- Head of trace ------------------------------------------------------- */

/* Coalesce BASE register for a root trace. */
static void asm_head_root_base(ASMState *as)
{
  IRIns *ir = IR(REF_BASE);
  Reg r = ir->r;
  if (ra_hasreg(r)) {
    ra_free(as, r);
    if (rset_test(as->modset, r))
      ir->r = RID_INIT;  /* No inheritance for modified BASE register. */
    if (r != RID_BASE)
      emit_mr(as, r, RID_BASE);
  }
}

/* Coalesce BASE register for a side trace. */
static RegSet asm_head_side_base(ASMState *as, IRIns *irp, RegSet allow)
{
  IRIns *ir = IR(REF_BASE);
  Reg r = ir->r;
  if (ra_hasreg(r)) {
    ra_free(as, r);
    if (rset_test(as->modset, r))
      ir->r = RID_INIT;  /* No inheritance for modified BASE register. */
    if (irp->r == r) {
      rset_clear(allow, r);  /* Mark same BASE register as coalesced. */
    } else if (ra_hasreg(irp->r) && rset_test(as->freeset, irp->r)) {
      rset_clear(allow, irp->r);
      emit_mr(as, r, irp->r);  /* Move from coalesced parent reg. */
    } else {
      emit_getgl(as, r, jit_base);  /* Otherwise reload BASE. */
    }
  }
  return allow;
}

/* -- Tail of trace ------------------------------------------------------- */

/* Fixup the tail code. */
static void asm_tail_fixup(ASMState *as, TraceNo lnk)
{
  MCode *p = as->mctop;
  MCode *target;
  int32_t spadj = as->T->spadjust;
  if (spadj == 0) {
    *--p = PPCI_NOP;
    *--p = PPCI_NOP;
    as->mctop = p;
  } else {
    /* Patch stack adjustment. */
    lua_assert(checki16(CFRAME_SIZE+spadj));
    p[-3] = PPCI_ADDI | PPCF_T(RID_TMP) | PPCF_A(RID_SP) | (CFRAME_SIZE+spadj);
    p[-2] = PPCI_STWU | PPCF_T(RID_TMP) | PPCF_A(RID_SP) | spadj;
  }
  /* Patch exit branch. */
  target = lnk ? traceref(as->J, lnk)->mcode : (MCode *)lj_vm_exit_interp;
  p[-1] = PPCI_B|(((target-p+1)&0x00ffffffu)<<2);
}

/* Prepare tail of code. */
static void asm_tail_prep(ASMState *as)
{
  MCode *p = as->mctop - 1;  /* Leave room for exit branch. */
  if (as->loopref) {
    as->invmcp = as->mcp = p;
  } else {
    as->mcp = p-2;  /* Leave room for stack pointer adjustment. */
    as->invmcp = NULL;
  }
}

/* -- Instruction dispatch ------------------------------------------------ */

/* Assemble a single instruction. */
static void asm_ir(ASMState *as, IRIns *ir)
{
  switch ((IROp)ir->o) {
  /* Miscellaneous ops. */
  case IR_LOOP: asm_loop(as); break;
  case IR_NOP: case IR_XBAR: lua_assert(!ra_used(ir)); break;
  case IR_USE:
    ra_alloc1(as, ir->op1, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR); break;
  case IR_PHI: asm_phi(as, ir); break;
  case IR_HIOP: asm_hiop(as, ir); break;
  case IR_GCSTEP: asm_gcstep(as, ir); break;

  /* Guarded assertions. */
  case IR_EQ: case IR_NE:
    if ((ir-1)->o == IR_HREF && ir->op1 == as->curins-1) {
      as->curins--;
      asm_href(as, ir-1, (IROp)ir->o);
      break;
    }
    /* fallthrough */
  case IR_LT: case IR_GE: case IR_LE: case IR_GT:
  case IR_ULT: case IR_UGE: case IR_ULE: case IR_UGT:
  case IR_ABC:
    asm_comp(as, ir);
    break;

  case IR_RETF: asm_retf(as, ir); break;

  /* Bit ops. */
  case IR_BNOT: asm_bitnot(as, ir); break;
  case IR_BSWAP: asm_bitswap(as, ir); break;

  case IR_BAND: asm_bitand(as, ir); break;
  case IR_BOR:  asm_bitop(as, ir, PPCI_OR, PPCI_ORI); break;
  case IR_BXOR: asm_bitop(as, ir, PPCI_XOR, PPCI_XORI); break;

  case IR_BSHL: asm_bitshift(as, ir, PPCI_SLW, 0); break;
  case IR_BSHR: asm_bitshift(as, ir, PPCI_SRW, 1); break;
  case IR_BSAR: asm_bitshift(as, ir, PPCI_SRAW, PPCI_SRAWI); break;
  case IR_BROL: asm_bitshift(as, ir, PPCI_RLWNM|PPCF_MB(0)|PPCF_ME(31),
			     PPCI_RLWINM|PPCF_MB(0)|PPCF_ME(31)); break;
  case IR_BROR: lua_assert(0); break;

  /* Arithmetic ops. */
  case IR_ADD: asm_add(as, ir); break;
  case IR_SUB: asm_sub(as, ir); break;
  case IR_MUL: asm_mul(as, ir); break;
  case IR_DIV: asm_fparith(as, ir, PPCI_FDIV); break;
  case IR_MOD: asm_callid(as, ir, IRCALL_lj_vm_modi); break;
  case IR_POW: asm_callid(as, ir, IRCALL_lj_vm_powi); break;
  case IR_NEG: asm_neg(as, ir); break;

  case IR_ABS: asm_fpunary(as, ir, PPCI_FABS); break;
  case IR_ATAN2: asm_callid(as, ir, IRCALL_atan2); break;
  case IR_LDEXP: asm_callid(as, ir, IRCALL_ldexp); break;
  case IR_MIN: asm_min_max(as, ir, 0); break;
  case IR_MAX: asm_min_max(as, ir, 1); break;
  case IR_FPMATH:
    if (ir->op2 == IRFPM_EXP2 && asm_fpjoin_pow(as, ir))
      break;
    if (ir->op2 == IRFPM_SQRT && (as->flags & JIT_F_SQRT))
      asm_fpunary(as, ir, PPCI_FSQRT);
    else
      asm_callid(as, ir, IRCALL_lj_vm_floor + ir->op2);
    break;

  /* Overflow-checking arithmetic ops. */
  case IR_ADDOV: asm_arithov(as, ir, PPCI_ADDO); break;
  case IR_SUBOV: asm_arithov(as, ir, PPCI_SUBFO); break;
  case IR_MULOV: asm_arithov(as, ir, PPCI_MULLWO); break;

  /* Memory references. */
  case IR_AREF: asm_aref(as, ir); break;
  case IR_HREF: asm_href(as, ir, 0); break;
  case IR_HREFK: asm_hrefk(as, ir); break;
  case IR_NEWREF: asm_newref(as, ir); break;
  case IR_UREFO: case IR_UREFC: asm_uref(as, ir); break;
  case IR_FREF: asm_fref(as, ir); break;
  case IR_STRREF: asm_strref(as, ir); break;

  /* Loads and stores. */
  case IR_ALOAD: case IR_HLOAD: case IR_ULOAD: case IR_VLOAD:
    asm_ahuvload(as, ir);
    break;
  case IR_FLOAD: asm_fload(as, ir); break;
  case IR_XLOAD: asm_xload(as, ir); break;
  case IR_SLOAD: asm_sload(as, ir); break;

  case IR_ASTORE: case IR_HSTORE: case IR_USTORE: asm_ahustore(as, ir); break;
  case IR_FSTORE: asm_fstore(as, ir); break;
  case IR_XSTORE: asm_xstore(as, ir, 0); break;

  /* Allocations. */
  case IR_SNEW: case IR_XSNEW: asm_snew(as, ir); break;
  case IR_TNEW: asm_tnew(as, ir); break;
  case IR_TDUP: asm_tdup(as, ir); break;
  case IR_CNEW: case IR_CNEWI: asm_cnew(as, ir); break;

  /* Write barriers. */
  case IR_TBAR: asm_tbar(as, ir); break;
  case IR_OBAR: asm_obar(as, ir); break;

  /* Type conversions. */
  case IR_CONV: asm_conv(as, ir); break;
  case IR_TOBIT: asm_tobit(as, ir); break;
  case IR_TOSTR: asm_tostr(as, ir); break;
  case IR_STRTO: asm_strto(as, ir); break;

  /* Calls. */
  case IR_CALLN: case IR_CALLL: case IR_CALLS: asm_call(as, ir); break;
  case IR_CALLXS: asm_callx(as, ir); break;
  case IR_CARG: break;

  default:
    setintV(&as->J->errinfo, ir->o);
    lj_trace_err_info(as->J, LJ_TRERR_NYIIR);
    break;
  }
}

/* -- Trace setup --------------------------------------------------------- */

/* Ensure there are enough stack slots for call arguments. */
static Reg asm_setup_call_slots(ASMState *as, IRIns *ir, const CCallInfo *ci)
{
  IRRef args[CCI_NARGS_MAX*2];
  uint32_t i, nargs = (int)CCI_NARGS(ci);
  int nslots = 2, ngpr = REGARG_NUMGPR, nfpr = REGARG_NUMFPR;
  asm_collectargs(as, ir, ci, args);
  for (i = 0; i < nargs; i++)
    if (args[i] && irt_isfp(IR(args[i])->t)) {
      if (nfpr > 0) nfpr--; else nslots = (nslots+3) & ~1;
    } else {
      if (ngpr > 0) ngpr--; else nslots++;
    }
  if (nslots > as->evenspill)  /* Leave room for args in stack slots. */
    as->evenspill = nslots;
  return irt_isfp(ir->t) ? REGSP_HINT(RID_FPRET) : REGSP_HINT(RID_RET);
}

static void asm_setup_target(ASMState *as)
{
  asm_exitstub_setup(as, as->T->nsnap + (as->parent ? 1 : 0));
}

/* -- Trace patching ------------------------------------------------------ */

/* Patch exit jumps of existing machine code to a new target. */
void lj_asm_patchexit(jit_State *J, GCtrace *T, ExitNo exitno, MCode *target)
{
  MCode *p = T->mcode;
  MCode *pe = (MCode *)((char *)p + T->szmcode);
  MCode *px = exitstub_trace_addr(T, exitno);
  MCode *cstart = NULL;
  MCode *mcarea = lj_mcode_patch(J, p, 0);
  int clearso = 0;
  for (; p < pe; p++) {
    /* Look for exitstub branch, try to replace with branch to target. */
    uint32_t ins = *p;
    if ((ins & 0xfc000000u) == 0x40000000u &&
	((ins ^ ((char *)px-(char *)p)) & 0xffffu) == 0) {
      ptrdiff_t delta = (char *)target - (char *)p;
      if (((ins >> 16) & 3) == (CC_SO&3)) {
	clearso = sizeof(MCode);
	delta -= sizeof(MCode);
      }
      /* Many, but not all short-range branches can be patched directly. */
      if (((delta + 0x8000) >> 16) == 0) {
	*p = (ins & 0xffdf0000u) | ((uint32_t)delta & 0xffffu) |
	     ((delta & 0x8000) * (PPCF_Y/0x8000));
	if (!cstart) cstart = p;
      }
    } else if ((ins & 0xfc000000u) == PPCI_B &&
	       ((ins ^ ((char *)px-(char *)p)) & 0x03ffffffu) == 0) {
      ptrdiff_t delta = (char *)target - (char *)p;
      lua_assert(((delta + 0x02000000) >> 26) == 0);
      *p = PPCI_B | ((uint32_t)delta & 0x03ffffffu);
      if (!cstart) cstart = p;
    }
  }
  {  /* Always patch long-range branch in exit stub itself. */
    ptrdiff_t delta = (char *)target - (char *)px - clearso;
    lua_assert(((delta + 0x02000000) >> 26) == 0);
    *px = PPCI_B | ((uint32_t)delta & 0x03ffffffu);
  }
  if (!cstart) cstart = px;
  lj_mcode_sync(cstart, px+1);
  if (clearso) {  /* Extend the current trace. Ugly workaround. */
    MCode *pp = J->cur.mcode;
    J->cur.szmcode += sizeof(MCode);
    *--pp = PPCI_MCRXR;  /* Clear SO flag. */
    J->cur.mcode = pp;
    lj_mcode_sync(pp, pp+1);
  }
  lj_mcode_patch(J, mcarea, 1);
}

