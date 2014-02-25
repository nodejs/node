/*
** Bytecode reader.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_bcread_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#include "lj_cdata.h"
#include "lualib.h"
#endif
#include "lj_lex.h"
#include "lj_bcdump.h"
#include "lj_state.h"

/* Reuse some lexer fields for our own purposes. */
#define bcread_flags(ls)	ls->level
#define bcread_swap(ls) \
  ((bcread_flags(ls) & BCDUMP_F_BE) != LJ_BE*BCDUMP_F_BE)
#define bcread_oldtop(L, ls)	restorestack(L, ls->lastline)
#define bcread_savetop(L, ls, top) \
  ls->lastline = (BCLine)savestack(L, (top))

/* -- Input buffer handling ----------------------------------------------- */

/* Throw reader error. */
static LJ_NOINLINE void bcread_error(LexState *ls, ErrMsg em)
{
  lua_State *L = ls->L;
  const char *name = ls->chunkarg;
  if (*name == BCDUMP_HEAD1) name = "(binary)";
  else if (*name == '@' || *name == '=') name++;
  lj_str_pushf(L, "%s: %s", name, err2msg(em));
  lj_err_throw(L, LUA_ERRSYNTAX);
}

/* Resize input buffer. */
static void bcread_resize(LexState *ls, MSize len)
{
  if (ls->sb.sz < len) {
    MSize sz = ls->sb.sz * 2;
    while (len > sz) sz = sz * 2;
    lj_str_resizebuf(ls->L, &ls->sb, sz);
    /* Caveat: this may change ls->sb.buf which may affect ls->p. */
  }
}

/* Refill buffer if needed. */
static LJ_NOINLINE void bcread_fill(LexState *ls, MSize len, int need)
{
  lua_assert(len != 0);
  if (len > LJ_MAX_MEM || ls->current < 0)
    bcread_error(ls, LJ_ERR_BCBAD);
  do {
    const char *buf;
    size_t size;
    if (ls->n) {  /* Copy remainder to buffer. */
      if (ls->sb.n) {  /* Move down in buffer. */
	lua_assert(ls->p + ls->n == ls->sb.buf + ls->sb.n);
	if (ls->n != ls->sb.n)
	  memmove(ls->sb.buf, ls->p, ls->n);
      } else {  /* Copy from buffer provided by reader. */
	bcread_resize(ls, len);
	memcpy(ls->sb.buf, ls->p, ls->n);
      }
      ls->p = ls->sb.buf;
    }
    ls->sb.n = ls->n;
    buf = ls->rfunc(ls->L, ls->rdata, &size);  /* Get more data from reader. */
    if (buf == NULL || size == 0) {  /* EOF? */
      if (need) bcread_error(ls, LJ_ERR_BCBAD);
      ls->current = -1;  /* Only bad if we get called again. */
      break;
    }
    if (ls->sb.n) {  /* Append to buffer. */
      MSize n = ls->sb.n + (MSize)size;
      bcread_resize(ls, n < len ? len : n);
      memcpy(ls->sb.buf + ls->sb.n, buf, size);
      ls->n = ls->sb.n = n;
      ls->p = ls->sb.buf;
    } else {  /* Return buffer provided by reader. */
      ls->n = (MSize)size;
      ls->p = buf;
    }
  } while (ls->n < len);
}

/* Need a certain number of bytes. */
static LJ_AINLINE void bcread_need(LexState *ls, MSize len)
{
  if (LJ_UNLIKELY(ls->n < len))
    bcread_fill(ls, len, 1);
}

/* Want to read up to a certain number of bytes, but may need less. */
static LJ_AINLINE void bcread_want(LexState *ls, MSize len)
{
  if (LJ_UNLIKELY(ls->n < len))
    bcread_fill(ls, len, 0);
}

#define bcread_dec(ls)		check_exp(ls->n > 0, ls->n--)
#define bcread_consume(ls, len)	check_exp(ls->n >= (len), ls->n -= (len))

/* Return memory block from buffer. */
static uint8_t *bcread_mem(LexState *ls, MSize len)
{
  uint8_t *p = (uint8_t *)ls->p;
  bcread_consume(ls, len);
  ls->p = (char *)p + len;
  return p;
}

/* Copy memory block from buffer. */
static void bcread_block(LexState *ls, void *q, MSize len)
{
  memcpy(q, bcread_mem(ls, len), len);
}

/* Read byte from buffer. */
static LJ_AINLINE uint32_t bcread_byte(LexState *ls)
{
  bcread_dec(ls);
  return (uint32_t)(uint8_t)*ls->p++;
}

/* Read ULEB128 value from buffer. */
static uint32_t bcread_uleb128(LexState *ls)
{
  const uint8_t *p = (const uint8_t *)ls->p;
  uint32_t v = *p++;
  if (LJ_UNLIKELY(v >= 0x80)) {
    int sh = 0;
    v &= 0x7f;
    do {
     v |= ((*p & 0x7f) << (sh += 7));
     bcread_dec(ls);
   } while (*p++ >= 0x80);
  }
  bcread_dec(ls);
  ls->p = (char *)p;
  return v;
}

/* Read top 32 bits of 33 bit ULEB128 value from buffer. */
static uint32_t bcread_uleb128_33(LexState *ls)
{
  const uint8_t *p = (const uint8_t *)ls->p;
  uint32_t v = (*p++ >> 1);
  if (LJ_UNLIKELY(v >= 0x40)) {
    int sh = -1;
    v &= 0x3f;
    do {
     v |= ((*p & 0x7f) << (sh += 7));
     bcread_dec(ls);
   } while (*p++ >= 0x80);
  }
  bcread_dec(ls);
  ls->p = (char *)p;
  return v;
}

/* -- Bytecode reader ----------------------------------------------------- */

/* Read debug info of a prototype. */
static void bcread_dbg(LexState *ls, GCproto *pt, MSize sizedbg)
{
  void *lineinfo = (void *)proto_lineinfo(pt);
  bcread_block(ls, lineinfo, sizedbg);
  /* Swap lineinfo if the endianess differs. */
  if (bcread_swap(ls) && pt->numline >= 256) {
    MSize i, n = pt->sizebc-1;
    if (pt->numline < 65536) {
      uint16_t *p = (uint16_t *)lineinfo;
      for (i = 0; i < n; i++) p[i] = (uint16_t)((p[i] >> 8)|(p[i] << 8));
    } else {
      uint32_t *p = (uint32_t *)lineinfo;
      for (i = 0; i < n; i++) p[i] = lj_bswap(p[i]);
    }
  }
}

/* Find pointer to varinfo. */
static const void *bcread_varinfo(GCproto *pt)
{
  const uint8_t *p = proto_uvinfo(pt);
  MSize n = pt->sizeuv;
  if (n) while (*p++ || --n) ;
  return p;
}

/* Read a single constant key/value of a template table. */
static void bcread_ktabk(LexState *ls, TValue *o)
{
  MSize tp = bcread_uleb128(ls);
  if (tp >= BCDUMP_KTAB_STR) {
    MSize len = tp - BCDUMP_KTAB_STR;
    const char *p = (const char *)bcread_mem(ls, len);
    setstrV(ls->L, o, lj_str_new(ls->L, p, len));
  } else if (tp == BCDUMP_KTAB_INT) {
    setintV(o, (int32_t)bcread_uleb128(ls));
  } else if (tp == BCDUMP_KTAB_NUM) {
    o->u32.lo = bcread_uleb128(ls);
    o->u32.hi = bcread_uleb128(ls);
  } else {
    lua_assert(tp <= BCDUMP_KTAB_TRUE);
    setitype(o, ~tp);
  }
}

/* Read a template table. */
static GCtab *bcread_ktab(LexState *ls)
{
  MSize narray = bcread_uleb128(ls);
  MSize nhash = bcread_uleb128(ls);
  GCtab *t = lj_tab_new(ls->L, narray, hsize2hbits(nhash));
  if (narray) {  /* Read array entries. */
    MSize i;
    TValue *o = tvref(t->array);
    for (i = 0; i < narray; i++, o++)
      bcread_ktabk(ls, o);
  }
  if (nhash) {  /* Read hash entries. */
    MSize i;
    for (i = 0; i < nhash; i++) {
      TValue key;
      bcread_ktabk(ls, &key);
      lua_assert(!tvisnil(&key));
      bcread_ktabk(ls, lj_tab_set(ls->L, t, &key));
    }
  }
  return t;
}

/* Read GC constants of a prototype. */
static void bcread_kgc(LexState *ls, GCproto *pt, MSize sizekgc)
{
  MSize i;
  GCRef *kr = mref(pt->k, GCRef) - (ptrdiff_t)sizekgc;
  for (i = 0; i < sizekgc; i++, kr++) {
    MSize tp = bcread_uleb128(ls);
    if (tp >= BCDUMP_KGC_STR) {
      MSize len = tp - BCDUMP_KGC_STR;
      const char *p = (const char *)bcread_mem(ls, len);
      setgcref(*kr, obj2gco(lj_str_new(ls->L, p, len)));
    } else if (tp == BCDUMP_KGC_TAB) {
      setgcref(*kr, obj2gco(bcread_ktab(ls)));
#if LJ_HASFFI
    } else if (tp != BCDUMP_KGC_CHILD) {
      CTypeID id = tp == BCDUMP_KGC_COMPLEX ? CTID_COMPLEX_DOUBLE :
		   tp == BCDUMP_KGC_I64 ? CTID_INT64 : CTID_UINT64;
      CTSize sz = tp == BCDUMP_KGC_COMPLEX ? 16 : 8;
      GCcdata *cd = lj_cdata_new_(ls->L, id, sz);
      TValue *p = (TValue *)cdataptr(cd);
      setgcref(*kr, obj2gco(cd));
      p[0].u32.lo = bcread_uleb128(ls);
      p[0].u32.hi = bcread_uleb128(ls);
      if (tp == BCDUMP_KGC_COMPLEX) {
	p[1].u32.lo = bcread_uleb128(ls);
	p[1].u32.hi = bcread_uleb128(ls);
      }
#endif
    } else {
      lua_State *L = ls->L;
      lua_assert(tp == BCDUMP_KGC_CHILD);
      if (L->top <= bcread_oldtop(L, ls))  /* Stack underflow? */
	bcread_error(ls, LJ_ERR_BCBAD);
      L->top--;
      setgcref(*kr, obj2gco(protoV(L->top)));
    }
  }
}

/* Read number constants of a prototype. */
static void bcread_knum(LexState *ls, GCproto *pt, MSize sizekn)
{
  MSize i;
  TValue *o = mref(pt->k, TValue);
  for (i = 0; i < sizekn; i++, o++) {
    int isnum = (ls->p[0] & 1);
    uint32_t lo = bcread_uleb128_33(ls);
    if (isnum) {
      o->u32.lo = lo;
      o->u32.hi = bcread_uleb128(ls);
    } else {
      setintV(o, lo);
    }
  }
}

/* Read bytecode instructions. */
static void bcread_bytecode(LexState *ls, GCproto *pt, MSize sizebc)
{
  BCIns *bc = proto_bc(pt);
  bc[0] = BCINS_AD((pt->flags & PROTO_VARARG) ? BC_FUNCV : BC_FUNCF,
		   pt->framesize, 0);
  bcread_block(ls, bc+1, (sizebc-1)*(MSize)sizeof(BCIns));
  /* Swap bytecode instructions if the endianess differs. */
  if (bcread_swap(ls)) {
    MSize i;
    for (i = 1; i < sizebc; i++) bc[i] = lj_bswap(bc[i]);
  }
}

/* Read upvalue refs. */
static void bcread_uv(LexState *ls, GCproto *pt, MSize sizeuv)
{
  if (sizeuv) {
    uint16_t *uv = proto_uv(pt);
    bcread_block(ls, uv, sizeuv*2);
    /* Swap upvalue refs if the endianess differs. */
    if (bcread_swap(ls)) {
      MSize i;
      for (i = 0; i < sizeuv; i++)
	uv[i] = (uint16_t)((uv[i] >> 8)|(uv[i] << 8));
    }
  }
}

/* Read a prototype. */
static GCproto *bcread_proto(LexState *ls)
{
  GCproto *pt;
  MSize framesize, numparams, flags, sizeuv, sizekgc, sizekn, sizebc, sizept;
  MSize ofsk, ofsuv, ofsdbg;
  MSize sizedbg = 0;
  BCLine firstline = 0, numline = 0;
  MSize len, startn;

  /* Read length. */
  if (ls->n > 0 && ls->p[0] == 0) {  /* Shortcut EOF. */
    ls->n--; ls->p++;
    return NULL;
  }
  bcread_want(ls, 5);
  len = bcread_uleb128(ls);
  if (!len) return NULL;  /* EOF */
  bcread_need(ls, len);
  startn = ls->n;

  /* Read prototype header. */
  flags = bcread_byte(ls);
  numparams = bcread_byte(ls);
  framesize = bcread_byte(ls);
  sizeuv = bcread_byte(ls);
  sizekgc = bcread_uleb128(ls);
  sizekn = bcread_uleb128(ls);
  sizebc = bcread_uleb128(ls) + 1;
  if (!(bcread_flags(ls) & BCDUMP_F_STRIP)) {
    sizedbg = bcread_uleb128(ls);
    if (sizedbg) {
      firstline = bcread_uleb128(ls);
      numline = bcread_uleb128(ls);
    }
  }

  /* Calculate total size of prototype including all colocated arrays. */
  sizept = (MSize)sizeof(GCproto) +
	   sizebc*(MSize)sizeof(BCIns) +
	   sizekgc*(MSize)sizeof(GCRef);
  sizept = (sizept + (MSize)sizeof(TValue)-1) & ~((MSize)sizeof(TValue)-1);
  ofsk = sizept; sizept += sizekn*(MSize)sizeof(TValue);
  ofsuv = sizept; sizept += ((sizeuv+1)&~1)*2;
  ofsdbg = sizept; sizept += sizedbg;

  /* Allocate prototype object and initialize its fields. */
  pt = (GCproto *)lj_mem_newgco(ls->L, (MSize)sizept);
  pt->gct = ~LJ_TPROTO;
  pt->numparams = (uint8_t)numparams;
  pt->framesize = (uint8_t)framesize;
  pt->sizebc = sizebc;
  setmref(pt->k, (char *)pt + ofsk);
  setmref(pt->uv, (char *)pt + ofsuv);
  pt->sizekgc = 0;  /* Set to zero until fully initialized. */
  pt->sizekn = sizekn;
  pt->sizept = sizept;
  pt->sizeuv = (uint8_t)sizeuv;
  pt->flags = (uint8_t)flags;
  pt->trace = 0;
  setgcref(pt->chunkname, obj2gco(ls->chunkname));

  /* Close potentially uninitialized gap between bc and kgc. */
  *(uint32_t *)((char *)pt + ofsk - sizeof(GCRef)*(sizekgc+1)) = 0;

  /* Read bytecode instructions and upvalue refs. */
  bcread_bytecode(ls, pt, sizebc);
  bcread_uv(ls, pt, sizeuv);

  /* Read constants. */
  bcread_kgc(ls, pt, sizekgc);
  pt->sizekgc = sizekgc;
  bcread_knum(ls, pt, sizekn);

  /* Read and initialize debug info. */
  pt->firstline = firstline;
  pt->numline = numline;
  if (sizedbg) {
    MSize sizeli = (sizebc-1) << (numline < 256 ? 0 : numline < 65536 ? 1 : 2);
    setmref(pt->lineinfo, (char *)pt + ofsdbg);
    setmref(pt->uvinfo, (char *)pt + ofsdbg + sizeli);
    bcread_dbg(ls, pt, sizedbg);
    setmref(pt->varinfo, bcread_varinfo(pt));
  } else {
    setmref(pt->lineinfo, NULL);
    setmref(pt->uvinfo, NULL);
    setmref(pt->varinfo, NULL);
  }

  if (len != startn - ls->n)
    bcread_error(ls, LJ_ERR_BCBAD);
  return pt;
}

/* Read and check header of bytecode dump. */
static int bcread_header(LexState *ls)
{
  uint32_t flags;
  bcread_want(ls, 3+5+5);
  if (bcread_byte(ls) != BCDUMP_HEAD2 ||
      bcread_byte(ls) != BCDUMP_HEAD3 ||
      bcread_byte(ls) != BCDUMP_VERSION) return 0;
  bcread_flags(ls) = flags = bcread_uleb128(ls);
  if ((flags & ~(BCDUMP_F_KNOWN)) != 0) return 0;
  if ((flags & BCDUMP_F_FFI)) {
#if LJ_HASFFI
    lua_State *L = ls->L;
    if (!ctype_ctsG(G(L))) {
      ptrdiff_t oldtop = savestack(L, L->top);
      luaopen_ffi(L);  /* Load FFI library on-demand. */
      L->top = restorestack(L, oldtop);
    }
#else
    return 0;
#endif
  }
  if ((flags & BCDUMP_F_STRIP)) {
    ls->chunkname = lj_str_newz(ls->L, ls->chunkarg);
  } else {
    MSize len = bcread_uleb128(ls);
    bcread_need(ls, len);
    ls->chunkname = lj_str_new(ls->L, (const char *)bcread_mem(ls, len), len);
  }
  return 1;  /* Ok. */
}

/* Read a bytecode dump. */
GCproto *lj_bcread(LexState *ls)
{
  lua_State *L = ls->L;
  lua_assert(ls->current == BCDUMP_HEAD1);
  bcread_savetop(L, ls, L->top);
  lj_str_resetbuf(&ls->sb);
  /* Check for a valid bytecode dump header. */
  if (!bcread_header(ls))
    bcread_error(ls, LJ_ERR_BCFMT);
  for (;;) {  /* Process all prototypes in the bytecode dump. */
    GCproto *pt = bcread_proto(ls);
    if (!pt) break;
    setprotoV(L, L->top, pt);
    incr_top(L);
  }
  if ((int32_t)ls->n > 0 || L->top-1 != bcread_oldtop(L, ls))
    bcread_error(ls, LJ_ERR_BCBAD);
  /* Pop off last prototype. */
  L->top--;
  return protoV(L->top);
}

