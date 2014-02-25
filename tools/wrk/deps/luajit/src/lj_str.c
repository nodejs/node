/*
** String handling.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
**
** Portions taken verbatim or adapted from the Lua interpreter.
** Copyright (C) 1994-2008 Lua.org, PUC-Rio. See Copyright Notice in lua.h
*/

#include <stdio.h>

#define lj_str_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_state.h"
#include "lj_char.h"

/* -- String interning ---------------------------------------------------- */

/* Ordered compare of strings. Assumes string data is 4-byte aligned. */
int32_t LJ_FASTCALL lj_str_cmp(GCstr *a, GCstr *b)
{
  MSize i, n = a->len > b->len ? b->len : a->len;
  for (i = 0; i < n; i += 4) {
    /* Note: innocuous access up to end of string + 3. */
    uint32_t va = *(const uint32_t *)(strdata(a)+i);
    uint32_t vb = *(const uint32_t *)(strdata(b)+i);
    if (va != vb) {
#if LJ_LE
      va = lj_bswap(va); vb = lj_bswap(vb);
#endif
      i -= n;
      if ((int32_t)i >= -3) {
	va >>= 32+(i<<3); vb >>= 32+(i<<3);
	if (va == vb) break;
      }
      return va < vb ? -1 : 1;
    }
  }
  return (int32_t)(a->len - b->len);
}

/* Fast string data comparison. Caveat: unaligned access to 1st string! */
static LJ_AINLINE int str_fastcmp(const char *a, const char *b, MSize len)
{
  MSize i = 0;
  lua_assert(len > 0);
  lua_assert((((uintptr_t)a+len-1) & (LJ_PAGESIZE-1)) <= LJ_PAGESIZE-4);
  do {  /* Note: innocuous access up to end of string + 3. */
    uint32_t v = lj_getu32(a+i) ^ *(const uint32_t *)(b+i);
    if (v) {
      i -= len;
#if LJ_LE
      return (int32_t)i >= -3 ? (v << (32+(i<<3))) : 1;
#else
      return (int32_t)i >= -3 ? (v >> (32+(i<<3))) : 1;
#endif
    }
    i += 4;
  } while (i < len);
  return 0;
}

/* Resize the string hash table (grow and shrink). */
void lj_str_resize(lua_State *L, MSize newmask)
{
  global_State *g = G(L);
  GCRef *newhash;
  MSize i;
  if (g->gc.state == GCSsweepstring || newmask >= LJ_MAX_STRTAB-1)
    return;  /* No resizing during GC traversal or if already too big. */
  newhash = lj_mem_newvec(L, newmask+1, GCRef);
  memset(newhash, 0, (newmask+1)*sizeof(GCRef));
  for (i = g->strmask; i != ~(MSize)0; i--) {  /* Rehash old table. */
    GCobj *p = gcref(g->strhash[i]);
    while (p) {  /* Follow each hash chain and reinsert all strings. */
      MSize h = gco2str(p)->hash & newmask;
      GCobj *next = gcnext(p);
      /* NOBARRIER: The string table is a GC root. */
      setgcrefr(p->gch.nextgc, newhash[h]);
      setgcref(newhash[h], p);
      p = next;
    }
  }
  lj_mem_freevec(g, g->strhash, g->strmask+1, GCRef);
  g->strmask = newmask;
  g->strhash = newhash;
}

/* Intern a string and return string object. */
GCstr *lj_str_new(lua_State *L, const char *str, size_t lenx)
{
  global_State *g;
  GCstr *s;
  GCobj *o;
  MSize len = (MSize)lenx;
  MSize a, b, h = len;
  if (lenx >= LJ_MAX_STR)
    lj_err_msg(L, LJ_ERR_STROV);
  g = G(L);
  /* Compute string hash. Constants taken from lookup3 hash by Bob Jenkins. */
  if (len >= 4) {  /* Caveat: unaligned access! */
    a = lj_getu32(str);
    h ^= lj_getu32(str+len-4);
    b = lj_getu32(str+(len>>1)-2);
    h ^= b; h -= lj_rol(b, 14);
    b += lj_getu32(str+(len>>2)-1);
  } else if (len > 0) {
    a = *(const uint8_t *)str;
    h ^= *(const uint8_t *)(str+len-1);
    b = *(const uint8_t *)(str+(len>>1));
    h ^= b; h -= lj_rol(b, 14);
  } else {
    return &g->strempty;
  }
  a ^= h; a -= lj_rol(h, 11);
  b ^= a; b -= lj_rol(a, 25);
  h ^= b; h -= lj_rol(b, 16);
  /* Check if the string has already been interned. */
  o = gcref(g->strhash[h & g->strmask]);
  if (LJ_LIKELY((((uintptr_t)str+len-1) & (LJ_PAGESIZE-1)) <= LJ_PAGESIZE-4)) {
    while (o != NULL) {
      GCstr *sx = gco2str(o);
      if (sx->len == len && str_fastcmp(str, strdata(sx), len) == 0) {
	/* Resurrect if dead. Can only happen with fixstring() (keywords). */
	if (isdead(g, o)) flipwhite(o);
	return sx;  /* Return existing string. */
      }
      o = gcnext(o);
    }
  } else {  /* Slow path: end of string is too close to a page boundary. */
    while (o != NULL) {
      GCstr *sx = gco2str(o);
      if (sx->len == len && memcmp(str, strdata(sx), len) == 0) {
	/* Resurrect if dead. Can only happen with fixstring() (keywords). */
	if (isdead(g, o)) flipwhite(o);
	return sx;  /* Return existing string. */
      }
      o = gcnext(o);
    }
  }
  /* Nope, create a new string. */
  s = lj_mem_newt(L, sizeof(GCstr)+len+1, GCstr);
  newwhite(g, s);
  s->gct = ~LJ_TSTR;
  s->len = len;
  s->hash = h;
  s->reserved = 0;
  memcpy(strdatawr(s), str, len);
  strdatawr(s)[len] = '\0';  /* Zero-terminate string. */
  /* Add it to string hash table. */
  h &= g->strmask;
  s->nextgc = g->strhash[h];
  /* NOBARRIER: The string table is a GC root. */
  setgcref(g->strhash[h], obj2gco(s));
  if (g->strnum++ > g->strmask)  /* Allow a 100% load factor. */
    lj_str_resize(L, (g->strmask<<1)+1);  /* Grow string table. */
  return s;  /* Return newly interned string. */
}

void LJ_FASTCALL lj_str_free(global_State *g, GCstr *s)
{
  g->strnum--;
  lj_mem_free(g, s, sizestring(s));
}

/* -- Type conversions ---------------------------------------------------- */

/* Print number to buffer. Canonicalizes non-finite values. */
size_t LJ_FASTCALL lj_str_bufnum(char *s, cTValue *o)
{
  if (LJ_LIKELY((o->u32.hi << 1) < 0xffe00000)) {  /* Finite? */
    lua_Number n = o->n;
#if __BIONIC__
    if (tvismzero(o)) { s[0] = '-'; s[1] = '0'; return 2; }
#endif
    return (size_t)lua_number2str(s, n);
  } else if (((o->u32.hi & 0x000fffff) | o->u32.lo) != 0) {
    s[0] = 'n'; s[1] = 'a'; s[2] = 'n'; return 3;
  } else if ((o->u32.hi & 0x80000000) == 0) {
    s[0] = 'i'; s[1] = 'n'; s[2] = 'f'; return 3;
  } else {
    s[0] = '-'; s[1] = 'i'; s[2] = 'n'; s[3] = 'f'; return 4;
  }
}

/* Print integer to buffer. Returns pointer to start. */
char * LJ_FASTCALL lj_str_bufint(char *p, int32_t k)
{
  uint32_t u = (uint32_t)(k < 0 ? -k : k);
  p += 1+10;
  do { *--p = (char)('0' + u % 10); } while (u /= 10);
  if (k < 0) *--p = '-';
  return p;
}

/* Convert number to string. */
GCstr * LJ_FASTCALL lj_str_fromnum(lua_State *L, const lua_Number *np)
{
  char buf[LJ_STR_NUMBUF];
  size_t len = lj_str_bufnum(buf, (TValue *)np);
  return lj_str_new(L, buf, len);
}

/* Convert integer to string. */
GCstr * LJ_FASTCALL lj_str_fromint(lua_State *L, int32_t k)
{
  char s[1+10];
  char *p = lj_str_bufint(s, k);
  return lj_str_new(L, p, (size_t)(s+sizeof(s)-p));
}

GCstr * LJ_FASTCALL lj_str_fromnumber(lua_State *L, cTValue *o)
{
  return tvisint(o) ? lj_str_fromint(L, intV(o)) : lj_str_fromnum(L, &o->n);
}

/* -- String formatting --------------------------------------------------- */

static void addstr(lua_State *L, SBuf *sb, const char *str, MSize len)
{
  char *p;
  MSize i;
  if (sb->n + len > sb->sz) {
    MSize sz = sb->sz * 2;
    while (sb->n + len > sz) sz = sz * 2;
    lj_str_resizebuf(L, sb, sz);
  }
  p = sb->buf + sb->n;
  sb->n += len;
  for (i = 0; i < len; i++) p[i] = str[i];
}

static void addchar(lua_State *L, SBuf *sb, int c)
{
  if (sb->n + 1 > sb->sz) {
    MSize sz = sb->sz * 2;
    lj_str_resizebuf(L, sb, sz);
  }
  sb->buf[sb->n++] = (char)c;
}

/* Push formatted message as a string object to Lua stack. va_list variant. */
const char *lj_str_pushvf(lua_State *L, const char *fmt, va_list argp)
{
  SBuf *sb = &G(L)->tmpbuf;
  lj_str_needbuf(L, sb, (MSize)strlen(fmt));
  lj_str_resetbuf(sb);
  for (;;) {
    const char *e = strchr(fmt, '%');
    if (e == NULL) break;
    addstr(L, sb, fmt, (MSize)(e-fmt));
    /* This function only handles %s, %c, %d, %f and %p formats. */
    switch (e[1]) {
    case 's': {
      const char *s = va_arg(argp, char *);
      if (s == NULL) s = "(null)";
      addstr(L, sb, s, (MSize)strlen(s));
      break;
      }
    case 'c':
      addchar(L, sb, va_arg(argp, int));
      break;
    case 'd': {
      char buf[LJ_STR_INTBUF];
      char *p = lj_str_bufint(buf, va_arg(argp, int32_t));
      addstr(L, sb, p, (MSize)(buf+LJ_STR_INTBUF-p));
      break;
      }
    case 'f': {
      char buf[LJ_STR_NUMBUF];
      TValue tv;
      MSize len;
      tv.n = (lua_Number)(va_arg(argp, LUAI_UACNUMBER));
      len = (MSize)lj_str_bufnum(buf, &tv);
      addstr(L, sb, buf, len);
      break;
      }
    case 'p': {
#define FMTP_CHARS	(2*sizeof(ptrdiff_t))
      char buf[2+FMTP_CHARS];
      ptrdiff_t p = (ptrdiff_t)(va_arg(argp, void *));
      ptrdiff_t i, lasti = 2+FMTP_CHARS;
      if (p == 0) {
	addstr(L, sb, "NULL", 4);
	break;
      }
#if LJ_64
      /* Shorten output for 64 bit pointers. */
      lasti = 2+2*4+((p >> 32) ? 2+2*(lj_fls((uint32_t)(p >> 32))>>3) : 0);
#endif
      buf[0] = '0';
      buf[1] = 'x';
      for (i = lasti-1; i >= 2; i--, p >>= 4)
	buf[i] = "0123456789abcdef"[(p & 15)];
      addstr(L, sb, buf, (MSize)lasti);
      break;
      }
    case '%':
      addchar(L, sb, '%');
      break;
    default:
      addchar(L, sb, '%');
      addchar(L, sb, e[1]);
      break;
    }
    fmt = e+2;
  }
  addstr(L, sb, fmt, (MSize)strlen(fmt));
  setstrV(L, L->top, lj_str_new(L, sb->buf, sb->n));
  incr_top(L);
  return strVdata(L->top - 1);
}

/* Push formatted message as a string object to Lua stack. Vararg variant. */
const char *lj_str_pushf(lua_State *L, const char *fmt, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = lj_str_pushvf(L, fmt, argp);
  va_end(argp);
  return msg;
}

/* -- Buffer handling ----------------------------------------------------- */

char *lj_str_needbuf(lua_State *L, SBuf *sb, MSize sz)
{
  if (sz > sb->sz) {
    if (sz < LJ_MIN_SBUF) sz = LJ_MIN_SBUF;
    lj_str_resizebuf(L, sb, sz);
  }
  return sb->buf;
}

