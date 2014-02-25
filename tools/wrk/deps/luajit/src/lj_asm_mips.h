/*
** MIPS IR assembler (SSA IR -> machine code).
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

/* Allocate a register or RID_ZERO. */
static Reg ra_alloc1z(ASMState *as, IRRef ref, RegSet allow)
{
  Reg r = IR(ref)->r;
  if (ra_noreg(r)) {
    if (!(allow & RSET_FPR) && irref_isk(ref) && IR(ref)->i == 0)
      return RID_ZERO;
    r = ra_allocref(as, ref, allow);
  } else {
    ra_noweak(as, r);
  }
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
      right = ra_alloc1z(as, ir->op2, rset_exclude(allow, left));
    else
      ra_noweak(as, right);
  } else if (ra_hasreg(right)) {
    ra_noweak(as, right);
    left = ra_alloc1z(as, ir->op1, rset_exclude(allow, right));
  } else if (ra_hashint(right)) {
    right = ra_alloc1z(as, ir->op2, allow);
    left = ra_alloc1z(as, ir->op1, rset_exclude(allow, right));
  } else {
    left = ra_alloc1z(as, ir->op1, allow);
    right = ra_alloc1z(as, ir->op2, rset_exclude(allow, left));
  }
  return left | (right << 8);
}

/* -- Guard handling ------------------------------------------------------ */

/* Need some spare long-range jump slots, for out-of-range branches. */
#define MIPS_SPAREJUMP		4

/* Setup spare long-range jump slots per mcarea. */
static void asm_sparejump_setup(ASMState *as)
{
  MCode *mxp = as->mcbot;
  /* Assumes sizeof(MCLink) == 8. */
  if (((uintptr_t)mxp & (LJ_PAGESIZE-1)) == 8) {
    lua_assert(MIPSI_NOP == 0);
    memset(mxp+2, 0, MIPS_SPAREJUMP*8);
    mxp += MIPS_SPAREJUMP*2;
    lua_assert(mxp < as->mctop);
    lj_mcode_sync(as->mcbot, mxp);
    lj_mcode_commitbot(as->J, mxp);
    as->mcbot = mxp;
    as->mclim = as->mcbot + MCLIM_REDZONE;
  }
}

/* Setup exit stub after the end of each trace. */
static void asm_exitstub_setup(ASMState *as)
{
  MCode *mxp = as->mctop;
  /* sw TMP, 0(sp); j ->vm_exit_handler; li TMP, traceno */
  *--mxp = MIPSI_LI|MIPSF_T(RID_TMP)|as->T->traceno;
  *--mxp = MIPSI_J|((((uintptr_t)(void *)lj_vm_exit_handler)>>2)&0x03ffffffu);
  lua_assert(((uintptr_t)mxp ^ (uintptr_t)(void *)lj_vm_exit_handler)>>28 == 0);
  *--mxp = MIPSI_SW|MIPSF_T(RID_TMP)|MIPSF_S(RID_SP)|0;
  as->mctop = mxp;
}

/* Keep this in-sync with exitstub_trace_addr(). */
#define asm_exitstub_addr(as)	((as)->mctop)

/* Emit conditional branch to exit for guard. */
static void asm_guard(ASMState *as, MIPSIns mi, Reg rs, Reg rt)
{
  MCode *target = asm_exitstub_addr(as);
  MCode *p = as->mcp;
  if (LJ_UNLIKELY(p == as->invmcp)) {
    as->invmcp = NULL;
    as->loopinv = 1;
    as->mcp = p+1;
    mi = mi ^ ((mi>>28) == 1 ? 0x04000000u : 0x00010000u);  /* Invert cond. */
    target = p;  /* Patch target later in asm_loop_fixup. */
  }
  emit_ti(as, MIPSI_LI, RID_TMP, as->snapno);
  emit_branch(as, mi, rs, rt, target);
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
static void asm_fusexref(ASMState *as, MIPSIns mi, Reg rt, IRRef ref,
			 RegSet allow, int32_t ofs)
{
  IRIns *ir = IR(ref);
  Reg base;
  if (ra_noreg(ir->r) && canfuse(as, ir)) {
    if (ir->o == IR_ADD) {
      int32_t ofs2;
      if (irref_isk(ir->op2) && (ofs2 = ofs + IR(ir->op2)->i, checki16(ofs2))) {
	ref = ir->op1;
	ofs = ofs2;
      }
    } else if (ir->o == IR_STRREF) {
      int32_t ofs2 = 65536;
      lua_assert(ofs == 0);
      ofs = (int32_t)sizeof(GCstr);
      if (irref_isk(ir->op2)) {
	ofs2 = ofs + IR(ir->op2)->i;
	ref = ir->op1;
      } else if (irref_isk(ir->op1)) {
	ofs2 = ofs + IR(ir->op1)->i;
	ref = ir->op2;
      }
      if (!checki16(ofs2)) {
	/* NYI: Fuse ADD with constant. */
	Reg right, left = ra_alloc2(as, ir, allow);
	right = (left >> 8); left &= 255;
	emit_hsi(as, mi, rt, RID_TMP, ofs);
	emit_dst(as, MIPSI_ADDU, RID_TMP, left, right);
	return;
      }
      ofs = ofs2;
    }
  }
  base = ra_alloc1(as, ref, allow);
  emit_hsi(as, mi, rt, base, ofs);
}

/* -- Calls --------------------------------------------------------------- */

/* Generate a call to a C function. */
static void asm_gencall(ASMState *as, const CCallInfo *ci, IRRef *args)
{
  uint32_t n, nargs = CCI_NARGS(ci);
  int32_t ofs = 16;
  Reg gpr, fpr = REGARG_FIRSTFPR;
  if ((void *)ci->func)
    emit_call(as, (void *)ci->func);
  for (gpr = REGARG_FIRSTGPR; gpr <= REGARG_LASTGPR; gpr++)
    as->cost[gpr] = REGCOST(~0u, ASMREF_L);
  gpr = REGARG_FIRSTGPR;
  for (n = 0; n < nargs; n++) {  /* Setup args. */
    IRRef ref = args[n];
    if (ref) {
      IRIns *ir = IR(ref);
      if (irt_isfp(ir->t) && fpr <= REGARG_LASTFPR &&
	  !(ci->flags & CCI_VARARG)) {
	lua_assert(rset_test(as->freeset, fpr));  /* Already evicted. */
	ra_leftov(as, fpr, ref);
	fpr += 2;
	gpr += irt_isnum(ir->t) ? 2 : 1;
      } else {
	fpr = REGARG_LASTFPR+1;
	if (irt_isnum(ir->t)) gpr = (gpr+1) & ~1;
	if (gpr <= REGARG_LASTGPR) {
	  lua_assert(rset_test(as->freeset, gpr));  /* Already evicted. */
	  if (irt_isfp(ir->t)) {
	    RegSet of = as->freeset;
	    Reg r;
	    /* Workaround to protect argument GPRs from being used for remat. */
	    as->freeset &= ~RSET_RANGE(REGARG_FIRSTGPR, REGARG_LASTGPR+1);
	    r = ra_alloc1(as, ref, RSET_FPR);
	    as->freeset |= (of & RSET_RANGE(REGARG_FIRSTGPR, REGARG_LASTGPR+1));
	    if (irt_isnum(ir->t)) {
	      emit_tg(as, MIPSI_MFC1, gpr+(LJ_BE?0:1), r+1);
	      emit_tg(as, MIPSI_MFC1, gpr+(LJ_BE?1:0), r);
	      lua_assert(rset_test(as->freeset, gpr+1));  /* Already evicted. */
	      gpr += 2;
	    } else if (irt_isfloat(ir->t)) {
	      emit_tg(as, MIPSI_MFC1, gpr, r);
	      gpr++;
	    }
	  } else {
	    ra_leftov(as, gpr, ref);
	    gpr++;
	  }
	} else {
	  Reg r = ra_alloc1z(as, ref, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
	  if (irt_isnum(ir->t)) ofs = (ofs + 4) & ~4;
	  emit_spstore(as, ir, r, ofs);
	  ofs += irt_isnum(ir->t) ? 8 : 4;
	}
      }
    } else {
      fpr = REGARG_LASTFPR+1;
      if (gpr <= REGARG_LASTGPR)
	gpr++;
      else
	ofs += 4;
    }
    checkmclim(as);
  }
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
	int32_t ofs = sps_scale(ir->s);
	Reg dest = ir->r;
	if (ra_hasreg(dest)) {
	  ra_free(as, dest);
	  ra_modified(as, dest);
	  emit_tg(as, MIPSI_MTC1, RID_RETHI, dest+1);
	  emit_tg(as, MIPSI_MTC1, RID_RETLO, dest);
	}
	if (ofs) {
	  emit_tsi(as, MIPSI_SW, RID_RETLO, RID_SP, ofs+(LJ_BE?4:0));
	  emit_tsi(as, MIPSI_SW, RID_RETHI, RID_SP, ofs+(LJ_BE?0:4));
	}
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
  } else {  /* Need specific register for indirect calls. */
    Reg r = ra_alloc1(as, func, RID2RSET(RID_CFUNCADDR));
    MCode *p = as->mcp;
    if (r == RID_CFUNCADDR)
      *--p = MIPSI_NOP;
    else
      *--p = MIPSI_MOVE | MIPSF_D(RID_CFUNCADDR) | MIPSF_S(r);
    *--p = MIPSI_JALR | MIPSF_S(r);
    as->mcp = p;
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

static void asm_callround(ASMState *as, IRIns *ir, IRCallID id)
{
  /* The modified regs must match with the *.dasc implementation. */
  RegSet drop = RID2RSET(RID_R1)|RID2RSET(RID_R12)|RID2RSET(RID_FPRET)|
		RID2RSET(RID_F2)|RID2RSET(RID_F4)|RID2RSET(REGARG_FIRSTFPR);
  if (ra_hasreg(ir->r)) rset_clear(drop, ir->r);
  ra_evictset(as, drop);
  ra_destreg(as, ir, RID_FPRET);
  emit_call(as, (void *)lj_ir_callinfo[id].func);
  ra_leftov(as, REGARG_FIRSTFPR, ir->op1);
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
  asm_guard(as, MIPSI_BNE, RID_TMP,
	    ra_allock(as, i32ptr(pc), rset_exclude(RSET_GPR, base)));
  emit_tsi(as, MIPSI_LW, RID_TMP, base, -8);
}

/* -- Type conversions ---------------------------------------------------- */

static void asm_tointg(ASMState *as, IRIns *ir, Reg left)
{
  Reg tmp = ra_scratch(as, rset_exclude(RSET_FPR, left));
  Reg dest = ra_dest(as, ir, RSET_GPR);
  asm_guard(as, MIPSI_BC1F, 0, 0);
  emit_fgh(as, MIPSI_C_EQ_D, 0, tmp, left);
  emit_fg(as, MIPSI_CVT_D_W, tmp, tmp);
  emit_tg(as, MIPSI_MFC1, dest, tmp);
  emit_fg(as, MIPSI_CVT_W_D, tmp, left);
}

static void asm_tobit(ASMState *as, IRIns *ir)
{
  RegSet allow = RSET_FPR;
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg left = ra_alloc1(as, ir->op1, allow);
  Reg right = ra_alloc1(as, ir->op2, rset_clear(allow, left));
  Reg tmp = ra_scratch(as, rset_clear(allow, right));
  emit_tg(as, MIPSI_MFC1, dest, tmp);
  emit_fgh(as, MIPSI_ADD_D, tmp, left, right);
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
      emit_fg(as, st == IRT_NUM ? MIPSI_CVT_S_D : MIPSI_CVT_D_S,
	      dest, ra_alloc1(as, lref, RSET_FPR));
    } else if (st == IRT_U32) {  /* U32 to FP conversion. */
      /* y = (x ^ 0x8000000) + 2147483648.0 */
      Reg left = ra_alloc1(as, lref, RSET_GPR);
      Reg tmp = ra_scratch(as, rset_exclude(RSET_FPR, dest));
      emit_fgh(as, irt_isfloat(ir->t) ? MIPSI_ADD_S : MIPSI_ADD_D,
	       dest, dest, tmp);
      emit_fg(as, irt_isfloat(ir->t) ? MIPSI_CVT_S_W : MIPSI_CVT_D_W,
	      dest, dest);
      if (irt_isfloat(ir->t))
	emit_lsptr(as, MIPSI_LWC1, (tmp & 31),
		   (void *)lj_ir_k64_find(as->J, U64x(4f000000,4f000000)),
		   RSET_GPR);
      else
	emit_lsptr(as, MIPSI_LDC1, (tmp & 31),
		   (void *)lj_ir_k64_find(as->J, U64x(41e00000,00000000)),
		   RSET_GPR);
      emit_tg(as, MIPSI_MTC1, RID_TMP, dest);
      emit_dst(as, MIPSI_XOR, RID_TMP, RID_TMP, left);
      emit_ti(as, MIPSI_LUI, RID_TMP, 0x8000);
    } else {  /* Integer to FP conversion. */
      Reg left = ra_alloc1(as, lref, RSET_GPR);
      emit_fg(as, irt_isfloat(ir->t) ? MIPSI_CVT_S_W : MIPSI_CVT_D_W,
	      dest, dest);
      emit_tg(as, MIPSI_MTC1, left, dest);
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
	/* y = (int)floor(x - 2147483648.0) ^ 0x80000000 */
	emit_dst(as, MIPSI_XOR, dest, dest, RID_TMP);
	emit_ti(as, MIPSI_LUI, RID_TMP, 0x8000);
	emit_tg(as, MIPSI_MFC1, dest, tmp);
	emit_fg(as, st == IRT_FLOAT ? MIPSI_FLOOR_W_S : MIPSI_FLOOR_W_D,
		tmp, tmp);
	emit_fgh(as, st == IRT_FLOAT ? MIPSI_SUB_S : MIPSI_SUB_D,
		 tmp, left, tmp);
	if (st == IRT_FLOAT)
	  emit_lsptr(as, MIPSI_LWC1, (tmp & 31),
		     (void *)lj_ir_k64_find(as->J, U64x(4f000000,4f000000)),
		     RSET_GPR);
	else
	  emit_lsptr(as, MIPSI_LDC1, (tmp & 31),
		     (void *)lj_ir_k64_find(as->J, U64x(41e00000,00000000)),
		     RSET_GPR);
      } else {
	emit_tg(as, MIPSI_MFC1, dest, tmp);
	emit_fg(as, st == IRT_FLOAT ? MIPSI_TRUNC_W_S : MIPSI_TRUNC_W_D,
		tmp, left);
      }
    }
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    if (st >= IRT_I8 && st <= IRT_U16) {  /* Extend to 32 bit integer. */
      Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
      lua_assert(irt_isint(ir->t) || irt_isu32(ir->t));
      if ((ir->op2 & IRCONV_SEXT)) {
	if ((as->flags & JIT_F_MIPS32R2)) {
	  emit_dst(as, st == IRT_I8 ? MIPSI_SEB : MIPSI_SEH, dest, 0, left);
	} else {
	  uint32_t shift = st == IRT_I8 ? 24 : 16;
	  emit_dta(as, MIPSI_SRA, dest, dest, shift);
	  emit_dta(as, MIPSI_SLL, dest, left, shift);
	}
      } else {
	emit_tsi(as, MIPSI_ANDI, dest, left,
		 (int32_t)(st == IRT_U8 ? 0xff : 0xffff));
      }
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
  args[LJ_BE?0:1] = ir->op1;
  args[LJ_BE?1:0] = (ir-1)->op1;
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
  RegSet drop = RSET_SCRATCH;
  if (ra_hasreg(ir->r)) rset_set(drop, ir->r);  /* Spill dest reg (if any). */
  ra_evictset(as, drop);
  asm_guard(as, MIPSI_BEQ, RID_RET, RID_ZERO);  /* Test return status. */
  args[0] = ir->op1;      /* GCstr *str */
  args[1] = ASMREF_TMP1;  /* TValue *n  */
  asm_gencall(as, ci, args);
  /* Store the result to the spill slot or temp slots. */
  emit_tsi(as, MIPSI_ADDIU, ra_releasetmp(as, ASMREF_TMP1),
	   RID_SP, sps_scale(ir->s));
}

/* Get pointer to TValue. */
static void asm_tvptr(ASMState *as, Reg dest, IRRef ref)
{
  IRIns *ir = IR(ref);
  if (irt_isnum(ir->t)) {
    if (irref_isk(ref))  /* Use the number constant itself as a TValue. */
      ra_allockreg(as, i32ptr(ir_knum(ir)), dest);
    else  /* Otherwise force a spill and use the spill slot. */
      emit_tsi(as, MIPSI_ADDIU, dest, RID_SP, ra_spill(as, ir));
  } else {
    /* Otherwise use g->tmptv to hold the TValue. */
    RegSet allow = rset_exclude(RSET_GPR, dest);
    Reg type;
    emit_tsi(as, MIPSI_ADDIU, dest, RID_JGL, offsetof(global_State, tmptv)-32768);
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
      emit_tsi(as, MIPSI_ADDIU, dest, base, ofs);
      return;
    }
  }
  base = ra_alloc1(as, ir->op1, RSET_GPR);
  idx = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, base));
  emit_dst(as, MIPSI_ADDU, dest, RID_TMP, base);
  emit_dta(as, MIPSI_SLL, RID_TMP, idx, 3);
}

/* Inlined hash lookup. Specialized for key type and for const keys.
** The equivalent C code is:
**   Node *n = hashkey(t, key);
**   do {
**     if (lj_obj_equal(&n->key, key)) return &n->val;
**   } while ((n = nextnode(n)));
**   return niltv(L);
*/
static void asm_href(ASMState *as, IRIns *ir)
{
  RegSet allow = RSET_GPR;
  int destused = ra_used(ir);
  Reg dest = ra_dest(as, ir, allow);
  Reg tab = ra_alloc1(as, ir->op1, rset_clear(allow, dest));
  Reg key = RID_NONE, type = RID_NONE, tmpnum = RID_NONE, tmp1 = RID_TMP, tmp2;
  IRRef refkey = ir->op2;
  IRIns *irkey = IR(refkey);
  IRType1 kt = irkey->t;
  uint32_t khash;
  MCLabel l_end, l_loop, l_next;

  rset_clear(allow, tab);
  if (irt_isnum(kt)) {
    key = ra_alloc1(as, refkey, RSET_FPR);
    tmpnum = ra_scratch(as, rset_exclude(RSET_FPR, key));
  } else if (!irt_ispri(kt)) {
    key = ra_alloc1(as, refkey, allow);
    rset_clear(allow, key);
    type = ra_allock(as, irt_toitype(irkey->t), allow);
    rset_clear(allow, type);
  }
  tmp2 = ra_scratch(as, allow);
  rset_clear(allow, tmp2);

  /* Key not found in chain: load niltv. */
  l_end = emit_label(as);
  if (destused)
    emit_loada(as, dest, niltvg(J2G(as->J)));
  else
    *--as->mcp = MIPSI_NOP;
  /* Follow hash chain until the end. */
  emit_move(as, dest, tmp1);
  l_loop = --as->mcp;
  emit_tsi(as, MIPSI_LW, tmp1, dest, (int32_t)offsetof(Node, next));
  l_next = emit_label(as);

  /* Type and value comparison. */
  if (irt_isnum(kt)) {
    emit_branch(as, MIPSI_BC1T, 0, 0, l_end);
    emit_fgh(as, MIPSI_C_EQ_D, 0, tmpnum, key);
	emit_tg(as, MIPSI_MFC1, tmp1, key+1);
    emit_branch(as, MIPSI_BEQ, tmp1, RID_ZERO, l_next);
    emit_tsi(as, MIPSI_SLTIU, tmp1, tmp1, (int32_t)LJ_TISNUM);
    emit_hsi(as, MIPSI_LDC1, tmpnum, dest, (int32_t)offsetof(Node, key.n));
  } else {
    if (irt_ispri(kt)) {
      emit_branch(as, MIPSI_BEQ, tmp1, type, l_end);
    } else {
      emit_branch(as, MIPSI_BEQ, tmp2, key, l_end);
      emit_tsi(as, MIPSI_LW, tmp2, dest, (int32_t)offsetof(Node, key.gcr));
      emit_branch(as, MIPSI_BNE, tmp1, type, l_next);
    }
  }
  emit_tsi(as, MIPSI_LW, tmp1, dest, (int32_t)offsetof(Node, key.it));
  *l_loop = MIPSI_BNE | MIPSF_S(tmp1) | ((as->mcp-l_loop-1) & 0xffffu);

  /* Load main position relative to tab->node into dest. */
  khash = irref_isk(refkey) ? ir_khash(irkey) : 1;
  if (khash == 0) {
    emit_tsi(as, MIPSI_LW, dest, tab, (int32_t)offsetof(GCtab, node));
  } else {
    Reg tmphash = tmp1;
    if (irref_isk(refkey))
      tmphash = ra_allock(as, khash, allow);
    emit_dst(as, MIPSI_ADDU, dest, dest, tmp1);
    lua_assert(sizeof(Node) == 24);
    emit_dst(as, MIPSI_SUBU, tmp1, tmp2, tmp1);
    emit_dta(as, MIPSI_SLL, tmp1, tmp1, 3);
    emit_dta(as, MIPSI_SLL, tmp2, tmp1, 5);
    emit_dst(as, MIPSI_AND, tmp1, tmp2, tmphash);
    emit_tsi(as, MIPSI_LW, dest, tab, (int32_t)offsetof(GCtab, node));
    emit_tsi(as, MIPSI_LW, tmp2, tab, (int32_t)offsetof(GCtab, hmask));
    if (irref_isk(refkey)) {
      /* Nothing to do. */
    } else if (irt_isstr(kt)) {
      emit_tsi(as, MIPSI_LW, tmp1, key, (int32_t)offsetof(GCstr, hash));
    } else {  /* Must match with hash*() in lj_tab.c. */
      emit_dst(as, MIPSI_SUBU, tmp1, tmp1, tmp2);
      emit_rotr(as, tmp2, tmp2, dest, (-HASH_ROT3)&31);
      emit_dst(as, MIPSI_XOR, tmp1, tmp1, tmp2);
      emit_rotr(as, tmp1, tmp1, dest, (-HASH_ROT2-HASH_ROT1)&31);
      emit_dst(as, MIPSI_SUBU, tmp2, tmp2, dest);
      if (irt_isnum(kt)) {
	emit_dst(as, MIPSI_XOR, tmp2, tmp2, tmp1);
	if ((as->flags & JIT_F_MIPS32R2)) {
	  emit_dta(as, MIPSI_ROTR, dest, tmp1, (-HASH_ROT1)&31);
	} else {
	  emit_dst(as, MIPSI_OR, dest, dest, tmp1);
	  emit_dta(as, MIPSI_SLL, tmp1, tmp1, HASH_ROT1);
	  emit_dta(as, MIPSI_SRL, dest, tmp1, (-HASH_ROT1)&31);
	}
	emit_dst(as, MIPSI_ADDU, tmp1, tmp1, tmp1);
	emit_tg(as, MIPSI_MFC1, tmp2, key);
	emit_tg(as, MIPSI_MFC1, tmp1, key+1);
      } else {
	emit_dst(as, MIPSI_XOR, tmp2, key, tmp1);
	emit_rotr(as, dest, tmp1, tmp2, (-HASH_ROT1)&31);
	emit_dst(as, MIPSI_ADDU, tmp1, key, ra_allock(as, HASH_BIAS, allow));
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
  int32_t lo, hi;
  lua_assert(ofs % sizeof(Node) == 0);
  if (ofs > 32736) {
    idx = dest;
    rset_clear(allow, dest);
    kofs = (int32_t)offsetof(Node, key);
  } else if (ra_hasreg(dest)) {
    emit_tsi(as, MIPSI_ADDIU, dest, node, ofs);
  }
  if (!irt_ispri(irkey->t)) {
    key = ra_scratch(as, allow);
    rset_clear(allow, key);
  }
  if (irt_isnum(irkey->t)) {
    lo = (int32_t)ir_knum(irkey)->u32.lo;
    hi = (int32_t)ir_knum(irkey)->u32.hi;
  } else {
    lo = irkey->i;
    hi = irt_toitype(irkey->t);
    if (!ra_hasreg(key))
      goto nolo;
  }
  asm_guard(as, MIPSI_BNE, key, lo ? ra_allock(as, lo, allow) : RID_ZERO);
nolo:
  asm_guard(as, MIPSI_BNE, type, hi ? ra_allock(as, hi, allow) : RID_ZERO);
  if (ra_hasreg(key)) emit_tsi(as, MIPSI_LW, key, idx, kofs+(LJ_BE?4:0));
  emit_tsi(as, MIPSI_LW, type, idx, kofs+(LJ_BE?0:4));
  if (ofs > 32736)
    emit_tsi(as, MIPSI_ADDU, dest, node, ra_allock(as, ofs, allow));
}

static void asm_newref(ASMState *as, IRIns *ir)
{
  if (ir->r != RID_SINK) {
    const CCallInfo *ci = &lj_ir_callinfo[IRCALL_lj_tab_newkey];
    IRRef args[3];
    args[0] = ASMREF_L;     /* lua_State *L */
    args[1] = ir->op1;      /* GCtab *t     */
    args[2] = ASMREF_TMP1;  /* cTValue *key */
    asm_setupresult(as, ir, ci);  /* TValue * */
    asm_gencall(as, ci, args);
    asm_tvptr(as, ra_releasetmp(as, ASMREF_TMP1), ir->op2);
  }
}

static void asm_uref(ASMState *as, IRIns *ir)
{
  /* NYI: Check that UREFO is still open and not aliasing a slot. */
  Reg dest = ra_dest(as, ir, RSET_GPR);
  if (irref_isk(ir->op1)) {
    GCfunc *fn = ir_kfunc(IR(ir->op1));
    MRef *v = &gcref(fn->l.uvptr[(ir->op2 >> 8)])->uv.v;
    emit_lsptr(as, MIPSI_LW, dest, v, RSET_GPR);
  } else {
    Reg uv = ra_scratch(as, RSET_GPR);
    Reg func = ra_alloc1(as, ir->op1, RSET_GPR);
    if (ir->o == IR_UREFC) {
      asm_guard(as, MIPSI_BEQ, RID_TMP, RID_ZERO);
      emit_tsi(as, MIPSI_ADDIU, dest, uv, (int32_t)offsetof(GCupval, tv));
      emit_tsi(as, MIPSI_LBU, RID_TMP, uv, (int32_t)offsetof(GCupval, closed));
    } else {
      emit_tsi(as, MIPSI_LW, dest, uv, (int32_t)offsetof(GCupval, v));
    }
    emit_tsi(as, MIPSI_LW, uv, func,
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
    emit_tsi(as, MIPSI_ADDIU, dest, dest, ofs);
    emit_dst(as, MIPSI_ADDU, dest, left, right);
    return;
  }
  r = ra_alloc1(as, ref, RSET_GPR);
  ofs += IR(refk)->i;
  if (checki16(ofs))
    emit_tsi(as, MIPSI_ADDIU, dest, r, ofs);
  else
    emit_dst(as, MIPSI_ADDU, dest, r,
	     ra_allock(as, ofs, rset_exclude(RSET_GPR, r)));
}

/* -- Loads and stores ---------------------------------------------------- */

static MIPSIns asm_fxloadins(IRIns *ir)
{
  switch (irt_type(ir->t)) {
  case IRT_I8: return MIPSI_LB;
  case IRT_U8: return MIPSI_LBU;
  case IRT_I16: return MIPSI_LH;
  case IRT_U16: return MIPSI_LHU;
  case IRT_NUM: return MIPSI_LDC1;
  case IRT_FLOAT: return MIPSI_LWC1;
  default: return MIPSI_LW;
  }
}

static MIPSIns asm_fxstoreins(IRIns *ir)
{
  switch (irt_type(ir->t)) {
  case IRT_I8: case IRT_U8: return MIPSI_SB;
  case IRT_I16: case IRT_U16: return MIPSI_SH;
  case IRT_NUM: return MIPSI_SDC1;
  case IRT_FLOAT: return MIPSI_SWC1;
  default: return MIPSI_SW;
  }
}

static void asm_fload(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg idx = ra_alloc1(as, ir->op1, RSET_GPR);
  MIPSIns mi = asm_fxloadins(ir);
  int32_t ofs;
  if (ir->op2 == IRFL_TAB_ARRAY) {
    ofs = asm_fuseabase(as, ir->op1);
    if (ofs) {  /* Turn the t->array load into an add for colocated arrays. */
      emit_tsi(as, MIPSI_ADDIU, dest, idx, ofs);
      return;
    }
  }
  ofs = field_ofs[ir->op2];
  lua_assert(!irt_isfp(ir->t));
  emit_tsi(as, mi, dest, idx, ofs);
}

static void asm_fstore(ASMState *as, IRIns *ir)
{
  if (ir->r != RID_SINK) {
    Reg src = ra_alloc1z(as, ir->op2, RSET_GPR);
    IRIns *irf = IR(ir->op1);
    Reg idx = ra_alloc1(as, irf->op1, rset_exclude(RSET_GPR, src));
    int32_t ofs = field_ofs[irf->op2];
    MIPSIns mi = asm_fxstoreins(ir);
    lua_assert(!irt_isfp(ir->t));
    emit_tsi(as, mi, src, idx, ofs);
  }
}

static void asm_xload(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
  lua_assert(!(ir->op2 & IRXLOAD_UNALIGNED));
  asm_fusexref(as, asm_fxloadins(ir), dest, ir->op1, RSET_GPR, 0);
}

static void asm_xstore(ASMState *as, IRIns *ir, int32_t ofs)
{
  if (ir->r != RID_SINK) {
    Reg src = ra_alloc1z(as, ir->op2, irt_isfp(ir->t) ? RSET_FPR : RSET_GPR);
    asm_fusexref(as, asm_fxstoreins(ir), src, ir->op1,
		 rset_exclude(RSET_GPR, src), ofs);
  }
}

static void asm_ahuvload(ASMState *as, IRIns *ir)
{
  IRType1 t = ir->t;
  Reg dest = RID_NONE, type = RID_TMP, idx;
  RegSet allow = RSET_GPR;
  int32_t ofs = 0;
  if (ra_used(ir)) {
    lua_assert(irt_isnum(t) || irt_isint(t) || irt_isaddr(t));
    dest = ra_dest(as, ir, irt_isnum(t) ? RSET_FPR : RSET_GPR);
    rset_clear(allow, dest);
  }
  idx = asm_fuseahuref(as, ir->op1, &ofs, allow);
  rset_clear(allow, idx);
  if (irt_isnum(t)) {
    asm_guard(as, MIPSI_BEQ, type, RID_ZERO);
    emit_tsi(as, MIPSI_SLTIU, type, type, (int32_t)LJ_TISNUM);
    if (ra_hasreg(dest))
      emit_hsi(as, MIPSI_LDC1, dest, idx, ofs);
  } else {
    asm_guard(as, MIPSI_BNE, type, ra_allock(as, irt_toitype(t), allow));
    if (ra_hasreg(dest)) emit_tsi(as, MIPSI_LW, dest, idx, ofs+(LJ_BE?4:0));
  }
  emit_tsi(as, MIPSI_LW, type, idx, ofs+(LJ_BE?0:4));
}

static void asm_ahustore(ASMState *as, IRIns *ir)
{
  RegSet allow = RSET_GPR;
  Reg idx, src = RID_NONE, type = RID_NONE;
  int32_t ofs = 0;
  if (ir->r == RID_SINK)
    return;
  if (irt_isnum(ir->t)) {
    src = ra_alloc1(as, ir->op2, RSET_FPR);
  } else {
    if (!irt_ispri(ir->t)) {
      src = ra_alloc1(as, ir->op2, allow);
      rset_clear(allow, src);
    }
    type = ra_allock(as, (int32_t)irt_toitype(ir->t), allow);
    rset_clear(allow, type);
  }
  idx = asm_fuseahuref(as, ir->op1, &ofs, allow);
  if (irt_isnum(ir->t)) {
    emit_hsi(as, MIPSI_SDC1, src, idx, ofs);
  } else {
    if (ra_hasreg(src))
      emit_tsi(as, MIPSI_SW, src, idx, ofs+(LJ_BE?4:0));
    emit_tsi(as, MIPSI_SW, type, idx, ofs+(LJ_BE?0:4));
  }
}

static void asm_sload(ASMState *as, IRIns *ir)
{
  int32_t ofs = 8*((int32_t)ir->op1-1) + ((ir->op2 & IRSLOAD_FRAME) ? 4 : 0);
  IRType1 t = ir->t;
  Reg dest = RID_NONE, type = RID_NONE, base;
  RegSet allow = RSET_GPR;
  lua_assert(!(ir->op2 & IRSLOAD_PARENT));  /* Handled by asm_head_side(). */
  lua_assert(irt_isguard(t) || !(ir->op2 & IRSLOAD_TYPECHECK));
  lua_assert(!irt_isint(t) || (ir->op2 & (IRSLOAD_CONVERT|IRSLOAD_FRAME)));
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
	Reg tmp = ra_scratch(as, RSET_FPR);
	emit_tg(as, MIPSI_MFC1, dest, tmp);
	emit_fg(as, MIPSI_CVT_W_D, tmp, tmp);
	dest = tmp;
	t.irt = IRT_NUM;  /* Check for original type. */
      } else {
	Reg tmp = ra_scratch(as, RSET_GPR);
	emit_fg(as, MIPSI_CVT_D_W, dest, dest);
	emit_tg(as, MIPSI_MTC1, tmp, dest);
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
      asm_guard(as, MIPSI_BEQ, RID_TMP, RID_ZERO);
      emit_tsi(as, MIPSI_SLTIU, RID_TMP, RID_TMP, (int32_t)LJ_TISNUM);
      type = RID_TMP;
    }
    if (ra_hasreg(dest)) emit_hsi(as, MIPSI_LDC1, dest, base, ofs);
  } else {
    if ((ir->op2 & IRSLOAD_TYPECHECK)) {
      Reg ktype = ra_allock(as, irt_toitype(t), allow);
      asm_guard(as, MIPSI_BNE, RID_TMP, ktype);
      type = RID_TMP;
    }
    if (ra_hasreg(dest)) emit_tsi(as, MIPSI_LW, dest, base, ofs ^ (LJ_BE?4:0));
  }
  if (ra_hasreg(type)) emit_tsi(as, MIPSI_LW, type, base, ofs ^ (LJ_BE?0:4));
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
      if (LJ_LE) ir++;
    }
    for (;;) {
      Reg r = ra_alloc1z(as, ir->op2, allow);
      emit_tsi(as, MIPSI_SW, r, RID_RET, ofs);
      rset_clear(allow, r);
      if (ofs == sizeof(GCcdata)) break;
      ofs -= 4; if (LJ_BE) ir++; else ir--;
    }
  }
  /* Initialize gct and ctypeid. lj_mem_newgco() already sets marked. */
  emit_tsi(as, MIPSI_SB, RID_RET+1, RID_RET, offsetof(GCcdata, gct));
  emit_tsi(as, MIPSI_SH, RID_TMP, RID_RET, offsetof(GCcdata, ctypeid));
  emit_ti(as, MIPSI_LI, RID_RET+1, ~LJ_TCDATA);
  emit_ti(as, MIPSI_LI, RID_TMP, ctypeid); /* Lower 16 bit used. Sign-ext ok. */
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
  emit_tsi(as, MIPSI_SW, link, tab, (int32_t)offsetof(GCtab, gclist));
  emit_tsi(as, MIPSI_SB, mark, tab, (int32_t)offsetof(GCtab, marked));
  emit_setgl(as, tab, gc.grayagain);
  emit_getgl(as, link, gc.grayagain);
  emit_dst(as, MIPSI_XOR, mark, mark, RID_TMP);  /* Clear black bit. */
  emit_branch(as, MIPSI_BEQ, RID_TMP, RID_ZERO, l_end);
  emit_tsi(as, MIPSI_ANDI, RID_TMP, mark, LJ_GC_BLACK);
  emit_tsi(as, MIPSI_LBU, mark, tab, (int32_t)offsetof(GCtab, marked));
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
  emit_tsi(as, MIPSI_ADDIU, ra_releasetmp(as, ASMREF_TMP1), RID_JGL, -32768);
  obj = IR(ir->op1)->r;
  tmp = ra_scratch(as, rset_exclude(RSET_GPR, obj));
  emit_branch(as, MIPSI_BEQ, RID_TMP, RID_ZERO, l_end);
  emit_tsi(as, MIPSI_ANDI, tmp, tmp, LJ_GC_BLACK);
  emit_branch(as, MIPSI_BEQ, RID_TMP, RID_ZERO, l_end);
  emit_tsi(as, MIPSI_ANDI, RID_TMP, RID_TMP, LJ_GC_WHITES);
  val = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, obj));
  emit_tsi(as, MIPSI_LBU, tmp, obj,
	   (int32_t)offsetof(GCupval, marked)-(int32_t)offsetof(GCupval, tv));
  emit_tsi(as, MIPSI_LBU, RID_TMP, val, (int32_t)offsetof(GChead, marked));
}

/* -- Arithmetic and logic operations ------------------------------------- */

static void asm_fparith(ASMState *as, IRIns *ir, MIPSIns mi)
{
  Reg dest = ra_dest(as, ir, RSET_FPR);
  Reg right, left = ra_alloc2(as, ir, RSET_FPR);
  right = (left >> 8); left &= 255;
  emit_fgh(as, mi, dest, left, right);
}

static void asm_fpunary(ASMState *as, IRIns *ir, MIPSIns mi)
{
  Reg dest = ra_dest(as, ir, RSET_FPR);
  Reg left = ra_hintalloc(as, ir->op1, dest, RSET_FPR);
  emit_fg(as, mi, dest, left);
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
    asm_fparith(as, ir, MIPSI_ADD_D);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg right, left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    if (irref_isk(ir->op2)) {
      int32_t k = IR(ir->op2)->i;
      if (checki16(k)) {
	emit_tsi(as, MIPSI_ADDIU, dest, left, k);
	return;
      }
    }
    right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
    emit_dst(as, MIPSI_ADDU, dest, left, right);
  }
}

static void asm_sub(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    asm_fparith(as, ir, MIPSI_SUB_D);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg right, left = ra_alloc2(as, ir, RSET_GPR);
    right = (left >> 8); left &= 255;
    emit_dst(as, MIPSI_SUBU, dest, left, right);
  }
}

static void asm_mul(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    asm_fparith(as, ir, MIPSI_MUL_D);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg right, left = ra_alloc2(as, ir, RSET_GPR);
    right = (left >> 8); left &= 255;
    emit_dst(as, MIPSI_MUL, dest, left, right);
  }
}

static void asm_neg(ASMState *as, IRIns *ir)
{
  if (irt_isnum(ir->t)) {
    asm_fpunary(as, ir, MIPSI_NEG_D);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    emit_dst(as, MIPSI_SUBU, dest, RID_ZERO, left);
  }
}

static void asm_arithov(ASMState *as, IRIns *ir)
{
  Reg right, left, tmp, dest = ra_dest(as, ir, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int k = IR(ir->op2)->i;
    if (ir->o == IR_SUBOV) k = -k;
    if (checki16(k)) {  /* (dest < left) == (k >= 0 ? 1 : 0) */
      left = ra_alloc1(as, ir->op1, RSET_GPR);
      asm_guard(as, k >= 0 ? MIPSI_BNE : MIPSI_BEQ, RID_TMP, RID_ZERO);
      emit_dst(as, MIPSI_SLT, RID_TMP, dest, dest == left ? RID_TMP : left);
      emit_tsi(as, MIPSI_ADDIU, dest, left, k);
      if (dest == left) emit_move(as, RID_TMP, left);
      return;
    }
  }
  left = ra_alloc2(as, ir, RSET_GPR);
  right = (left >> 8); left &= 255;
  tmp = ra_scratch(as, rset_exclude(rset_exclude(rset_exclude(RSET_GPR, left),
						 right), dest));
  asm_guard(as, MIPSI_BLTZ, RID_TMP, 0);
  emit_dst(as, MIPSI_AND, RID_TMP, RID_TMP, tmp);
  if (ir->o == IR_ADDOV) {  /* ((dest^left) & (dest^right)) < 0 */
    emit_dst(as, MIPSI_XOR, RID_TMP, dest, dest == right ? RID_TMP : right);
  } else {  /* ((dest^left) & (dest^~right)) < 0 */
    emit_dst(as, MIPSI_XOR, RID_TMP, RID_TMP, dest);
    emit_dst(as, MIPSI_NOR, RID_TMP, dest == right ? RID_TMP : right, RID_ZERO);
  }
  emit_dst(as, MIPSI_XOR, tmp, dest, dest == left ? RID_TMP : left);
  emit_dst(as, ir->o == IR_ADDOV ? MIPSI_ADDU : MIPSI_SUBU, dest, left, right);
  if (dest == left || dest == right)
    emit_move(as, RID_TMP, dest == left ? left : right);
}

static void asm_mulov(ASMState *as, IRIns *ir)
{
#if LJ_DUALNUM
#error "NYI: MULOV"
#else
  UNUSED(as); UNUSED(ir); lua_assert(0);  /* Unused in single-number mode. */
#endif
}

#if LJ_HASFFI
static void asm_add64(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg right, left = ra_alloc1(as, ir->op1, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    if (k == 0) {
      emit_dst(as, MIPSI_ADDU, dest, left, RID_TMP);
      goto loarith;
    } else if (checki16(k)) {
      emit_dst(as, MIPSI_ADDU, dest, dest, RID_TMP);
      emit_tsi(as, MIPSI_ADDIU, dest, left, k);
      goto loarith;
    }
  }
  emit_dst(as, MIPSI_ADDU, dest, dest, RID_TMP);
  right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
  emit_dst(as, MIPSI_ADDU, dest, left, right);
loarith:
  ir--;
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc1(as, ir->op1, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    if (k == 0) {
      if (dest != left)
	emit_move(as, dest, left);
      return;
    } else if (checki16(k)) {
      if (dest == left) {
	Reg tmp = ra_scratch(as, rset_exclude(RSET_GPR, left));
	emit_move(as, dest, tmp);
	dest = tmp;
      }
      emit_dst(as, MIPSI_SLTU, RID_TMP, dest, left);
      emit_tsi(as, MIPSI_ADDIU, dest, left, k);
      return;
    }
  }
  right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
  if (dest == left && dest == right) {
    Reg tmp = ra_scratch(as, rset_exclude(rset_exclude(RSET_GPR, left), right));
    emit_move(as, dest, tmp);
    dest = tmp;
  }
  emit_dst(as, MIPSI_SLTU, RID_TMP, dest, dest == left ? right : left);
  emit_dst(as, MIPSI_ADDU, dest, left, right);
}

static void asm_sub64(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg right, left = ra_alloc2(as, ir, RSET_GPR);
  right = (left >> 8); left &= 255;
  emit_dst(as, MIPSI_SUBU, dest, dest, RID_TMP);
  emit_dst(as, MIPSI_SUBU, dest, left, right);
  ir--;
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc2(as, ir, RSET_GPR);
  right = (left >> 8); left &= 255;
  if (dest == left) {
    Reg tmp = ra_scratch(as, rset_exclude(rset_exclude(RSET_GPR, left), right));
    emit_move(as, dest, tmp);
    dest = tmp;
  }
  emit_dst(as, MIPSI_SLTU, RID_TMP, left, dest);
  emit_dst(as, MIPSI_SUBU, dest, left, right);
}

static void asm_neg64(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
  emit_dst(as, MIPSI_SUBU, dest, dest, RID_TMP);
  emit_dst(as, MIPSI_SUBU, dest, RID_ZERO, left);
  ir--;
  dest = ra_dest(as, ir, RSET_GPR);
  left = ra_alloc1(as, ir->op1, RSET_GPR);
  emit_dst(as, MIPSI_SLTU, RID_TMP, RID_ZERO, dest);
  emit_dst(as, MIPSI_SUBU, dest, RID_ZERO, left);
}
#endif

static void asm_bitnot(ASMState *as, IRIns *ir)
{
  Reg left, right, dest = ra_dest(as, ir, RSET_GPR);
  IRIns *irl = IR(ir->op1);
  if (mayfuse(as, ir->op1) && irl->o == IR_BOR) {
    left = ra_alloc2(as, irl, RSET_GPR);
    right = (left >> 8); left &= 255;
  } else {
    left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
    right = RID_ZERO;
  }
  emit_dst(as, MIPSI_NOR, dest, left, right);
}

static void asm_bitswap(ASMState *as, IRIns *ir)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg left = ra_alloc1(as, ir->op1, RSET_GPR);
  if ((as->flags & JIT_F_MIPS32R2)) {
    emit_dta(as, MIPSI_ROTR, dest, RID_TMP, 16);
    emit_dst(as, MIPSI_WSBH, RID_TMP, 0, left);
  } else {
    Reg tmp = ra_scratch(as, rset_exclude(rset_exclude(RSET_GPR, left), dest));
    emit_dst(as, MIPSI_OR, dest, dest, tmp);
    emit_dst(as, MIPSI_OR, dest, dest, RID_TMP);
    emit_tsi(as, MIPSI_ANDI, dest, dest, 0xff00);
    emit_dta(as, MIPSI_SLL, RID_TMP, RID_TMP, 8);
    emit_dta(as, MIPSI_SRL, dest, left, 8);
    emit_tsi(as, MIPSI_ANDI, RID_TMP, left, 0xff00);
    emit_dst(as, MIPSI_OR, tmp, tmp, RID_TMP);
    emit_dta(as, MIPSI_SRL, tmp, left, 24);
    emit_dta(as, MIPSI_SLL, RID_TMP, left, 24);
  }
}

static void asm_bitop(ASMState *as, IRIns *ir, MIPSIns mi, MIPSIns mik)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  Reg right, left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
  if (irref_isk(ir->op2)) {
    int32_t k = IR(ir->op2)->i;
    if (checku16(k)) {
      emit_tsi(as, mik, dest, left, k);
      return;
    }
  }
  right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
  emit_dst(as, mi, dest, left, right);
}

static void asm_bitshift(ASMState *as, IRIns *ir, MIPSIns mi, MIPSIns mik)
{
  Reg dest = ra_dest(as, ir, RSET_GPR);
  if (irref_isk(ir->op2)) {  /* Constant shifts. */
    uint32_t shift = (uint32_t)(IR(ir->op2)->i & 31);
    emit_dta(as, mik, dest, ra_hintalloc(as, ir->op1, dest, RSET_GPR), shift);
  } else {
    Reg right, left = ra_alloc2(as, ir, RSET_GPR);
    right = (left >> 8); left &= 255;
    emit_dst(as, mi, dest, right, left);  /* Shift amount is in rs. */
  }
}

static void asm_bitror(ASMState *as, IRIns *ir)
{
  if ((as->flags & JIT_F_MIPS32R2)) {
    asm_bitshift(as, ir, MIPSI_ROTRV, MIPSI_ROTR);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    if (irref_isk(ir->op2)) {  /* Constant shifts. */
      uint32_t shift = (uint32_t)(IR(ir->op2)->i & 31);
      Reg left = ra_hintalloc(as, ir->op1, dest, RSET_GPR);
      emit_rotr(as, dest, left, RID_TMP, shift);
    } else {
      Reg right, left = ra_alloc2(as, ir, RSET_GPR);
      right = (left >> 8); left &= 255;
      emit_dst(as, MIPSI_OR, dest, dest, RID_TMP);
      emit_dst(as, MIPSI_SRLV, dest, right, left);
      emit_dst(as, MIPSI_SLLV, RID_TMP, RID_TMP, left);
      emit_dst(as, MIPSI_SUBU, RID_TMP, ra_allock(as, 32, RSET_GPR), right);
    }
  }
}

static void asm_min_max(ASMState *as, IRIns *ir, int ismax)
{
  if (irt_isnum(ir->t)) {
    Reg dest = ra_dest(as, ir, RSET_FPR);
    Reg right, left = ra_alloc2(as, ir, RSET_FPR);
    right = (left >> 8); left &= 255;
    if (dest == left) {
      emit_fg(as, MIPSI_MOVT_D, dest, right);
    } else {
      emit_fg(as, MIPSI_MOVF_D, dest, left);
      if (dest != right) emit_fg(as, MIPSI_MOV_D, dest, right);
    }
    emit_fgh(as, MIPSI_C_OLT_D, 0, ismax ? left : right, ismax ? right : left);
  } else {
    Reg dest = ra_dest(as, ir, RSET_GPR);
    Reg right, left = ra_alloc2(as, ir, RSET_GPR);
    right = (left >> 8); left &= 255;
    if (dest == left) {
      emit_dst(as, MIPSI_MOVN, dest, right, RID_TMP);
    } else {
      emit_dst(as, MIPSI_MOVZ, dest, left, RID_TMP);
      if (dest != right) emit_move(as, dest, right);
    }
    emit_dst(as, MIPSI_SLT, RID_TMP,
	     ismax ? left : right, ismax ? right : left);
  }
}

/* -- Comparisons --------------------------------------------------------- */

static void asm_comp(ASMState *as, IRIns *ir)
{
  /* ORDER IR: LT GE LE GT  ULT UGE ULE UGT. */
  IROp op = ir->o;
  if (irt_isnum(ir->t)) {
    Reg right, left = ra_alloc2(as, ir, RSET_FPR);
    right = (left >> 8); left &= 255;
    asm_guard(as, (op&1) ? MIPSI_BC1T : MIPSI_BC1F, 0, 0);
    emit_fgh(as, MIPSI_C_OLT_D + ((op&3) ^ ((op>>2)&1)), 0, left, right);
  } else {
    Reg right, left = ra_alloc1(as, ir->op1, RSET_GPR);
    if (op == IR_ABC) op = IR_UGT;
    if ((op&4) == 0 && irref_isk(ir->op2) && IR(ir->op2)->i == 0) {
      MIPSIns mi = (op&2) ? ((op&1) ? MIPSI_BLEZ : MIPSI_BGTZ) :
			    ((op&1) ? MIPSI_BLTZ : MIPSI_BGEZ);
      asm_guard(as, mi, left, 0);
    } else {
      if (irref_isk(ir->op2)) {
	int32_t k = IR(ir->op2)->i;
	if ((op&2)) k++;
	if (checki16(k)) {
	  asm_guard(as, (op&1) ? MIPSI_BNE : MIPSI_BEQ, RID_TMP, RID_ZERO);
	  emit_tsi(as, (op&4) ? MIPSI_SLTIU : MIPSI_SLTI,
		   RID_TMP, left, k);
	  return;
	}
      }
      right = ra_alloc1(as, ir->op2, rset_exclude(RSET_GPR, left));
      asm_guard(as, ((op^(op>>1))&1) ? MIPSI_BNE : MIPSI_BEQ, RID_TMP, RID_ZERO);
      emit_dst(as, (op&4) ? MIPSI_SLTU : MIPSI_SLT,
	       RID_TMP, (op&2) ? right : left, (op&2) ? left : right);
    }
  }
}

static void asm_compeq(ASMState *as, IRIns *ir)
{
  Reg right, left = ra_alloc2(as, ir, irt_isnum(ir->t) ? RSET_FPR : RSET_GPR);
  right = (left >> 8); left &= 255;
  if (irt_isnum(ir->t)) {
    asm_guard(as, (ir->o & 1) ? MIPSI_BC1T : MIPSI_BC1F, 0, 0);
    emit_fgh(as, MIPSI_C_EQ_D, 0, left, right);
  } else {
    asm_guard(as, (ir->o & 1) ? MIPSI_BEQ : MIPSI_BNE, left, right);
  }
}

#if LJ_HASFFI
/* 64 bit integer comparisons. */
static void asm_comp64(ASMState *as, IRIns *ir)
{
  /* ORDER IR: LT GE LE GT  ULT UGE ULE UGT. */
  IROp op = (ir-1)->o;
  MCLabel l_end;
  Reg rightlo, leftlo, righthi, lefthi = ra_alloc2(as, ir, RSET_GPR);
  righthi = (lefthi >> 8); lefthi &= 255;
  leftlo = ra_alloc2(as, ir-1,
		     rset_exclude(rset_exclude(RSET_GPR, lefthi), righthi));
  rightlo = (leftlo >> 8); leftlo &= 255;
  asm_guard(as, ((op^(op>>1))&1) ? MIPSI_BNE : MIPSI_BEQ, RID_TMP, RID_ZERO);
  l_end = emit_label(as);
  if (lefthi != righthi)
    emit_dst(as, (op&4) ? MIPSI_SLTU : MIPSI_SLT, RID_TMP,
	     (op&2) ? righthi : lefthi, (op&2) ? lefthi : righthi);
  emit_dst(as, MIPSI_SLTU, RID_TMP,
	   (op&2) ? rightlo : leftlo, (op&2) ? leftlo : rightlo);
  if (lefthi != righthi)
    emit_branch(as, MIPSI_BEQ, lefthi, righthi, l_end);
}

static void asm_comp64eq(ASMState *as, IRIns *ir)
{
  Reg tmp, right, left = ra_alloc2(as, ir, RSET_GPR);
  right = (left >> 8); left &= 255;
  asm_guard(as, ((ir-1)->o & 1) ? MIPSI_BEQ : MIPSI_BNE, RID_TMP, RID_ZERO);
  tmp = ra_scratch(as, rset_exclude(rset_exclude(RSET_GPR, left), right));
  emit_dst(as, MIPSI_OR, RID_TMP, RID_TMP, tmp);
  emit_dst(as, MIPSI_XOR, tmp, left, right);
  left = ra_alloc2(as, ir-1, RSET_GPR);
  right = (left >> 8); left &= 255;
  emit_dst(as, MIPSI_XOR, RID_TMP, left, right);
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
  } else if ((ir-1)->o < IR_EQ) {  /* 64 bit integer comparisons. ORDER IR. */
    as->curins--;  /* Always skip the loword comparison. */
    asm_comp64(as, ir);
    return;
  } else if ((ir-1)->o <= IR_NE) {  /* 64 bit integer comparisons. ORDER IR. */
    as->curins--;  /* Always skip the loword comparison. */
    asm_comp64eq(as, ir);
    return;
  } else if ((ir-1)->o == IR_XSTORE) {
    as->curins--;  /* Handle both stores here. */
    if ((ir-1)->r != RID_SINK) {
      asm_xstore(as, ir, LJ_LE ? 4 : 0);
      asm_xstore(as, ir-1, LJ_LE ? 0 : 4);
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
  ExitNo oldsnap = as->snapno;
  rset_clear(allow, pbase);
  tmp = allow ? rset_pickbot(allow) :
		(pbase == RID_RETHI ? RID_RETLO : RID_RETHI);
  as->snapno = exitno;
  asm_guard(as, MIPSI_BNE, RID_TMP, RID_ZERO);
  as->snapno = oldsnap;
  if (allow == RSET_EMPTY)  /* Restore temp. register. */
    emit_tsi(as, MIPSI_LW, tmp, RID_SP, 0);
  else
    ra_modified(as, tmp);
  emit_tsi(as, MIPSI_SLTIU, RID_TMP, RID_TMP, (int32_t)(8*topslot));
  emit_dst(as, MIPSI_SUBU, RID_TMP, tmp, pbase);
  emit_tsi(as, MIPSI_LW, tmp, tmp, offsetof(lua_State, maxstack));
  if (pbase == RID_TMP)
    emit_getgl(as, RID_TMP, jit_base);
  emit_getgl(as, tmp, jit_L);
  if (allow == RSET_EMPTY)  /* Spill temp. register. */
    emit_tsi(as, MIPSI_SW, tmp, RID_SP, 0);
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
      emit_hsi(as, MIPSI_SDC1, src, RID_BASE, ofs);
    } else {
      Reg type;
      RegSet allow = rset_exclude(RSET_GPR, RID_BASE);
      lua_assert(irt_ispri(ir->t) || irt_isaddr(ir->t) || irt_isinteger(ir->t));
      if (!irt_ispri(ir->t)) {
	Reg src = ra_alloc1(as, ref, allow);
	rset_clear(allow, src);
	emit_tsi(as, MIPSI_SW, src, RID_BASE, ofs+(LJ_BE?4:0));
      }
      if ((sn & (SNAP_CONT|SNAP_FRAME))) {
	if (s == 0) continue;  /* Do not overwrite link to previous frame. */
	type = ra_allock(as, (int32_t)(*flinks--), allow);
      } else {
	type = ra_allock(as, (int32_t)irt_toitype(ir->t), allow);
      }
      emit_tsi(as, MIPSI_SW, type, RID_BASE, ofs+(LJ_BE?0:4));
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
  /* Assumes asm_snap_prep() already done. */
  asm_guard(as, MIPSI_BNE, RID_RET, RID_ZERO);
  args[0] = ASMREF_TMP1;  /* global_State *g */
  args[1] = ASMREF_TMP2;  /* MSize steps     */
  asm_gencall(as, ci, args);
  emit_tsi(as, MIPSI_ADDIU, ra_releasetmp(as, ASMREF_TMP1), RID_JGL, -32768);
  tmp = ra_releasetmp(as, ASMREF_TMP2);
  emit_loadi(as, tmp, as->gcsteps);
  /* Jump around GC step if GC total < GC threshold. */
  emit_branch(as, MIPSI_BNE, RID_TMP, RID_ZERO, l_end);
  emit_dst(as, MIPSI_SLTU, RID_TMP, RID_TMP, tmp);
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
  p[-1] = MIPSI_NOP;
  if (as->loopinv) {  /* Inverted loop branch? */
    /* asm_guard already inverted the cond branch. Only patch the target. */
    p[-3] |= ((target-p+2) & 0x0000ffffu);
  } else {
    p[-2] = MIPSI_J|(((uintptr_t)target>>2)&0x03ffffffu);
  }
}

/* -- Head of trace ------------------------------------------------------- */

/* Coalesce BASE register for a root trace. */
static void asm_head_root_base(ASMState *as)
{
  IRIns *ir = IR(REF_BASE);
  Reg r = ir->r;
  if (as->loopinv) as->mctop--;
  if (ra_hasreg(r)) {
    ra_free(as, r);
    if (rset_test(as->modset, r))
      ir->r = RID_INIT;  /* No inheritance for modified BASE register. */
    if (r != RID_BASE)
      emit_move(as, r, RID_BASE);
  }
}

/* Coalesce BASE register for a side trace. */
static RegSet asm_head_side_base(ASMState *as, IRIns *irp, RegSet allow)
{
  IRIns *ir = IR(REF_BASE);
  Reg r = ir->r;
  if (as->loopinv) as->mctop--;
  if (ra_hasreg(r)) {
    ra_free(as, r);
    if (rset_test(as->modset, r))
      ir->r = RID_INIT;  /* No inheritance for modified BASE register. */
    if (irp->r == r) {
      rset_clear(allow, r);  /* Mark same BASE register as coalesced. */
    } else if (ra_hasreg(irp->r) && rset_test(as->freeset, irp->r)) {
      rset_clear(allow, irp->r);
      emit_move(as, r, irp->r);  /* Move from coalesced parent reg. */
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
  MCode *target = lnk ? traceref(as->J,lnk)->mcode : (MCode *)lj_vm_exit_interp;
  int32_t spadj = as->T->spadjust;
  MCode *p = as->mctop-1;
  *p = spadj ? (MIPSI_ADDIU|MIPSF_T(RID_SP)|MIPSF_S(RID_SP)|spadj) : MIPSI_NOP;
  p[-1] = MIPSI_J|(((uintptr_t)target>>2)&0x03ffffffu);
}

/* Prepare tail of code. */
static void asm_tail_prep(ASMState *as)
{
  as->mcp = as->mctop-2;  /* Leave room for branch plus nop or stack adj. */
  as->invmcp = as->loopref ? as->mcp : NULL;
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
  case IR_EQ: case IR_NE: asm_compeq(as, ir); break;
  case IR_LT: case IR_GE: case IR_LE: case IR_GT:
  case IR_ULT: case IR_UGE: case IR_ULE: case IR_UGT:
  case IR_ABC:
    asm_comp(as, ir);
    break;

  case IR_RETF: asm_retf(as, ir); break;

  /* Bit ops. */
  case IR_BNOT: asm_bitnot(as, ir); break;
  case IR_BSWAP: asm_bitswap(as, ir); break;

  case IR_BAND: asm_bitop(as, ir, MIPSI_AND, MIPSI_ANDI); break;
  case IR_BOR:  asm_bitop(as, ir, MIPSI_OR, MIPSI_ORI); break;
  case IR_BXOR: asm_bitop(as, ir, MIPSI_XOR, MIPSI_XORI); break;

  case IR_BSHL: asm_bitshift(as, ir, MIPSI_SLLV, MIPSI_SLL); break;
  case IR_BSHR: asm_bitshift(as, ir, MIPSI_SRLV, MIPSI_SRL); break;
  case IR_BSAR: asm_bitshift(as, ir, MIPSI_SRAV, MIPSI_SRA); break;
  case IR_BROL: lua_assert(0); break;
  case IR_BROR: asm_bitror(as, ir); break;

  /* Arithmetic ops. */
  case IR_ADD: asm_add(as, ir); break;
  case IR_SUB: asm_sub(as, ir); break;
  case IR_MUL: asm_mul(as, ir); break;
  case IR_DIV: asm_fparith(as, ir, MIPSI_DIV_D); break;
  case IR_MOD: asm_callid(as, ir, IRCALL_lj_vm_modi); break;
  case IR_POW: asm_callid(as, ir, IRCALL_lj_vm_powi); break;
  case IR_NEG: asm_neg(as, ir); break;

  case IR_ABS: asm_fpunary(as, ir, MIPSI_ABS_D); break;
  case IR_ATAN2: asm_callid(as, ir, IRCALL_atan2); break;
  case IR_LDEXP: asm_callid(as, ir, IRCALL_ldexp); break;
  case IR_MIN: asm_min_max(as, ir, 0); break;
  case IR_MAX: asm_min_max(as, ir, 1); break;
  case IR_FPMATH:
    if (ir->op2 == IRFPM_EXP2 && asm_fpjoin_pow(as, ir))
      break;
    if (ir->op2 <= IRFPM_TRUNC)
      asm_callround(as, ir, IRCALL_lj_vm_floor + ir->op2);
    else if (ir->op2 == IRFPM_SQRT)
      asm_fpunary(as, ir, MIPSI_SQRT_D);
    else
      asm_callid(as, ir, IRCALL_lj_vm_floor + ir->op2);
    break;

  /* Overflow-checking arithmetic ops. */
  case IR_ADDOV: asm_arithov(as, ir); break;
  case IR_SUBOV: asm_arithov(as, ir); break;
  case IR_MULOV: asm_mulov(as, ir); break;

  /* Memory references. */
  case IR_AREF: asm_aref(as, ir); break;
  case IR_HREF: asm_href(as, ir); break;
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
  int nslots = 4, ngpr = REGARG_NUMGPR, nfpr = REGARG_NUMFPR;
  asm_collectargs(as, ir, ci, args);
  for (i = 0; i < nargs; i++) {
    if (args[i] && irt_isfp(IR(args[i])->t) &&
	nfpr > 0 && !(ci->flags & CCI_VARARG)) {
      nfpr--;
      ngpr -= irt_isnum(IR(args[i])->t) ? 2 : 1;
    } else if (args[i] && irt_isnum(IR(args[i])->t)) {
      nfpr = 0;
      ngpr = ngpr & ~1;
      if (ngpr > 0) ngpr -= 2; else nslots = (nslots+3) & ~1;
    } else {
      nfpr = 0;
      if (ngpr > 0) ngpr--; else nslots++;
    }
  }
  if (nslots > as->evenspill)  /* Leave room for args in stack slots. */
    as->evenspill = nslots;
  return irt_isfp(ir->t) ? REGSP_HINT(RID_FPRET) : REGSP_HINT(RID_RET);
}

static void asm_setup_target(ASMState *as)
{
  asm_sparejump_setup(as);
  asm_exitstub_setup(as);
}

/* -- Trace patching ------------------------------------------------------ */

/* Patch exit jumps of existing machine code to a new target. */
void lj_asm_patchexit(jit_State *J, GCtrace *T, ExitNo exitno, MCode *target)
{
  MCode *p = T->mcode;
  MCode *pe = (MCode *)((char *)p + T->szmcode);
  MCode *px = exitstub_trace_addr(T, exitno);
  MCode *cstart = NULL, *cstop = NULL;
  MCode *mcarea = lj_mcode_patch(J, p, 0);
  MCode exitload = MIPSI_LI | MIPSF_T(RID_TMP) | exitno;
  MCode tjump = MIPSI_J|(((uintptr_t)target>>2)&0x03ffffffu);
  for (p++; p < pe; p++) {
    if (*p == exitload) {  /* Look for load of exit number. */
      if (((p[-1] ^ (px-p)) & 0xffffu) == 0) {  /* Look for exitstub branch. */
	ptrdiff_t delta = target - p;
	if (((delta + 0x8000) >> 16) == 0) {  /* Patch in-range branch. */
	patchbranch:
	  p[-1] = (p[-1] & 0xffff0000u) | (delta & 0xffffu);
	  *p = MIPSI_NOP;  /* Replace the load of the exit number. */
	  cstop = p;
	  if (!cstart) cstart = p-1;
	} else {  /* Branch out of range. Use spare jump slot in mcarea. */
	  int i;
	  for (i = 2; i < 2+MIPS_SPAREJUMP*2; i += 2) {
	    if (mcarea[i] == tjump) {
	      delta = mcarea+i - p;
	      goto patchbranch;
	    } else if (mcarea[i] == MIPSI_NOP) {
	      mcarea[i] = tjump;
	      cstart = mcarea+i;
	      delta = mcarea+i - p;
	      goto patchbranch;
	    }
	  }
	  /* Ignore jump slot overflow. Child trace is simply not attached. */
	}
      } else if (p+1 == pe) {
	/* Patch NOP after code for inverted loop branch. Use of J is ok. */
	lua_assert(p[1] == MIPSI_NOP);
	p[1] = tjump;
	*p = MIPSI_NOP;  /* Replace the load of the exit number. */
	cstop = p+2;
	if (!cstart) cstart = p+1;
      }
    }
  }
  if (cstart) lj_mcode_sync(cstart, cstop);
  lj_mcode_patch(J, mcarea, 1);
}

