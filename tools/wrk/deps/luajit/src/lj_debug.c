/*
** Debugging and introspection.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_debug_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_err.h"
#include "lj_debug.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_state.h"
#include "lj_frame.h"
#include "lj_bc.h"
#if LJ_HASJIT
#include "lj_jit.h"
#endif

/* -- Frames -------------------------------------------------------------- */

/* Get frame corresponding to a level. */
cTValue *lj_debug_frame(lua_State *L, int level, int *size)
{
  cTValue *frame, *nextframe, *bot = tvref(L->stack);
  /* Traverse frames backwards. */
  for (nextframe = frame = L->base-1; frame > bot; ) {
    if (frame_gc(frame) == obj2gco(L))
      level++;  /* Skip dummy frames. See lj_meta_call(). */
    if (level-- == 0) {
      *size = (int)(nextframe - frame);
      return frame;  /* Level found. */
    }
    nextframe = frame;
    if (frame_islua(frame)) {
      frame = frame_prevl(frame);
    } else {
      if (frame_isvarg(frame))
	level++;  /* Skip vararg pseudo-frame. */
      frame = frame_prevd(frame);
    }
  }
  *size = level;
  return NULL;  /* Level not found. */
}

/* Invalid bytecode position. */
#define NO_BCPOS	(~(BCPos)0)

/* Return bytecode position for function/frame or NO_BCPOS. */
static BCPos debug_framepc(lua_State *L, GCfunc *fn, cTValue *nextframe)
{
  const BCIns *ins;
  GCproto *pt;
  BCPos pos;
  lua_assert(fn->c.gct == ~LJ_TFUNC || fn->c.gct == ~LJ_TTHREAD);
  if (!isluafunc(fn)) {  /* Cannot derive a PC for non-Lua functions. */
    return NO_BCPOS;
  } else if (nextframe == NULL) {  /* Lua function on top. */
    void *cf = cframe_raw(L->cframe);
    if (cf == NULL || (char *)cframe_pc(cf) == (char *)cframe_L(cf))
      return NO_BCPOS;
    ins = cframe_pc(cf);  /* Only happens during error/hook handling. */
  } else {
    if (frame_islua(nextframe)) {
      ins = frame_pc(nextframe);
    } else if (frame_iscont(nextframe)) {
      ins = frame_contpc(nextframe);
    } else {
      /* Lua function below errfunc/gc/hook: find cframe to get the PC. */
      void *cf = cframe_raw(L->cframe);
      TValue *f = L->base-1;
      if (cf == NULL)
	return NO_BCPOS;
      while (f > nextframe) {
	if (frame_islua(f)) {
	  f = frame_prevl(f);
	} else {
	  if (frame_isc(f))
	    cf = cframe_raw(cframe_prev(cf));
	  f = frame_prevd(f);
	}
      }
      if (cframe_prev(cf))
	cf = cframe_raw(cframe_prev(cf));
      ins = cframe_pc(cf);
    }
  }
  pt = funcproto(fn);
  pos = proto_bcpos(pt, ins) - 1;
#if LJ_HASJIT
  if (pos > pt->sizebc) {  /* Undo the effects of lj_trace_exit for JLOOP. */
    GCtrace *T = (GCtrace *)((char *)(ins-1) - offsetof(GCtrace, startins));
    lua_assert(bc_isret(bc_op(ins[-1])));
    pos = proto_bcpos(pt, mref(T->startpc, const BCIns));
  }
#endif
  return pos;
}

/* -- Line numbers -------------------------------------------------------- */

/* Get line number for a bytecode position. */
BCLine LJ_FASTCALL lj_debug_line(GCproto *pt, BCPos pc)
{
  const void *lineinfo = proto_lineinfo(pt);
  if (pc <= pt->sizebc && lineinfo) {
    BCLine first = pt->firstline;
    if (pc == pt->sizebc) return first + pt->numline;
    if (pc-- == 0) return first;
    if (pt->numline < 256)
      return first + (BCLine)((const uint8_t *)lineinfo)[pc];
    else if (pt->numline < 65536)
      return first + (BCLine)((const uint16_t *)lineinfo)[pc];
    else
      return first + (BCLine)((const uint32_t *)lineinfo)[pc];
  }
  return 0;
}

/* Get line number for function/frame. */
static BCLine debug_frameline(lua_State *L, GCfunc *fn, cTValue *nextframe)
{
  BCPos pc = debug_framepc(L, fn, nextframe);
  if (pc != NO_BCPOS) {
    GCproto *pt = funcproto(fn);
    lua_assert(pc <= pt->sizebc);
    return lj_debug_line(pt, pc);
  }
  return -1;
}

/* -- Variable names ------------------------------------------------------ */

/* Read ULEB128 value. */
static uint32_t debug_read_uleb128(const uint8_t **pp)
{
  const uint8_t *p = *pp;
  uint32_t v = *p++;
  if (LJ_UNLIKELY(v >= 0x80)) {
    int sh = 0;
    v &= 0x7f;
    do { v |= ((*p & 0x7f) << (sh += 7)); } while (*p++ >= 0x80);
  }
  *pp = p;
  return v;
}

/* Get name of a local variable from slot number and PC. */
static const char *debug_varname(const GCproto *pt, BCPos pc, BCReg slot)
{
  const uint8_t *p = proto_varinfo(pt);
  if (p) {
    BCPos lastpc = 0;
    for (;;) {
      const char *name = (const char *)p;
      uint32_t vn = *p++;
      BCPos startpc, endpc;
      if (vn < VARNAME__MAX) {
	if (vn == VARNAME_END) break;  /* End of varinfo. */
      } else {
	while (*p++) ;  /* Skip over variable name string. */
      }
      lastpc = startpc = lastpc + debug_read_uleb128(&p);
      if (startpc > pc) break;
      endpc = startpc + debug_read_uleb128(&p);
      if (pc < endpc && slot-- == 0) {
	if (vn < VARNAME__MAX) {
#define VARNAMESTR(name, str)	str "\0"
	  name = VARNAMEDEF(VARNAMESTR);
#undef VARNAMESTR
	  if (--vn) while (*name++ || --vn) ;
	}
	return name;
      }
    }
  }
  return NULL;
}

/* Get name of local variable from 1-based slot number and function/frame. */
static TValue *debug_localname(lua_State *L, const lua_Debug *ar,
			       const char **name, BCReg slot1)
{
  uint32_t offset = (uint32_t)ar->i_ci & 0xffff;
  uint32_t size = (uint32_t)ar->i_ci >> 16;
  TValue *frame = tvref(L->stack) + offset;
  TValue *nextframe = size ? frame + size : NULL;
  GCfunc *fn = frame_func(frame);
  BCPos pc = debug_framepc(L, fn, nextframe);
  if (!nextframe) nextframe = L->top;
  if ((int)slot1 < 0) {  /* Negative slot number is for varargs. */
    if (pc != NO_BCPOS) {
      GCproto *pt = funcproto(fn);
      if ((pt->flags & PROTO_VARARG)) {
	slot1 = pt->numparams + (BCReg)(-(int)slot1);
	if (frame_isvarg(frame)) {  /* Vararg frame has been set up? (pc!=0) */
	  nextframe = frame;
	  frame = frame_prevd(frame);
	}
	if (frame + slot1 < nextframe) {
	  *name = "(*vararg)";
	  return frame+slot1;
	}
      }
    }
    return NULL;
  }
  if (pc != NO_BCPOS &&
      (*name = debug_varname(funcproto(fn), pc, slot1-1)) != NULL)
    ;
  else if (slot1 > 0 && frame + slot1 < nextframe)
    *name = "(*temporary)";
  return frame+slot1;
}

/* Get name of upvalue. */
const char *lj_debug_uvname(GCproto *pt, uint32_t idx)
{
  const uint8_t *p = proto_uvinfo(pt);
  lua_assert(idx < pt->sizeuv);
  if (!p) return "";
  if (idx) while (*p++ || --idx) ;
  return (const char *)p;
}

/* Get name and value of upvalue. */
const char *lj_debug_uvnamev(cTValue *o, uint32_t idx, TValue **tvp)
{
  if (tvisfunc(o)) {
    GCfunc *fn = funcV(o);
    if (isluafunc(fn)) {
      GCproto *pt = funcproto(fn);
      if (idx < pt->sizeuv) {
	*tvp = uvval(&gcref(fn->l.uvptr[idx])->uv);
	return lj_debug_uvname(pt, idx);
      }
    } else {
      if (idx < fn->c.nupvalues) {
	*tvp = &fn->c.upvalue[idx];
	return "";
      }
    }
  }
  return NULL;
}

/* Deduce name of an object from slot number and PC. */
const char *lj_debug_slotname(GCproto *pt, const BCIns *ip, BCReg slot,
			      const char **name)
{
  const char *lname;
restart:
  lname = debug_varname(pt, proto_bcpos(pt, ip), slot);
  if (lname != NULL) { *name = lname; return "local"; }
  while (--ip > proto_bc(pt)) {
    BCIns ins = *ip;
    BCOp op = bc_op(ins);
    BCReg ra = bc_a(ins);
    if (bcmode_a(op) == BCMbase) {
      if (slot >= ra && (op != BC_KNIL || slot <= bc_d(ins)))
	return NULL;
    } else if (bcmode_a(op) == BCMdst && ra == slot) {
      switch (bc_op(ins)) {
      case BC_MOV:
	if (ra == slot) { slot = bc_d(ins); goto restart; }
	break;
      case BC_GGET:
	*name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_d(ins))));
	return "global";
      case BC_TGETS:
	*name = strdata(gco2str(proto_kgc(pt, ~(ptrdiff_t)bc_c(ins))));
	if (ip > proto_bc(pt)) {
	  BCIns insp = ip[-1];
	  if (bc_op(insp) == BC_MOV && bc_a(insp) == ra+1 &&
	      bc_d(insp) == bc_b(ins))
	    return "method";
	}
	return "field";
      case BC_UGET:
	*name = lj_debug_uvname(pt, bc_d(ins));
	return "upvalue";
      default:
	return NULL;
      }
    }
  }
  return NULL;
}

/* Deduce function name from caller of a frame. */
const char *lj_debug_funcname(lua_State *L, TValue *frame, const char **name)
{
  TValue *pframe;
  GCfunc *fn;
  BCPos pc;
  if (frame <= tvref(L->stack))
    return NULL;
  if (frame_isvarg(frame))
    frame = frame_prevd(frame);
  pframe = frame_prev(frame);
  fn = frame_func(pframe);
  pc = debug_framepc(L, fn, frame);
  if (pc != NO_BCPOS) {
    GCproto *pt = funcproto(fn);
    const BCIns *ip = &proto_bc(pt)[check_exp(pc < pt->sizebc, pc)];
    MMS mm = bcmode_mm(bc_op(*ip));
    if (mm == MM_call) {
      BCReg slot = bc_a(*ip);
      if (bc_op(*ip) == BC_ITERC) slot -= 3;
      return lj_debug_slotname(pt, ip, slot, name);
    } else if (mm != MM__MAX) {
      *name = strdata(mmname_str(G(L), mm));
      return "metamethod";
    }
  }
  return NULL;
}

/* -- Source code locations ----------------------------------------------- */

/* Generate shortened source name. */
void lj_debug_shortname(char *out, GCstr *str)
{
  const char *src = strdata(str);
  if (*src == '=') {
    strncpy(out, src+1, LUA_IDSIZE);  /* Remove first char. */
    out[LUA_IDSIZE-1] = '\0';  /* Ensures null termination. */
  } else if (*src == '@') {  /* Output "source", or "...source". */
    size_t len = str->len-1;
    src++;  /* Skip the `@' */
    if (len >= LUA_IDSIZE) {
      src += len-(LUA_IDSIZE-4);  /* Get last part of file name. */
      *out++ = '.'; *out++ = '.'; *out++ = '.';
    }
    strcpy(out, src);
  } else {  /* Output [string "string"]. */
    size_t len;  /* Length, up to first control char. */
    for (len = 0; len < LUA_IDSIZE-12; len++)
      if (((const unsigned char *)src)[len] < ' ') break;
    strcpy(out, "[string \""); out += 9;
    if (src[len] != '\0') {  /* Must truncate? */
      if (len > LUA_IDSIZE-15) len = LUA_IDSIZE-15;
      strncpy(out, src, len); out += len;
      strcpy(out, "..."); out += 3;
    } else {
      strcpy(out, src); out += len;
    }
    strcpy(out, "\"]");
  }
}

/* Add current location of a frame to error message. */
void lj_debug_addloc(lua_State *L, const char *msg,
		     cTValue *frame, cTValue *nextframe)
{
  if (frame) {
    GCfunc *fn = frame_func(frame);
    if (isluafunc(fn)) {
      BCLine line = debug_frameline(L, fn, nextframe);
      if (line >= 0) {
	char buf[LUA_IDSIZE];
	lj_debug_shortname(buf, proto_chunkname(funcproto(fn)));
	lj_str_pushf(L, "%s:%d: %s", buf, line, msg);
	return;
      }
    }
  }
  lj_str_pushf(L, "%s", msg);
}

/* Push location string for a bytecode position to Lua stack. */
void lj_debug_pushloc(lua_State *L, GCproto *pt, BCPos pc)
{
  GCstr *name = proto_chunkname(pt);
  const char *s = strdata(name);
  MSize i, len = name->len;
  BCLine line = lj_debug_line(pt, pc);
  if (*s == '@') {
    s++; len--;
    for (i = len; i > 0; i--)
      if (s[i] == '/' || s[i] == '\\') {
	s += i+1;
	break;
      }
    lj_str_pushf(L, "%s:%d", s, line);
  } else if (len > 40) {
    lj_str_pushf(L, "%p:%d", pt, line);
  } else if (*s == '=') {
    lj_str_pushf(L, "%s:%d", s+1, line);
  } else {
    lj_str_pushf(L, "\"%s\":%d", s, line);
  }
}

/* -- Public debug API ---------------------------------------------------- */

/* lua_getupvalue() and lua_setupvalue() are in lj_api.c. */

LUA_API const char *lua_getlocal(lua_State *L, const lua_Debug *ar, int n)
{
  const char *name = NULL;
  if (ar) {
    TValue *o = debug_localname(L, ar, &name, (BCReg)n);
    if (name) {
      copyTV(L, L->top, o);
      incr_top(L);
    }
  } else if (tvisfunc(L->top-1) && isluafunc(funcV(L->top-1))) {
    name = debug_varname(funcproto(funcV(L->top-1)), 0, (BCReg)n-1);
  }
  return name;
}

LUA_API const char *lua_setlocal(lua_State *L, const lua_Debug *ar, int n)
{
  const char *name = NULL;
  TValue *o = debug_localname(L, ar, &name, (BCReg)n);
  if (name)
    copyTV(L, o, L->top-1);
  L->top--;
  return name;
}

int lj_debug_getinfo(lua_State *L, const char *what, lj_Debug *ar, int ext)
{
  int opt_f = 0, opt_L = 0;
  TValue *frame = NULL;
  TValue *nextframe = NULL;
  GCfunc *fn;
  if (*what == '>') {
    TValue *func = L->top - 1;
    api_check(L, tvisfunc(func));
    fn = funcV(func);
    L->top--;
    what++;
  } else {
    uint32_t offset = (uint32_t)ar->i_ci & 0xffff;
    uint32_t size = (uint32_t)ar->i_ci >> 16;
    lua_assert(offset != 0);
    frame = tvref(L->stack) + offset;
    if (size) nextframe = frame + size;
    lua_assert(frame <= tvref(L->maxstack) &&
	       (!nextframe || nextframe <= tvref(L->maxstack)));
    fn = frame_func(frame);
    lua_assert(fn->c.gct == ~LJ_TFUNC);
  }
  for (; *what; what++) {
    if (*what == 'S') {
      if (isluafunc(fn)) {
	GCproto *pt = funcproto(fn);
	BCLine firstline = pt->firstline;
	GCstr *name = proto_chunkname(pt);
	ar->source = strdata(name);
	lj_debug_shortname(ar->short_src, name);
	ar->linedefined = (int)firstline;
	ar->lastlinedefined = (int)(firstline + pt->numline);
	ar->what = firstline ? "Lua" : "main";
      } else {
	ar->source = "=[C]";
	ar->short_src[0] = '[';
	ar->short_src[1] = 'C';
	ar->short_src[2] = ']';
	ar->short_src[3] = '\0';
	ar->linedefined = -1;
	ar->lastlinedefined = -1;
	ar->what = "C";
      }
    } else if (*what == 'l') {
      ar->currentline = frame ? debug_frameline(L, fn, nextframe) : -1;
    } else if (*what == 'u') {
      ar->nups = fn->c.nupvalues;
      if (ext) {
	if (isluafunc(fn)) {
	  GCproto *pt = funcproto(fn);
	  ar->nparams = pt->numparams;
	  ar->isvararg = !!(pt->flags & PROTO_VARARG);
	} else {
	  ar->nparams = 0;
	  ar->isvararg = 1;
	}
      }
    } else if (*what == 'n') {
      ar->namewhat = frame ? lj_debug_funcname(L, frame, &ar->name) : NULL;
      if (ar->namewhat == NULL) {
	ar->namewhat = "";
	ar->name = NULL;
      }
    } else if (*what == 'f') {
      opt_f = 1;
    } else if (*what == 'L') {
      opt_L = 1;
    } else {
      return 0;  /* Bad option. */
    }
  }
  if (opt_f) {
    setfuncV(L, L->top, fn);
    incr_top(L);
  }
  if (opt_L) {
    if (isluafunc(fn)) {
      GCtab *t = lj_tab_new(L, 0, 0);
      GCproto *pt = funcproto(fn);
      const void *lineinfo = proto_lineinfo(pt);
      if (lineinfo) {
	BCLine first = pt->firstline;
	int sz = pt->numline < 256 ? 1 : pt->numline < 65536 ? 2 : 4;
	MSize i, szl = pt->sizebc-1;
	for (i = 0; i < szl; i++) {
	  BCLine line = first +
	    (sz == 1 ? (BCLine)((const uint8_t *)lineinfo)[i] :
	     sz == 2 ? (BCLine)((const uint16_t *)lineinfo)[i] :
	     (BCLine)((const uint32_t *)lineinfo)[i]);
	  setboolV(lj_tab_setint(L, t, line), 1);
	}
      }
      settabV(L, L->top, t);
    } else {
      setnilV(L->top);
    }
    incr_top(L);
  }
  return 1;  /* Ok. */
}

LUA_API int lua_getinfo(lua_State *L, const char *what, lua_Debug *ar)
{
  return lj_debug_getinfo(L, what, (lj_Debug *)ar, 0);
}

LUA_API int lua_getstack(lua_State *L, int level, lua_Debug *ar)
{
  int size;
  cTValue *frame = lj_debug_frame(L, level, &size);
  if (frame) {
    ar->i_ci = (size << 16) + (int)(frame - tvref(L->stack));
    return 1;
  } else {
    ar->i_ci = level - size;
    return 0;
  }
}

/* Number of frames for the leading and trailing part of a traceback. */
#define TRACEBACK_LEVELS1	12
#define TRACEBACK_LEVELS2	10

LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1, const char *msg,
				int level)
{
  int top = (int)(L->top - L->base);
  int lim = TRACEBACK_LEVELS1;
  lua_Debug ar;
  if (msg) lua_pushfstring(L, "%s\n", msg);
  lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    GCfunc *fn;
    if (level > lim) {
      if (!lua_getstack(L1, level + TRACEBACK_LEVELS2, &ar)) {
	level--;
      } else {
	lua_pushliteral(L, "\n\t...");
	lua_getstack(L1, -10, &ar);
	level = ar.i_ci - TRACEBACK_LEVELS2;
      }
      lim = 2147483647;
      continue;
    }
    lua_getinfo(L1, "Snlf", &ar);
    fn = funcV(L1->top-1); L1->top--;
    if (isffunc(fn) && !*ar.namewhat)
      lua_pushfstring(L, "\n\t[builtin#%d]:", fn->c.ffid);
    else
      lua_pushfstring(L, "\n\t%s:", ar.short_src);
    if (ar.currentline > 0)
      lua_pushfstring(L, "%d:", ar.currentline);
    if (*ar.namewhat) {
      lua_pushfstring(L, " in function " LUA_QS, ar.name);
    } else {
      if (*ar.what == 'm') {
	lua_pushliteral(L, " in main chunk");
      } else if (*ar.what == 'C') {
	lua_pushfstring(L, " at %p", fn->c.f);
      } else {
	lua_pushfstring(L, " in function <%s:%d>",
			ar.short_src, ar.linedefined);
      }
    }
    if ((int)(L->top - L->base) - top >= 15)
      lua_concat(L, (int)(L->top - L->base) - top);
  }
  lua_concat(L, (int)(L->top - L->base) - top);
}

