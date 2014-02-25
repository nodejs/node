/*
** Fast function call recorder.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_ffrecord_c
#define LUA_CORE

#include "lj_obj.h"

#if LJ_HASJIT

#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_frame.h"
#include "lj_bc.h"
#include "lj_ff.h"
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_trace.h"
#include "lj_record.h"
#include "lj_ffrecord.h"
#include "lj_crecord.h"
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_strscan.h"

/* Some local macros to save typing. Undef'd at the end. */
#define IR(ref)			(&J->cur.ir[(ref)])

/* Pass IR on to next optimization in chain (FOLD). */
#define emitir(ot, a, b)	(lj_ir_set(J, (ot), (a), (b)), lj_opt_fold(J))

/* -- Fast function recording handlers ------------------------------------ */

/* Conventions for fast function call handlers:
**
** The argument slots start at J->base[0]. All of them are guaranteed to be
** valid and type-specialized references. J->base[J->maxslot] is set to 0
** as a sentinel. The runtime argument values start at rd->argv[0].
**
** In general fast functions should check for presence of all of their
** arguments and for the correct argument types. Some simplifications
** are allowed if the interpreter throws instead. But even if recording
** is aborted, the generated IR must be consistent (no zero-refs).
**
** The number of results in rd->nres is set to 1. Handlers that return
** a different number of results need to override it. A negative value
** prevents return processing (e.g. for pending calls).
**
** Results need to be stored starting at J->base[0]. Return processing
** moves them to the right slots later.
**
** The per-ffid auxiliary data is the value of the 2nd part of the
** LJLIB_REC() annotation. This allows handling similar functionality
** in a common handler.
*/

/* Type of handler to record a fast function. */
typedef void (LJ_FASTCALL *RecordFunc)(jit_State *J, RecordFFData *rd);

/* Get runtime value of int argument. */
static int32_t argv2int(jit_State *J, TValue *o)
{
  if (!lj_strscan_numberobj(o))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  return tvisint(o) ? intV(o) : lj_num2int(numV(o));
}

/* Get runtime value of string argument. */
static GCstr *argv2str(jit_State *J, TValue *o)
{
  if (LJ_LIKELY(tvisstr(o))) {
    return strV(o);
  } else {
    GCstr *s;
    if (!tvisnumber(o))
      lj_trace_err(J, LJ_TRERR_BADTYPE);
    if (tvisint(o))
      s = lj_str_fromint(J->L, intV(o));
    else
      s = lj_str_fromnum(J->L, &o->n);
    setstrV(J->L, o, s);
    return s;
  }
}

/* Return number of results wanted by caller. */
static ptrdiff_t results_wanted(jit_State *J)
{
  TValue *frame = J->L->base-1;
  if (frame_islua(frame))
    return (ptrdiff_t)bc_b(frame_pc(frame)[-1]) - 1;
  else
    return -1;
}

/* Throw error for unsupported variant of fast function. */
LJ_NORET static void recff_nyiu(jit_State *J)
{
  setfuncV(J->L, &J->errinfo, J->fn);
  lj_trace_err_info(J, LJ_TRERR_NYIFFU);
}

/* Fallback handler for all fast functions that are not recorded (yet). */
static void LJ_FASTCALL recff_nyi(jit_State *J, RecordFFData *rd)
{
  setfuncV(J->L, &J->errinfo, J->fn);
  lj_trace_err_info(J, LJ_TRERR_NYIFF);
  UNUSED(rd);
}

/* C functions can have arbitrary side-effects and are not recorded (yet). */
static void LJ_FASTCALL recff_c(jit_State *J, RecordFFData *rd)
{
  setfuncV(J->L, &J->errinfo, J->fn);
  lj_trace_err_info(J, LJ_TRERR_NYICF);
  UNUSED(rd);
}

/* -- Base library fast functions ----------------------------------------- */

static void LJ_FASTCALL recff_assert(jit_State *J, RecordFFData *rd)
{
  /* Arguments already specialized. The interpreter throws for nil/false. */
  rd->nres = J->maxslot;  /* Pass through all arguments. */
}

static void LJ_FASTCALL recff_type(jit_State *J, RecordFFData *rd)
{
  /* Arguments already specialized. Result is a constant string. Neat, huh? */
  uint32_t t;
  if (tvisnumber(&rd->argv[0]))
    t = ~LJ_TNUMX;
  else if (LJ_64 && tvislightud(&rd->argv[0]))
    t = ~LJ_TLIGHTUD;
  else
    t = ~itype(&rd->argv[0]);
  J->base[0] = lj_ir_kstr(J, strV(&J->fn->c.upvalue[t]));
  UNUSED(rd);
}

static void LJ_FASTCALL recff_getmetatable(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tr) {
    RecordIndex ix;
    ix.tab = tr;
    copyTV(J->L, &ix.tabv, &rd->argv[0]);
    if (lj_record_mm_lookup(J, &ix, MM_metatable))
      J->base[0] = ix.mobj;
    else
      J->base[0] = ix.mt;
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_setmetatable(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  TRef mt = J->base[1];
  if (tref_istab(tr) && (tref_istab(mt) || (mt && tref_isnil(mt)))) {
    TRef fref, mtref;
    RecordIndex ix;
    ix.tab = tr;
    copyTV(J->L, &ix.tabv, &rd->argv[0]);
    lj_record_mm_lookup(J, &ix, MM_metatable); /* Guard for no __metatable. */
    fref = emitir(IRT(IR_FREF, IRT_P32), tr, IRFL_TAB_META);
    mtref = tref_isnil(mt) ? lj_ir_knull(J, IRT_TAB) : mt;
    emitir(IRT(IR_FSTORE, IRT_TAB), fref, mtref);
    if (!tref_isnil(mt))
      emitir(IRT(IR_TBAR, IRT_TAB), tr, 0);
    J->base[0] = tr;
    J->needsnap = 1;
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_rawget(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0]; ix.key = J->base[1];
  if (tref_istab(ix.tab) && ix.key) {
    ix.val = 0; ix.idxchain = 0;
    settabV(J->L, &ix.tabv, tabV(&rd->argv[0]));
    copyTV(J->L, &ix.keyv, &rd->argv[1]);
    J->base[0] = lj_record_idx(J, &ix);
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_rawset(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0]; ix.key = J->base[1]; ix.val = J->base[2];
  if (tref_istab(ix.tab) && ix.key && ix.val) {
    ix.idxchain = 0;
    settabV(J->L, &ix.tabv, tabV(&rd->argv[0]));
    copyTV(J->L, &ix.keyv, &rd->argv[1]);
    copyTV(J->L, &ix.valv, &rd->argv[2]);
    lj_record_idx(J, &ix);
    /* Pass through table at J->base[0] as result. */
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_rawequal(jit_State *J, RecordFFData *rd)
{
  TRef tra = J->base[0];
  TRef trb = J->base[1];
  if (tra && trb) {
    int diff = lj_record_objcmp(J, tra, trb, &rd->argv[0], &rd->argv[1]);
    J->base[0] = diff ? TREF_FALSE : TREF_TRUE;
  }  /* else: Interpreter will throw. */
}

#if LJ_52
static void LJ_FASTCALL recff_rawlen(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tref_isstr(tr))
    J->base[0] = emitir(IRTI(IR_FLOAD), tr, IRFL_STR_LEN);
  else if (tref_istab(tr))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_len, tr);
  /* else: Interpreter will throw. */
  UNUSED(rd);
}
#endif

/* Determine mode of select() call. */
int32_t lj_ffrecord_select_mode(jit_State *J, TRef tr, TValue *tv)
{
  if (tref_isstr(tr) && *strVdata(tv) == '#') {  /* select('#', ...) */
    if (strV(tv)->len == 1) {
      emitir(IRTG(IR_EQ, IRT_STR), tr, lj_ir_kstr(J, strV(tv)));
    } else {
      TRef trptr = emitir(IRT(IR_STRREF, IRT_P32), tr, lj_ir_kint(J, 0));
      TRef trchar = emitir(IRT(IR_XLOAD, IRT_U8), trptr, IRXLOAD_READONLY);
      emitir(IRTG(IR_EQ, IRT_INT), trchar, lj_ir_kint(J, '#'));
    }
    return 0;
  } else {  /* select(n, ...) */
    int32_t start = argv2int(J, tv);
    if (start == 0) lj_trace_err(J, LJ_TRERR_BADTYPE);  /* A bit misleading. */
    return start;
  }
}

static void LJ_FASTCALL recff_select(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tr) {
    ptrdiff_t start = lj_ffrecord_select_mode(J, tr, &rd->argv[0]);
    if (start == 0) {  /* select('#', ...) */
      J->base[0] = lj_ir_kint(J, J->maxslot - 1);
    } else if (tref_isk(tr)) {  /* select(k, ...) */
      ptrdiff_t n = (ptrdiff_t)J->maxslot;
      if (start < 0) start += n;
      else if (start > n) start = n;
      rd->nres = n - start;
      if (start >= 1) {
	ptrdiff_t i;
	for (i = 0; i < n - start; i++)
	  J->base[i] = J->base[start+i];
      }  /* else: Interpreter will throw. */
    } else {
      recff_nyiu(J);
    }
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_tonumber(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  TRef base = J->base[1];
  if (tr && !tref_isnil(base)) {
    base = lj_opt_narrow_toint(J, base);
    if (!tref_isk(base) || IR(tref_ref(base))->i != 10)
      recff_nyiu(J);
  }
  if (tref_isnumber_str(tr)) {
    if (tref_isstr(tr)) {
      TValue tmp;
      if (!lj_strscan_num(strV(&rd->argv[0]), &tmp))
	recff_nyiu(J);  /* Would need an inverted STRTO for this case. */
      tr = emitir(IRTG(IR_STRTO, IRT_NUM), tr, 0);
    }
#if LJ_HASFFI
  } else if (tref_iscdata(tr)) {
    lj_crecord_tonumber(J, rd);
    return;
#endif
  } else {
    tr = TREF_NIL;
  }
  J->base[0] = tr;
  UNUSED(rd);
}

static TValue *recff_metacall_cp(lua_State *L, lua_CFunction dummy, void *ud)
{
  jit_State *J = (jit_State *)ud;
  lj_record_tailcall(J, 0, 1);
  UNUSED(L); UNUSED(dummy);
  return NULL;
}

static int recff_metacall(jit_State *J, RecordFFData *rd, MMS mm)
{
  RecordIndex ix;
  ix.tab = J->base[0];
  copyTV(J->L, &ix.tabv, &rd->argv[0]);
  if (lj_record_mm_lookup(J, &ix, mm)) {  /* Has metamethod? */
    int errcode;
    TValue argv0;
    /* Temporarily insert metamethod below object. */
    J->base[1] = J->base[0];
    J->base[0] = ix.mobj;
    copyTV(J->L, &argv0, &rd->argv[0]);
    copyTV(J->L, &rd->argv[1], &rd->argv[0]);
    copyTV(J->L, &rd->argv[0], &ix.mobjv);
    /* Need to protect lj_record_tailcall because it may throw. */
    errcode = lj_vm_cpcall(J->L, NULL, J, recff_metacall_cp);
    /* Always undo Lua stack changes to avoid confusing the interpreter. */
    copyTV(J->L, &rd->argv[0], &argv0);
    if (errcode)
      lj_err_throw(J->L, errcode);  /* Propagate errors. */
    rd->nres = -1;  /* Pending call. */
    return 1;  /* Tailcalled to metamethod. */
  }
  return 0;
}

static void LJ_FASTCALL recff_tostring(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tref_isstr(tr)) {
    /* Ignore __tostring in the string base metatable. */
    /* Pass on result in J->base[0]. */
  } else if (!recff_metacall(J, rd, MM_tostring)) {
    if (tref_isnumber(tr)) {
      J->base[0] = emitir(IRT(IR_TOSTR, IRT_STR), tr, 0);
    } else if (tref_ispri(tr)) {
      J->base[0] = lj_ir_kstr(J, strV(&J->fn->c.upvalue[tref_type(tr)]));
    } else {
      recff_nyiu(J);
    }
  }
}

static void LJ_FASTCALL recff_ipairs_aux(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0];
  if (tref_istab(ix.tab)) {
    if (!tvisnumber(&rd->argv[1]))  /* No support for string coercion. */
      lj_trace_err(J, LJ_TRERR_BADTYPE);
    setintV(&ix.keyv, numberVint(&rd->argv[1])+1);
    settabV(J->L, &ix.tabv, tabV(&rd->argv[0]));
    ix.val = 0; ix.idxchain = 0;
    ix.key = lj_opt_narrow_toint(J, J->base[1]);
    J->base[0] = ix.key = emitir(IRTI(IR_ADD), ix.key, lj_ir_kint(J, 1));
    J->base[1] = lj_record_idx(J, &ix);
    rd->nres = tref_isnil(J->base[1]) ? 0 : 2;
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_ipairs(jit_State *J, RecordFFData *rd)
{
  if (!(LJ_52 && recff_metacall(J, rd, MM_ipairs))) {
    TRef tab = J->base[0];
    if (tref_istab(tab)) {
      J->base[0] = lj_ir_kfunc(J, funcV(&J->fn->c.upvalue[0]));
      J->base[1] = tab;
      J->base[2] = lj_ir_kint(J, 0);
      rd->nres = 3;
    }  /* else: Interpreter will throw. */
  }
}

static void LJ_FASTCALL recff_pcall(jit_State *J, RecordFFData *rd)
{
  if (J->maxslot >= 1) {
    lj_record_call(J, 0, J->maxslot - 1);
    rd->nres = -1;  /* Pending call. */
  }  /* else: Interpreter will throw. */
}

static TValue *recff_xpcall_cp(lua_State *L, lua_CFunction dummy, void *ud)
{
  jit_State *J = (jit_State *)ud;
  lj_record_call(J, 1, J->maxslot - 2);
  UNUSED(L); UNUSED(dummy);
  return NULL;
}

static void LJ_FASTCALL recff_xpcall(jit_State *J, RecordFFData *rd)
{
  if (J->maxslot >= 2) {
    TValue argv0, argv1;
    TRef tmp;
    int errcode;
    /* Swap function and traceback. */
    tmp = J->base[0]; J->base[0] = J->base[1]; J->base[1] = tmp;
    copyTV(J->L, &argv0, &rd->argv[0]);
    copyTV(J->L, &argv1, &rd->argv[1]);
    copyTV(J->L, &rd->argv[0], &argv1);
    copyTV(J->L, &rd->argv[1], &argv0);
    /* Need to protect lj_record_call because it may throw. */
    errcode = lj_vm_cpcall(J->L, NULL, J, recff_xpcall_cp);
    /* Always undo Lua stack swap to avoid confusing the interpreter. */
    copyTV(J->L, &rd->argv[0], &argv0);
    copyTV(J->L, &rd->argv[1], &argv1);
    if (errcode)
      lj_err_throw(J->L, errcode);  /* Propagate errors. */
    rd->nres = -1;  /* Pending call. */
  }  /* else: Interpreter will throw. */
}

/* -- Math library fast functions ----------------------------------------- */

static void LJ_FASTCALL recff_math_abs(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  J->base[0] = emitir(IRTN(IR_ABS), tr, lj_ir_knum_abs(J));
  UNUSED(rd);
}

/* Record rounding functions math.floor and math.ceil. */
static void LJ_FASTCALL recff_math_round(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (!tref_isinteger(tr)) {  /* Pass through integers unmodified. */
    tr = emitir(IRTN(IR_FPMATH), lj_ir_tonum(J, tr), rd->data);
    /* Result is integral (or NaN/Inf), but may not fit an int32_t. */
    if (LJ_DUALNUM) {  /* Try to narrow using a guarded conversion to int. */
      lua_Number n = lj_vm_foldfpm(numberVnum(&rd->argv[0]), rd->data);
      if (n == (lua_Number)lj_num2int(n))
	tr = emitir(IRTGI(IR_CONV), tr, IRCONV_INT_NUM|IRCONV_CHECK);
    }
    J->base[0] = tr;
  }
}

/* Record unary math.* functions, mapped to IR_FPMATH opcode. */
static void LJ_FASTCALL recff_math_unary(jit_State *J, RecordFFData *rd)
{
  J->base[0] = emitir(IRTN(IR_FPMATH), lj_ir_tonum(J, J->base[0]), rd->data);
}

/* Record math.log. */
static void LJ_FASTCALL recff_math_log(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  if (J->base[1]) {
#ifdef LUAJIT_NO_LOG2
    uint32_t fpm = IRFPM_LOG;
#else
    uint32_t fpm = IRFPM_LOG2;
#endif
    TRef trb = lj_ir_tonum(J, J->base[1]);
    tr = emitir(IRTN(IR_FPMATH), tr, fpm);
    trb = emitir(IRTN(IR_FPMATH), trb, fpm);
    trb = emitir(IRTN(IR_DIV), lj_ir_knum_one(J), trb);
    tr = emitir(IRTN(IR_MUL), tr, trb);
  } else {
    tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_LOG);
  }
  J->base[0] = tr;
  UNUSED(rd);
}

/* Record math.atan2. */
static void LJ_FASTCALL recff_math_atan2(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef tr2 = lj_ir_tonum(J, J->base[1]);
  J->base[0] = emitir(IRTN(IR_ATAN2), tr, tr2);
  UNUSED(rd);
}

/* Record math.ldexp. */
static void LJ_FASTCALL recff_math_ldexp(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
#if LJ_TARGET_X86ORX64
  TRef tr2 = lj_ir_tonum(J, J->base[1]);
#else
  TRef tr2 = lj_opt_narrow_toint(J, J->base[1]);
#endif
  J->base[0] = emitir(IRTN(IR_LDEXP), tr, tr2);
  UNUSED(rd);
}

/* Record math.asin, math.acos, math.atan. */
static void LJ_FASTCALL recff_math_atrig(jit_State *J, RecordFFData *rd)
{
  TRef y = lj_ir_tonum(J, J->base[0]);
  TRef x = lj_ir_knum_one(J);
  uint32_t ffid = rd->data;
  if (ffid != FF_math_atan) {
    TRef tmp = emitir(IRTN(IR_MUL), y, y);
    tmp = emitir(IRTN(IR_SUB), x, tmp);
    tmp = emitir(IRTN(IR_FPMATH), tmp, IRFPM_SQRT);
    if (ffid == FF_math_asin) { x = tmp; } else { x = y; y = tmp; }
  }
  J->base[0] = emitir(IRTN(IR_ATAN2), y, x);
}

static void LJ_FASTCALL recff_math_htrig(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  J->base[0] = emitir(IRTN(IR_CALLN), tr, rd->data);
}

static void LJ_FASTCALL recff_math_modf(jit_State *J, RecordFFData *rd)
{
  TRef tr = J->base[0];
  if (tref_isinteger(tr)) {
    J->base[0] = tr;
    J->base[1] = lj_ir_kint(J, 0);
  } else {
    TRef trt;
    tr = lj_ir_tonum(J, tr);
    trt = emitir(IRTN(IR_FPMATH), tr, IRFPM_TRUNC);
    J->base[0] = trt;
    J->base[1] = emitir(IRTN(IR_SUB), tr, trt);
  }
  rd->nres = 2;
}

static void LJ_FASTCALL recff_math_degrad(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  TRef trm = lj_ir_knum(J, numV(&J->fn->c.upvalue[0]));
  J->base[0] = emitir(IRTN(IR_MUL), tr, trm);
  UNUSED(rd);
}

static void LJ_FASTCALL recff_math_pow(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonum(J, J->base[0]);
  if (!tref_isnumber_str(J->base[1]))
    lj_trace_err(J, LJ_TRERR_BADTYPE);
  J->base[0] = lj_opt_narrow_pow(J, tr, J->base[1], &rd->argv[1]);
  UNUSED(rd);
}

static void LJ_FASTCALL recff_math_minmax(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_ir_tonumber(J, J->base[0]);
  uint32_t op = rd->data;
  BCReg i;
  for (i = 1; J->base[i] != 0; i++) {
    TRef tr2 = lj_ir_tonumber(J, J->base[i]);
    IRType t = IRT_INT;
    if (!(tref_isinteger(tr) && tref_isinteger(tr2))) {
      if (tref_isinteger(tr)) tr = emitir(IRTN(IR_CONV), tr, IRCONV_NUM_INT);
      if (tref_isinteger(tr2)) tr2 = emitir(IRTN(IR_CONV), tr2, IRCONV_NUM_INT);
      t = IRT_NUM;
    }
    tr = emitir(IRT(op, t), tr, tr2);
  }
  J->base[0] = tr;
}

static void LJ_FASTCALL recff_math_random(jit_State *J, RecordFFData *rd)
{
  GCudata *ud = udataV(&J->fn->c.upvalue[0]);
  TRef tr, one;
  lj_ir_kgc(J, obj2gco(ud), IRT_UDATA);  /* Prevent collection. */
  tr = lj_ir_call(J, IRCALL_lj_math_random_step, lj_ir_kptr(J, uddata(ud)));
  one = lj_ir_knum_one(J);
  tr = emitir(IRTN(IR_SUB), tr, one);
  if (J->base[0]) {
    TRef tr1 = lj_ir_tonum(J, J->base[0]);
    if (J->base[1]) {  /* d = floor(d*(r2-r1+1.0)) + r1 */
      TRef tr2 = lj_ir_tonum(J, J->base[1]);
      tr2 = emitir(IRTN(IR_SUB), tr2, tr1);
      tr2 = emitir(IRTN(IR_ADD), tr2, one);
      tr = emitir(IRTN(IR_MUL), tr, tr2);
      tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_FLOOR);
      tr = emitir(IRTN(IR_ADD), tr, tr1);
    } else {  /* d = floor(d*r1) + 1.0 */
      tr = emitir(IRTN(IR_MUL), tr, tr1);
      tr = emitir(IRTN(IR_FPMATH), tr, IRFPM_FLOOR);
      tr = emitir(IRTN(IR_ADD), tr, one);
    }
  }
  J->base[0] = tr;
  UNUSED(rd);
}

/* -- Bit library fast functions ------------------------------------------ */

/* Record unary bit.tobit, bit.bnot, bit.bswap. */
static void LJ_FASTCALL recff_bit_unary(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_opt_narrow_tobit(J, J->base[0]);
  J->base[0] = (rd->data == IR_TOBIT) ? tr : emitir(IRTI(rd->data), tr, 0);
}

/* Record N-ary bit.band, bit.bor, bit.bxor. */
static void LJ_FASTCALL recff_bit_nary(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_opt_narrow_tobit(J, J->base[0]);
  uint32_t op = rd->data;
  BCReg i;
  for (i = 1; J->base[i] != 0; i++)
    tr = emitir(IRTI(op), tr, lj_opt_narrow_tobit(J, J->base[i]));
  J->base[0] = tr;
}

/* Record bit shifts. */
static void LJ_FASTCALL recff_bit_shift(jit_State *J, RecordFFData *rd)
{
  TRef tr = lj_opt_narrow_tobit(J, J->base[0]);
  TRef tsh = lj_opt_narrow_tobit(J, J->base[1]);
  IROp op = (IROp)rd->data;
  if (!(op < IR_BROL ? LJ_TARGET_MASKSHIFT : LJ_TARGET_MASKROT) &&
      !tref_isk(tsh))
    tsh = emitir(IRTI(IR_BAND), tsh, lj_ir_kint(J, 31));
#ifdef LJ_TARGET_UNIFYROT
  if (op == (LJ_TARGET_UNIFYROT == 1 ? IR_BROR : IR_BROL)) {
    op = LJ_TARGET_UNIFYROT == 1 ? IR_BROL : IR_BROR;
    tsh = emitir(IRTI(IR_NEG), tsh, tsh);
  }
#endif
  J->base[0] = emitir(IRTI(op), tr, tsh);
}

/* -- String library fast functions --------------------------------------- */

static void LJ_FASTCALL recff_string_len(jit_State *J, RecordFFData *rd)
{
  J->base[0] = emitir(IRTI(IR_FLOAD), lj_ir_tostr(J, J->base[0]), IRFL_STR_LEN);
  UNUSED(rd);
}

/* Handle string.byte (rd->data = 0) and string.sub (rd->data = 1). */
static void LJ_FASTCALL recff_string_range(jit_State *J, RecordFFData *rd)
{
  TRef trstr = lj_ir_tostr(J, J->base[0]);
  TRef trlen = emitir(IRTI(IR_FLOAD), trstr, IRFL_STR_LEN);
  TRef tr0 = lj_ir_kint(J, 0);
  TRef trstart, trend;
  GCstr *str = argv2str(J, &rd->argv[0]);
  int32_t start, end;
  if (rd->data) {  /* string.sub(str, start [,end]) */
    start = argv2int(J, &rd->argv[1]);
    trstart = lj_opt_narrow_toint(J, J->base[1]);
    trend = J->base[2];
    if (tref_isnil(trend)) {
      trend = lj_ir_kint(J, -1);
      end = -1;
    } else {
      trend = lj_opt_narrow_toint(J, trend);
      end = argv2int(J, &rd->argv[2]);
    }
  } else {  /* string.byte(str, [,start [,end]]) */
    if (!tref_isnil(J->base[1])) {
      start = argv2int(J, &rd->argv[1]);
      trstart = lj_opt_narrow_toint(J, J->base[1]);
      trend = J->base[2];
      if (tref_isnil(trend)) {
	trend = trstart;
	end = start;
      } else {
	trend = lj_opt_narrow_toint(J, trend);
	end = argv2int(J, &rd->argv[2]);
      }
    } else {
      trend = trstart = lj_ir_kint(J, 1);
      end = start = 1;
    }
  }
  if (end < 0) {
    emitir(IRTGI(IR_LT), trend, tr0);
    trend = emitir(IRTI(IR_ADD), emitir(IRTI(IR_ADD), trlen, trend),
		   lj_ir_kint(J, 1));
    end = end+(int32_t)str->len+1;
  } else if ((MSize)end <= str->len) {
    emitir(IRTGI(IR_ULE), trend, trlen);
  } else {
    emitir(IRTGI(IR_GT), trend, trlen);
    end = (int32_t)str->len;
    trend = trlen;
  }
  if (start < 0) {
    emitir(IRTGI(IR_LT), trstart, tr0);
    trstart = emitir(IRTI(IR_ADD), trlen, trstart);
    start = start+(int32_t)str->len;
    emitir(start < 0 ? IRTGI(IR_LT) : IRTGI(IR_GE), trstart, tr0);
    if (start < 0) {
      trstart = tr0;
      start = 0;
    }
  } else {
    if (start == 0) {
      emitir(IRTGI(IR_EQ), trstart, tr0);
      trstart = tr0;
    } else {
      trstart = emitir(IRTI(IR_ADD), trstart, lj_ir_kint(J, -1));
      emitir(IRTGI(IR_GE), trstart, tr0);
      start--;
    }
  }
  if (rd->data) {  /* Return string.sub result. */
    if (end - start >= 0) {
      /* Also handle empty range here, to avoid extra traces. */
      TRef trptr, trslen = emitir(IRTI(IR_SUB), trend, trstart);
      emitir(IRTGI(IR_GE), trslen, tr0);
      trptr = emitir(IRT(IR_STRREF, IRT_P32), trstr, trstart);
      J->base[0] = emitir(IRT(IR_SNEW, IRT_STR), trptr, trslen);
    } else {  /* Range underflow: return empty string. */
      emitir(IRTGI(IR_LT), trend, trstart);
      J->base[0] = lj_ir_kstr(J, lj_str_new(J->L, strdata(str), 0));
    }
  } else {  /* Return string.byte result(s). */
    ptrdiff_t i, len = end - start;
    if (len > 0) {
      TRef trslen = emitir(IRTI(IR_SUB), trend, trstart);
      emitir(IRTGI(IR_EQ), trslen, lj_ir_kint(J, (int32_t)len));
      if (J->baseslot + len > LJ_MAX_JSLOTS)
	lj_trace_err_info(J, LJ_TRERR_STACKOV);
      rd->nres = len;
      for (i = 0; i < len; i++) {
	TRef tmp = emitir(IRTI(IR_ADD), trstart, lj_ir_kint(J, (int32_t)i));
	tmp = emitir(IRT(IR_STRREF, IRT_P32), trstr, tmp);
	J->base[i] = emitir(IRT(IR_XLOAD, IRT_U8), tmp, IRXLOAD_READONLY);
      }
    } else {  /* Empty range or range underflow: return no results. */
      emitir(IRTGI(IR_LE), trend, trstart);
      rd->nres = 0;
    }
  }
}

/* -- Table library fast functions ---------------------------------------- */

static void LJ_FASTCALL recff_table_getn(jit_State *J, RecordFFData *rd)
{
  if (tref_istab(J->base[0]))
    J->base[0] = lj_ir_call(J, IRCALL_lj_tab_len, J->base[0]);
  /* else: Interpreter will throw. */
  UNUSED(rd);
}

static void LJ_FASTCALL recff_table_remove(jit_State *J, RecordFFData *rd)
{
  TRef tab = J->base[0];
  rd->nres = 0;
  if (tref_istab(tab)) {
    if (tref_isnil(J->base[1])) {  /* Simple pop: t[#t] = nil */
      TRef trlen = lj_ir_call(J, IRCALL_lj_tab_len, tab);
      GCtab *t = tabV(&rd->argv[0]);
      MSize len = lj_tab_len(t);
      emitir(IRTGI(len ? IR_NE : IR_EQ), trlen, lj_ir_kint(J, 0));
      if (len) {
	RecordIndex ix;
	ix.tab = tab;
	ix.key = trlen;
	settabV(J->L, &ix.tabv, t);
	setintV(&ix.keyv, len);
	ix.idxchain = 0;
	if (results_wanted(J) != 0) {  /* Specialize load only if needed. */
	  ix.val = 0;
	  J->base[0] = lj_record_idx(J, &ix);  /* Load previous value. */
	  rd->nres = 1;
	  /* Assumes ix.key/ix.tab is not modified for raw lj_record_idx(). */
	}
	ix.val = TREF_NIL;
	lj_record_idx(J, &ix);  /* Remove value. */
      }
    } else {  /* Complex case: remove in the middle. */
      recff_nyiu(J);
    }
  }  /* else: Interpreter will throw. */
}

static void LJ_FASTCALL recff_table_insert(jit_State *J, RecordFFData *rd)
{
  RecordIndex ix;
  ix.tab = J->base[0];
  ix.val = J->base[1];
  rd->nres = 0;
  if (tref_istab(ix.tab) && ix.val) {
    if (!J->base[2]) {  /* Simple push: t[#t+1] = v */
      TRef trlen = lj_ir_call(J, IRCALL_lj_tab_len, ix.tab);
      GCtab *t = tabV(&rd->argv[0]);
      ix.key = emitir(IRTI(IR_ADD), trlen, lj_ir_kint(J, 1));
      settabV(J->L, &ix.tabv, t);
      setintV(&ix.keyv, lj_tab_len(t) + 1);
      ix.idxchain = 0;
      lj_record_idx(J, &ix);  /* Set new value. */
    } else {  /* Complex case: insert in the middle. */
      recff_nyiu(J);
    }
  }  /* else: Interpreter will throw. */
}

/* -- I/O library fast functions ------------------------------------------ */

/* Get FILE* for I/O function. Any I/O error aborts recording, so there's
** no need to encode the alternate cases for any of the guards.
*/
static TRef recff_io_fp(jit_State *J, TRef *udp, int32_t id)
{
  TRef tr, ud, fp;
  if (id) {  /* io.func() */
    tr = lj_ir_kptr(J, &J2G(J)->gcroot[id]);
    ud = emitir(IRT(IR_XLOAD, IRT_UDATA), tr, 0);
  } else {  /* fp:method() */
    ud = J->base[0];
    if (!tref_isudata(ud))
      lj_trace_err(J, LJ_TRERR_BADTYPE);
    tr = emitir(IRT(IR_FLOAD, IRT_U8), ud, IRFL_UDATA_UDTYPE);
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, UDTYPE_IO_FILE));
  }
  *udp = ud;
  fp = emitir(IRT(IR_FLOAD, IRT_PTR), ud, IRFL_UDATA_FILE);
  emitir(IRTG(IR_NE, IRT_PTR), fp, lj_ir_knull(J, IRT_PTR));
  return fp;
}

static void LJ_FASTCALL recff_io_write(jit_State *J, RecordFFData *rd)
{
  TRef ud, fp = recff_io_fp(J, &ud, rd->data);
  TRef zero = lj_ir_kint(J, 0);
  TRef one = lj_ir_kint(J, 1);
  ptrdiff_t i = rd->data == 0 ? 1 : 0;
  for (; J->base[i]; i++) {
    TRef str = lj_ir_tostr(J, J->base[i]);
    TRef buf = emitir(IRT(IR_STRREF, IRT_P32), str, zero);
    TRef len = emitir(IRTI(IR_FLOAD), str, IRFL_STR_LEN);
    if (tref_isk(len) && IR(tref_ref(len))->i == 1) {
      TRef tr = emitir(IRT(IR_XLOAD, IRT_U8), buf, IRXLOAD_READONLY);
      tr = lj_ir_call(J, IRCALL_fputc, tr, fp);
      if (results_wanted(J) != 0)  /* Check result only if not ignored. */
	emitir(IRTGI(IR_NE), tr, lj_ir_kint(J, -1));
    } else {
      TRef tr = lj_ir_call(J, IRCALL_fwrite, buf, one, len, fp);
      if (results_wanted(J) != 0)  /* Check result only if not ignored. */
	emitir(IRTGI(IR_EQ), tr, len);
    }
  }
  J->base[0] = LJ_52 ? ud : TREF_TRUE;
}

static void LJ_FASTCALL recff_io_flush(jit_State *J, RecordFFData *rd)
{
  TRef ud, fp = recff_io_fp(J, &ud, rd->data);
  TRef tr = lj_ir_call(J, IRCALL_fflush, fp);
  if (results_wanted(J) != 0)  /* Check result only if not ignored. */
    emitir(IRTGI(IR_EQ), tr, lj_ir_kint(J, 0));
  J->base[0] = TREF_TRUE;
}

/* -- Record calls to fast functions -------------------------------------- */

#include "lj_recdef.h"

static uint32_t recdef_lookup(GCfunc *fn)
{
  if (fn->c.ffid < sizeof(recff_idmap)/sizeof(recff_idmap[0]))
    return recff_idmap[fn->c.ffid];
  else
    return 0;
}

/* Record entry to a fast function or C function. */
void lj_ffrecord_func(jit_State *J)
{
  RecordFFData rd;
  uint32_t m = recdef_lookup(J->fn);
  rd.data = m & 0xff;
  rd.nres = 1;  /* Default is one result. */
  rd.argv = J->L->base;
  J->base[J->maxslot] = 0;  /* Mark end of arguments. */
  (recff_func[m >> 8])(J, &rd);  /* Call recff_* handler. */
  if (rd.nres >= 0) {
    if (J->postproc == LJ_POST_NONE) J->postproc = LJ_POST_FFRETRY;
    lj_record_ret(J, 0, rd.nres);
  }
}

#undef IR
#undef emitir

#endif
