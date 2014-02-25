/*
** Lua parser (source code -> bytecode).
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#define lj_parse_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_func.h"
#include "lj_state.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#include "lj_lex.h"
#include "lj_parse.h"
#include "lj_vm.h"
#include "lj_vmevent.h"

/* -- Parser structures and definitions ----------------------------------- */

/* Expression kinds. */
typedef enum {
  /* Constant expressions must be first and in this order: */
  VKNIL,
  VKFALSE,
  VKTRUE,
  VKSTR,	/* sval = string value */
  VKNUM,	/* nval = number value */
  VKLAST = VKNUM,
  VKCDATA,	/* nval = cdata value, not treated as a constant expression */
  /* Non-constant expressions follow: */
  VLOCAL,	/* info = local register, aux = vstack index */
  VUPVAL,	/* info = upvalue index, aux = vstack index */
  VGLOBAL,	/* sval = string value */
  VINDEXED,	/* info = table register, aux = index reg/byte/string const */
  VJMP,		/* info = instruction PC */
  VRELOCABLE,	/* info = instruction PC */
  VNONRELOC,	/* info = result register */
  VCALL,	/* info = instruction PC, aux = base */
  VVOID
} ExpKind;

/* Expression descriptor. */
typedef struct ExpDesc {
  union {
    struct {
      uint32_t info;	/* Primary info. */
      uint32_t aux;	/* Secondary info. */
    } s;
    TValue nval;	/* Number value. */
    GCstr *sval;	/* String value. */
  } u;
  ExpKind k;
  BCPos t;		/* True condition jump list. */
  BCPos f;		/* False condition jump list. */
} ExpDesc;

/* Macros for expressions. */
#define expr_hasjump(e)		((e)->t != (e)->f)

#define expr_isk(e)		((e)->k <= VKLAST)
#define expr_isk_nojump(e)	(expr_isk(e) && !expr_hasjump(e))
#define expr_isnumk(e)		((e)->k == VKNUM)
#define expr_isnumk_nojump(e)	(expr_isnumk(e) && !expr_hasjump(e))
#define expr_isstrk(e)		((e)->k == VKSTR)

#define expr_numtv(e)		check_exp(expr_isnumk((e)), &(e)->u.nval)
#define expr_numberV(e)		numberVnum(expr_numtv((e)))

/* Initialize expression. */
static LJ_AINLINE void expr_init(ExpDesc *e, ExpKind k, uint32_t info)
{
  e->k = k;
  e->u.s.info = info;
  e->f = e->t = NO_JMP;
}

/* Check number constant for +-0. */
static int expr_numiszero(ExpDesc *e)
{
  TValue *o = expr_numtv(e);
  return tvisint(o) ? (intV(o) == 0) : tviszero(o);
}

/* Per-function linked list of scope blocks. */
typedef struct FuncScope {
  struct FuncScope *prev;	/* Link to outer scope. */
  MSize vstart;			/* Start of block-local variables. */
  uint8_t nactvar;		/* Number of active vars outside the scope. */
  uint8_t flags;		/* Scope flags. */
} FuncScope;

#define FSCOPE_LOOP		0x01	/* Scope is a (breakable) loop. */
#define FSCOPE_BREAK		0x02	/* Break used in scope. */
#define FSCOPE_GOLA		0x04	/* Goto or label used in scope. */
#define FSCOPE_UPVAL		0x08	/* Upvalue in scope. */
#define FSCOPE_NOCLOSE		0x10	/* Do not close upvalues. */

#define NAME_BREAK		((GCstr *)(uintptr_t)1)

/* Index into variable stack. */
typedef uint16_t VarIndex;
#define LJ_MAX_VSTACK		(65536 - LJ_MAX_UPVAL)

/* Variable/goto/label info. */
#define VSTACK_VAR_RW		0x01	/* R/W variable. */
#define VSTACK_GOTO		0x02	/* Pending goto. */
#define VSTACK_LABEL		0x04	/* Label. */

/* Per-function state. */
typedef struct FuncState {
  GCtab *kt;			/* Hash table for constants. */
  LexState *ls;			/* Lexer state. */
  lua_State *L;			/* Lua state. */
  FuncScope *bl;		/* Current scope. */
  struct FuncState *prev;	/* Enclosing function. */
  BCPos pc;			/* Next bytecode position. */
  BCPos lasttarget;		/* Bytecode position of last jump target. */
  BCPos jpc;			/* Pending jump list to next bytecode. */
  BCReg freereg;		/* First free register. */
  BCReg nactvar;		/* Number of active local variables. */
  BCReg nkn, nkgc;		/* Number of lua_Number/GCobj constants */
  BCLine linedefined;		/* First line of the function definition. */
  BCInsLine *bcbase;		/* Base of bytecode stack. */
  BCPos bclim;			/* Limit of bytecode stack. */
  MSize vbase;			/* Base of variable stack for this function. */
  uint8_t flags;		/* Prototype flags. */
  uint8_t numparams;		/* Number of parameters. */
  uint8_t framesize;		/* Fixed frame size. */
  uint8_t nuv;			/* Number of upvalues */
  VarIndex varmap[LJ_MAX_LOCVAR];  /* Map from register to variable idx. */
  VarIndex uvmap[LJ_MAX_UPVAL];	/* Map from upvalue to variable idx. */
  VarIndex uvtmp[LJ_MAX_UPVAL];	/* Temporary upvalue map. */
} FuncState;

/* Binary and unary operators. ORDER OPR */
typedef enum BinOpr {
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_DIV, OPR_MOD, OPR_POW,  /* ORDER ARITH */
  OPR_CONCAT,
  OPR_NE, OPR_EQ,
  OPR_LT, OPR_GE, OPR_LE, OPR_GT,
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;

LJ_STATIC_ASSERT((int)BC_ISGE-(int)BC_ISLT == (int)OPR_GE-(int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_ISLE-(int)BC_ISLT == (int)OPR_LE-(int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_ISGT-(int)BC_ISLT == (int)OPR_GT-(int)OPR_LT);
LJ_STATIC_ASSERT((int)BC_SUBVV-(int)BC_ADDVV == (int)OPR_SUB-(int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_MULVV-(int)BC_ADDVV == (int)OPR_MUL-(int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_DIVVV-(int)BC_ADDVV == (int)OPR_DIV-(int)OPR_ADD);
LJ_STATIC_ASSERT((int)BC_MODVV-(int)BC_ADDVV == (int)OPR_MOD-(int)OPR_ADD);

/* -- Error handling ------------------------------------------------------ */

LJ_NORET LJ_NOINLINE static void err_syntax(LexState *ls, ErrMsg em)
{
  lj_lex_error(ls, ls->token, em);
}

LJ_NORET LJ_NOINLINE static void err_token(LexState *ls, LexToken token)
{
  lj_lex_error(ls, ls->token, LJ_ERR_XTOKEN, lj_lex_token2str(ls, token));
}

LJ_NORET static void err_limit(FuncState *fs, uint32_t limit, const char *what)
{
  if (fs->linedefined == 0)
    lj_lex_error(fs->ls, 0, LJ_ERR_XLIMM, limit, what);
  else
    lj_lex_error(fs->ls, 0, LJ_ERR_XLIMF, fs->linedefined, limit, what);
}

#define checklimit(fs, v, l, m)		if ((v) >= (l)) err_limit(fs, l, m)
#define checklimitgt(fs, v, l, m)	if ((v) > (l)) err_limit(fs, l, m)
#define checkcond(ls, c, em)		{ if (!(c)) err_syntax(ls, em); }

/* -- Management of constants --------------------------------------------- */

/* Return bytecode encoding for primitive constant. */
#define const_pri(e)		check_exp((e)->k <= VKTRUE, (e)->k)

#define tvhaskslot(o)	((o)->u32.hi == 0)
#define tvkslot(o)	((o)->u32.lo)

/* Add a number constant. */
static BCReg const_num(FuncState *fs, ExpDesc *e)
{
  lua_State *L = fs->L;
  TValue *o;
  lua_assert(expr_isnumk(e));
  o = lj_tab_set(L, fs->kt, &e->u.nval);
  if (tvhaskslot(o))
    return tvkslot(o);
  o->u64 = fs->nkn;
  return fs->nkn++;
}

/* Add a GC object constant. */
static BCReg const_gc(FuncState *fs, GCobj *gc, uint32_t itype)
{
  lua_State *L = fs->L;
  TValue key, *o;
  setgcV(L, &key, gc, itype);
  /* NOBARRIER: the key is new or kept alive. */
  o = lj_tab_set(L, fs->kt, &key);
  if (tvhaskslot(o))
    return tvkslot(o);
  o->u64 = fs->nkgc;
  return fs->nkgc++;
}

/* Add a string constant. */
static BCReg const_str(FuncState *fs, ExpDesc *e)
{
  lua_assert(expr_isstrk(e) || e->k == VGLOBAL);
  return const_gc(fs, obj2gco(e->u.sval), LJ_TSTR);
}

/* Anchor string constant to avoid GC. */
GCstr *lj_parse_keepstr(LexState *ls, const char *str, size_t len)
{
  /* NOBARRIER: the key is new or kept alive. */
  lua_State *L = ls->L;
  GCstr *s = lj_str_new(L, str, len);
  TValue *tv = lj_tab_setstr(L, ls->fs->kt, s);
  if (tvisnil(tv)) setboolV(tv, 1);
  lj_gc_check(L);
  return s;
}

#if LJ_HASFFI
/* Anchor cdata to avoid GC. */
void lj_parse_keepcdata(LexState *ls, TValue *tv, GCcdata *cd)
{
  /* NOBARRIER: the key is new or kept alive. */
  lua_State *L = ls->L;
  setcdataV(L, tv, cd);
  setboolV(lj_tab_set(L, ls->fs->kt, tv), 1);
}
#endif

/* -- Jump list handling -------------------------------------------------- */

/* Get next element in jump list. */
static BCPos jmp_next(FuncState *fs, BCPos pc)
{
  ptrdiff_t delta = bc_j(fs->bcbase[pc].ins);
  if ((BCPos)delta == NO_JMP)
    return NO_JMP;
  else
    return (BCPos)(((ptrdiff_t)pc+1)+delta);
}

/* Check if any of the instructions on the jump list produce no value. */
static int jmp_novalue(FuncState *fs, BCPos list)
{
  for (; list != NO_JMP; list = jmp_next(fs, list)) {
    BCIns p = fs->bcbase[list >= 1 ? list-1 : list].ins;
    if (!(bc_op(p) == BC_ISTC || bc_op(p) == BC_ISFC || bc_a(p) == NO_REG))
      return 1;
  }
  return 0;
}

/* Patch register of test instructions. */
static int jmp_patchtestreg(FuncState *fs, BCPos pc, BCReg reg)
{
  BCInsLine *ilp = &fs->bcbase[pc >= 1 ? pc-1 : pc];
  BCOp op = bc_op(ilp->ins);
  if (op == BC_ISTC || op == BC_ISFC) {
    if (reg != NO_REG && reg != bc_d(ilp->ins)) {
      setbc_a(&ilp->ins, reg);
    } else {  /* Nothing to store or already in the right register. */
      setbc_op(&ilp->ins, op+(BC_IST-BC_ISTC));
      setbc_a(&ilp->ins, 0);
    }
  } else if (bc_a(ilp->ins) == NO_REG) {
    if (reg == NO_REG) {
      ilp->ins = BCINS_AJ(BC_JMP, bc_a(fs->bcbase[pc].ins), 0);
    } else {
      setbc_a(&ilp->ins, reg);
      if (reg >= bc_a(ilp[1].ins))
	setbc_a(&ilp[1].ins, reg+1);
    }
  } else {
    return 0;  /* Cannot patch other instructions. */
  }
  return 1;
}

/* Drop values for all instructions on jump list. */
static void jmp_dropval(FuncState *fs, BCPos list)
{
  for (; list != NO_JMP; list = jmp_next(fs, list))
    jmp_patchtestreg(fs, list, NO_REG);
}

/* Patch jump instruction to target. */
static void jmp_patchins(FuncState *fs, BCPos pc, BCPos dest)
{
  BCIns *jmp = &fs->bcbase[pc].ins;
  BCPos offset = dest-(pc+1)+BCBIAS_J;
  lua_assert(dest != NO_JMP);
  if (offset > BCMAX_D)
    err_syntax(fs->ls, LJ_ERR_XJUMP);
  setbc_d(jmp, offset);
}

/* Append to jump list. */
static void jmp_append(FuncState *fs, BCPos *l1, BCPos l2)
{
  if (l2 == NO_JMP) {
    return;
  } else if (*l1 == NO_JMP) {
    *l1 = l2;
  } else {
    BCPos list = *l1;
    BCPos next;
    while ((next = jmp_next(fs, list)) != NO_JMP)  /* Find last element. */
      list = next;
    jmp_patchins(fs, list, l2);
  }
}

/* Patch jump list and preserve produced values. */
static void jmp_patchval(FuncState *fs, BCPos list, BCPos vtarget,
			 BCReg reg, BCPos dtarget)
{
  while (list != NO_JMP) {
    BCPos next = jmp_next(fs, list);
    if (jmp_patchtestreg(fs, list, reg))
      jmp_patchins(fs, list, vtarget);  /* Jump to target with value. */
    else
      jmp_patchins(fs, list, dtarget);  /* Jump to default target. */
    list = next;
  }
}

/* Jump to following instruction. Append to list of pending jumps. */
static void jmp_tohere(FuncState *fs, BCPos list)
{
  fs->lasttarget = fs->pc;
  jmp_append(fs, &fs->jpc, list);
}

/* Patch jump list to target. */
static void jmp_patch(FuncState *fs, BCPos list, BCPos target)
{
  if (target == fs->pc) {
    jmp_tohere(fs, list);
  } else {
    lua_assert(target < fs->pc);
    jmp_patchval(fs, list, target, NO_REG, target);
  }
}

/* -- Bytecode register allocator ----------------------------------------- */

/* Bump frame size. */
static void bcreg_bump(FuncState *fs, BCReg n)
{
  BCReg sz = fs->freereg + n;
  if (sz > fs->framesize) {
    if (sz >= LJ_MAX_SLOTS)
      err_syntax(fs->ls, LJ_ERR_XSLOTS);
    fs->framesize = (uint8_t)sz;
  }
}

/* Reserve registers. */
static void bcreg_reserve(FuncState *fs, BCReg n)
{
  bcreg_bump(fs, n);
  fs->freereg += n;
}

/* Free register. */
static void bcreg_free(FuncState *fs, BCReg reg)
{
  if (reg >= fs->nactvar) {
    fs->freereg--;
    lua_assert(reg == fs->freereg);
  }
}

/* Free register for expression. */
static void expr_free(FuncState *fs, ExpDesc *e)
{
  if (e->k == VNONRELOC)
    bcreg_free(fs, e->u.s.info);
}

/* -- Bytecode emitter ---------------------------------------------------- */

/* Emit bytecode instruction. */
static BCPos bcemit_INS(FuncState *fs, BCIns ins)
{
  BCPos pc = fs->pc;
  LexState *ls = fs->ls;
  jmp_patchval(fs, fs->jpc, pc, NO_REG, pc);
  fs->jpc = NO_JMP;
  if (LJ_UNLIKELY(pc >= fs->bclim)) {
    ptrdiff_t base = fs->bcbase - ls->bcstack;
    checklimit(fs, ls->sizebcstack, LJ_MAX_BCINS, "bytecode instructions");
    lj_mem_growvec(fs->L, ls->bcstack, ls->sizebcstack, LJ_MAX_BCINS,BCInsLine);
    fs->bclim = (BCPos)(ls->sizebcstack - base);
    fs->bcbase = ls->bcstack + base;
  }
  fs->bcbase[pc].ins = ins;
  fs->bcbase[pc].line = ls->lastline;
  fs->pc = pc+1;
  return pc;
}

#define bcemit_ABC(fs, o, a, b, c)	bcemit_INS(fs, BCINS_ABC(o, a, b, c))
#define bcemit_AD(fs, o, a, d)		bcemit_INS(fs, BCINS_AD(o, a, d))
#define bcemit_AJ(fs, o, a, j)		bcemit_INS(fs, BCINS_AJ(o, a, j))

#define bcptr(fs, e)			(&(fs)->bcbase[(e)->u.s.info].ins)

/* -- Bytecode emitter for expressions ------------------------------------ */

/* Discharge non-constant expression to any register. */
static void expr_discharge(FuncState *fs, ExpDesc *e)
{
  BCIns ins;
  if (e->k == VUPVAL) {
    ins = BCINS_AD(BC_UGET, 0, e->u.s.info);
  } else if (e->k == VGLOBAL) {
    ins = BCINS_AD(BC_GGET, 0, const_str(fs, e));
  } else if (e->k == VINDEXED) {
    BCReg rc = e->u.s.aux;
    if ((int32_t)rc < 0) {
      ins = BCINS_ABC(BC_TGETS, 0, e->u.s.info, ~rc);
    } else if (rc > BCMAX_C) {
      ins = BCINS_ABC(BC_TGETB, 0, e->u.s.info, rc-(BCMAX_C+1));
    } else {
      bcreg_free(fs, rc);
      ins = BCINS_ABC(BC_TGETV, 0, e->u.s.info, rc);
    }
    bcreg_free(fs, e->u.s.info);
  } else if (e->k == VCALL) {
    e->u.s.info = e->u.s.aux;
    e->k = VNONRELOC;
    return;
  } else if (e->k == VLOCAL) {
    e->k = VNONRELOC;
    return;
  } else {
    return;
  }
  e->u.s.info = bcemit_INS(fs, ins);
  e->k = VRELOCABLE;
}

/* Emit bytecode to set a range of registers to nil. */
static void bcemit_nil(FuncState *fs, BCReg from, BCReg n)
{
  if (fs->pc > fs->lasttarget) {  /* No jumps to current position? */
    BCIns *ip = &fs->bcbase[fs->pc-1].ins;
    BCReg pto, pfrom = bc_a(*ip);
    switch (bc_op(*ip)) {  /* Try to merge with the previous instruction. */
    case BC_KPRI:
      if (bc_d(*ip) != ~LJ_TNIL) break;
      if (from == pfrom) {
	if (n == 1) return;
      } else if (from == pfrom+1) {
	from = pfrom;
	n++;
      } else {
	break;
      }
      *ip = BCINS_AD(BC_KNIL, from, from+n-1);  /* Replace KPRI. */
      return;
    case BC_KNIL:
      pto = bc_d(*ip);
      if (pfrom <= from && from <= pto+1) {  /* Can we connect both ranges? */
	if (from+n-1 > pto)
	  setbc_d(ip, from+n-1);  /* Patch previous instruction range. */
	return;
      }
      break;
    default:
      break;
    }
  }
  /* Emit new instruction or replace old instruction. */
  bcemit_INS(fs, n == 1 ? BCINS_AD(BC_KPRI, from, VKNIL) :
			  BCINS_AD(BC_KNIL, from, from+n-1));
}

/* Discharge an expression to a specific register. Ignore branches. */
static void expr_toreg_nobranch(FuncState *fs, ExpDesc *e, BCReg reg)
{
  BCIns ins;
  expr_discharge(fs, e);
  if (e->k == VKSTR) {
    ins = BCINS_AD(BC_KSTR, reg, const_str(fs, e));
  } else if (e->k == VKNUM) {
#if LJ_DUALNUM
    cTValue *tv = expr_numtv(e);
    if (tvisint(tv) && checki16(intV(tv)))
      ins = BCINS_AD(BC_KSHORT, reg, (BCReg)(uint16_t)intV(tv));
    else
#else
    lua_Number n = expr_numberV(e);
    int32_t k = lj_num2int(n);
    if (checki16(k) && n == (lua_Number)k)
      ins = BCINS_AD(BC_KSHORT, reg, (BCReg)(uint16_t)k);
    else
#endif
      ins = BCINS_AD(BC_KNUM, reg, const_num(fs, e));
#if LJ_HASFFI
  } else if (e->k == VKCDATA) {
    fs->flags |= PROTO_FFI;
    ins = BCINS_AD(BC_KCDATA, reg,
		   const_gc(fs, obj2gco(cdataV(&e->u.nval)), LJ_TCDATA));
#endif
  } else if (e->k == VRELOCABLE) {
    setbc_a(bcptr(fs, e), reg);
    goto noins;
  } else if (e->k == VNONRELOC) {
    if (reg == e->u.s.info)
      goto noins;
    ins = BCINS_AD(BC_MOV, reg, e->u.s.info);
  } else if (e->k == VKNIL) {
    bcemit_nil(fs, reg, 1);
    goto noins;
  } else if (e->k <= VKTRUE) {
    ins = BCINS_AD(BC_KPRI, reg, const_pri(e));
  } else {
    lua_assert(e->k == VVOID || e->k == VJMP);
    return;
  }
  bcemit_INS(fs, ins);
noins:
  e->u.s.info = reg;
  e->k = VNONRELOC;
}

/* Forward declaration. */
static BCPos bcemit_jmp(FuncState *fs);

/* Discharge an expression to a specific register. */
static void expr_toreg(FuncState *fs, ExpDesc *e, BCReg reg)
{
  expr_toreg_nobranch(fs, e, reg);
  if (e->k == VJMP)
    jmp_append(fs, &e->t, e->u.s.info);  /* Add it to the true jump list. */
  if (expr_hasjump(e)) {  /* Discharge expression with branches. */
    BCPos jend, jfalse = NO_JMP, jtrue = NO_JMP;
    if (jmp_novalue(fs, e->t) || jmp_novalue(fs, e->f)) {
      BCPos jval = (e->k == VJMP) ? NO_JMP : bcemit_jmp(fs);
      jfalse = bcemit_AD(fs, BC_KPRI, reg, VKFALSE);
      bcemit_AJ(fs, BC_JMP, fs->freereg, 1);
      jtrue = bcemit_AD(fs, BC_KPRI, reg, VKTRUE);
      jmp_tohere(fs, jval);
    }
    jend = fs->pc;
    fs->lasttarget = jend;
    jmp_patchval(fs, e->f, jend, reg, jfalse);
    jmp_patchval(fs, e->t, jend, reg, jtrue);
  }
  e->f = e->t = NO_JMP;
  e->u.s.info = reg;
  e->k = VNONRELOC;
}

/* Discharge an expression to the next free register. */
static void expr_tonextreg(FuncState *fs, ExpDesc *e)
{
  expr_discharge(fs, e);
  expr_free(fs, e);
  bcreg_reserve(fs, 1);
  expr_toreg(fs, e, fs->freereg - 1);
}

/* Discharge an expression to any register. */
static BCReg expr_toanyreg(FuncState *fs, ExpDesc *e)
{
  expr_discharge(fs, e);
  if (e->k == VNONRELOC) {
    if (!expr_hasjump(e)) return e->u.s.info;  /* Already in a register. */
    if (e->u.s.info >= fs->nactvar) {
      expr_toreg(fs, e, e->u.s.info);  /* Discharge to temp. register. */
      return e->u.s.info;
    }
  }
  expr_tonextreg(fs, e);  /* Discharge to next register. */
  return e->u.s.info;
}

/* Partially discharge expression to a value. */
static void expr_toval(FuncState *fs, ExpDesc *e)
{
  if (expr_hasjump(e))
    expr_toanyreg(fs, e);
  else
    expr_discharge(fs, e);
}

/* Emit store for LHS expression. */
static void bcemit_store(FuncState *fs, ExpDesc *var, ExpDesc *e)
{
  BCIns ins;
  if (var->k == VLOCAL) {
    fs->ls->vstack[var->u.s.aux].info |= VSTACK_VAR_RW;
    expr_free(fs, e);
    expr_toreg(fs, e, var->u.s.info);
    return;
  } else if (var->k == VUPVAL) {
    fs->ls->vstack[var->u.s.aux].info |= VSTACK_VAR_RW;
    expr_toval(fs, e);
    if (e->k <= VKTRUE)
      ins = BCINS_AD(BC_USETP, var->u.s.info, const_pri(e));
    else if (e->k == VKSTR)
      ins = BCINS_AD(BC_USETS, var->u.s.info, const_str(fs, e));
    else if (e->k == VKNUM)
      ins = BCINS_AD(BC_USETN, var->u.s.info, const_num(fs, e));
    else
      ins = BCINS_AD(BC_USETV, var->u.s.info, expr_toanyreg(fs, e));
  } else if (var->k == VGLOBAL) {
    BCReg ra = expr_toanyreg(fs, e);
    ins = BCINS_AD(BC_GSET, ra, const_str(fs, var));
  } else {
    BCReg ra, rc;
    lua_assert(var->k == VINDEXED);
    ra = expr_toanyreg(fs, e);
    rc = var->u.s.aux;
    if ((int32_t)rc < 0) {
      ins = BCINS_ABC(BC_TSETS, ra, var->u.s.info, ~rc);
    } else if (rc > BCMAX_C) {
      ins = BCINS_ABC(BC_TSETB, ra, var->u.s.info, rc-(BCMAX_C+1));
    } else {
      /* Free late alloced key reg to avoid assert on free of value reg. */
      /* This can only happen when called from expr_table(). */
      lua_assert(e->k != VNONRELOC || ra < fs->nactvar ||
		 rc < ra || (bcreg_free(fs, rc),1));
      ins = BCINS_ABC(BC_TSETV, ra, var->u.s.info, rc);
    }
  }
  bcemit_INS(fs, ins);
  expr_free(fs, e);
}

/* Emit method lookup expression. */
static void bcemit_method(FuncState *fs, ExpDesc *e, ExpDesc *key)
{
  BCReg idx, func, obj = expr_toanyreg(fs, e);
  expr_free(fs, e);
  func = fs->freereg;
  bcemit_AD(fs, BC_MOV, func+1, obj);  /* Copy object to first argument. */
  lua_assert(expr_isstrk(key));
  idx = const_str(fs, key);
  if (idx <= BCMAX_C) {
    bcreg_reserve(fs, 2);
    bcemit_ABC(fs, BC_TGETS, func, obj, idx);
  } else {
    bcreg_reserve(fs, 3);
    bcemit_AD(fs, BC_KSTR, func+2, idx);
    bcemit_ABC(fs, BC_TGETV, func, obj, func+2);
    fs->freereg--;
  }
  e->u.s.info = func;
  e->k = VNONRELOC;
}

/* -- Bytecode emitter for branches --------------------------------------- */

/* Emit unconditional branch. */
static BCPos bcemit_jmp(FuncState *fs)
{
  BCPos jpc = fs->jpc;
  BCPos j = fs->pc - 1;
  BCIns *ip = &fs->bcbase[j].ins;
  fs->jpc = NO_JMP;
  if ((int32_t)j >= (int32_t)fs->lasttarget && bc_op(*ip) == BC_UCLO)
    setbc_j(ip, NO_JMP);
  else
    j = bcemit_AJ(fs, BC_JMP, fs->freereg, NO_JMP);
  jmp_append(fs, &j, jpc);
  return j;
}

/* Invert branch condition of bytecode instruction. */
static void invertcond(FuncState *fs, ExpDesc *e)
{
  BCIns *ip = &fs->bcbase[e->u.s.info - 1].ins;
  setbc_op(ip, bc_op(*ip)^1);
}

/* Emit conditional branch. */
static BCPos bcemit_branch(FuncState *fs, ExpDesc *e, int cond)
{
  BCPos pc;
  if (e->k == VRELOCABLE) {
    BCIns *ip = bcptr(fs, e);
    if (bc_op(*ip) == BC_NOT) {
      *ip = BCINS_AD(cond ? BC_ISF : BC_IST, 0, bc_d(*ip));
      return bcemit_jmp(fs);
    }
  }
  if (e->k != VNONRELOC) {
    bcreg_reserve(fs, 1);
    expr_toreg_nobranch(fs, e, fs->freereg-1);
  }
  bcemit_AD(fs, cond ? BC_ISTC : BC_ISFC, NO_REG, e->u.s.info);
  pc = bcemit_jmp(fs);
  expr_free(fs, e);
  return pc;
}

/* Emit branch on true condition. */
static void bcemit_branch_t(FuncState *fs, ExpDesc *e)
{
  BCPos pc;
  expr_discharge(fs, e);
  if (e->k == VKSTR || e->k == VKNUM || e->k == VKTRUE)
    pc = NO_JMP;  /* Never jump. */
  else if (e->k == VJMP)
    invertcond(fs, e), pc = e->u.s.info;
  else if (e->k == VKFALSE || e->k == VKNIL)
    expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
  else
    pc = bcemit_branch(fs, e, 0);
  jmp_append(fs, &e->f, pc);
  jmp_tohere(fs, e->t);
  e->t = NO_JMP;
}

/* Emit branch on false condition. */
static void bcemit_branch_f(FuncState *fs, ExpDesc *e)
{
  BCPos pc;
  expr_discharge(fs, e);
  if (e->k == VKNIL || e->k == VKFALSE)
    pc = NO_JMP;  /* Never jump. */
  else if (e->k == VJMP)
    pc = e->u.s.info;
  else if (e->k == VKSTR || e->k == VKNUM || e->k == VKTRUE)
    expr_toreg_nobranch(fs, e, NO_REG), pc = bcemit_jmp(fs);
  else
    pc = bcemit_branch(fs, e, 1);
  jmp_append(fs, &e->t, pc);
  jmp_tohere(fs, e->f);
  e->f = NO_JMP;
}

/* -- Bytecode emitter for operators -------------------------------------- */

/* Try constant-folding of arithmetic operators. */
static int foldarith(BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  TValue o;
  lua_Number n;
  if (!expr_isnumk_nojump(e1) || !expr_isnumk_nojump(e2)) return 0;
  n = lj_vm_foldarith(expr_numberV(e1), expr_numberV(e2), (int)opr-OPR_ADD);
  setnumV(&o, n);
  if (tvisnan(&o) || tvismzero(&o)) return 0;  /* Avoid NaN and -0 as consts. */
  if (LJ_DUALNUM) {
    int32_t k = lj_num2int(n);
    if ((lua_Number)k == n) {
      setintV(&e1->u.nval, k);
      return 1;
    }
  }
  setnumV(&e1->u.nval, n);
  return 1;
}

/* Emit arithmetic operator. */
static void bcemit_arith(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  BCReg rb, rc, t;
  uint32_t op;
  if (foldarith(opr, e1, e2))
    return;
  if (opr == OPR_POW) {
    op = BC_POW;
    rc = expr_toanyreg(fs, e2);
    rb = expr_toanyreg(fs, e1);
  } else {
    op = opr-OPR_ADD+BC_ADDVV;
    /* Must discharge 2nd operand first since VINDEXED might free regs. */
    expr_toval(fs, e2);
    if (expr_isnumk(e2) && (rc = const_num(fs, e2)) <= BCMAX_C)
      op -= BC_ADDVV-BC_ADDVN;
    else
      rc = expr_toanyreg(fs, e2);
    /* 1st operand discharged by bcemit_binop_left, but need KNUM/KSHORT. */
    lua_assert(expr_isnumk(e1) || e1->k == VNONRELOC);
    expr_toval(fs, e1);
    /* Avoid two consts to satisfy bytecode constraints. */
    if (expr_isnumk(e1) && !expr_isnumk(e2) &&
	(t = const_num(fs, e1)) <= BCMAX_B) {
      rb = rc; rc = t; op -= BC_ADDVV-BC_ADDNV;
    } else {
      rb = expr_toanyreg(fs, e1);
    }
  }
  /* Using expr_free might cause asserts if the order is wrong. */
  if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
  if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
  e1->u.s.info = bcemit_ABC(fs, op, 0, rb, rc);
  e1->k = VRELOCABLE;
}

/* Emit comparison operator. */
static void bcemit_comp(FuncState *fs, BinOpr opr, ExpDesc *e1, ExpDesc *e2)
{
  ExpDesc *eret = e1;
  BCIns ins;
  expr_toval(fs, e1);
  if (opr == OPR_EQ || opr == OPR_NE) {
    BCOp op = opr == OPR_EQ ? BC_ISEQV : BC_ISNEV;
    BCReg ra;
    if (expr_isk(e1)) { e1 = e2; e2 = eret; }  /* Need constant in 2nd arg. */
    ra = expr_toanyreg(fs, e1);  /* First arg must be in a reg. */
    expr_toval(fs, e2);
    switch (e2->k) {
    case VKNIL: case VKFALSE: case VKTRUE:
      ins = BCINS_AD(op+(BC_ISEQP-BC_ISEQV), ra, const_pri(e2));
      break;
    case VKSTR:
      ins = BCINS_AD(op+(BC_ISEQS-BC_ISEQV), ra, const_str(fs, e2));
      break;
    case VKNUM:
      ins = BCINS_AD(op+(BC_ISEQN-BC_ISEQV), ra, const_num(fs, e2));
      break;
    default:
      ins = BCINS_AD(op, ra, expr_toanyreg(fs, e2));
      break;
    }
  } else {
    uint32_t op = opr-OPR_LT+BC_ISLT;
    BCReg ra, rd;
    if ((op-BC_ISLT) & 1) {  /* GT -> LT, GE -> LE */
      e1 = e2; e2 = eret;  /* Swap operands. */
      op = ((op-BC_ISLT)^3)+BC_ISLT;
      expr_toval(fs, e1);
    }
    rd = expr_toanyreg(fs, e2);
    ra = expr_toanyreg(fs, e1);
    ins = BCINS_AD(op, ra, rd);
  }
  /* Using expr_free might cause asserts if the order is wrong. */
  if (e1->k == VNONRELOC && e1->u.s.info >= fs->nactvar) fs->freereg--;
  if (e2->k == VNONRELOC && e2->u.s.info >= fs->nactvar) fs->freereg--;
  bcemit_INS(fs, ins);
  eret->u.s.info = bcemit_jmp(fs);
  eret->k = VJMP;
}

/* Fixup left side of binary operator. */
static void bcemit_binop_left(FuncState *fs, BinOpr op, ExpDesc *e)
{
  if (op == OPR_AND) {
    bcemit_branch_t(fs, e);
  } else if (op == OPR_OR) {
    bcemit_branch_f(fs, e);
  } else if (op == OPR_CONCAT) {
    expr_tonextreg(fs, e);
  } else if (op == OPR_EQ || op == OPR_NE) {
    if (!expr_isk_nojump(e)) expr_toanyreg(fs, e);
  } else {
    if (!expr_isnumk_nojump(e)) expr_toanyreg(fs, e);
  }
}

/* Emit binary operator. */
static void bcemit_binop(FuncState *fs, BinOpr op, ExpDesc *e1, ExpDesc *e2)
{
  if (op <= OPR_POW) {
    bcemit_arith(fs, op, e1, e2);
  } else if (op == OPR_AND) {
    lua_assert(e1->t == NO_JMP);  /* List must be closed. */
    expr_discharge(fs, e2);
    jmp_append(fs, &e2->f, e1->f);
    *e1 = *e2;
  } else if (op == OPR_OR) {
    lua_assert(e1->f == NO_JMP);  /* List must be closed. */
    expr_discharge(fs, e2);
    jmp_append(fs, &e2->t, e1->t);
    *e1 = *e2;
  } else if (op == OPR_CONCAT) {
    expr_toval(fs, e2);
    if (e2->k == VRELOCABLE && bc_op(*bcptr(fs, e2)) == BC_CAT) {
      lua_assert(e1->u.s.info == bc_b(*bcptr(fs, e2))-1);
      expr_free(fs, e1);
      setbc_b(bcptr(fs, e2), e1->u.s.info);
      e1->u.s.info = e2->u.s.info;
    } else {
      expr_tonextreg(fs, e2);
      expr_free(fs, e2);
      expr_free(fs, e1);
      e1->u.s.info = bcemit_ABC(fs, BC_CAT, 0, e1->u.s.info, e2->u.s.info);
    }
    e1->k = VRELOCABLE;
  } else {
    lua_assert(op == OPR_NE || op == OPR_EQ ||
	       op == OPR_LT || op == OPR_GE || op == OPR_LE || op == OPR_GT);
    bcemit_comp(fs, op, e1, e2);
  }
}

/* Emit unary operator. */
static void bcemit_unop(FuncState *fs, BCOp op, ExpDesc *e)
{
  if (op == BC_NOT) {
    /* Swap true and false lists. */
    { BCPos temp = e->f; e->f = e->t; e->t = temp; }
    jmp_dropval(fs, e->f);
    jmp_dropval(fs, e->t);
    expr_discharge(fs, e);
    if (e->k == VKNIL || e->k == VKFALSE) {
      e->k = VKTRUE;
      return;
    } else if (expr_isk(e) || (LJ_HASFFI && e->k == VKCDATA)) {
      e->k = VKFALSE;
      return;
    } else if (e->k == VJMP) {
      invertcond(fs, e);
      return;
    } else if (e->k == VRELOCABLE) {
      bcreg_reserve(fs, 1);
      setbc_a(bcptr(fs, e), fs->freereg-1);
      e->u.s.info = fs->freereg-1;
      e->k = VNONRELOC;
    } else {
      lua_assert(e->k == VNONRELOC);
    }
  } else {
    lua_assert(op == BC_UNM || op == BC_LEN);
    if (op == BC_UNM && !expr_hasjump(e)) {  /* Constant-fold negations. */
#if LJ_HASFFI
      if (e->k == VKCDATA) {  /* Fold in-place since cdata is not interned. */
	GCcdata *cd = cdataV(&e->u.nval);
	int64_t *p = (int64_t *)cdataptr(cd);
	if (cd->ctypeid == CTID_COMPLEX_DOUBLE)
	  p[1] ^= (int64_t)U64x(80000000,00000000);
	else
	  *p = -*p;
	return;
      } else
#endif
      if (expr_isnumk(e) && !expr_numiszero(e)) {  /* Avoid folding to -0. */
	TValue *o = expr_numtv(e);
	if (tvisint(o)) {
	  int32_t k = intV(o);
	  if (k == -k)
	    setnumV(o, -(lua_Number)k);
	  else
	    setintV(o, -k);
	  return;
	} else {
	  o->u64 ^= U64x(80000000,00000000);
	  return;
	}
      }
    }
    expr_toanyreg(fs, e);
  }
  expr_free(fs, e);
  e->u.s.info = bcemit_AD(fs, op, 0, e->u.s.info);
  e->k = VRELOCABLE;
}

/* -- Lexer support ------------------------------------------------------- */

/* Check and consume optional token. */
static int lex_opt(LexState *ls, LexToken tok)
{
  if (ls->token == tok) {
    lj_lex_next(ls);
    return 1;
  }
  return 0;
}

/* Check and consume token. */
static void lex_check(LexState *ls, LexToken tok)
{
  if (ls->token != tok)
    err_token(ls, tok);
  lj_lex_next(ls);
}

/* Check for matching token. */
static void lex_match(LexState *ls, LexToken what, LexToken who, BCLine line)
{
  if (!lex_opt(ls, what)) {
    if (line == ls->linenumber) {
      err_token(ls, what);
    } else {
      const char *swhat = lj_lex_token2str(ls, what);
      const char *swho = lj_lex_token2str(ls, who);
      lj_lex_error(ls, ls->token, LJ_ERR_XMATCH, swhat, swho, line);
    }
  }
}

/* Check for string token. */
static GCstr *lex_str(LexState *ls)
{
  GCstr *s;
  if (ls->token != TK_name && (LJ_52 || ls->token != TK_goto))
    err_token(ls, TK_name);
  s = strV(&ls->tokenval);
  lj_lex_next(ls);
  return s;
}

/* -- Variable handling --------------------------------------------------- */

#define var_get(ls, fs, i)	((ls)->vstack[(fs)->varmap[(i)]])

/* Define a new local variable. */
static void var_new(LexState *ls, BCReg n, GCstr *name)
{
  FuncState *fs = ls->fs;
  MSize vtop = ls->vtop;
  checklimit(fs, fs->nactvar+n, LJ_MAX_LOCVAR, "local variables");
  if (LJ_UNLIKELY(vtop >= ls->sizevstack)) {
    if (ls->sizevstack >= LJ_MAX_VSTACK)
      lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
    lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
  }
  lua_assert((uintptr_t)name < VARNAME__MAX ||
	     lj_tab_getstr(fs->kt, name) != NULL);
  /* NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj. */
  setgcref(ls->vstack[vtop].name, obj2gco(name));
  fs->varmap[fs->nactvar+n] = (uint16_t)vtop;
  ls->vtop = vtop+1;
}

#define var_new_lit(ls, n, v) \
  var_new(ls, (n), lj_parse_keepstr(ls, "" v, sizeof(v)-1))

#define var_new_fixed(ls, n, vn) \
  var_new(ls, (n), (GCstr *)(uintptr_t)(vn))

/* Add local variables. */
static void var_add(LexState *ls, BCReg nvars)
{
  FuncState *fs = ls->fs;
  BCReg nactvar = fs->nactvar;
  while (nvars--) {
    VarInfo *v = &var_get(ls, fs, nactvar);
    v->startpc = fs->pc;
    v->slot = nactvar++;
    v->info = 0;
  }
  fs->nactvar = nactvar;
}

/* Remove local variables. */
static void var_remove(LexState *ls, BCReg tolevel)
{
  FuncState *fs = ls->fs;
  while (fs->nactvar > tolevel)
    var_get(ls, fs, --fs->nactvar).endpc = fs->pc;
}

/* Lookup local variable name. */
static BCReg var_lookup_local(FuncState *fs, GCstr *n)
{
  int i;
  for (i = fs->nactvar-1; i >= 0; i--) {
    if (n == strref(var_get(fs->ls, fs, i).name))
      return (BCReg)i;
  }
  return (BCReg)-1;  /* Not found. */
}

/* Lookup or add upvalue index. */
static MSize var_lookup_uv(FuncState *fs, MSize vidx, ExpDesc *e)
{
  MSize i, n = fs->nuv;
  for (i = 0; i < n; i++)
    if (fs->uvmap[i] == vidx)
      return i;  /* Already exists. */
  /* Otherwise create a new one. */
  checklimit(fs, fs->nuv, LJ_MAX_UPVAL, "upvalues");
  lua_assert(e->k == VLOCAL || e->k == VUPVAL);
  fs->uvmap[n] = (uint16_t)vidx;
  fs->uvtmp[n] = (uint16_t)(e->k == VLOCAL ? vidx : LJ_MAX_VSTACK+e->u.s.info);
  fs->nuv = n+1;
  return n;
}

/* Forward declaration. */
static void fscope_uvmark(FuncState *fs, BCReg level);

/* Recursively lookup variables in enclosing functions. */
static MSize var_lookup_(FuncState *fs, GCstr *name, ExpDesc *e, int first)
{
  if (fs) {
    BCReg reg = var_lookup_local(fs, name);
    if ((int32_t)reg >= 0) {  /* Local in this function? */
      expr_init(e, VLOCAL, reg);
      if (!first)
	fscope_uvmark(fs, reg);  /* Scope now has an upvalue. */
      return (MSize)(e->u.s.aux = (uint32_t)fs->varmap[reg]);
    } else {
      MSize vidx = var_lookup_(fs->prev, name, e, 0);  /* Var in outer func? */
      if ((int32_t)vidx >= 0) {  /* Yes, make it an upvalue here. */
	e->u.s.info = (uint8_t)var_lookup_uv(fs, vidx, e);
	e->k = VUPVAL;
	return vidx;
      }
    }
  } else {  /* Not found in any function, must be a global. */
    expr_init(e, VGLOBAL, 0);
    e->u.sval = name;
  }
  return (MSize)-1;  /* Global. */
}

/* Lookup variable name. */
#define var_lookup(ls, e) \
  var_lookup_((ls)->fs, lex_str(ls), (e), 1)

/* -- Goto an label handling ---------------------------------------------- */

/* Add a new goto or label. */
static MSize gola_new(LexState *ls, GCstr *name, uint8_t info, BCPos pc)
{
  FuncState *fs = ls->fs;
  MSize vtop = ls->vtop;
  if (LJ_UNLIKELY(vtop >= ls->sizevstack)) {
    if (ls->sizevstack >= LJ_MAX_VSTACK)
      lj_lex_error(ls, 0, LJ_ERR_XLIMC, LJ_MAX_VSTACK);
    lj_mem_growvec(ls->L, ls->vstack, ls->sizevstack, LJ_MAX_VSTACK, VarInfo);
  }
  lua_assert(name == NAME_BREAK || lj_tab_getstr(fs->kt, name) != NULL);
  /* NOBARRIER: name is anchored in fs->kt and ls->vstack is not a GCobj. */
  setgcref(ls->vstack[vtop].name, obj2gco(name));
  ls->vstack[vtop].startpc = pc;
  ls->vstack[vtop].slot = (uint8_t)fs->nactvar;
  ls->vstack[vtop].info = info;
  ls->vtop = vtop+1;
  return vtop;
}

#define gola_isgoto(v)		((v)->info & VSTACK_GOTO)
#define gola_islabel(v)		((v)->info & VSTACK_LABEL)
#define gola_isgotolabel(v)	((v)->info & (VSTACK_GOTO|VSTACK_LABEL))

/* Patch goto to jump to label. */
static void gola_patch(LexState *ls, VarInfo *vg, VarInfo *vl)
{
  FuncState *fs = ls->fs;
  BCPos pc = vg->startpc;
  setgcrefnull(vg->name);  /* Invalidate pending goto. */
  setbc_a(&fs->bcbase[pc].ins, vl->slot);
  jmp_patch(fs, pc, vl->startpc);
}

/* Patch goto to close upvalues. */
static void gola_close(LexState *ls, VarInfo *vg)
{
  FuncState *fs = ls->fs;
  BCPos pc = vg->startpc;
  BCIns *ip = &fs->bcbase[pc].ins;
  lua_assert(gola_isgoto(vg));
  lua_assert(bc_op(*ip) == BC_JMP || bc_op(*ip) == BC_UCLO);
  setbc_a(ip, vg->slot);
  if (bc_op(*ip) == BC_JMP) {
    BCPos next = jmp_next(fs, pc);
    if (next != NO_JMP) jmp_patch(fs, next, pc);  /* Jump to UCLO. */
    setbc_op(ip, BC_UCLO);  /* Turn into UCLO. */
    setbc_j(ip, NO_JMP);
  }
}

/* Resolve pending forward gotos for label. */
static void gola_resolve(LexState *ls, FuncScope *bl, MSize idx)
{
  VarInfo *vg = ls->vstack + bl->vstart;
  VarInfo *vl = ls->vstack + idx;
  for (; vg < vl; vg++)
    if (gcrefeq(vg->name, vl->name) && gola_isgoto(vg)) {
      if (vg->slot < vl->slot) {
	GCstr *name = strref(var_get(ls, ls->fs, vg->slot).name);
	lua_assert((uintptr_t)name >= VARNAME__MAX);
	ls->linenumber = ls->fs->bcbase[vg->startpc].line;
	lua_assert(strref(vg->name) != NAME_BREAK);
	lj_lex_error(ls, 0, LJ_ERR_XGSCOPE,
		     strdata(strref(vg->name)), strdata(name));
      }
      gola_patch(ls, vg, vl);
    }
}

/* Fixup remaining gotos and labels for scope. */
static void gola_fixup(LexState *ls, FuncScope *bl)
{
  VarInfo *v = ls->vstack + bl->vstart;
  VarInfo *ve = ls->vstack + ls->vtop;
  for (; v < ve; v++) {
    GCstr *name = strref(v->name);
    if (name != NULL) {  /* Only consider remaining valid gotos/labels. */
      if (gola_islabel(v)) {
	VarInfo *vg;
	setgcrefnull(v->name);  /* Invalidate label that goes out of scope. */
	for (vg = v+1; vg < ve; vg++)  /* Resolve pending backward gotos. */
	  if (strref(vg->name) == name && gola_isgoto(vg)) {
	    if ((bl->flags&FSCOPE_UPVAL) && vg->slot > v->slot)
	      gola_close(ls, vg);
	    gola_patch(ls, vg, v);
	  }
      } else if (gola_isgoto(v)) {
	if (bl->prev) {  /* Propagate goto or break to outer scope. */
	  bl->prev->flags |= name == NAME_BREAK ? FSCOPE_BREAK : FSCOPE_GOLA;
	  v->slot = bl->nactvar;
	  if ((bl->flags & FSCOPE_UPVAL))
	    gola_close(ls, v);
	} else {  /* No outer scope: undefined goto label or no loop. */
	  ls->linenumber = ls->fs->bcbase[v->startpc].line;
	  if (name == NAME_BREAK)
	    lj_lex_error(ls, 0, LJ_ERR_XBREAK);
	  else
	    lj_lex_error(ls, 0, LJ_ERR_XLUNDEF, strdata(name));
	}
      }
    }
  }
}

/* Find existing label. */
static VarInfo *gola_findlabel(LexState *ls, GCstr *name)
{
  VarInfo *v = ls->vstack + ls->fs->bl->vstart;
  VarInfo *ve = ls->vstack + ls->vtop;
  for (; v < ve; v++)
    if (strref(v->name) == name && gola_islabel(v))
      return v;
  return NULL;
}

/* -- Scope handling ------------------------------------------------------ */

/* Begin a scope. */
static void fscope_begin(FuncState *fs, FuncScope *bl, int flags)
{
  bl->nactvar = (uint8_t)fs->nactvar;
  bl->flags = flags;
  bl->vstart = fs->ls->vtop;
  bl->prev = fs->bl;
  fs->bl = bl;
  lua_assert(fs->freereg == fs->nactvar);
}

/* End a scope. */
static void fscope_end(FuncState *fs)
{
  FuncScope *bl = fs->bl;
  LexState *ls = fs->ls;
  fs->bl = bl->prev;
  var_remove(ls, bl->nactvar);
  fs->freereg = fs->nactvar;
  lua_assert(bl->nactvar == fs->nactvar);
  if ((bl->flags & (FSCOPE_UPVAL|FSCOPE_NOCLOSE)) == FSCOPE_UPVAL)
    bcemit_AJ(fs, BC_UCLO, bl->nactvar, 0);
  if ((bl->flags & FSCOPE_BREAK)) {
    if ((bl->flags & FSCOPE_LOOP)) {
      MSize idx = gola_new(ls, NAME_BREAK, VSTACK_LABEL, fs->pc);
      ls->vtop = idx;  /* Drop break label immediately. */
      gola_resolve(ls, bl, idx);
      return;
    }  /* else: need the fixup step to propagate the breaks. */
  } else if (!(bl->flags & FSCOPE_GOLA)) {
    return;
  }
  gola_fixup(ls, bl);
}

/* Mark scope as having an upvalue. */
static void fscope_uvmark(FuncState *fs, BCReg level)
{
  FuncScope *bl;
  for (bl = fs->bl; bl && bl->nactvar > level; bl = bl->prev)
    ;
  if (bl)
    bl->flags |= FSCOPE_UPVAL;
}

/* -- Function state management ------------------------------------------- */

/* Fixup bytecode for prototype. */
static void fs_fixup_bc(FuncState *fs, GCproto *pt, BCIns *bc, MSize n)
{
  BCInsLine *base = fs->bcbase;
  MSize i;
  pt->sizebc = n;
  bc[0] = BCINS_AD((fs->flags & PROTO_VARARG) ? BC_FUNCV : BC_FUNCF,
		   fs->framesize, 0);
  for (i = 1; i < n; i++)
    bc[i] = base[i].ins;
}

/* Fixup upvalues for child prototype, step #2. */
static void fs_fixup_uv2(FuncState *fs, GCproto *pt)
{
  VarInfo *vstack = fs->ls->vstack;
  uint16_t *uv = proto_uv(pt);
  MSize i, n = pt->sizeuv;
  for (i = 0; i < n; i++) {
    VarIndex vidx = uv[i];
    if (vidx >= LJ_MAX_VSTACK)
      uv[i] = vidx - LJ_MAX_VSTACK;
    else if ((vstack[vidx].info & VSTACK_VAR_RW))
      uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL;
    else
      uv[i] = vstack[vidx].slot | PROTO_UV_LOCAL | PROTO_UV_IMMUTABLE;
  }
}

/* Fixup constants for prototype. */
static void fs_fixup_k(FuncState *fs, GCproto *pt, void *kptr)
{
  GCtab *kt;
  TValue *array;
  Node *node;
  MSize i, hmask;
  checklimitgt(fs, fs->nkn, BCMAX_D+1, "constants");
  checklimitgt(fs, fs->nkgc, BCMAX_D+1, "constants");
  setmref(pt->k, kptr);
  pt->sizekn = fs->nkn;
  pt->sizekgc = fs->nkgc;
  kt = fs->kt;
  array = tvref(kt->array);
  for (i = 0; i < kt->asize; i++)
    if (tvhaskslot(&array[i])) {
      TValue *tv = &((TValue *)kptr)[tvkslot(&array[i])];
      if (LJ_DUALNUM)
	setintV(tv, (int32_t)i);
      else
	setnumV(tv, (lua_Number)i);
    }
  node = noderef(kt->node);
  hmask = kt->hmask;
  for (i = 0; i <= hmask; i++) {
    Node *n = &node[i];
    if (tvhaskslot(&n->val)) {
      ptrdiff_t kidx = (ptrdiff_t)tvkslot(&n->val);
      lua_assert(!tvisint(&n->key));
      if (tvisnum(&n->key)) {
	TValue *tv = &((TValue *)kptr)[kidx];
	if (LJ_DUALNUM) {
	  lua_Number nn = numV(&n->key);
	  int32_t k = lj_num2int(nn);
	  lua_assert(!tvismzero(&n->key));
	  if ((lua_Number)k == nn)
	    setintV(tv, k);
	  else
	    *tv = n->key;
	} else {
	  *tv = n->key;
	}
      } else {
	GCobj *o = gcV(&n->key);
	setgcref(((GCRef *)kptr)[~kidx], o);
	lj_gc_objbarrier(fs->L, pt, o);
	if (tvisproto(&n->key))
	  fs_fixup_uv2(fs, gco2pt(o));
      }
    }
  }
}

/* Fixup upvalues for prototype, step #1. */
static void fs_fixup_uv1(FuncState *fs, GCproto *pt, uint16_t *uv)
{
  setmref(pt->uv, uv);
  pt->sizeuv = fs->nuv;
  memcpy(uv, fs->uvtmp, fs->nuv*sizeof(VarIndex));
}

#ifndef LUAJIT_DISABLE_DEBUGINFO
/* Prepare lineinfo for prototype. */
static size_t fs_prep_line(FuncState *fs, BCLine numline)
{
  return (fs->pc-1) << (numline < 256 ? 0 : numline < 65536 ? 1 : 2);
}

/* Fixup lineinfo for prototype. */
static void fs_fixup_line(FuncState *fs, GCproto *pt,
			  void *lineinfo, BCLine numline)
{
  BCInsLine *base = fs->bcbase + 1;
  BCLine first = fs->linedefined;
  MSize i = 0, n = fs->pc-1;
  pt->firstline = fs->linedefined;
  pt->numline = numline;
  setmref(pt->lineinfo, lineinfo);
  if (LJ_LIKELY(numline < 256)) {
    uint8_t *li = (uint8_t *)lineinfo;
    do {
      BCLine delta = base[i].line - first;
      lua_assert(delta >= 0 && delta < 256);
      li[i] = (uint8_t)delta;
    } while (++i < n);
  } else if (LJ_LIKELY(numline < 65536)) {
    uint16_t *li = (uint16_t *)lineinfo;
    do {
      BCLine delta = base[i].line - first;
      lua_assert(delta >= 0 && delta < 65536);
      li[i] = (uint16_t)delta;
    } while (++i < n);
  } else {
    uint32_t *li = (uint32_t *)lineinfo;
    do {
      BCLine delta = base[i].line - first;
      lua_assert(delta >= 0);
      li[i] = (uint32_t)delta;
    } while (++i < n);
  }
}

/* Resize buffer if needed. */
static LJ_NOINLINE void fs_buf_resize(LexState *ls, MSize len)
{
  MSize sz = ls->sb.sz * 2;
  while (ls->sb.n + len > sz) sz = sz * 2;
  lj_str_resizebuf(ls->L, &ls->sb, sz);
}

static LJ_AINLINE void fs_buf_need(LexState *ls, MSize len)
{
  if (LJ_UNLIKELY(ls->sb.n + len > ls->sb.sz))
    fs_buf_resize(ls, len);
}

/* Add string to buffer. */
static void fs_buf_str(LexState *ls, const char *str, MSize len)
{
  char *p = ls->sb.buf + ls->sb.n;
  MSize i;
  ls->sb.n += len;
  for (i = 0; i < len; i++) p[i] = str[i];
}

/* Add ULEB128 value to buffer. */
static void fs_buf_uleb128(LexState *ls, uint32_t v)
{
  MSize n = ls->sb.n;
  uint8_t *p = (uint8_t *)ls->sb.buf;
  for (; v >= 0x80; v >>= 7)
    p[n++] = (uint8_t)((v & 0x7f) | 0x80);
  p[n++] = (uint8_t)v;
  ls->sb.n = n;
}

/* Prepare variable info for prototype. */
static size_t fs_prep_var(LexState *ls, FuncState *fs, size_t *ofsvar)
{
  VarInfo *vs =ls->vstack, *ve;
  MSize i, n;
  BCPos lastpc;
  lj_str_resetbuf(&ls->sb);  /* Copy to temp. string buffer. */
  /* Store upvalue names. */
  for (i = 0, n = fs->nuv; i < n; i++) {
    GCstr *s = strref(vs[fs->uvmap[i]].name);
    MSize len = s->len+1;
    fs_buf_need(ls, len);
    fs_buf_str(ls, strdata(s), len);
  }
  *ofsvar = ls->sb.n;
  lastpc = 0;
  /* Store local variable names and compressed ranges. */
  for (ve = vs + ls->vtop, vs += fs->vbase; vs < ve; vs++) {
    if (!gola_isgotolabel(vs)) {
      GCstr *s = strref(vs->name);
      BCPos startpc;
      if ((uintptr_t)s < VARNAME__MAX) {
	fs_buf_need(ls, 1 + 2*5);
	ls->sb.buf[ls->sb.n++] = (uint8_t)(uintptr_t)s;
      } else {
	MSize len = s->len+1;
	fs_buf_need(ls, len + 2*5);
	fs_buf_str(ls, strdata(s), len);
      }
      startpc = vs->startpc;
      fs_buf_uleb128(ls, startpc-lastpc);
      fs_buf_uleb128(ls, vs->endpc-startpc);
      lastpc = startpc;
    }
  }
  fs_buf_need(ls, 1);
  ls->sb.buf[ls->sb.n++] = '\0';  /* Terminator for varinfo. */
  return ls->sb.n;
}

/* Fixup variable info for prototype. */
static void fs_fixup_var(LexState *ls, GCproto *pt, uint8_t *p, size_t ofsvar)
{
  setmref(pt->uvinfo, p);
  setmref(pt->varinfo, (char *)p + ofsvar);
  memcpy(p, ls->sb.buf, ls->sb.n);  /* Copy from temp. string buffer. */
}
#else

/* Initialize with empty debug info, if disabled. */
#define fs_prep_line(fs, numline)		(UNUSED(numline), 0)
#define fs_fixup_line(fs, pt, li, numline) \
  pt->firstline = pt->numline = 0, setmref((pt)->lineinfo, NULL)
#define fs_prep_var(ls, fs, ofsvar)		(UNUSED(ofsvar), 0)
#define fs_fixup_var(ls, pt, p, ofsvar) \
  setmref((pt)->uvinfo, NULL), setmref((pt)->varinfo, NULL)

#endif

/* Check if bytecode op returns. */
static int bcopisret(BCOp op)
{
  switch (op) {
  case BC_CALLMT: case BC_CALLT:
  case BC_RETM: case BC_RET: case BC_RET0: case BC_RET1:
    return 1;
  default:
    return 0;
  }
}

/* Fixup return instruction for prototype. */
static void fs_fixup_ret(FuncState *fs)
{
  BCPos lastpc = fs->pc;
  if (lastpc <= fs->lasttarget || !bcopisret(bc_op(fs->bcbase[lastpc-1].ins))) {
    if ((fs->bl->flags & FSCOPE_UPVAL))
      bcemit_AJ(fs, BC_UCLO, 0, 0);
    bcemit_AD(fs, BC_RET0, 0, 1);  /* Need final return. */
  }
  fs->bl->flags |= FSCOPE_NOCLOSE;  /* Handled above. */
  fscope_end(fs);
  lua_assert(fs->bl == NULL);
  /* May need to fixup returns encoded before first function was created. */
  if (fs->flags & PROTO_FIXUP_RETURN) {
    BCPos pc;
    for (pc = 1; pc < lastpc; pc++) {
      BCIns ins = fs->bcbase[pc].ins;
      BCPos offset;
      switch (bc_op(ins)) {
      case BC_CALLMT: case BC_CALLT:
      case BC_RETM: case BC_RET: case BC_RET0: case BC_RET1:
	offset = bcemit_INS(fs, ins)-(pc+1)+BCBIAS_J;  /* Copy return ins. */
	if (offset > BCMAX_D)
	  err_syntax(fs->ls, LJ_ERR_XFIXUP);
	/* Replace with UCLO plus branch. */
	fs->bcbase[pc].ins = BCINS_AD(BC_UCLO, 0, offset);
	break;
      case BC_UCLO:
	return;  /* We're done. */
      default:
	break;
      }
    }
  }
}

/* Finish a FuncState and return the new prototype. */
static GCproto *fs_finish(LexState *ls, BCLine line)
{
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  BCLine numline = line - fs->linedefined;
  size_t sizept, ofsk, ofsuv, ofsli, ofsdbg, ofsvar;
  GCproto *pt;

  /* Apply final fixups. */
  fs_fixup_ret(fs);

  /* Calculate total size of prototype including all colocated arrays. */
  sizept = sizeof(GCproto) + fs->pc*sizeof(BCIns) + fs->nkgc*sizeof(GCRef);
  sizept = (sizept + sizeof(TValue)-1) & ~(sizeof(TValue)-1);
  ofsk = sizept; sizept += fs->nkn*sizeof(TValue);
  ofsuv = sizept; sizept += ((fs->nuv+1)&~1)*2;
  ofsli = sizept; sizept += fs_prep_line(fs, numline);
  ofsdbg = sizept; sizept += fs_prep_var(ls, fs, &ofsvar);

  /* Allocate prototype and initialize its fields. */
  pt = (GCproto *)lj_mem_newgco(L, (MSize)sizept);
  pt->gct = ~LJ_TPROTO;
  pt->sizept = (MSize)sizept;
  pt->trace = 0;
  pt->flags = (uint8_t)(fs->flags & ~(PROTO_HAS_RETURN|PROTO_FIXUP_RETURN));
  pt->numparams = fs->numparams;
  pt->framesize = fs->framesize;
  setgcref(pt->chunkname, obj2gco(ls->chunkname));

  /* Close potentially uninitialized gap between bc and kgc. */
  *(uint32_t *)((char *)pt + ofsk - sizeof(GCRef)*(fs->nkgc+1)) = 0;
  fs_fixup_bc(fs, pt, (BCIns *)((char *)pt + sizeof(GCproto)), fs->pc);
  fs_fixup_k(fs, pt, (void *)((char *)pt + ofsk));
  fs_fixup_uv1(fs, pt, (uint16_t *)((char *)pt + ofsuv));
  fs_fixup_line(fs, pt, (void *)((char *)pt + ofsli), numline);
  fs_fixup_var(ls, pt, (uint8_t *)((char *)pt + ofsdbg), ofsvar);

  lj_vmevent_send(L, BC,
    setprotoV(L, L->top++, pt);
  );

  L->top--;  /* Pop table of constants. */
  ls->vtop = fs->vbase;  /* Reset variable stack. */
  ls->fs = fs->prev;
  lua_assert(ls->fs != NULL || ls->token == TK_eof);
  return pt;
}

/* Initialize a new FuncState. */
static void fs_init(LexState *ls, FuncState *fs)
{
  lua_State *L = ls->L;
  fs->prev = ls->fs; ls->fs = fs;  /* Append to list. */
  fs->ls = ls;
  fs->vbase = ls->vtop;
  fs->L = L;
  fs->pc = 0;
  fs->lasttarget = 0;
  fs->jpc = NO_JMP;
  fs->freereg = 0;
  fs->nkgc = 0;
  fs->nkn = 0;
  fs->nactvar = 0;
  fs->nuv = 0;
  fs->bl = NULL;
  fs->flags = 0;
  fs->framesize = 1;  /* Minimum frame size. */
  fs->kt = lj_tab_new(L, 0, 0);
  /* Anchor table of constants in stack to avoid being collected. */
  settabV(L, L->top, fs->kt);
  incr_top(L);
}

/* -- Expressions --------------------------------------------------------- */

/* Forward declaration. */
static void expr(LexState *ls, ExpDesc *v);

/* Return string expression. */
static void expr_str(LexState *ls, ExpDesc *e)
{
  expr_init(e, VKSTR, 0);
  e->u.sval = lex_str(ls);
}

/* Return index expression. */
static void expr_index(FuncState *fs, ExpDesc *t, ExpDesc *e)
{
  /* Already called: expr_toval(fs, e). */
  t->k = VINDEXED;
  if (expr_isnumk(e)) {
#if LJ_DUALNUM
    if (tvisint(expr_numtv(e))) {
      int32_t k = intV(expr_numtv(e));
      if (checku8(k)) {
	t->u.s.aux = BCMAX_C+1+(uint32_t)k;  /* 256..511: const byte key */
	return;
      }
    }
#else
    lua_Number n = expr_numberV(e);
    int32_t k = lj_num2int(n);
    if (checku8(k) && n == (lua_Number)k) {
      t->u.s.aux = BCMAX_C+1+(uint32_t)k;  /* 256..511: const byte key */
      return;
    }
#endif
  } else if (expr_isstrk(e)) {
    BCReg idx = const_str(fs, e);
    if (idx <= BCMAX_C) {
      t->u.s.aux = ~idx;  /* -256..-1: const string key */
      return;
    }
  }
  t->u.s.aux = expr_toanyreg(fs, e);  /* 0..255: register */
}

/* Parse index expression with named field. */
static void expr_field(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  ExpDesc key;
  expr_toanyreg(fs, v);
  lj_lex_next(ls);  /* Skip dot or colon. */
  expr_str(ls, &key);
  expr_index(fs, v, &key);
}

/* Parse index expression with brackets. */
static void expr_bracket(LexState *ls, ExpDesc *v)
{
  lj_lex_next(ls);  /* Skip '['. */
  expr(ls, v);
  expr_toval(ls->fs, v);
  lex_check(ls, ']');
}

/* Get value of constant expression. */
static void expr_kvalue(TValue *v, ExpDesc *e)
{
  if (e->k <= VKTRUE) {
    setitype(v, ~(uint32_t)e->k);
  } else if (e->k == VKSTR) {
    setgcref(v->gcr, obj2gco(e->u.sval));
    setitype(v, LJ_TSTR);
  } else {
    lua_assert(tvisnumber(expr_numtv(e)));
    *v = *expr_numtv(e);
  }
}

/* Parse table constructor expression. */
static void expr_table(LexState *ls, ExpDesc *e)
{
  FuncState *fs = ls->fs;
  BCLine line = ls->linenumber;
  GCtab *t = NULL;
  int vcall = 0, needarr = 0, fixt = 0;
  uint32_t narr = 1;  /* First array index. */
  uint32_t nhash = 0;  /* Number of hash entries. */
  BCReg freg = fs->freereg;
  BCPos pc = bcemit_AD(fs, BC_TNEW, freg, 0);
  expr_init(e, VNONRELOC, freg);
  bcreg_reserve(fs, 1);
  freg++;
  lex_check(ls, '{');
  while (ls->token != '}') {
    ExpDesc key, val;
    vcall = 0;
    if (ls->token == '[') {
      expr_bracket(ls, &key);  /* Already calls expr_toval. */
      if (!expr_isk(&key)) expr_index(fs, e, &key);
      if (expr_isnumk(&key) && expr_numiszero(&key)) needarr = 1; else nhash++;
      lex_check(ls, '=');
    } else if ((ls->token == TK_name || (!LJ_52 && ls->token == TK_goto)) &&
	       lj_lex_lookahead(ls) == '=') {
      expr_str(ls, &key);
      lex_check(ls, '=');
      nhash++;
    } else {
      expr_init(&key, VKNUM, 0);
      setintV(&key.u.nval, (int)narr);
      narr++;
      needarr = vcall = 1;
    }
    expr(ls, &val);
    if (expr_isk(&key) && key.k != VKNIL &&
	(key.k == VKSTR || expr_isk_nojump(&val))) {
      TValue k, *v;
      if (!t) {  /* Create template table on demand. */
	BCReg kidx;
	t = lj_tab_new(fs->L, needarr ? narr : 0, hsize2hbits(nhash));
	kidx = const_gc(fs, obj2gco(t), LJ_TTAB);
	fs->bcbase[pc].ins = BCINS_AD(BC_TDUP, freg-1, kidx);
      }
      vcall = 0;
      expr_kvalue(&k, &key);
      v = lj_tab_set(fs->L, t, &k);
      lj_gc_anybarriert(fs->L, t);
      if (expr_isk_nojump(&val)) {  /* Add const key/value to template table. */
	expr_kvalue(v, &val);
      } else {  /* Otherwise create dummy string key (avoids lj_tab_newkey). */
	settabV(fs->L, v, t);  /* Preserve key with table itself as value. */
	fixt = 1;   /* Fix this later, after all resizes. */
	goto nonconst;
      }
    } else {
    nonconst:
      if (val.k != VCALL) { expr_toanyreg(fs, &val); vcall = 0; }
      if (expr_isk(&key)) expr_index(fs, e, &key);
      bcemit_store(fs, e, &val);
    }
    fs->freereg = freg;
    if (!lex_opt(ls, ',') && !lex_opt(ls, ';')) break;
  }
  lex_match(ls, '}', '{', line);
  if (vcall) {
    BCInsLine *ilp = &fs->bcbase[fs->pc-1];
    ExpDesc en;
    lua_assert(bc_a(ilp->ins) == freg &&
	       bc_op(ilp->ins) == (narr > 256 ? BC_TSETV : BC_TSETB));
    expr_init(&en, VKNUM, 0);
    en.u.nval.u32.lo = narr-1;
    en.u.nval.u32.hi = 0x43300000;  /* Biased integer to avoid denormals. */
    if (narr > 256) { fs->pc--; ilp--; }
    ilp->ins = BCINS_AD(BC_TSETM, freg, const_num(fs, &en));
    setbc_b(&ilp[-1].ins, 0);
  }
  if (pc == fs->pc-1) {  /* Make expr relocable if possible. */
    e->u.s.info = pc;
    fs->freereg--;
    e->k = VRELOCABLE;
  } else {
    e->k = VNONRELOC;  /* May have been changed by expr_index. */
  }
  if (!t) {  /* Construct TNEW RD: hhhhhaaaaaaaaaaa. */
    BCIns *ip = &fs->bcbase[pc].ins;
    if (!needarr) narr = 0;
    else if (narr < 3) narr = 3;
    else if (narr > 0x7ff) narr = 0x7ff;
    setbc_d(ip, narr|(hsize2hbits(nhash)<<11));
  } else {
    if (needarr && t->asize < narr)
      lj_tab_reasize(fs->L, t, narr-1);
    if (fixt) {  /* Fix value for dummy keys in template table. */
      Node *node = noderef(t->node);
      uint32_t i, hmask = t->hmask;
      for (i = 0; i <= hmask; i++) {
	Node *n = &node[i];
	if (tvistab(&n->val)) {
	  lua_assert(tabV(&n->val) == t);
	  setnilV(&n->val);  /* Turn value into nil. */
	}
      }
    }
    lj_gc_check(fs->L);
  }
}

/* Parse function parameters. */
static BCReg parse_params(LexState *ls, int needself)
{
  FuncState *fs = ls->fs;
  BCReg nparams = 0;
  lex_check(ls, '(');
  if (needself)
    var_new_lit(ls, nparams++, "self");
  if (ls->token != ')') {
    do {
      if (ls->token == TK_name || (!LJ_52 && ls->token == TK_goto)) {
	var_new(ls, nparams++, lex_str(ls));
      } else if (ls->token == TK_dots) {
	lj_lex_next(ls);
	fs->flags |= PROTO_VARARG;
	break;
      } else {
	err_syntax(ls, LJ_ERR_XPARAM);
      }
    } while (lex_opt(ls, ','));
  }
  var_add(ls, nparams);
  lua_assert(fs->nactvar == nparams);
  bcreg_reserve(fs, nparams);
  lex_check(ls, ')');
  return nparams;
}

/* Forward declaration. */
static void parse_chunk(LexState *ls);

/* Parse body of a function. */
static void parse_body(LexState *ls, ExpDesc *e, int needself, BCLine line)
{
  FuncState fs, *pfs = ls->fs;
  FuncScope bl;
  GCproto *pt;
  ptrdiff_t oldbase = pfs->bcbase - ls->bcstack;
  fs_init(ls, &fs);
  fscope_begin(&fs, &bl, 0);
  fs.linedefined = line;
  fs.numparams = (uint8_t)parse_params(ls, needself);
  fs.bcbase = pfs->bcbase + pfs->pc;
  fs.bclim = pfs->bclim - pfs->pc;
  bcemit_AD(&fs, BC_FUNCF, 0, 0);  /* Placeholder. */
  parse_chunk(ls);
  if (ls->token != TK_end) lex_match(ls, TK_end, TK_function, line);
  pt = fs_finish(ls, (ls->lastline = ls->linenumber));
  pfs->bcbase = ls->bcstack + oldbase;  /* May have been reallocated. */
  pfs->bclim = (BCPos)(ls->sizebcstack - oldbase);
  /* Store new prototype in the constant array of the parent. */
  expr_init(e, VRELOCABLE,
	    bcemit_AD(pfs, BC_FNEW, 0, const_gc(pfs, obj2gco(pt), LJ_TPROTO)));
#if LJ_HASFFI
  pfs->flags |= (fs.flags & PROTO_FFI);
#endif
  if (!(pfs->flags & PROTO_CHILD)) {
    if (pfs->flags & PROTO_HAS_RETURN)
      pfs->flags |= PROTO_FIXUP_RETURN;
    pfs->flags |= PROTO_CHILD;
  }
  lj_lex_next(ls);
}

/* Parse expression list. Last expression is left open. */
static BCReg expr_list(LexState *ls, ExpDesc *v)
{
  BCReg n = 1;
  expr(ls, v);
  while (lex_opt(ls, ',')) {
    expr_tonextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}

/* Parse function argument list. */
static void parse_args(LexState *ls, ExpDesc *e)
{
  FuncState *fs = ls->fs;
  ExpDesc args;
  BCIns ins;
  BCReg base;
  BCLine line = ls->linenumber;
  if (ls->token == '(') {
#if !LJ_52
    if (line != ls->lastline)
      err_syntax(ls, LJ_ERR_XAMBIG);
#endif
    lj_lex_next(ls);
    if (ls->token == ')') {  /* f(). */
      args.k = VVOID;
    } else {
      expr_list(ls, &args);
      if (args.k == VCALL)  /* f(a, b, g()) or f(a, b, ...). */
	setbc_b(bcptr(fs, &args), 0);  /* Pass on multiple results. */
    }
    lex_match(ls, ')', '(', line);
  } else if (ls->token == '{') {
    expr_table(ls, &args);
  } else if (ls->token == TK_string) {
    expr_init(&args, VKSTR, 0);
    args.u.sval = strV(&ls->tokenval);
    lj_lex_next(ls);
  } else {
    err_syntax(ls, LJ_ERR_XFUNARG);
    return;  /* Silence compiler. */
  }
  lua_assert(e->k == VNONRELOC);
  base = e->u.s.info;  /* Base register for call. */
  if (args.k == VCALL) {
    ins = BCINS_ABC(BC_CALLM, base, 2, args.u.s.aux - base - 1);
  } else {
    if (args.k != VVOID)
      expr_tonextreg(fs, &args);
    ins = BCINS_ABC(BC_CALL, base, 2, fs->freereg - base);
  }
  expr_init(e, VCALL, bcemit_INS(fs, ins));
  e->u.s.aux = base;
  fs->bcbase[fs->pc - 1].line = line;
  fs->freereg = base+1;  /* Leave one result by default. */
}

/* Parse primary expression. */
static void expr_primary(LexState *ls, ExpDesc *v)
{
  FuncState *fs = ls->fs;
  /* Parse prefix expression. */
  if (ls->token == '(') {
    BCLine line = ls->linenumber;
    lj_lex_next(ls);
    expr(ls, v);
    lex_match(ls, ')', '(', line);
    expr_discharge(ls->fs, v);
  } else if (ls->token == TK_name || (!LJ_52 && ls->token == TK_goto)) {
    var_lookup(ls, v);
  } else {
    err_syntax(ls, LJ_ERR_XSYMBOL);
  }
  for (;;) {  /* Parse multiple expression suffixes. */
    if (ls->token == '.') {
      expr_field(ls, v);
    } else if (ls->token == '[') {
      ExpDesc key;
      expr_toanyreg(fs, v);
      expr_bracket(ls, &key);
      expr_index(fs, v, &key);
    } else if (ls->token == ':') {
      ExpDesc key;
      lj_lex_next(ls);
      expr_str(ls, &key);
      bcemit_method(fs, v, &key);
      parse_args(ls, v);
    } else if (ls->token == '(' || ls->token == TK_string || ls->token == '{') {
      expr_tonextreg(fs, v);
      parse_args(ls, v);
    } else {
      break;
    }
  }
}

/* Parse simple expression. */
static void expr_simple(LexState *ls, ExpDesc *v)
{
  switch (ls->token) {
  case TK_number:
    expr_init(v, (LJ_HASFFI && tviscdata(&ls->tokenval)) ? VKCDATA : VKNUM, 0);
    copyTV(ls->L, &v->u.nval, &ls->tokenval);
    break;
  case TK_string:
    expr_init(v, VKSTR, 0);
    v->u.sval = strV(&ls->tokenval);
    break;
  case TK_nil:
    expr_init(v, VKNIL, 0);
    break;
  case TK_true:
    expr_init(v, VKTRUE, 0);
    break;
  case TK_false:
    expr_init(v, VKFALSE, 0);
    break;
  case TK_dots: {  /* Vararg. */
    FuncState *fs = ls->fs;
    BCReg base;
    checkcond(ls, fs->flags & PROTO_VARARG, LJ_ERR_XDOTS);
    bcreg_reserve(fs, 1);
    base = fs->freereg-1;
    expr_init(v, VCALL, bcemit_ABC(fs, BC_VARG, base, 2, fs->numparams));
    v->u.s.aux = base;
    break;
  }
  case '{':  /* Table constructor. */
    expr_table(ls, v);
    return;
  case TK_function:
    lj_lex_next(ls);
    parse_body(ls, v, 0, ls->linenumber);
    return;
  default:
    expr_primary(ls, v);
    return;
  }
  lj_lex_next(ls);
}

/* Manage syntactic levels to avoid blowing up the stack. */
static void synlevel_begin(LexState *ls)
{
  if (++ls->level >= LJ_MAX_XLEVEL)
    lj_lex_error(ls, 0, LJ_ERR_XLEVELS);
}

#define synlevel_end(ls)	((ls)->level--)

/* Convert token to binary operator. */
static BinOpr token2binop(LexToken tok)
{
  switch (tok) {
  case '+':	return OPR_ADD;
  case '-':	return OPR_SUB;
  case '*':	return OPR_MUL;
  case '/':	return OPR_DIV;
  case '%':	return OPR_MOD;
  case '^':	return OPR_POW;
  case TK_concat: return OPR_CONCAT;
  case TK_ne:	return OPR_NE;
  case TK_eq:	return OPR_EQ;
  case '<':	return OPR_LT;
  case TK_le:	return OPR_LE;
  case '>':	return OPR_GT;
  case TK_ge:	return OPR_GE;
  case TK_and:	return OPR_AND;
  case TK_or:	return OPR_OR;
  default:	return OPR_NOBINOPR;
  }
}

/* Priorities for each binary operator. ORDER OPR. */
static const struct {
  uint8_t left;		/* Left priority. */
  uint8_t right;	/* Right priority. */
} priority[] = {
  {6,6}, {6,6}, {7,7}, {7,7}, {7,7},	/* ADD SUB MUL DIV MOD */
  {10,9}, {5,4},			/* POW CONCAT (right associative) */
  {3,3}, {3,3},				/* EQ NE */
  {3,3}, {3,3}, {3,3}, {3,3},		/* LT GE GT LE */
  {2,2}, {1,1}				/* AND OR */
};

#define UNARY_PRIORITY		8  /* Priority for unary operators. */

/* Forward declaration. */
static BinOpr expr_binop(LexState *ls, ExpDesc *v, uint32_t limit);

/* Parse unary expression. */
static void expr_unop(LexState *ls, ExpDesc *v)
{
  BCOp op;
  if (ls->token == TK_not) {
    op = BC_NOT;
  } else if (ls->token == '-') {
    op = BC_UNM;
  } else if (ls->token == '#') {
    op = BC_LEN;
  } else {
    expr_simple(ls, v);
    return;
  }
  lj_lex_next(ls);
  expr_binop(ls, v, UNARY_PRIORITY);
  bcemit_unop(ls->fs, op, v);
}

/* Parse binary expressions with priority higher than the limit. */
static BinOpr expr_binop(LexState *ls, ExpDesc *v, uint32_t limit)
{
  BinOpr op;
  synlevel_begin(ls);
  expr_unop(ls, v);
  op = token2binop(ls->token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    ExpDesc v2;
    BinOpr nextop;
    lj_lex_next(ls);
    bcemit_binop_left(ls->fs, op, v);
    /* Parse binary expression with higher priority. */
    nextop = expr_binop(ls, &v2, priority[op].right);
    bcemit_binop(ls->fs, op, v, &v2);
    op = nextop;
  }
  synlevel_end(ls);
  return op;  /* Return unconsumed binary operator (if any). */
}

/* Parse expression. */
static void expr(LexState *ls, ExpDesc *v)
{
  expr_binop(ls, v, 0);  /* Priority 0: parse whole expression. */
}

/* Assign expression to the next register. */
static void expr_next(LexState *ls)
{
  ExpDesc e;
  expr(ls, &e);
  expr_tonextreg(ls->fs, &e);
}

/* Parse conditional expression. */
static BCPos expr_cond(LexState *ls)
{
  ExpDesc v;
  expr(ls, &v);
  if (v.k == VKNIL) v.k = VKFALSE;
  bcemit_branch_t(ls->fs, &v);
  return v.f;
}

/* -- Assignments --------------------------------------------------------- */

/* List of LHS variables. */
typedef struct LHSVarList {
  ExpDesc v;			/* LHS variable. */
  struct LHSVarList *prev;	/* Link to previous LHS variable. */
} LHSVarList;

/* Eliminate write-after-read hazards for local variable assignment. */
static void assign_hazard(LexState *ls, LHSVarList *lh, const ExpDesc *v)
{
  FuncState *fs = ls->fs;
  BCReg reg = v->u.s.info;  /* Check against this variable. */
  BCReg tmp = fs->freereg;  /* Rename to this temp. register (if needed). */
  int hazard = 0;
  for (; lh; lh = lh->prev) {
    if (lh->v.k == VINDEXED) {
      if (lh->v.u.s.info == reg) {  /* t[i], t = 1, 2 */
	hazard = 1;
	lh->v.u.s.info = tmp;
      }
      if (lh->v.u.s.aux == reg) {  /* t[i], i = 1, 2 */
	hazard = 1;
	lh->v.u.s.aux = tmp;
      }
    }
  }
  if (hazard) {
    bcemit_AD(fs, BC_MOV, tmp, reg);  /* Rename conflicting variable. */
    bcreg_reserve(fs, 1);
  }
}

/* Adjust LHS/RHS of an assignment. */
static void assign_adjust(LexState *ls, BCReg nvars, BCReg nexps, ExpDesc *e)
{
  FuncState *fs = ls->fs;
  int32_t extra = (int32_t)nvars - (int32_t)nexps;
  if (e->k == VCALL) {
    extra++;  /* Compensate for the VCALL itself. */
    if (extra < 0) extra = 0;
    setbc_b(bcptr(fs, e), extra+1);  /* Fixup call results. */
    if (extra > 1) bcreg_reserve(fs, (BCReg)extra-1);
  } else {
    if (e->k != VVOID)
      expr_tonextreg(fs, e);  /* Close last expression. */
    if (extra > 0) {  /* Leftover LHS are set to nil. */
      BCReg reg = fs->freereg;
      bcreg_reserve(fs, (BCReg)extra);
      bcemit_nil(fs, reg, (BCReg)extra);
    }
  }
}

/* Recursively parse assignment statement. */
static void parse_assignment(LexState *ls, LHSVarList *lh, BCReg nvars)
{
  ExpDesc e;
  checkcond(ls, VLOCAL <= lh->v.k && lh->v.k <= VINDEXED, LJ_ERR_XSYNTAX);
  if (lex_opt(ls, ',')) {  /* Collect LHS list and recurse upwards. */
    LHSVarList vl;
    vl.prev = lh;
    expr_primary(ls, &vl.v);
    if (vl.v.k == VLOCAL)
      assign_hazard(ls, lh, &vl.v);
    checklimit(ls->fs, ls->level + nvars, LJ_MAX_XLEVEL, "variable names");
    parse_assignment(ls, &vl, nvars+1);
  } else {  /* Parse RHS. */
    BCReg nexps;
    lex_check(ls, '=');
    nexps = expr_list(ls, &e);
    if (nexps == nvars) {
      if (e.k == VCALL) {
	if (bc_op(*bcptr(ls->fs, &e)) == BC_VARG) {  /* Vararg assignment. */
	  ls->fs->freereg--;
	  e.k = VRELOCABLE;
	} else {  /* Multiple call results. */
	  e.u.s.info = e.u.s.aux;  /* Base of call is not relocatable. */
	  e.k = VNONRELOC;
	}
      }
      bcemit_store(ls->fs, &lh->v, &e);
      return;
    }
    assign_adjust(ls, nvars, nexps, &e);
    if (nexps > nvars)
      ls->fs->freereg -= nexps - nvars;  /* Drop leftover regs. */
  }
  /* Assign RHS to LHS and recurse downwards. */
  expr_init(&e, VNONRELOC, ls->fs->freereg-1);
  bcemit_store(ls->fs, &lh->v, &e);
}

/* Parse call statement or assignment. */
static void parse_call_assign(LexState *ls)
{
  FuncState *fs = ls->fs;
  LHSVarList vl;
  expr_primary(ls, &vl.v);
  if (vl.v.k == VCALL) {  /* Function call statement. */
    setbc_b(bcptr(fs, &vl.v), 1);  /* No results. */
  } else {  /* Start of an assignment. */
    vl.prev = NULL;
    parse_assignment(ls, &vl, 1);
  }
}

/* Parse 'local' statement. */
static void parse_local(LexState *ls)
{
  if (lex_opt(ls, TK_function)) {  /* Local function declaration. */
    ExpDesc v, b;
    FuncState *fs = ls->fs;
    var_new(ls, 0, lex_str(ls));
    expr_init(&v, VLOCAL, fs->freereg);
    v.u.s.aux = fs->varmap[fs->freereg];
    bcreg_reserve(fs, 1);
    var_add(ls, 1);
    parse_body(ls, &b, 0, ls->linenumber);
    /* bcemit_store(fs, &v, &b) without setting VSTACK_VAR_RW. */
    expr_free(fs, &b);
    expr_toreg(fs, &b, v.u.s.info);
    /* The upvalue is in scope, but the local is only valid after the store. */
    var_get(ls, fs, fs->nactvar - 1).startpc = fs->pc;
  } else {  /* Local variable declaration. */
    ExpDesc e;
    BCReg nexps, nvars = 0;
    do {  /* Collect LHS. */
      var_new(ls, nvars++, lex_str(ls));
    } while (lex_opt(ls, ','));
    if (lex_opt(ls, '=')) {  /* Optional RHS. */
      nexps = expr_list(ls, &e);
    } else {  /* Or implicitly set to nil. */
      e.k = VVOID;
      nexps = 0;
    }
    assign_adjust(ls, nvars, nexps, &e);
    var_add(ls, nvars);
  }
}

/* Parse 'function' statement. */
static void parse_func(LexState *ls, BCLine line)
{
  FuncState *fs;
  ExpDesc v, b;
  int needself = 0;
  lj_lex_next(ls);  /* Skip 'function'. */
  /* Parse function name. */
  var_lookup(ls, &v);
  while (ls->token == '.')  /* Multiple dot-separated fields. */
    expr_field(ls, &v);
  if (ls->token == ':') {  /* Optional colon to signify method call. */
    needself = 1;
    expr_field(ls, &v);
  }
  parse_body(ls, &b, needself, line);
  fs = ls->fs;
  bcemit_store(fs, &v, &b);
  fs->bcbase[fs->pc - 1].line = line;  /* Set line for the store. */
}

/* -- Control transfer statements ----------------------------------------- */

/* Check for end of block. */
static int endofblock(LexToken token)
{
  switch (token) {
  case TK_else: case TK_elseif: case TK_end: case TK_until: case TK_eof:
    return 1;
  default:
    return 0;
  }
}

/* Parse 'return' statement. */
static void parse_return(LexState *ls)
{
  BCIns ins;
  FuncState *fs = ls->fs;
  lj_lex_next(ls);  /* Skip 'return'. */
  fs->flags |= PROTO_HAS_RETURN;
  if (endofblock(ls->token) || ls->token == ';') {  /* Bare return. */
    ins = BCINS_AD(BC_RET0, 0, 1);
  } else {  /* Return with one or more values. */
    ExpDesc e;  /* Receives the _last_ expression in the list. */
    BCReg nret = expr_list(ls, &e);
    if (nret == 1) {  /* Return one result. */
      if (e.k == VCALL) {  /* Check for tail call. */
	BCIns *ip = bcptr(fs, &e);
	/* It doesn't pay off to add BC_VARGT just for 'return ...'. */
	if (bc_op(*ip) == BC_VARG) goto notailcall;
	fs->pc--;
	ins = BCINS_AD(bc_op(*ip)-BC_CALL+BC_CALLT, bc_a(*ip), bc_c(*ip));
      } else {  /* Can return the result from any register. */
	ins = BCINS_AD(BC_RET1, expr_toanyreg(fs, &e), 2);
      }
    } else {
      if (e.k == VCALL) {  /* Append all results from a call. */
      notailcall:
	setbc_b(bcptr(fs, &e), 0);
	ins = BCINS_AD(BC_RETM, fs->nactvar, e.u.s.aux - fs->nactvar);
      } else {
	expr_tonextreg(fs, &e);  /* Force contiguous registers. */
	ins = BCINS_AD(BC_RET, fs->nactvar, nret+1);
      }
    }
  }
  if (fs->flags & PROTO_CHILD)
    bcemit_AJ(fs, BC_UCLO, 0, 0);  /* May need to close upvalues first. */
  bcemit_INS(fs, ins);
}

/* Parse 'break' statement. */
static void parse_break(LexState *ls)
{
  ls->fs->bl->flags |= FSCOPE_BREAK;
  gola_new(ls, NAME_BREAK, VSTACK_GOTO, bcemit_jmp(ls->fs));
}

/* Parse 'goto' statement. */
static void parse_goto(LexState *ls)
{
  FuncState *fs = ls->fs;
  GCstr *name = lex_str(ls);
  VarInfo *vl = gola_findlabel(ls, name);
  if (vl)  /* Treat backwards goto within same scope like a loop. */
    bcemit_AJ(fs, BC_LOOP, vl->slot, -1);  /* No BC range check. */
  fs->bl->flags |= FSCOPE_GOLA;
  gola_new(ls, name, VSTACK_GOTO, bcemit_jmp(fs));
}

/* Parse label. */
static void parse_label(LexState *ls)
{
  FuncState *fs = ls->fs;
  GCstr *name;
  MSize idx;
  fs->lasttarget = fs->pc;
  fs->bl->flags |= FSCOPE_GOLA;
  lj_lex_next(ls);  /* Skip '::'. */
  name = lex_str(ls);
  if (gola_findlabel(ls, name))
    lj_lex_error(ls, 0, LJ_ERR_XLDUP, strdata(name));
  idx = gola_new(ls, name, VSTACK_LABEL, fs->pc);
  lex_check(ls, TK_label);
  /* Recursively parse trailing statements: labels and ';' (Lua 5.2 only). */
  for (;;) {
    if (ls->token == TK_label) {
      synlevel_begin(ls);
      parse_label(ls);
      synlevel_end(ls);
    } else if (LJ_52 && ls->token == ';') {
      lj_lex_next(ls);
    } else {
      break;
    }
  }
  /* Trailing label is considered to be outside of scope. */
  if (endofblock(ls->token) && ls->token != TK_until)
    ls->vstack[idx].slot = fs->bl->nactvar;
  gola_resolve(ls, fs->bl, idx);
}

/* -- Blocks, loops and conditional statements ---------------------------- */

/* Parse a block. */
static void parse_block(LexState *ls)
{
  FuncState *fs = ls->fs;
  FuncScope bl;
  fscope_begin(fs, &bl, 0);
  parse_chunk(ls);
  fscope_end(fs);
}

/* Parse 'while' statement. */
static void parse_while(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  BCPos start, loop, condexit;
  FuncScope bl;
  lj_lex_next(ls);  /* Skip 'while'. */
  start = fs->lasttarget = fs->pc;
  condexit = expr_cond(ls);
  fscope_begin(fs, &bl, FSCOPE_LOOP);
  lex_check(ls, TK_do);
  loop = bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
  parse_block(ls);
  jmp_patch(fs, bcemit_jmp(fs), start);
  lex_match(ls, TK_end, TK_while, line);
  fscope_end(fs);
  jmp_tohere(fs, condexit);
  jmp_patchins(fs, loop, fs->pc);
}

/* Parse 'repeat' statement. */
static void parse_repeat(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  BCPos loop = fs->lasttarget = fs->pc;
  BCPos condexit;
  FuncScope bl1, bl2;
  fscope_begin(fs, &bl1, FSCOPE_LOOP);  /* Breakable loop scope. */
  fscope_begin(fs, &bl2, 0);  /* Inner scope. */
  lj_lex_next(ls);  /* Skip 'repeat'. */
  bcemit_AD(fs, BC_LOOP, fs->nactvar, 0);
  parse_chunk(ls);
  lex_match(ls, TK_until, TK_repeat, line);
  condexit = expr_cond(ls);  /* Parse condition (still inside inner scope). */
  if (!(bl2.flags & FSCOPE_UPVAL)) {  /* No upvalues? Just end inner scope. */
    fscope_end(fs);
  } else {  /* Otherwise generate: cond: UCLO+JMP out, !cond: UCLO+JMP loop. */
    parse_break(ls);  /* Break from loop and close upvalues. */
    jmp_tohere(fs, condexit);
    fscope_end(fs);  /* End inner scope and close upvalues. */
    condexit = bcemit_jmp(fs);
  }
  jmp_patch(fs, condexit, loop);  /* Jump backwards if !cond. */
  jmp_patchins(fs, loop, fs->pc);
  fscope_end(fs);  /* End loop scope. */
}

/* Parse numeric 'for'. */
static void parse_for_num(LexState *ls, GCstr *varname, BCLine line)
{
  FuncState *fs = ls->fs;
  BCReg base = fs->freereg;
  FuncScope bl;
  BCPos loop, loopend;
  /* Hidden control variables. */
  var_new_fixed(ls, FORL_IDX, VARNAME_FOR_IDX);
  var_new_fixed(ls, FORL_STOP, VARNAME_FOR_STOP);
  var_new_fixed(ls, FORL_STEP, VARNAME_FOR_STEP);
  /* Visible copy of index variable. */
  var_new(ls, FORL_EXT, varname);
  lex_check(ls, '=');
  expr_next(ls);
  lex_check(ls, ',');
  expr_next(ls);
  if (lex_opt(ls, ',')) {
    expr_next(ls);
  } else {
    bcemit_AD(fs, BC_KSHORT, fs->freereg, 1);  /* Default step is 1. */
    bcreg_reserve(fs, 1);
  }
  var_add(ls, 3);  /* Hidden control variables. */
  lex_check(ls, TK_do);
  loop = bcemit_AJ(fs, BC_FORI, base, NO_JMP);
  fscope_begin(fs, &bl, 0);  /* Scope for visible variables. */
  var_add(ls, 1);
  bcreg_reserve(fs, 1);
  parse_block(ls);
  fscope_end(fs);
  /* Perform loop inversion. Loop control instructions are at the end. */
  loopend = bcemit_AJ(fs, BC_FORL, base, NO_JMP);
  fs->bcbase[loopend].line = line;  /* Fix line for control ins. */
  jmp_patchins(fs, loopend, loop+1);
  jmp_patchins(fs, loop, fs->pc);
}

/* Try to predict whether the iterator is next() and specialize the bytecode.
** Detecting next() and pairs() by name is simplistic, but quite effective.
** The interpreter backs off if the check for the closure fails at runtime.
*/
static int predict_next(LexState *ls, FuncState *fs, BCPos pc)
{
  BCIns ins = fs->bcbase[pc].ins;
  GCstr *name;
  cTValue *o;
  switch (bc_op(ins)) {
  case BC_MOV:
    name = gco2str(gcref(var_get(ls, fs, bc_d(ins)).name));
    break;
  case BC_UGET:
    name = gco2str(gcref(ls->vstack[fs->uvmap[bc_d(ins)]].name));
    break;
  case BC_GGET:
    /* There's no inverse index (yet), so lookup the strings. */
    o = lj_tab_getstr(fs->kt, lj_str_newlit(ls->L, "pairs"));
    if (o && tvhaskslot(o) && tvkslot(o) == bc_d(ins))
      return 1;
    o = lj_tab_getstr(fs->kt, lj_str_newlit(ls->L, "next"));
    if (o && tvhaskslot(o) && tvkslot(o) == bc_d(ins))
      return 1;
    return 0;
  default:
    return 0;
  }
  return (name->len == 5 && !strcmp(strdata(name), "pairs")) ||
	 (name->len == 4 && !strcmp(strdata(name), "next"));
}

/* Parse 'for' iterator. */
static void parse_for_iter(LexState *ls, GCstr *indexname)
{
  FuncState *fs = ls->fs;
  ExpDesc e;
  BCReg nvars = 0;
  BCLine line;
  BCReg base = fs->freereg + 3;
  BCPos loop, loopend, exprpc = fs->pc;
  FuncScope bl;
  int isnext;
  /* Hidden control variables. */
  var_new_fixed(ls, nvars++, VARNAME_FOR_GEN);
  var_new_fixed(ls, nvars++, VARNAME_FOR_STATE);
  var_new_fixed(ls, nvars++, VARNAME_FOR_CTL);
  /* Visible variables returned from iterator. */
  var_new(ls, nvars++, indexname);
  while (lex_opt(ls, ','))
    var_new(ls, nvars++, lex_str(ls));
  lex_check(ls, TK_in);
  line = ls->linenumber;
  assign_adjust(ls, 3, expr_list(ls, &e), &e);
  bcreg_bump(fs, 3);  /* The iterator needs another 3 slots (func + 2 args). */
  isnext = (nvars <= 5 && predict_next(ls, fs, exprpc));
  var_add(ls, 3);  /* Hidden control variables. */
  lex_check(ls, TK_do);
  loop = bcemit_AJ(fs, isnext ? BC_ISNEXT : BC_JMP, base, NO_JMP);
  fscope_begin(fs, &bl, 0);  /* Scope for visible variables. */
  var_add(ls, nvars-3);
  bcreg_reserve(fs, nvars-3);
  parse_block(ls);
  fscope_end(fs);
  /* Perform loop inversion. Loop control instructions are at the end. */
  jmp_patchins(fs, loop, fs->pc);
  bcemit_ABC(fs, isnext ? BC_ITERN : BC_ITERC, base, nvars-3+1, 2+1);
  loopend = bcemit_AJ(fs, BC_ITERL, base, NO_JMP);
  fs->bcbase[loopend-1].line = line;  /* Fix line for control ins. */
  fs->bcbase[loopend].line = line;
  jmp_patchins(fs, loopend, loop+1);
}

/* Parse 'for' statement. */
static void parse_for(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  GCstr *varname;
  FuncScope bl;
  fscope_begin(fs, &bl, FSCOPE_LOOP);
  lj_lex_next(ls);  /* Skip 'for'. */
  varname = lex_str(ls);  /* Get first variable name. */
  if (ls->token == '=')
    parse_for_num(ls, varname, line);
  else if (ls->token == ',' || ls->token == TK_in)
    parse_for_iter(ls, varname);
  else
    err_syntax(ls, LJ_ERR_XFOR);
  lex_match(ls, TK_end, TK_for, line);
  fscope_end(fs);  /* Resolve break list. */
}

/* Parse condition and 'then' block. */
static BCPos parse_then(LexState *ls)
{
  BCPos condexit;
  lj_lex_next(ls);  /* Skip 'if' or 'elseif'. */
  condexit = expr_cond(ls);
  lex_check(ls, TK_then);
  parse_block(ls);
  return condexit;
}

/* Parse 'if' statement. */
static void parse_if(LexState *ls, BCLine line)
{
  FuncState *fs = ls->fs;
  BCPos flist;
  BCPos escapelist = NO_JMP;
  flist = parse_then(ls);
  while (ls->token == TK_elseif) {  /* Parse multiple 'elseif' blocks. */
    jmp_append(fs, &escapelist, bcemit_jmp(fs));
    jmp_tohere(fs, flist);
    flist = parse_then(ls);
  }
  if (ls->token == TK_else) {  /* Parse optional 'else' block. */
    jmp_append(fs, &escapelist, bcemit_jmp(fs));
    jmp_tohere(fs, flist);
    lj_lex_next(ls);  /* Skip 'else'. */
    parse_block(ls);
  } else {
    jmp_append(fs, &escapelist, flist);
  }
  jmp_tohere(fs, escapelist);
  lex_match(ls, TK_end, TK_if, line);
}

/* -- Parse statements ---------------------------------------------------- */

/* Parse a statement. Returns 1 if it must be the last one in a chunk. */
static int parse_stmt(LexState *ls)
{
  BCLine line = ls->linenumber;
  switch (ls->token) {
  case TK_if:
    parse_if(ls, line);
    break;
  case TK_while:
    parse_while(ls, line);
    break;
  case TK_do:
    lj_lex_next(ls);
    parse_block(ls);
    lex_match(ls, TK_end, TK_do, line);
    break;
  case TK_for:
    parse_for(ls, line);
    break;
  case TK_repeat:
    parse_repeat(ls, line);
    break;
  case TK_function:
    parse_func(ls, line);
    break;
  case TK_local:
    lj_lex_next(ls);
    parse_local(ls);
    break;
  case TK_return:
    parse_return(ls);
    return 1;  /* Must be last. */
  case TK_break:
    lj_lex_next(ls);
    parse_break(ls);
    return !LJ_52;  /* Must be last in Lua 5.1. */
#if LJ_52
  case ';':
    lj_lex_next(ls);
    break;
#endif
  case TK_label:
    parse_label(ls);
    break;
  case TK_goto:
    if (LJ_52 || lj_lex_lookahead(ls) == TK_name) {
      lj_lex_next(ls);
      parse_goto(ls);
      break;
    }  /* else: fallthrough */
  default:
    parse_call_assign(ls);
    break;
  }
  return 0;
}

/* A chunk is a list of statements optionally separated by semicolons. */
static void parse_chunk(LexState *ls)
{
  int islast = 0;
  synlevel_begin(ls);
  while (!islast && !endofblock(ls->token)) {
    islast = parse_stmt(ls);
    lex_opt(ls, ';');
    lua_assert(ls->fs->framesize >= ls->fs->freereg &&
	       ls->fs->freereg >= ls->fs->nactvar);
    ls->fs->freereg = ls->fs->nactvar;  /* Free registers after each stmt. */
  }
  synlevel_end(ls);
}

/* Entry point of bytecode parser. */
GCproto *lj_parse(LexState *ls)
{
  FuncState fs;
  FuncScope bl;
  GCproto *pt;
  lua_State *L = ls->L;
#ifdef LUAJIT_DISABLE_DEBUGINFO
  ls->chunkname = lj_str_newlit(L, "=");
#else
  ls->chunkname = lj_str_newz(L, ls->chunkarg);
#endif
  setstrV(L, L->top, ls->chunkname);  /* Anchor chunkname string. */
  incr_top(L);
  ls->level = 0;
  fs_init(ls, &fs);
  fs.linedefined = 0;
  fs.numparams = 0;
  fs.bcbase = NULL;
  fs.bclim = 0;
  fs.flags |= PROTO_VARARG;  /* Main chunk is always a vararg func. */
  fscope_begin(&fs, &bl, 0);
  bcemit_AD(&fs, BC_FUNCV, 0, 0);  /* Placeholder. */
  lj_lex_next(ls);  /* Read-ahead first token. */
  parse_chunk(ls);
  if (ls->token != TK_eof)
    err_token(ls, TK_eof);
  pt = fs_finish(ls, ls->linenumber);
  L->top--;  /* Drop chunkname. */
  lua_assert(fs.prev == NULL);
  lua_assert(ls->fs == NULL);
  lua_assert(pt->sizeuv == 0);
  return pt;
}

