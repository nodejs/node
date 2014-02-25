/*
** Instruction dispatch handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_dispatch_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_func.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_debug.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_ff.h"
#if LJ_HASJIT
#include "lj_jit.h"
#endif
#if LJ_HASFFI
#include "lj_ccallback.h"
#endif
#include "lj_trace.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "luajit.h"

/* Bump GG_NUM_ASMFF in lj_dispatch.h as needed. Ugly. */
LJ_STATIC_ASSERT(GG_NUM_ASMFF == FF_NUM_ASMFUNC);

/* -- Dispatch table management ------------------------------------------- */

#if LJ_TARGET_MIPS
#include <math.h>
LJ_FUNCA_NORET void LJ_FASTCALL lj_ffh_coroutine_wrap_err(lua_State *L,
							  lua_State *co);

#define GOTFUNC(name)	(ASMFunction)name,
static const ASMFunction dispatch_got[] = {
  GOTDEF(GOTFUNC)
};
#undef GOTFUNC
#endif

/* Initialize instruction dispatch table and hot counters. */
void lj_dispatch_init(GG_State *GG)
{
  uint32_t i;
  ASMFunction *disp = GG->dispatch;
  for (i = 0; i < GG_LEN_SDISP; i++)
    disp[GG_LEN_DDISP+i] = disp[i] = makeasmfunc(lj_bc_ofs[i]);
  for (i = GG_LEN_SDISP; i < GG_LEN_DDISP; i++)
    disp[i] = makeasmfunc(lj_bc_ofs[i]);
  /* The JIT engine is off by default. luaopen_jit() turns it on. */
  disp[BC_FORL] = disp[BC_IFORL];
  disp[BC_ITERL] = disp[BC_IITERL];
  disp[BC_LOOP] = disp[BC_ILOOP];
  disp[BC_FUNCF] = disp[BC_IFUNCF];
  disp[BC_FUNCV] = disp[BC_IFUNCV];
  GG->g.bc_cfunc_ext = GG->g.bc_cfunc_int = BCINS_AD(BC_FUNCC, LUA_MINSTACK, 0);
  for (i = 0; i < GG_NUM_ASMFF; i++)
    GG->bcff[i] = BCINS_AD(BC__MAX+i, 0, 0);
#if LJ_TARGET_MIPS
  memcpy(GG->got, dispatch_got, LJ_GOT__MAX*4);
#endif
}

#if LJ_HASJIT
/* Initialize hotcount table. */
void lj_dispatch_init_hotcount(global_State *g)
{
  int32_t hotloop = G2J(g)->param[JIT_P_hotloop];
  HotCount start = (HotCount)(hotloop*HOTCOUNT_LOOP - 1);
  HotCount *hotcount = G2GG(g)->hotcount;
  uint32_t i;
  for (i = 0; i < HOTCOUNT_SIZE; i++)
    hotcount[i] = start;
}
#endif

/* Internal dispatch mode bits. */
#define DISPMODE_JIT	0x01	/* JIT compiler on. */
#define DISPMODE_REC	0x02	/* Recording active. */
#define DISPMODE_INS	0x04	/* Override instruction dispatch. */
#define DISPMODE_CALL	0x08	/* Override call dispatch. */
#define DISPMODE_RET	0x10	/* Override return dispatch. */

/* Update dispatch table depending on various flags. */
void lj_dispatch_update(global_State *g)
{
  uint8_t oldmode = g->dispatchmode;
  uint8_t mode = 0;
#if LJ_HASJIT
  mode |= (G2J(g)->flags & JIT_F_ON) ? DISPMODE_JIT : 0;
  mode |= G2J(g)->state != LJ_TRACE_IDLE ?
	    (DISPMODE_REC|DISPMODE_INS|DISPMODE_CALL) : 0;
#endif
  mode |= (g->hookmask & (LUA_MASKLINE|LUA_MASKCOUNT)) ? DISPMODE_INS : 0;
  mode |= (g->hookmask & LUA_MASKCALL) ? DISPMODE_CALL : 0;
  mode |= (g->hookmask & LUA_MASKRET) ? DISPMODE_RET : 0;
  if (oldmode != mode) {  /* Mode changed? */
    ASMFunction *disp = G2GG(g)->dispatch;
    ASMFunction f_forl, f_iterl, f_loop, f_funcf, f_funcv;
    g->dispatchmode = mode;

    /* Hotcount if JIT is on, but not while recording. */
    if ((mode & (DISPMODE_JIT|DISPMODE_REC)) == DISPMODE_JIT) {
      f_forl = makeasmfunc(lj_bc_ofs[BC_FORL]);
      f_iterl = makeasmfunc(lj_bc_ofs[BC_ITERL]);
      f_loop = makeasmfunc(lj_bc_ofs[BC_LOOP]);
      f_funcf = makeasmfunc(lj_bc_ofs[BC_FUNCF]);
      f_funcv = makeasmfunc(lj_bc_ofs[BC_FUNCV]);
    } else {  /* Otherwise use the non-hotcounting instructions. */
      f_forl = disp[GG_LEN_DDISP+BC_IFORL];
      f_iterl = disp[GG_LEN_DDISP+BC_IITERL];
      f_loop = disp[GG_LEN_DDISP+BC_ILOOP];
      f_funcf = makeasmfunc(lj_bc_ofs[BC_IFUNCF]);
      f_funcv = makeasmfunc(lj_bc_ofs[BC_IFUNCV]);
    }
    /* Init static counting instruction dispatch first (may be copied below). */
    disp[GG_LEN_DDISP+BC_FORL] = f_forl;
    disp[GG_LEN_DDISP+BC_ITERL] = f_iterl;
    disp[GG_LEN_DDISP+BC_LOOP] = f_loop;

    /* Set dynamic instruction dispatch. */
    if ((oldmode ^ mode) & (DISPMODE_REC|DISPMODE_INS)) {
      /* Need to update the whole table. */
      if (!(mode & (DISPMODE_REC|DISPMODE_INS))) {  /* No ins dispatch? */
	/* Copy static dispatch table to dynamic dispatch table. */
	memcpy(&disp[0], &disp[GG_LEN_DDISP], GG_LEN_SDISP*sizeof(ASMFunction));
	/* Overwrite with dynamic return dispatch. */
	if ((mode & DISPMODE_RET)) {
	  disp[BC_RETM] = lj_vm_rethook;
	  disp[BC_RET] = lj_vm_rethook;
	  disp[BC_RET0] = lj_vm_rethook;
	  disp[BC_RET1] = lj_vm_rethook;
	}
      } else {
	/* The recording dispatch also checks for hooks. */
	ASMFunction f = (mode & DISPMODE_REC) ? lj_vm_record : lj_vm_inshook;
	uint32_t i;
	for (i = 0; i < GG_LEN_SDISP; i++)
	  disp[i] = f;
      }
    } else if (!(mode & (DISPMODE_REC|DISPMODE_INS))) {
      /* Otherwise set dynamic counting ins. */
      disp[BC_FORL] = f_forl;
      disp[BC_ITERL] = f_iterl;
      disp[BC_LOOP] = f_loop;
      /* Set dynamic return dispatch. */
      if ((mode & DISPMODE_RET)) {
	disp[BC_RETM] = lj_vm_rethook;
	disp[BC_RET] = lj_vm_rethook;
	disp[BC_RET0] = lj_vm_rethook;
	disp[BC_RET1] = lj_vm_rethook;
      } else {
	disp[BC_RETM] = disp[GG_LEN_DDISP+BC_RETM];
	disp[BC_RET] = disp[GG_LEN_DDISP+BC_RET];
	disp[BC_RET0] = disp[GG_LEN_DDISP+BC_RET0];
	disp[BC_RET1] = disp[GG_LEN_DDISP+BC_RET1];
      }
    }

    /* Set dynamic call dispatch. */
    if ((oldmode ^ mode) & DISPMODE_CALL) {  /* Update the whole table? */
      uint32_t i;
      if ((mode & DISPMODE_CALL) == 0) {  /* No call hooks? */
	for (i = GG_LEN_SDISP; i < GG_LEN_DDISP; i++)
	  disp[i] = makeasmfunc(lj_bc_ofs[i]);
      } else {
	for (i = GG_LEN_SDISP; i < GG_LEN_DDISP; i++)
	  disp[i] = lj_vm_callhook;
      }
    }
    if (!(mode & DISPMODE_CALL)) {  /* Overwrite dynamic counting ins. */
      disp[BC_FUNCF] = f_funcf;
      disp[BC_FUNCV] = f_funcv;
    }

#if LJ_HASJIT
    /* Reset hotcounts for JIT off to on transition. */
    if ((mode & DISPMODE_JIT) && !(oldmode & DISPMODE_JIT))
      lj_dispatch_init_hotcount(g);
#endif
  }
}

/* -- JIT mode setting ---------------------------------------------------- */

#if LJ_HASJIT
/* Set JIT mode for a single prototype. */
static void setptmode(global_State *g, GCproto *pt, int mode)
{
  if ((mode & LUAJIT_MODE_ON)) {  /* (Re-)enable JIT compilation. */
    pt->flags &= ~PROTO_NOJIT;
    lj_trace_reenableproto(pt);  /* Unpatch all ILOOP etc. bytecodes. */
  } else {  /* Flush and/or disable JIT compilation. */
    if (!(mode & LUAJIT_MODE_FLUSH))
      pt->flags |= PROTO_NOJIT;
    lj_trace_flushproto(g, pt);  /* Flush all traces of prototype. */
  }
}

/* Recursively set the JIT mode for all children of a prototype. */
static void setptmode_all(global_State *g, GCproto *pt, int mode)
{
  ptrdiff_t i;
  if (!(pt->flags & PROTO_CHILD)) return;
  for (i = -(ptrdiff_t)pt->sizekgc; i < 0; i++) {
    GCobj *o = proto_kgc(pt, i);
    if (o->gch.gct == ~LJ_TPROTO) {
      setptmode(g, gco2pt(o), mode);
      setptmode_all(g, gco2pt(o), mode);
    }
  }
}
#endif

/* Public API function: control the JIT engine. */
int luaJIT_setmode(lua_State *L, int idx, int mode)
{
  global_State *g = G(L);
  int mm = mode & LUAJIT_MODE_MASK;
  lj_trace_abort(g);  /* Abort recording on any state change. */
  /* Avoid pulling the rug from under our own feet. */
  if ((g->hookmask & HOOK_GC))
    lj_err_caller(L, LJ_ERR_NOGCMM);
  switch (mm) {
#if LJ_HASJIT
  case LUAJIT_MODE_ENGINE:
    if ((mode & LUAJIT_MODE_FLUSH)) {
      lj_trace_flushall(L);
    } else {
      if (!(mode & LUAJIT_MODE_ON))
	G2J(g)->flags &= ~(uint32_t)JIT_F_ON;
#if LJ_TARGET_X86ORX64
      else if ((G2J(g)->flags & JIT_F_SSE2))
	G2J(g)->flags |= (uint32_t)JIT_F_ON;
      else
	return 0;  /* Don't turn on JIT compiler without SSE2 support. */
#else
      else
	G2J(g)->flags |= (uint32_t)JIT_F_ON;
#endif
      lj_dispatch_update(g);
    }
    break;
  case LUAJIT_MODE_FUNC:
  case LUAJIT_MODE_ALLFUNC:
  case LUAJIT_MODE_ALLSUBFUNC: {
    cTValue *tv = idx == 0 ? frame_prev(L->base-1) :
		  idx > 0 ? L->base + (idx-1) : L->top + idx;
    GCproto *pt;
    if ((idx == 0 || tvisfunc(tv)) && isluafunc(&gcval(tv)->fn))
      pt = funcproto(&gcval(tv)->fn);  /* Cannot use funcV() for frame slot. */
    else if (tvisproto(tv))
      pt = protoV(tv);
    else
      return 0;  /* Failed. */
    if (mm != LUAJIT_MODE_ALLSUBFUNC)
      setptmode(g, pt, mode);
    if (mm != LUAJIT_MODE_FUNC)
      setptmode_all(g, pt, mode);
    break;
    }
  case LUAJIT_MODE_TRACE:
    if (!(mode & LUAJIT_MODE_FLUSH))
      return 0;  /* Failed. */
    lj_trace_flush(G2J(g), idx);
    break;
#else
  case LUAJIT_MODE_ENGINE:
  case LUAJIT_MODE_FUNC:
  case LUAJIT_MODE_ALLFUNC:
  case LUAJIT_MODE_ALLSUBFUNC:
    UNUSED(idx);
    if ((mode & LUAJIT_MODE_ON))
      return 0;  /* Failed. */
    break;
#endif
  case LUAJIT_MODE_WRAPCFUNC:
    if ((mode & LUAJIT_MODE_ON)) {
      if (idx != 0) {
	cTValue *tv = idx > 0 ? L->base + (idx-1) : L->top + idx;
	if (tvislightud(tv))
	  g->wrapf = (lua_CFunction)lightudV(tv);
	else
	  return 0;  /* Failed. */
      } else {
	return 0;  /* Failed. */
      }
      g->bc_cfunc_ext = BCINS_AD(BC_FUNCCW, 0, 0);
    } else {
      g->bc_cfunc_ext = BCINS_AD(BC_FUNCC, 0, 0);
    }
    break;
  default:
    return 0;  /* Failed. */
  }
  return 1;  /* OK. */
}

/* Enforce (dynamic) linker error for version mismatches. See luajit.c. */
LUA_API void LUAJIT_VERSION_SYM(void)
{
}

/* -- Hooks --------------------------------------------------------------- */

/* This function can be called asynchronously (e.g. during a signal). */
LUA_API int lua_sethook(lua_State *L, lua_Hook func, int mask, int count)
{
  global_State *g = G(L);
  mask &= HOOK_EVENTMASK;
  if (func == NULL || mask == 0) { mask = 0; func = NULL; }  /* Consistency. */
  g->hookf = func;
  g->hookcount = g->hookcstart = (int32_t)count;
  g->hookmask = (uint8_t)((g->hookmask & ~HOOK_EVENTMASK) | mask);
  lj_trace_abort(g);  /* Abort recording on any hook change. */
  lj_dispatch_update(g);
  return 1;
}

LUA_API lua_Hook lua_gethook(lua_State *L)
{
  return G(L)->hookf;
}

LUA_API int lua_gethookmask(lua_State *L)
{
  return G(L)->hookmask & HOOK_EVENTMASK;
}

LUA_API int lua_gethookcount(lua_State *L)
{
  return (int)G(L)->hookcstart;
}

/* Call a hook. */
static void callhook(lua_State *L, int event, BCLine line)
{
  global_State *g = G(L);
  lua_Hook hookf = g->hookf;
  if (hookf && !hook_active(g)) {
    lua_Debug ar;
    lj_trace_abort(g);  /* Abort recording on any hook call. */
    ar.event = event;
    ar.currentline = line;
    /* Top frame, nextframe = NULL. */
    ar.i_ci = (int)((L->base-1) - tvref(L->stack));
    lj_state_checkstack(L, 1+LUA_MINSTACK);
    hook_enter(g);
    hookf(L, &ar);
    lua_assert(hook_active(g));
    hook_leave(g);
  }
}

/* -- Dispatch callbacks -------------------------------------------------- */

/* Calculate number of used stack slots in the current frame. */
static BCReg cur_topslot(GCproto *pt, const BCIns *pc, uint32_t nres)
{
  BCIns ins = pc[-1];
  if (bc_op(ins) == BC_UCLO)
    ins = pc[bc_j(ins)];
  switch (bc_op(ins)) {
  case BC_CALLM: case BC_CALLMT: return bc_a(ins) + bc_c(ins) + nres-1+1;
  case BC_RETM: return bc_a(ins) + bc_d(ins) + nres-1;
  case BC_TSETM: return bc_a(ins) + nres-1;
  default: return pt->framesize;
  }
}

/* Instruction dispatch. Used by instr/line/return hooks or when recording. */
void LJ_FASTCALL lj_dispatch_ins(lua_State *L, const BCIns *pc)
{
  ERRNO_SAVE
  GCfunc *fn = curr_func(L);
  GCproto *pt = funcproto(fn);
  void *cf = cframe_raw(L->cframe);
  const BCIns *oldpc = cframe_pc(cf);
  global_State *g = G(L);
  BCReg slots;
  setcframe_pc(cf, pc);
  slots = cur_topslot(pt, pc, cframe_multres_n(cf));
  L->top = L->base + slots;  /* Fix top. */
#if LJ_HASJIT
  {
    jit_State *J = G2J(g);
    if (J->state != LJ_TRACE_IDLE) {
#ifdef LUA_USE_ASSERT
      ptrdiff_t delta = L->top - L->base;
#endif
      J->L = L;
      lj_trace_ins(J, pc-1);  /* The interpreter bytecode PC is offset by 1. */
      lua_assert(L->top - L->base == delta);
    }
  }
#endif
  if ((g->hookmask & LUA_MASKCOUNT) && g->hookcount == 0) {
    g->hookcount = g->hookcstart;
    callhook(L, LUA_HOOKCOUNT, -1);
    L->top = L->base + slots;  /* Fix top again. */
  }
  if ((g->hookmask & LUA_MASKLINE)) {
    BCPos npc = proto_bcpos(pt, pc) - 1;
    BCPos opc = proto_bcpos(pt, oldpc) - 1;
    BCLine line = lj_debug_line(pt, npc);
    if (pc <= oldpc || opc >= pt->sizebc || line != lj_debug_line(pt, opc)) {
      callhook(L, LUA_HOOKLINE, line);
      L->top = L->base + slots;  /* Fix top again. */
    }
  }
  if ((g->hookmask & LUA_MASKRET) && bc_isret(bc_op(pc[-1])))
    callhook(L, LUA_HOOKRET, -1);
  ERRNO_RESTORE
}

/* Initialize call. Ensure stack space and return # of missing parameters. */
static int call_init(lua_State *L, GCfunc *fn)
{
  if (isluafunc(fn)) {
    GCproto *pt = funcproto(fn);
    int numparams = pt->numparams;
    int gotparams = (int)(L->top - L->base);
    int need = pt->framesize;
    if ((pt->flags & PROTO_VARARG)) need += 1+gotparams;
    lj_state_checkstack(L, (MSize)need);
    numparams -= gotparams;
    return numparams >= 0 ? numparams : 0;
  } else {
    lj_state_checkstack(L, LUA_MINSTACK);
    return 0;
  }
}

/* Call dispatch. Used by call hooks, hot calls or when recording. */
ASMFunction LJ_FASTCALL lj_dispatch_call(lua_State *L, const BCIns *pc)
{
  ERRNO_SAVE
  GCfunc *fn = curr_func(L);
  BCOp op;
  global_State *g = G(L);
#if LJ_HASJIT
  jit_State *J = G2J(g);
#endif
  int missing = call_init(L, fn);
#if LJ_HASJIT
  J->L = L;
  if ((uintptr_t)pc & 1) {  /* Marker for hot call. */
#ifdef LUA_USE_ASSERT
    ptrdiff_t delta = L->top - L->base;
#endif
    pc = (const BCIns *)((uintptr_t)pc & ~(uintptr_t)1);
    lj_trace_hot(J, pc);
    lua_assert(L->top - L->base == delta);
    goto out;
  } else if (J->state != LJ_TRACE_IDLE &&
	     !(g->hookmask & (HOOK_GC|HOOK_VMEVENT))) {
#ifdef LUA_USE_ASSERT
    ptrdiff_t delta = L->top - L->base;
#endif
    /* Record the FUNC* bytecodes, too. */
    lj_trace_ins(J, pc-1);  /* The interpreter bytecode PC is offset by 1. */
    lua_assert(L->top - L->base == delta);
  }
#endif
  if ((g->hookmask & LUA_MASKCALL)) {
    int i;
    for (i = 0; i < missing; i++)  /* Add missing parameters. */
      setnilV(L->top++);
    callhook(L, LUA_HOOKCALL, -1);
    /* Preserve modifications of missing parameters by lua_setlocal(). */
    while (missing-- > 0 && tvisnil(L->top - 1))
      L->top--;
  }
#if LJ_HASJIT
out:
#endif
  op = bc_op(pc[-1]);  /* Get FUNC* op. */
#if LJ_HASJIT
  /* Use the non-hotcounting variants if JIT is off or while recording. */
  if ((!(J->flags & JIT_F_ON) || J->state != LJ_TRACE_IDLE) &&
      (op == BC_FUNCF || op == BC_FUNCV))
    op = (BCOp)((int)op+(int)BC_IFUNCF-(int)BC_FUNCF);
#endif
  ERRNO_RESTORE
  return makeasmfunc(lj_bc_ofs[op]);  /* Return static dispatch target. */
}

