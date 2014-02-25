/*
** JIT library.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lib_jit_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_arch.h"
#include "lj_obj.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_bc.h"
#if LJ_HASJIT
#include "lj_ir.h"
#include "lj_jit.h"
#include "lj_ircall.h"
#include "lj_iropt.h"
#include "lj_target.h"
#endif
#include "lj_dispatch.h"
#include "lj_vm.h"
#include "lj_vmevent.h"
#include "lj_lib.h"

#include "luajit.h"

/* -- jit.* functions ----------------------------------------------------- */

#define LJLIB_MODULE_jit

static int setjitmode(lua_State *L, int mode)
{
  int idx = 0;
  if (L->base == L->top || tvisnil(L->base)) {  /* jit.on/off/flush([nil]) */
    mode |= LUAJIT_MODE_ENGINE;
  } else {
    /* jit.on/off/flush(func|proto, nil|true|false) */
    if (tvisfunc(L->base) || tvisproto(L->base))
      idx = 1;
    else if (!tvistrue(L->base))  /* jit.on/off/flush(true, nil|true|false) */
      goto err;
    if (L->base+1 < L->top && tvisbool(L->base+1))
      mode |= boolV(L->base+1) ? LUAJIT_MODE_ALLFUNC : LUAJIT_MODE_ALLSUBFUNC;
    else
      mode |= LUAJIT_MODE_FUNC;
  }
  if (luaJIT_setmode(L, idx, mode) != 1) {
    if ((mode & LUAJIT_MODE_MASK) == LUAJIT_MODE_ENGINE)
      lj_err_caller(L, LJ_ERR_NOJIT);
  err:
    lj_err_argt(L, 1, LUA_TFUNCTION);
  }
  return 0;
}

LJLIB_CF(jit_on)
{
  return setjitmode(L, LUAJIT_MODE_ON);
}

LJLIB_CF(jit_off)
{
  return setjitmode(L, LUAJIT_MODE_OFF);
}

LJLIB_CF(jit_flush)
{
#if LJ_HASJIT
  if (L->base < L->top && !tvisnil(L->base)) {
    int traceno = lj_lib_checkint(L, 1);
    luaJIT_setmode(L, traceno, LUAJIT_MODE_FLUSH|LUAJIT_MODE_TRACE);
    return 0;
  }
#endif
  return setjitmode(L, LUAJIT_MODE_FLUSH);
}

#if LJ_HASJIT
/* Push a string for every flag bit that is set. */
static void flagbits_to_strings(lua_State *L, uint32_t flags, uint32_t base,
				const char *str)
{
  for (; *str; base <<= 1, str += 1+*str)
    if (flags & base)
      setstrV(L, L->top++, lj_str_new(L, str+1, *(uint8_t *)str));
}
#endif

LJLIB_CF(jit_status)
{
#if LJ_HASJIT
  jit_State *J = L2J(L);
  L->top = L->base;
  setboolV(L->top++, (J->flags & JIT_F_ON) ? 1 : 0);
  flagbits_to_strings(L, J->flags, JIT_F_CPU_FIRST, JIT_F_CPUSTRING);
  flagbits_to_strings(L, J->flags, JIT_F_OPT_FIRST, JIT_F_OPTSTRING);
  return (int)(L->top - L->base);
#else
  setboolV(L->top++, 0);
  return 1;
#endif
}

LJLIB_CF(jit_attach)
{
#ifdef LUAJIT_DISABLE_VMEVENT
  luaL_error(L, "vmevent API disabled");
#else
  GCfunc *fn = lj_lib_checkfunc(L, 1);
  GCstr *s = lj_lib_optstr(L, 2);
  luaL_findtable(L, LUA_REGISTRYINDEX, LJ_VMEVENTS_REGKEY, LJ_VMEVENTS_HSIZE);
  if (s) {  /* Attach to given event. */
    const uint8_t *p = (const uint8_t *)strdata(s);
    uint32_t h = s->len;
    while (*p) h = h ^ (lj_rol(h, 6) + *p++);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, VMEVENT_HASHIDX(h));
    G(L)->vmevmask = VMEVENT_NOCACHE;  /* Invalidate cache. */
  } else {  /* Detach if no event given. */
    setnilV(L->top++);
    while (lua_next(L, -2)) {
      L->top--;
      if (tvisfunc(L->top) && funcV(L->top) == fn) {
	setnilV(lj_tab_set(L, tabV(L->top-2), L->top-1));
      }
    }
  }
#endif
  return 0;
}

LJLIB_PUSH(top-5) LJLIB_SET(os)
LJLIB_PUSH(top-4) LJLIB_SET(arch)
LJLIB_PUSH(top-3) LJLIB_SET(version_num)
LJLIB_PUSH(top-2) LJLIB_SET(version)

#include "lj_libdef.h"

/* -- jit.util.* functions ------------------------------------------------ */

#define LJLIB_MODULE_jit_util

/* -- Reflection API for Lua functions ------------------------------------ */

/* Return prototype of first argument (Lua function or prototype object) */
static GCproto *check_Lproto(lua_State *L, int nolua)
{
  TValue *o = L->base;
  if (L->top > o) {
    if (tvisproto(o)) {
      return protoV(o);
    } else if (tvisfunc(o)) {
      if (isluafunc(funcV(o)))
	return funcproto(funcV(o));
      else if (nolua)
	return NULL;
    }
  }
  lj_err_argt(L, 1, LUA_TFUNCTION);
  return NULL;  /* unreachable */
}

static void setintfield(lua_State *L, GCtab *t, const char *name, int32_t val)
{
  setintV(lj_tab_setstr(L, t, lj_str_newz(L, name)), val);
}

/* local info = jit.util.funcinfo(func [,pc]) */
LJLIB_CF(jit_util_funcinfo)
{
  GCproto *pt = check_Lproto(L, 1);
  if (pt) {
    BCPos pc = (BCPos)lj_lib_optint(L, 2, 0);
    GCtab *t;
    lua_createtable(L, 0, 16);  /* Increment hash size if fields are added. */
    t = tabV(L->top-1);
    setintfield(L, t, "linedefined", pt->firstline);
    setintfield(L, t, "lastlinedefined", pt->firstline + pt->numline);
    setintfield(L, t, "stackslots", pt->framesize);
    setintfield(L, t, "params", pt->numparams);
    setintfield(L, t, "bytecodes", (int32_t)pt->sizebc);
    setintfield(L, t, "gcconsts", (int32_t)pt->sizekgc);
    setintfield(L, t, "nconsts", (int32_t)pt->sizekn);
    setintfield(L, t, "upvalues", (int32_t)pt->sizeuv);
    if (pc < pt->sizebc)
      setintfield(L, t, "currentline", lj_debug_line(pt, pc));
    lua_pushboolean(L, (pt->flags & PROTO_VARARG));
    lua_setfield(L, -2, "isvararg");
    lua_pushboolean(L, (pt->flags & PROTO_CHILD));
    lua_setfield(L, -2, "children");
    setstrV(L, L->top++, proto_chunkname(pt));
    lua_setfield(L, -2, "source");
    lj_debug_pushloc(L, pt, pc);
    lua_setfield(L, -2, "loc");
  } else {
    GCfunc *fn = funcV(L->base);
    GCtab *t;
    lua_createtable(L, 0, 4);  /* Increment hash size if fields are added. */
    t = tabV(L->top-1);
    if (!iscfunc(fn))
      setintfield(L, t, "ffid", fn->c.ffid);
    setintptrV(lj_tab_setstr(L, t, lj_str_newlit(L, "addr")),
	       (intptr_t)(void *)fn->c.f);
    setintfield(L, t, "upvalues", fn->c.nupvalues);
  }
  return 1;
}

/* local ins, m = jit.util.funcbc(func, pc) */
LJLIB_CF(jit_util_funcbc)
{
  GCproto *pt = check_Lproto(L, 0);
  BCPos pc = (BCPos)lj_lib_checkint(L, 2);
  if (pc < pt->sizebc) {
    BCIns ins = proto_bc(pt)[pc];
    BCOp op = bc_op(ins);
    lua_assert(op < BC__MAX);
    setintV(L->top, ins);
    setintV(L->top+1, lj_bc_mode[op]);
    L->top += 2;
    return 2;
  }
  return 0;
}

/* local k = jit.util.funck(func, idx) */
LJLIB_CF(jit_util_funck)
{
  GCproto *pt = check_Lproto(L, 0);
  ptrdiff_t idx = (ptrdiff_t)lj_lib_checkint(L, 2);
  if (idx >= 0) {
    if (idx < (ptrdiff_t)pt->sizekn) {
      copyTV(L, L->top-1, proto_knumtv(pt, idx));
      return 1;
    }
  } else {
    if (~idx < (ptrdiff_t)pt->sizekgc) {
      GCobj *gc = proto_kgc(pt, idx);
      setgcV(L, L->top-1, gc, ~gc->gch.gct);
      return 1;
    }
  }
  return 0;
}

/* local name = jit.util.funcuvname(func, idx) */
LJLIB_CF(jit_util_funcuvname)
{
  GCproto *pt = check_Lproto(L, 0);
  uint32_t idx = (uint32_t)lj_lib_checkint(L, 2);
  if (idx < pt->sizeuv) {
    setstrV(L, L->top-1, lj_str_newz(L, lj_debug_uvname(pt, idx)));
    return 1;
  }
  return 0;
}

/* -- Reflection API for traces ------------------------------------------- */

#if LJ_HASJIT

/* Check trace argument. Must not throw for non-existent trace numbers. */
static GCtrace *jit_checktrace(lua_State *L)
{
  TraceNo tr = (TraceNo)lj_lib_checkint(L, 1);
  jit_State *J = L2J(L);
  if (tr > 0 && tr < J->sizetrace)
    return traceref(J, tr);
  return NULL;
}

/* Names of link types. ORDER LJ_TRLINK */
static const char *const jit_trlinkname[] = {
  "none", "root", "loop", "tail-recursion", "up-recursion", "down-recursion",
  "interpreter", "return"
};

/* local info = jit.util.traceinfo(tr) */
LJLIB_CF(jit_util_traceinfo)
{
  GCtrace *T = jit_checktrace(L);
  if (T) {
    GCtab *t;
    lua_createtable(L, 0, 8);  /* Increment hash size if fields are added. */
    t = tabV(L->top-1);
    setintfield(L, t, "nins", (int32_t)T->nins - REF_BIAS - 1);
    setintfield(L, t, "nk", REF_BIAS - (int32_t)T->nk);
    setintfield(L, t, "link", T->link);
    setintfield(L, t, "nexit", T->nsnap);
    setstrV(L, L->top++, lj_str_newz(L, jit_trlinkname[T->linktype]));
    lua_setfield(L, -2, "linktype");
    /* There are many more fields. Add them only when needed. */
    return 1;
  }
  return 0;
}

/* local m, ot, op1, op2, prev = jit.util.traceir(tr, idx) */
LJLIB_CF(jit_util_traceir)
{
  GCtrace *T = jit_checktrace(L);
  IRRef ref = (IRRef)lj_lib_checkint(L, 2) + REF_BIAS;
  if (T && ref >= REF_BIAS && ref < T->nins) {
    IRIns *ir = &T->ir[ref];
    int32_t m = lj_ir_mode[ir->o];
    setintV(L->top-2, m);
    setintV(L->top-1, ir->ot);
    setintV(L->top++, (int32_t)ir->op1 - (irm_op1(m)==IRMref ? REF_BIAS : 0));
    setintV(L->top++, (int32_t)ir->op2 - (irm_op2(m)==IRMref ? REF_BIAS : 0));
    setintV(L->top++, ir->prev);
    return 5;
  }
  return 0;
}

/* local k, t [, slot] = jit.util.tracek(tr, idx) */
LJLIB_CF(jit_util_tracek)
{
  GCtrace *T = jit_checktrace(L);
  IRRef ref = (IRRef)lj_lib_checkint(L, 2) + REF_BIAS;
  if (T && ref >= T->nk && ref < REF_BIAS) {
    IRIns *ir = &T->ir[ref];
    int32_t slot = -1;
    if (ir->o == IR_KSLOT) {
      slot = ir->op2;
      ir = &T->ir[ir->op1];
    }
    lj_ir_kvalue(L, L->top-2, ir);
    setintV(L->top-1, (int32_t)irt_type(ir->t));
    if (slot == -1)
      return 2;
    setintV(L->top++, slot);
    return 3;
  }
  return 0;
}

/* local snap = jit.util.tracesnap(tr, sn) */
LJLIB_CF(jit_util_tracesnap)
{
  GCtrace *T = jit_checktrace(L);
  SnapNo sn = (SnapNo)lj_lib_checkint(L, 2);
  if (T && sn < T->nsnap) {
    SnapShot *snap = &T->snap[sn];
    SnapEntry *map = &T->snapmap[snap->mapofs];
    MSize n, nent = snap->nent;
    GCtab *t;
    lua_createtable(L, nent+2, 0);
    t = tabV(L->top-1);
    setintV(lj_tab_setint(L, t, 0), (int32_t)snap->ref - REF_BIAS);
    setintV(lj_tab_setint(L, t, 1), (int32_t)snap->nslots);
    for (n = 0; n < nent; n++)
      setintV(lj_tab_setint(L, t, (int32_t)(n+2)), (int32_t)map[n]);
    setintV(lj_tab_setint(L, t, (int32_t)(nent+2)), (int32_t)SNAP(255, 0, 0));
    return 1;
  }
  return 0;
}

/* local mcode, addr, loop = jit.util.tracemc(tr) */
LJLIB_CF(jit_util_tracemc)
{
  GCtrace *T = jit_checktrace(L);
  if (T && T->mcode != NULL) {
    setstrV(L, L->top-1, lj_str_new(L, (const char *)T->mcode, T->szmcode));
    setintptrV(L->top++, (intptr_t)(void *)T->mcode);
    setintV(L->top++, T->mcloop);
    return 3;
  }
  return 0;
}

/* local addr = jit.util.traceexitstub([tr,] exitno) */
LJLIB_CF(jit_util_traceexitstub)
{
#ifdef EXITSTUBS_PER_GROUP
  ExitNo exitno = (ExitNo)lj_lib_checkint(L, 1);
  jit_State *J = L2J(L);
  if (exitno < EXITSTUBS_PER_GROUP*LJ_MAX_EXITSTUBGR) {
    setintptrV(L->top-1, (intptr_t)(void *)exitstub_addr(J, exitno));
    return 1;
  }
#else
  if (L->top > L->base+1) {  /* Don't throw for one-argument variant. */
    GCtrace *T = jit_checktrace(L);
    ExitNo exitno = (ExitNo)lj_lib_checkint(L, 2);
    ExitNo maxexit = T->root ? T->nsnap+1 : T->nsnap;
    if (T && T->mcode != NULL && exitno < maxexit) {
      setintptrV(L->top-1, (intptr_t)(void *)exitstub_trace_addr(T, exitno));
      return 1;
    }
  }
#endif
  return 0;
}

/* local addr = jit.util.ircalladdr(idx) */
LJLIB_CF(jit_util_ircalladdr)
{
  uint32_t idx = (uint32_t)lj_lib_checkint(L, 1);
  if (idx < IRCALL__MAX) {
    setintptrV(L->top-1, (intptr_t)(void *)lj_ir_callinfo[idx].func);
    return 1;
  }
  return 0;
}

#endif

#include "lj_libdef.h"

/* -- jit.opt module ------------------------------------------------------ */

#if LJ_HASJIT

#define LJLIB_MODULE_jit_opt

/* Parse optimization level. */
static int jitopt_level(jit_State *J, const char *str)
{
  if (str[0] >= '0' && str[0] <= '9' && str[1] == '\0') {
    uint32_t flags;
    if (str[0] == '0') flags = JIT_F_OPT_0;
    else if (str[0] == '1') flags = JIT_F_OPT_1;
    else if (str[0] == '2') flags = JIT_F_OPT_2;
    else flags = JIT_F_OPT_3;
    J->flags = (J->flags & ~JIT_F_OPT_MASK) | flags;
    return 1;  /* Ok. */
  }
  return 0;  /* No match. */
}

/* Parse optimization flag. */
static int jitopt_flag(jit_State *J, const char *str)
{
  const char *lst = JIT_F_OPTSTRING;
  uint32_t opt;
  int set = 1;
  if (str[0] == '+') {
    str++;
  } else if (str[0] == '-') {
    str++;
    set = 0;
  } else if (str[0] == 'n' && str[1] == 'o') {
    str += str[2] == '-' ? 3 : 2;
    set = 0;
  }
  for (opt = JIT_F_OPT_FIRST; ; opt <<= 1) {
    size_t len = *(const uint8_t *)lst;
    if (len == 0)
      break;
    if (strncmp(str, lst+1, len) == 0 && str[len] == '\0') {
      if (set) J->flags |= opt; else J->flags &= ~opt;
      return 1;  /* Ok. */
    }
    lst += 1+len;
  }
  return 0;  /* No match. */
}

/* Parse optimization parameter. */
static int jitopt_param(jit_State *J, const char *str)
{
  const char *lst = JIT_P_STRING;
  int i;
  for (i = 0; i < JIT_P__MAX; i++) {
    size_t len = *(const uint8_t *)lst;
    lua_assert(len != 0);
    if (strncmp(str, lst+1, len) == 0 && str[len] == '=') {
      int32_t n = 0;
      const char *p = &str[len+1];
      while (*p >= '0' && *p <= '9')
	n = n*10 + (*p++ - '0');
      if (*p) return 0;  /* Malformed number. */
      J->param[i] = n;
      if (i == JIT_P_hotloop)
	lj_dispatch_init_hotcount(J2G(J));
      return 1;  /* Ok. */
    }
    lst += 1+len;
  }
  return 0;  /* No match. */
}

/* jit.opt.start(flags...) */
LJLIB_CF(jit_opt_start)
{
  jit_State *J = L2J(L);
  int nargs = (int)(L->top - L->base);
  if (nargs == 0) {
    J->flags = (J->flags & ~JIT_F_OPT_MASK) | JIT_F_OPT_DEFAULT;
  } else {
    int i;
    for (i = 1; i <= nargs; i++) {
      const char *str = strdata(lj_lib_checkstr(L, i));
      if (!jitopt_level(J, str) &&
	  !jitopt_flag(J, str) &&
	  !jitopt_param(J, str))
	lj_err_callerv(L, LJ_ERR_JITOPT, str);
    }
  }
  return 0;
}

#include "lj_libdef.h"

#endif

/* -- JIT compiler initialization ----------------------------------------- */

#if LJ_HASJIT
/* Default values for JIT parameters. */
static const int32_t jit_param_default[JIT_P__MAX+1] = {
#define JIT_PARAMINIT(len, name, value)	(value),
JIT_PARAMDEF(JIT_PARAMINIT)
#undef JIT_PARAMINIT
  0
};
#endif

#if LJ_TARGET_ARM && LJ_TARGET_LINUX
#include <sys/utsname.h>
#endif

/* Arch-dependent CPU detection. */
static uint32_t jit_cpudetect(lua_State *L)
{
  uint32_t flags = 0;
#if LJ_TARGET_X86ORX64
  uint32_t vendor[4];
  uint32_t features[4];
  if (lj_vm_cpuid(0, vendor) && lj_vm_cpuid(1, features)) {
#if !LJ_HASJIT
#define JIT_F_CMOV	1
#define JIT_F_SSE2	2
#endif
    flags |= ((features[3] >> 15)&1) * JIT_F_CMOV;
    flags |= ((features[3] >> 26)&1) * JIT_F_SSE2;
#if LJ_HASJIT
    flags |= ((features[2] >> 0)&1) * JIT_F_SSE3;
    flags |= ((features[2] >> 19)&1) * JIT_F_SSE4_1;
    if (vendor[2] == 0x6c65746e) {  /* Intel. */
      if ((features[0] & 0x0ff00f00) == 0x00000f00)  /* P4. */
	flags |= JIT_F_P4;  /* Currently unused. */
      else if ((features[0] & 0x0fff0ff0) == 0x000106c0)  /* Atom. */
	flags |= JIT_F_LEA_AGU;
    } else if (vendor[2] == 0x444d4163) {  /* AMD. */
      uint32_t fam = (features[0] & 0x0ff00f00);
      if (fam == 0x00000f00)  /* K8. */
	flags |= JIT_F_SPLIT_XMM;
      if (fam >= 0x00000f00)  /* K8, K10. */
	flags |= JIT_F_PREFER_IMUL;
    }
#endif
  }
  /* Check for required instruction set support on x86 (unnecessary on x64). */
#if LJ_TARGET_X86
#if !defined(LUAJIT_CPU_NOCMOV)
  if (!(flags & JIT_F_CMOV))
    luaL_error(L, "CPU not supported");
#endif
#if defined(LUAJIT_CPU_SSE2)
  if (!(flags & JIT_F_SSE2))
    luaL_error(L, "CPU does not support SSE2 (recompile without -DLUAJIT_CPU_SSE2)");
#endif
#endif
#elif LJ_TARGET_ARM
#if LJ_HASJIT
  int ver = LJ_ARCH_VERSION;  /* Compile-time ARM CPU detection. */
#if LJ_TARGET_LINUX
  if (ver < 70) {  /* Runtime ARM CPU detection. */
    struct utsname ut;
    uname(&ut);
    if (strncmp(ut.machine, "armv", 4) == 0) {
      if (ut.machine[4] >= '7')
	ver = 70;
      else if (ut.machine[4] == '6')
	ver = 60;
    }
  }
#endif
  flags |= ver >= 70 ? JIT_F_ARMV7 :
	   ver >= 61 ? JIT_F_ARMV6T2_ :
	   ver >= 60 ? JIT_F_ARMV6_ : 0;
  flags |= LJ_ARCH_HASFPU == 0 ? 0 : ver >= 70 ? JIT_F_VFPV3 : JIT_F_VFPV2;
#endif
#elif LJ_TARGET_PPC
#if LJ_HASJIT
#if LJ_ARCH_SQRT
  flags |= JIT_F_SQRT;
#endif
#if LJ_ARCH_ROUND
  flags |= JIT_F_ROUND;
#endif
#endif
#elif LJ_TARGET_PPCSPE
  /* Nothing to do. */
#elif LJ_TARGET_MIPS
#if LJ_HASJIT
  /* Compile-time MIPS CPU detection. */
#if LJ_ARCH_VERSION >= 20
  flags |= JIT_F_MIPS32R2;
#endif
  /* Runtime MIPS CPU detection. */
#if defined(__GNUC__)
  if (!(flags & JIT_F_MIPS32R2)) {
    int x;
    /* On MIPS32R1 rotr is treated as srl. rotr r2,r2,1 -> srl r2,r2,1. */
    __asm__("li $2, 1\n\t.long 0x00221042\n\tmove %0, $2" : "=r"(x) : : "$2");
    if (x) flags |= JIT_F_MIPS32R2;  /* Either 0x80000000 (R2) or 0 (R1). */
  }
#endif
#endif
#else
#error "Missing CPU detection for this architecture"
#endif
  UNUSED(L);
  return flags;
}

/* Initialize JIT compiler. */
static void jit_init(lua_State *L)
{
  uint32_t flags = jit_cpudetect(L);
#if LJ_HASJIT
  jit_State *J = L2J(L);
#if LJ_TARGET_X86
  /* Silently turn off the JIT compiler on CPUs without SSE2. */
  if ((flags & JIT_F_SSE2))
#endif
    J->flags = flags | JIT_F_ON | JIT_F_OPT_DEFAULT;
  memcpy(J->param, jit_param_default, sizeof(J->param));
  lj_dispatch_update(G(L));
#else
  UNUSED(flags);
#endif
}

LUALIB_API int luaopen_jit(lua_State *L)
{
  lua_pushliteral(L, LJ_OS_NAME);
  lua_pushliteral(L, LJ_ARCH_NAME);
  lua_pushinteger(L, LUAJIT_VERSION_NUM);
  lua_pushliteral(L, LUAJIT_VERSION);
  LJ_LIB_REG(L, LUA_JITLIBNAME, jit);
#ifndef LUAJIT_DISABLE_JITUTIL
  LJ_LIB_REG(L, "jit.util", jit_util);
#endif
#if LJ_HASJIT
  LJ_LIB_REG(L, "jit.opt", jit_opt);
#endif
  L->top -= 2;
  jit_init(L);
  return 1;
}

