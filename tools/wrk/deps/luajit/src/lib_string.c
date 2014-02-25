/*
** String library.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Major portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <stdio.h>

#define lib_string_c
#define LUA_LIB

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_meta.h"
#include "lj_state.h"
#include "lj_ff.h"
#include "lj_bcdump.h"
#include "lj_char.h"
#include "lj_lib.h"

/* ------------------------------------------------------------------------ */

#define LJLIB_MODULE_string

LJLIB_ASM(string_len)		LJLIB_REC(.)
{
  lj_lib_checkstr(L, 1);
  return FFH_RETRY;
}

LJLIB_ASM(string_byte)		LJLIB_REC(string_range 0)
{
  GCstr *s = lj_lib_checkstr(L, 1);
  int32_t len = (int32_t)s->len;
  int32_t start = lj_lib_optint(L, 2, 1);
  int32_t stop = lj_lib_optint(L, 3, start);
  int32_t n, i;
  const unsigned char *p;
  if (stop < 0) stop += len+1;
  if (start < 0) start += len+1;
  if (start <= 0) start = 1;
  if (stop > len) stop = len;
  if (start > stop) return FFH_RES(0);  /* Empty interval: return no results. */
  start--;
  n = stop - start;
  if ((uint32_t)n > LUAI_MAXCSTACK)
    lj_err_caller(L, LJ_ERR_STRSLC);
  lj_state_checkstack(L, (MSize)n);
  p = (const unsigned char *)strdata(s) + start;
  for (i = 0; i < n; i++)
    setintV(L->base + i-1, p[i]);
  return FFH_RES(n);
}

LJLIB_ASM(string_char)
{
  int i, nargs = (int)(L->top - L->base);
  char *buf = lj_str_needbuf(L, &G(L)->tmpbuf, (size_t)nargs);
  for (i = 1; i <= nargs; i++) {
    int32_t k = lj_lib_checkint(L, i);
    if (!checku8(k))
      lj_err_arg(L, i, LJ_ERR_BADVAL);
    buf[i-1] = (char)k;
  }
  setstrV(L, L->base-1, lj_str_new(L, buf, (size_t)nargs));
  return FFH_RES(1);
}

LJLIB_ASM(string_sub)		LJLIB_REC(string_range 1)
{
  lj_lib_checkstr(L, 1);
  lj_lib_checkint(L, 2);
  setintV(L->base+2, lj_lib_optint(L, 3, -1));
  return FFH_RETRY;
}

LJLIB_ASM(string_rep)
{
  GCstr *s = lj_lib_checkstr(L, 1);
  int32_t k = lj_lib_checkint(L, 2);
  GCstr *sep = lj_lib_optstr(L, 3);
  int32_t len = (int32_t)s->len;
  global_State *g = G(L);
  int64_t tlen;
  const char *src;
  char *buf;
  if (k <= 0) {
  empty:
    setstrV(L, L->base-1, &g->strempty);
    return FFH_RES(1);
  }
  if (sep) {
    tlen = (int64_t)len + sep->len;
    if (tlen > LJ_MAX_STR)
      lj_err_caller(L, LJ_ERR_STROV);
    tlen *= k;
    if (tlen > LJ_MAX_STR)
      lj_err_caller(L, LJ_ERR_STROV);
  } else {
    tlen = (int64_t)k * len;
    if (tlen > LJ_MAX_STR)
      lj_err_caller(L, LJ_ERR_STROV);
  }
  if (tlen == 0) goto empty;
  buf = lj_str_needbuf(L, &g->tmpbuf, (MSize)tlen);
  src = strdata(s);
  if (sep) {
    tlen -= sep->len;  /* Ignore trailing separator. */
    if (k > 1) {  /* Paste one string and one separator. */
      int32_t i;
      i = 0; while (i < len) *buf++ = src[i++];
      src = strdata(sep); len = sep->len;
      i = 0; while (i < len) *buf++ = src[i++];
      src = g->tmpbuf.buf; len += s->len; k--;  /* Now copy that k-1 times. */
    }
  }
  do {
    int32_t i = 0;
    do { *buf++ = src[i++]; } while (i < len);
  } while (--k > 0);
  setstrV(L, L->base-1, lj_str_new(L, g->tmpbuf.buf, (size_t)tlen));
  return FFH_RES(1);
}

LJLIB_ASM(string_reverse)
{
  GCstr *s = lj_lib_checkstr(L, 1);
  lj_str_needbuf(L, &G(L)->tmpbuf, s->len);
  return FFH_RETRY;
}
LJLIB_ASM_(string_lower)
LJLIB_ASM_(string_upper)

/* ------------------------------------------------------------------------ */

static int writer_buf(lua_State *L, const void *p, size_t size, void *b)
{
  luaL_addlstring((luaL_Buffer *)b, (const char *)p, size);
  UNUSED(L);
  return 0;
}

LJLIB_CF(string_dump)
{
  GCfunc *fn = lj_lib_checkfunc(L, 1);
  int strip = L->base+1 < L->top && tvistruecond(L->base+1);
  luaL_Buffer b;
  L->top = L->base+1;
  luaL_buffinit(L, &b);
  if (!isluafunc(fn) || lj_bcwrite(L, funcproto(fn), writer_buf, &b, strip))
    lj_err_caller(L, LJ_ERR_STRDUMP);
  luaL_pushresult(&b);
  return 1;
}

/* ------------------------------------------------------------------------ */

/* macro to `unsign' a character */
#define uchar(c)        ((unsigned char)(c))

#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)

typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end (`\0') of source string */
  lua_State *L;
  int level;  /* total number of captures (finished or unfinished) */
  int depth;
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;

#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"

static int check_capture(MatchState *ms, int l)
{
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    lj_err_caller(ms->L, LJ_ERR_STRCAPI);
  return l;
}

static int capture_to_close(MatchState *ms)
{
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  lj_err_caller(ms->L, LJ_ERR_STRPATC);
  return 0;  /* unreachable */
}

static const char *classend(MatchState *ms, const char *p)
{
  switch (*p++) {
  case L_ESC:
    if (*p == '\0')
      lj_err_caller(ms->L, LJ_ERR_STRPATE);
    return p+1;
  case '[':
    if (*p == '^') p++;
    do {  /* look for a `]' */
      if (*p == '\0')
	lj_err_caller(ms->L, LJ_ERR_STRPATM);
      if (*(p++) == L_ESC && *p != '\0')
	p++;  /* skip escapes (e.g. `%]') */
    } while (*p != ']');
    return p+1;
  default:
    return p;
  }
}

static const unsigned char match_class_map[32] = {
  0,LJ_CHAR_ALPHA,0,LJ_CHAR_CNTRL,LJ_CHAR_DIGIT,0,0,LJ_CHAR_GRAPH,0,0,0,0,
  LJ_CHAR_LOWER,0,0,0,LJ_CHAR_PUNCT,0,0,LJ_CHAR_SPACE,0,
  LJ_CHAR_UPPER,0,LJ_CHAR_ALNUM,LJ_CHAR_XDIGIT,0,0,0,0,0,0,0
};

static int match_class(int c, int cl)
{
  if ((cl & 0xc0) == 0x40) {
    int t = match_class_map[(cl&0x1f)];
    if (t) {
      t = lj_char_isa(c, t);
      return (cl & 0x20) ? t : !t;
    }
    if (cl == 'z') return c == 0;
    if (cl == 'Z') return c != 0;
  }
  return (cl == c);
}

static int matchbracketclass(int c, const char *p, const char *ec)
{
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the `^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
	return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
	return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}

static int singlematch(int c, const char *p, const char *ep)
{
  switch (*p) {
  case '.': return 1;  /* matches any char */
  case L_ESC: return match_class(c, uchar(*(p+1)));
  case '[': return matchbracketclass(c, p, ep-1);
  default:  return (uchar(*p) == c);
  }
}

static const char *match(MatchState *ms, const char *s, const char *p);

static const char *matchbalance(MatchState *ms, const char *s, const char *p)
{
  if (*p == 0 || *(p+1) == 0)
    lj_err_caller(ms->L, LJ_ERR_STRPATU);
  if (*s != *p) {
    return NULL;
  } else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
	if (--cont == 0) return s+1;
      } else if (*s == b) {
	cont++;
      }
    }
  }
  return NULL;  /* string ends out of balance */
}

static const char *max_expand(MatchState *ms, const char *s,
			      const char *p, const char *ep)
{
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while ((s+i)<ms->src_end && singlematch(uchar(*(s+i)), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}

static const char *min_expand(MatchState *ms, const char *s,
			      const char *p, const char *ep)
{
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (s<ms->src_end && singlematch(uchar(*s), p, ep))
      s++;  /* try with one more repetition */
    else
      return NULL;
  }
}

static const char *start_capture(MatchState *ms, const char *s,
				 const char *p, int what)
{
  const char *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) lj_err_caller(ms->L, LJ_ERR_STRCAPN);
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}

static const char *end_capture(MatchState *ms, const char *s,
			       const char *p)
{
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}

static const char *match_capture(MatchState *ms, const char *s, int l)
{
  size_t len;
  l = check_capture(ms, l);
  len = (size_t)ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else
    return NULL;
}

static const char *match(MatchState *ms, const char *s, const char *p)
{
  if (++ms->depth > LJ_MAX_XLEVEL)
    lj_err_caller(ms->L, LJ_ERR_STRPATX);
  init: /* using goto's to optimize tail recursion */
  switch (*p) {
  case '(':  /* start capture */
    if (*(p+1) == ')')  /* position capture? */
      s = start_capture(ms, s, p+2, CAP_POSITION);
    else
      s = start_capture(ms, s, p+1, CAP_UNFINISHED);
    break;
  case ')':  /* end capture */
    s = end_capture(ms, s, p+1);
    break;
  case L_ESC:
    switch (*(p+1)) {
    case 'b':  /* balanced string? */
      s = matchbalance(ms, s, p+2);
      if (s == NULL) break;
      p+=4;
      goto init;  /* else s = match(ms, s, p+4); */
    case 'f': {  /* frontier? */
      const char *ep; char previous;
      p += 2;
      if (*p != '[')
	lj_err_caller(ms->L, LJ_ERR_STRPATB);
      ep = classend(ms, p);  /* points to what is next */
      previous = (s == ms->src_init) ? '\0' : *(s-1);
      if (matchbracketclass(uchar(previous), p, ep-1) ||
	 !matchbracketclass(uchar(*s), p, ep-1)) { s = NULL; break; }
      p=ep;
      goto init;  /* else s = match(ms, s, ep); */
      }
    default:
      if (lj_char_isdigit(uchar(*(p+1)))) {  /* capture results (%0-%9)? */
	s = match_capture(ms, s, uchar(*(p+1)));
	if (s == NULL) break;
	p+=2;
	goto init;  /* else s = match(ms, s, p+2) */
      }
      goto dflt;  /* case default */
    }
    break;
  case '\0':  /* end of pattern */
    break;  /* match succeeded */
  case '$':
    /* is the `$' the last char in pattern? */
    if (*(p+1) != '\0') goto dflt;
    if (s != ms->src_end) s = NULL;  /* check end of string */
    break;
  default: dflt: {  /* it is a pattern item */
    const char *ep = classend(ms, p);  /* points to what is next */
    int m = s<ms->src_end && singlematch(uchar(*s), p, ep);
    switch (*ep) {
    case '?': {  /* optional */
      const char *res;
      if (m && ((res=match(ms, s+1, ep+1)) != NULL)) {
	s = res;
	break;
      }
      p=ep+1;
      goto init;  /* else s = match(ms, s, ep+1); */
      }
    case '*':  /* 0 or more repetitions */
      s = max_expand(ms, s, p, ep);
      break;
    case '+':  /* 1 or more repetitions */
      s = (m ? max_expand(ms, s+1, p, ep) : NULL);
      break;
    case '-':  /* 0 or more repetitions (minimum) */
      s = min_expand(ms, s, p, ep);
      break;
    default:
      if (m) { s++; p=ep; goto init; }  /* else s = match(ms, s+1, ep); */
      s = NULL;
      break;
    }
    break;
    }
  }
  ms->depth--;
  return s;
}

static const char *lmemfind(const char *s1, size_t l1,
			    const char *s2, size_t l2)
{
  if (l2 == 0) {
    return s1;  /* empty strings are everywhere */
  } else if (l2 > l1) {
    return NULL;  /* avoids a negative `l1' */
  } else {
    const char *init;  /* to search for a `*s2' inside `s1' */
    l2--;  /* 1st char will be checked by `memchr' */
    l1 = l1-l2;  /* `s2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0) {
	return init-1;
      } else {  /* correct `l1' and `s1' to try again */
	l1 -= (size_t)(init-s1);
	s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}

static void push_onecapture(MatchState *ms, int i, const char *s, const char *e)
{
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
      lua_pushlstring(ms->L, s, (size_t)(e - s));  /* add whole match */
    else
      lj_err_caller(ms->L, LJ_ERR_STRCAPI);
  } else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED) lj_err_caller(ms->L, LJ_ERR_STRCAPU);
    if (l == CAP_POSITION)
      lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + 1);
    else
      lua_pushlstring(ms->L, ms->capture[i].init, (size_t)l);
  }
}

static int push_captures(MatchState *ms, const char *s, const char *e)
{
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  luaL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}

static ptrdiff_t posrelat(ptrdiff_t pos, size_t len)
{
  /* relative string position: negative means back from end */
  if (pos < 0) pos += (ptrdiff_t)len + 1;
  return (pos >= 0) ? pos : 0;
}

static int str_find_aux(lua_State *L, int find)
{
  size_t l1, l2;
  const char *s = luaL_checklstring(L, 1, &l1);
  const char *p = luaL_checklstring(L, 2, &l2);
  ptrdiff_t init = posrelat(luaL_optinteger(L, 3, 1), l1) - 1;
  if (init < 0) {
    init = 0;
  } else if ((size_t)(init) > l1) {
#if LJ_52
    setnilV(L->top-1);
    return 1;
#else
    init = (ptrdiff_t)l1;
#endif
  }
  if (find && (lua_toboolean(L, 4) ||  /* explicit request? */
      strpbrk(p, SPECIALS) == NULL)) {  /* or no special characters? */
    /* do a plain search */
    const char *s2 = lmemfind(s+init, l1-(size_t)init, p, l2);
    if (s2) {
      lua_pushinteger(L, s2-s+1);
      lua_pushinteger(L, s2-s+(ptrdiff_t)l2);
      return 2;
    }
  } else {
    MatchState ms;
    int anchor = (*p == '^') ? (p++, 1) : 0;
    const char *s1=s+init;
    ms.L = L;
    ms.src_init = s;
    ms.src_end = s+l1;
    do {
      const char *res;
      ms.level = ms.depth = 0;
      if ((res=match(&ms, s1, p)) != NULL) {
	if (find) {
	  lua_pushinteger(L, s1-s+1);  /* start */
	  lua_pushinteger(L, res-s);   /* end */
	  return push_captures(&ms, NULL, 0) + 2;
	} else {
	  return push_captures(&ms, s1, res);
	}
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  lua_pushnil(L);  /* not found */
  return 1;
}

LJLIB_CF(string_find)
{
  return str_find_aux(L, 1);
}

LJLIB_CF(string_match)
{
  return str_find_aux(L, 0);
}

LJLIB_NOREG LJLIB_CF(string_gmatch_aux)
{
  const char *p = strVdata(lj_lib_upvalue(L, 2));
  GCstr *str = strV(lj_lib_upvalue(L, 1));
  const char *s = strdata(str);
  TValue *tvpos = lj_lib_upvalue(L, 3);
  const char *src = s + tvpos->u32.lo;
  MatchState ms;
  ms.L = L;
  ms.src_init = s;
  ms.src_end = s + str->len;
  for (; src <= ms.src_end; src++) {
    const char *e;
    ms.level = ms.depth = 0;
    if ((e = match(&ms, src, p)) != NULL) {
      int32_t pos = (int32_t)(e - s);
      if (e == src) pos++;  /* Ensure progress for empty match. */
      tvpos->u32.lo = (uint32_t)pos;
      return push_captures(&ms, src, e);
    }
  }
  return 0;  /* not found */
}

LJLIB_CF(string_gmatch)
{
  lj_lib_checkstr(L, 1);
  lj_lib_checkstr(L, 2);
  L->top = L->base+3;
  (L->top-1)->u64 = 0;
  lj_lib_pushcc(L, lj_cf_string_gmatch_aux, FF_string_gmatch_aux, 3);
  return 1;
}

static void add_s(MatchState *ms, luaL_Buffer *b, const char *s, const char *e)
{
  size_t l, i;
  const char *news = lua_tolstring(ms->L, 3, &l);
  for (i = 0; i < l; i++) {
    if (news[i] != L_ESC) {
      luaL_addchar(b, news[i]);
    } else {
      i++;  /* skip ESC */
      if (!lj_char_isdigit(uchar(news[i]))) {
	luaL_addchar(b, news[i]);
      } else if (news[i] == '0') {
	luaL_addlstring(b, s, (size_t)(e - s));
      } else {
	push_onecapture(ms, news[i] - '1', s, e);
	luaL_addvalue(b);  /* add capture to accumulated result */
      }
    }
  }
}

static void add_value(MatchState *ms, luaL_Buffer *b,
		      const char *s, const char *e)
{
  lua_State *L = ms->L;
  switch (lua_type(L, 3)) {
    case LUA_TNUMBER:
    case LUA_TSTRING: {
      add_s(ms, b, s, e);
      return;
    }
    case LUA_TFUNCTION: {
      int n;
      lua_pushvalue(L, 3);
      n = push_captures(ms, s, e);
      lua_call(L, n, 1);
      break;
    }
    case LUA_TTABLE: {
      push_onecapture(ms, 0, s, e);
      lua_gettable(L, 3);
      break;
    }
  }
  if (!lua_toboolean(L, -1)) {  /* nil or false? */
    lua_pop(L, 1);
    lua_pushlstring(L, s, (size_t)(e - s));  /* keep original text */
  } else if (!lua_isstring(L, -1)) {
    lj_err_callerv(L, LJ_ERR_STRGSRV, luaL_typename(L, -1));
  }
  luaL_addvalue(b);  /* add result to accumulator */
}

LJLIB_CF(string_gsub)
{
  size_t srcl;
  const char *src = luaL_checklstring(L, 1, &srcl);
  const char *p = luaL_checkstring(L, 2);
  int  tr = lua_type(L, 3);
  int max_s = luaL_optint(L, 4, (int)(srcl+1));
  int anchor = (*p == '^') ? (p++, 1) : 0;
  int n = 0;
  MatchState ms;
  luaL_Buffer b;
  if (!(tr == LUA_TNUMBER || tr == LUA_TSTRING ||
	tr == LUA_TFUNCTION || tr == LUA_TTABLE))
    lj_err_arg(L, 3, LJ_ERR_NOSFT);
  luaL_buffinit(L, &b);
  ms.L = L;
  ms.src_init = src;
  ms.src_end = src+srcl;
  while (n < max_s) {
    const char *e;
    ms.level = ms.depth = 0;
    e = match(&ms, src, p);
    if (e) {
      n++;
      add_value(&ms, &b, src, e);
    }
    if (e && e>src) /* non empty match? */
      src = e;  /* skip it */
    else if (src < ms.src_end)
      luaL_addchar(&b, *src++);
    else
      break;
    if (anchor)
      break;
  }
  luaL_addlstring(&b, src, (size_t)(ms.src_end-src));
  luaL_pushresult(&b);
  lua_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* ------------------------------------------------------------------------ */

/* maximum size of each formatted item (> len(format('%99.99f', -1e308))) */
#define MAX_FMTITEM	512
/* valid flags in a format specification */
#define FMT_FLAGS	"-+ #0"
/*
** maximum size of each format specification (such as '%-099.99d')
** (+10 accounts for %99.99x plus margin of error)
*/
#define MAX_FMTSPEC	(sizeof(FMT_FLAGS) + sizeof(LUA_INTFRMLEN) + 10)

static void addquoted(lua_State *L, luaL_Buffer *b, int arg)
{
  GCstr *str = lj_lib_checkstr(L, arg);
  int32_t len = (int32_t)str->len;
  const char *s = strdata(str);
  luaL_addchar(b, '"');
  while (len--) {
    uint32_t c = uchar(*s);
    if (c == '"' || c == '\\' || c == '\n') {
      luaL_addchar(b, '\\');
    } else if (lj_char_iscntrl(c)) {  /* This can only be 0-31 or 127. */
      uint32_t d;
      luaL_addchar(b, '\\');
      if (c >= 100 || lj_char_isdigit(uchar(s[1]))) {
	luaL_addchar(b, '0'+(c >= 100)); if (c >= 100) c -= 100;
	goto tens;
      } else if (c >= 10) {
      tens:
	d = (c * 205) >> 11; c -= d * 10; luaL_addchar(b, '0'+d);
      }
      c += '0';
    }
    luaL_addchar(b, c);
    s++;
  }
  luaL_addchar(b, '"');
}

static const char *scanformat(lua_State *L, const char *strfrmt, char *form)
{
  const char *p = strfrmt;
  while (*p != '\0' && strchr(FMT_FLAGS, *p) != NULL) p++;  /* skip flags */
  if ((size_t)(p - strfrmt) >= sizeof(FMT_FLAGS))
    lj_err_caller(L, LJ_ERR_STRFMTR);
  if (lj_char_isdigit(uchar(*p))) p++;  /* skip width */
  if (lj_char_isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  if (*p == '.') {
    p++;
    if (lj_char_isdigit(uchar(*p))) p++;  /* skip precision */
    if (lj_char_isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  }
  if (lj_char_isdigit(uchar(*p)))
    lj_err_caller(L, LJ_ERR_STRFMTW);
  *(form++) = '%';
  strncpy(form, strfrmt, (size_t)(p - strfrmt + 1));
  form += p - strfrmt + 1;
  *form = '\0';
  return p;
}

static void addintlen(char *form)
{
  size_t l = strlen(form);
  char spec = form[l - 1];
  strcpy(form + l - 1, LUA_INTFRMLEN);
  form[l + sizeof(LUA_INTFRMLEN) - 2] = spec;
  form[l + sizeof(LUA_INTFRMLEN) - 1] = '\0';
}

static unsigned LUA_INTFRM_T num2intfrm(lua_State *L, int arg)
{
  if (sizeof(LUA_INTFRM_T) == 4) {
    return (LUA_INTFRM_T)lj_lib_checkbit(L, arg);
  } else {
    cTValue *o;
    lj_lib_checknumber(L, arg);
    o = L->base+arg-1;
    if (tvisint(o))
      return (LUA_INTFRM_T)intV(o);
    else
      return (LUA_INTFRM_T)numV(o);
  }
}

static unsigned LUA_INTFRM_T num2uintfrm(lua_State *L, int arg)
{
  if (sizeof(LUA_INTFRM_T) == 4) {
    return (unsigned LUA_INTFRM_T)lj_lib_checkbit(L, arg);
  } else {
    cTValue *o;
    lj_lib_checknumber(L, arg);
    o = L->base+arg-1;
    if (tvisint(o))
      return (unsigned LUA_INTFRM_T)intV(o);
    else if ((int32_t)o->u32.hi < 0)
      return (unsigned LUA_INTFRM_T)(LUA_INTFRM_T)numV(o);
    else
      return (unsigned LUA_INTFRM_T)numV(o);
  }
}

static GCstr *meta_tostring(lua_State *L, int arg)
{
  TValue *o = L->base+arg-1;
  cTValue *mo;
  lua_assert(o < L->top);  /* Caller already checks for existence. */
  if (LJ_LIKELY(tvisstr(o)))
    return strV(o);
  if (!tvisnil(mo = lj_meta_lookup(L, o, MM_tostring))) {
    copyTV(L, L->top++, mo);
    copyTV(L, L->top++, o);
    lua_call(L, 1, 1);
    L->top--;
    if (tvisstr(L->top))
      return strV(L->top);
    o = L->base+arg-1;
    copyTV(L, o, L->top);
  }
  if (tvisnumber(o)) {
    return lj_str_fromnumber(L, o);
  } else if (tvisnil(o)) {
    return lj_str_newlit(L, "nil");
  } else if (tvisfalse(o)) {
    return lj_str_newlit(L, "false");
  } else if (tvistrue(o)) {
    return lj_str_newlit(L, "true");
  } else {
    if (tvisfunc(o) && isffunc(funcV(o)))
      lj_str_pushf(L, "function: builtin#%d", funcV(o)->c.ffid);
    else
      lj_str_pushf(L, "%s: %p", lj_typename(o), lua_topointer(L, arg));
    L->top--;
    return strV(L->top);
  }
}

LJLIB_CF(string_format)
{
  int arg = 1, top = (int)(L->top - L->base);
  GCstr *fmt = lj_lib_checkstr(L, arg);
  const char *strfrmt = strdata(fmt);
  const char *strfrmt_end = strfrmt + fmt->len;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (strfrmt < strfrmt_end) {
    if (*strfrmt != L_ESC) {
      luaL_addchar(&b, *strfrmt++);
    } else if (*++strfrmt == L_ESC) {
      luaL_addchar(&b, *strfrmt++);  /* %% */
    } else { /* format item */
      char form[MAX_FMTSPEC];  /* to store the format (`%...') */
      char buff[MAX_FMTITEM];  /* to store the formatted item */
      if (++arg > top)
	luaL_argerror(L, arg, lj_obj_typename[0]);
      strfrmt = scanformat(L, strfrmt, form);
      switch (*strfrmt++) {
      case 'c':
	sprintf(buff, form, lj_lib_checkint(L, arg));
	break;
      case 'd':  case 'i':
	addintlen(form);
	sprintf(buff, form, num2intfrm(L, arg));
	break;
      case 'o':  case 'u':  case 'x':  case 'X':
	addintlen(form);
	sprintf(buff, form, num2uintfrm(L, arg));
	break;
      case 'e':  case 'E': case 'f': case 'g': case 'G': case 'a': case 'A': {
	TValue tv;
	tv.n = lj_lib_checknum(L, arg);
	if (LJ_UNLIKELY((tv.u32.hi << 1) >= 0xffe00000)) {
	  /* Canonicalize output of non-finite values. */
	  char *p, nbuf[LJ_STR_NUMBUF];
	  size_t len = lj_str_bufnum(nbuf, &tv);
	  if (strfrmt[-1] < 'a') {
	    nbuf[len-3] = nbuf[len-3] - 0x20;
	    nbuf[len-2] = nbuf[len-2] - 0x20;
	    nbuf[len-1] = nbuf[len-1] - 0x20;
	  }
	  nbuf[len] = '\0';
	  for (p = form; *p < 'A' && *p != '.'; p++) ;
	  *p++ = 's'; *p = '\0';
	  sprintf(buff, form, nbuf);
	  break;
	}
	sprintf(buff, form, (double)tv.n);
	break;
	}
      case 'q':
	addquoted(L, &b, arg);
	continue;
      case 'p':
	lj_str_pushf(L, "%p", lua_topointer(L, arg));
	luaL_addvalue(&b);
	continue;
      case 's': {
	GCstr *str = meta_tostring(L, arg);
	if (!strchr(form, '.') && str->len >= 100) {
	  /* no precision and string is too long to be formatted;
	     keep original string */
	  setstrV(L, L->top++, str);
	  luaL_addvalue(&b);
	  continue;
	}
	sprintf(buff, form, strdata(str));
	break;
	}
      default:
	lj_err_callerv(L, LJ_ERR_STRFMTO, *(strfrmt -1));
	break;
      }
      luaL_addlstring(&b, buff, strlen(buff));
    }
  }
  luaL_pushresult(&b);
  return 1;
}

/* ------------------------------------------------------------------------ */

#include "lj_libdef.h"

LUALIB_API int luaopen_string(lua_State *L)
{
  GCtab *mt;
  global_State *g;
  LJ_LIB_REG(L, LUA_STRLIBNAME, string);
#if defined(LUA_COMPAT_GFIND) && !LJ_52
  lua_getfield(L, -1, "gmatch");
  lua_setfield(L, -2, "gfind");
#endif
  mt = lj_tab_new(L, 0, 1);
  /* NOBARRIER: basemt is a GC root. */
  g = G(L);
  setgcref(basemt_it(g, LJ_TSTR), obj2gco(mt));
  settabV(L, lj_tab_setstr(L, mt, mmname_str(g, MM_index)), tabV(L->top-1));
  mt->nomm = (uint8_t)(~(1u<<MM_index));
  return 1;
}

