/*
** Bytecode writer.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#define lj_bcwrite_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_gc.h"
#include "lj_str.h"
#include "lj_bc.h"
#if LJ_HASFFI
#include "lj_ctype.h"
#endif
#if LJ_HASJIT
#include "lj_dispatch.h"
#include "lj_jit.h"
#endif
#include "lj_bcdump.h"
#include "lj_vm.h"

/* Context for bytecode writer. */
typedef struct BCWriteCtx {
  SBuf sb;			/* Output buffer. */
  lua_State *L;			/* Lua state. */
  GCproto *pt;			/* Root prototype. */
  lua_Writer wfunc;		/* Writer callback. */
  void *wdata;			/* Writer callback data. */
  int strip;			/* Strip debug info. */
  int status;			/* Status from writer callback. */
} BCWriteCtx;

/* -- Output buffer handling ---------------------------------------------- */

/* Resize buffer if needed. */
static LJ_NOINLINE void bcwrite_resize(BCWriteCtx *ctx, MSize len)
{
  MSize sz = ctx->sb.sz * 2;
  while (ctx->sb.n + len > sz) sz = sz * 2;
  lj_str_resizebuf(ctx->L, &ctx->sb, sz);
}

/* Need a certain amount of buffer space. */
static LJ_AINLINE void bcwrite_need(BCWriteCtx *ctx, MSize len)
{
  if (LJ_UNLIKELY(ctx->sb.n + len > ctx->sb.sz))
    bcwrite_resize(ctx, len);
}

/* Add memory block to buffer. */
static void bcwrite_block(BCWriteCtx *ctx, const void *p, MSize len)
{
  uint8_t *q = (uint8_t *)(ctx->sb.buf + ctx->sb.n);
  MSize i;
  ctx->sb.n += len;
  for (i = 0; i < len; i++) q[i] = ((uint8_t *)p)[i];
}

/* Add byte to buffer. */
static LJ_AINLINE void bcwrite_byte(BCWriteCtx *ctx, uint8_t b)
{
  ctx->sb.buf[ctx->sb.n++] = b;
}

/* Add ULEB128 value to buffer. */
static void bcwrite_uleb128(BCWriteCtx *ctx, uint32_t v)
{
  MSize n = ctx->sb.n;
  uint8_t *p = (uint8_t *)ctx->sb.buf;
  for (; v >= 0x80; v >>= 7)
    p[n++] = (uint8_t)((v & 0x7f) | 0x80);
  p[n++] = (uint8_t)v;
  ctx->sb.n = n;
}

/* -- Bytecode writer ----------------------------------------------------- */

/* Write a single constant key/value of a template table. */
static void bcwrite_ktabk(BCWriteCtx *ctx, cTValue *o, int narrow)
{
  bcwrite_need(ctx, 1+10);
  if (tvisstr(o)) {
    const GCstr *str = strV(o);
    MSize len = str->len;
    bcwrite_need(ctx, 5+len);
    bcwrite_uleb128(ctx, BCDUMP_KTAB_STR+len);
    bcwrite_block(ctx, strdata(str), len);
  } else if (tvisint(o)) {
    bcwrite_byte(ctx, BCDUMP_KTAB_INT);
    bcwrite_uleb128(ctx, intV(o));
  } else if (tvisnum(o)) {
    if (!LJ_DUALNUM && narrow) {  /* Narrow number constants to integers. */
      lua_Number num = numV(o);
      int32_t k = lj_num2int(num);
      if (num == (lua_Number)k) {  /* -0 is never a constant. */
	bcwrite_byte(ctx, BCDUMP_KTAB_INT);
	bcwrite_uleb128(ctx, k);
	return;
      }
    }
    bcwrite_byte(ctx, BCDUMP_KTAB_NUM);
    bcwrite_uleb128(ctx, o->u32.lo);
    bcwrite_uleb128(ctx, o->u32.hi);
  } else {
    lua_assert(tvispri(o));
    bcwrite_byte(ctx, BCDUMP_KTAB_NIL+~itype(o));
  }
}

/* Write a template table. */
static void bcwrite_ktab(BCWriteCtx *ctx, const GCtab *t)
{
  MSize narray = 0, nhash = 0;
  if (t->asize > 0) {  /* Determine max. length of array part. */
    ptrdiff_t i;
    TValue *array = tvref(t->array);
    for (i = (ptrdiff_t)t->asize-1; i >= 0; i--)
      if (!tvisnil(&array[i]))
	break;
    narray = (MSize)(i+1);
  }
  if (t->hmask > 0) {  /* Count number of used hash slots. */
    MSize i, hmask = t->hmask;
    Node *node = noderef(t->node);
    for (i = 0; i <= hmask; i++)
      nhash += !tvisnil(&node[i].val);
  }
  /* Write number of array slots and hash slots. */
  bcwrite_uleb128(ctx, narray);
  bcwrite_uleb128(ctx, nhash);
  if (narray) {  /* Write array entries (may contain nil). */
    MSize i;
    TValue *o = tvref(t->array);
    for (i = 0; i < narray; i++, o++)
      bcwrite_ktabk(ctx, o, 1);
  }
  if (nhash) {  /* Write hash entries. */
    MSize i = nhash;
    Node *node = noderef(t->node) + t->hmask;
    for (;; node--)
      if (!tvisnil(&node->val)) {
	bcwrite_ktabk(ctx, &node->key, 0);
	bcwrite_ktabk(ctx, &node->val, 1);
	if (--i == 0) break;
      }
  }
}

/* Write GC constants of a prototype. */
static void bcwrite_kgc(BCWriteCtx *ctx, GCproto *pt)
{
  MSize i, sizekgc = pt->sizekgc;
  GCRef *kr = mref(pt->k, GCRef) - (ptrdiff_t)sizekgc;
  for (i = 0; i < sizekgc; i++, kr++) {
    GCobj *o = gcref(*kr);
    MSize tp, need = 1;
    /* Determine constant type and needed size. */
    if (o->gch.gct == ~LJ_TSTR) {
      tp = BCDUMP_KGC_STR + gco2str(o)->len;
      need = 5+gco2str(o)->len;
    } else if (o->gch.gct == ~LJ_TPROTO) {
      lua_assert((pt->flags & PROTO_CHILD));
      tp = BCDUMP_KGC_CHILD;
#if LJ_HASFFI
    } else if (o->gch.gct == ~LJ_TCDATA) {
      CTypeID id = gco2cd(o)->ctypeid;
      need = 1+4*5;
      if (id == CTID_INT64) {
	tp = BCDUMP_KGC_I64;
      } else if (id == CTID_UINT64) {
	tp = BCDUMP_KGC_U64;
      } else {
	lua_assert(id == CTID_COMPLEX_DOUBLE);
	tp = BCDUMP_KGC_COMPLEX;
      }
#endif
    } else {
      lua_assert(o->gch.gct == ~LJ_TTAB);
      tp = BCDUMP_KGC_TAB;
      need = 1+2*5;
    }
    /* Write constant type. */
    bcwrite_need(ctx, need);
    bcwrite_uleb128(ctx, tp);
    /* Write constant data (if any). */
    if (tp >= BCDUMP_KGC_STR) {
      bcwrite_block(ctx, strdata(gco2str(o)), gco2str(o)->len);
    } else if (tp == BCDUMP_KGC_TAB) {
      bcwrite_ktab(ctx, gco2tab(o));
#if LJ_HASFFI
    } else if (tp != BCDUMP_KGC_CHILD) {
      cTValue *p = (TValue *)cdataptr(gco2cd(o));
      bcwrite_uleb128(ctx, p[0].u32.lo);
      bcwrite_uleb128(ctx, p[0].u32.hi);
      if (tp == BCDUMP_KGC_COMPLEX) {
	bcwrite_uleb128(ctx, p[1].u32.lo);
	bcwrite_uleb128(ctx, p[1].u32.hi);
      }
#endif
    }
  }
}

/* Write number constants of a prototype. */
static void bcwrite_knum(BCWriteCtx *ctx, GCproto *pt)
{
  MSize i, sizekn = pt->sizekn;
  cTValue *o = mref(pt->k, TValue);
  bcwrite_need(ctx, 10*sizekn);
  for (i = 0; i < sizekn; i++, o++) {
    int32_t k;
    if (tvisint(o)) {
      k = intV(o);
      goto save_int;
    } else {
      /* Write a 33 bit ULEB128 for the int (lsb=0) or loword (lsb=1). */
      if (!LJ_DUALNUM) {  /* Narrow number constants to integers. */
	lua_Number num = numV(o);
	k = lj_num2int(num);
	if (num == (lua_Number)k) {  /* -0 is never a constant. */
	save_int:
	  bcwrite_uleb128(ctx, 2*(uint32_t)k | ((uint32_t)k & 0x80000000u));
	  if (k < 0) {
	    char *p = &ctx->sb.buf[ctx->sb.n-1];
	    *p = (*p & 7) | ((k>>27) & 0x18);
	  }
	  continue;
	}
      }
      bcwrite_uleb128(ctx, 1+(2*o->u32.lo | (o->u32.lo & 0x80000000u)));
      if (o->u32.lo >= 0x80000000u) {
	char *p = &ctx->sb.buf[ctx->sb.n-1];
	*p = (*p & 7) | ((o->u32.lo>>27) & 0x18);
      }
      bcwrite_uleb128(ctx, o->u32.hi);
    }
  }
}

/* Write bytecode instructions. */
static void bcwrite_bytecode(BCWriteCtx *ctx, GCproto *pt)
{
  MSize nbc = pt->sizebc-1;  /* Omit the [JI]FUNC* header. */
#if LJ_HASJIT
  uint8_t *p = (uint8_t *)&ctx->sb.buf[ctx->sb.n];
#endif
  bcwrite_block(ctx, proto_bc(pt)+1, nbc*(MSize)sizeof(BCIns));
#if LJ_HASJIT
  /* Unpatch modified bytecode containing ILOOP/JLOOP etc. */
  if ((pt->flags & PROTO_ILOOP) || pt->trace) {
    jit_State *J = L2J(ctx->L);
    MSize i;
    for (i = 0; i < nbc; i++, p += sizeof(BCIns)) {
      BCOp op = (BCOp)p[LJ_ENDIAN_SELECT(0, 3)];
      if (op == BC_IFORL || op == BC_IITERL || op == BC_ILOOP ||
	  op == BC_JFORI) {
	p[LJ_ENDIAN_SELECT(0, 3)] = (uint8_t)(op-BC_IFORL+BC_FORL);
      } else if (op == BC_JFORL || op == BC_JITERL || op == BC_JLOOP) {
	BCReg rd = p[LJ_ENDIAN_SELECT(2, 1)] + (p[LJ_ENDIAN_SELECT(3, 0)] << 8);
	BCIns ins = traceref(J, rd)->startins;
	p[LJ_ENDIAN_SELECT(0, 3)] = (uint8_t)(op-BC_JFORL+BC_FORL);
	p[LJ_ENDIAN_SELECT(2, 1)] = bc_c(ins);
	p[LJ_ENDIAN_SELECT(3, 0)] = bc_b(ins);
      }
    }
  }
#endif
}

/* Write prototype. */
static void bcwrite_proto(BCWriteCtx *ctx, GCproto *pt)
{
  MSize sizedbg = 0;

  /* Recursively write children of prototype. */
  if ((pt->flags & PROTO_CHILD)) {
    ptrdiff_t i, n = pt->sizekgc;
    GCRef *kr = mref(pt->k, GCRef) - 1;
    for (i = 0; i < n; i++, kr--) {
      GCobj *o = gcref(*kr);
      if (o->gch.gct == ~LJ_TPROTO)
	bcwrite_proto(ctx, gco2pt(o));
    }
  }

  /* Start writing the prototype info to a buffer. */
  lj_str_resetbuf(&ctx->sb);
  ctx->sb.n = 5;  /* Leave room for final size. */
  bcwrite_need(ctx, 4+6*5+(pt->sizebc-1)*(MSize)sizeof(BCIns)+pt->sizeuv*2);

  /* Write prototype header. */
  bcwrite_byte(ctx, (pt->flags & (PROTO_CHILD|PROTO_VARARG|PROTO_FFI)));
  bcwrite_byte(ctx, pt->numparams);
  bcwrite_byte(ctx, pt->framesize);
  bcwrite_byte(ctx, pt->sizeuv);
  bcwrite_uleb128(ctx, pt->sizekgc);
  bcwrite_uleb128(ctx, pt->sizekn);
  bcwrite_uleb128(ctx, pt->sizebc-1);
  if (!ctx->strip) {
    if (proto_lineinfo(pt))
      sizedbg = pt->sizept - (MSize)((char *)proto_lineinfo(pt) - (char *)pt);
    bcwrite_uleb128(ctx, sizedbg);
    if (sizedbg) {
      bcwrite_uleb128(ctx, pt->firstline);
      bcwrite_uleb128(ctx, pt->numline);
    }
  }

  /* Write bytecode instructions and upvalue refs. */
  bcwrite_bytecode(ctx, pt);
  bcwrite_block(ctx, proto_uv(pt), pt->sizeuv*2);

  /* Write constants. */
  bcwrite_kgc(ctx, pt);
  bcwrite_knum(ctx, pt);

  /* Write debug info, if not stripped. */
  if (sizedbg) {
    bcwrite_need(ctx, sizedbg);
    bcwrite_block(ctx, proto_lineinfo(pt), sizedbg);
  }

  /* Pass buffer to writer function. */
  if (ctx->status == 0) {
    MSize n = ctx->sb.n - 5;
    MSize nn = (lj_fls(n)+8)*9 >> 6;
    ctx->sb.n = 5 - nn;
    bcwrite_uleb128(ctx, n);  /* Fill in final size. */
    lua_assert(ctx->sb.n == 5);
    ctx->status = ctx->wfunc(ctx->L, ctx->sb.buf+5-nn, nn+n, ctx->wdata);
  }
}

/* Write header of bytecode dump. */
static void bcwrite_header(BCWriteCtx *ctx)
{
  GCstr *chunkname = proto_chunkname(ctx->pt);
  const char *name = strdata(chunkname);
  MSize len = chunkname->len;
  lj_str_resetbuf(&ctx->sb);
  bcwrite_need(ctx, 5+5+len);
  bcwrite_byte(ctx, BCDUMP_HEAD1);
  bcwrite_byte(ctx, BCDUMP_HEAD2);
  bcwrite_byte(ctx, BCDUMP_HEAD3);
  bcwrite_byte(ctx, BCDUMP_VERSION);
  bcwrite_byte(ctx, (ctx->strip ? BCDUMP_F_STRIP : 0) +
		   (LJ_BE ? BCDUMP_F_BE : 0) +
		   ((ctx->pt->flags & PROTO_FFI) ? BCDUMP_F_FFI : 0));
  if (!ctx->strip) {
    bcwrite_uleb128(ctx, len);
    bcwrite_block(ctx, name, len);
  }
  ctx->status = ctx->wfunc(ctx->L, ctx->sb.buf, ctx->sb.n, ctx->wdata);
}

/* Write footer of bytecode dump. */
static void bcwrite_footer(BCWriteCtx *ctx)
{
  if (ctx->status == 0) {
    uint8_t zero = 0;
    ctx->status = ctx->wfunc(ctx->L, &zero, 1, ctx->wdata);
  }
}

/* Protected callback for bytecode writer. */
static TValue *cpwriter(lua_State *L, lua_CFunction dummy, void *ud)
{
  BCWriteCtx *ctx = (BCWriteCtx *)ud;
  UNUSED(dummy);
  lj_str_resizebuf(L, &ctx->sb, 1024);  /* Avoids resize for most prototypes. */
  bcwrite_header(ctx);
  bcwrite_proto(ctx, ctx->pt);
  bcwrite_footer(ctx);
  return NULL;
}

/* Write bytecode for a prototype. */
int lj_bcwrite(lua_State *L, GCproto *pt, lua_Writer writer, void *data,
	      int strip)
{
  BCWriteCtx ctx;
  int status;
  ctx.L = L;
  ctx.pt = pt;
  ctx.wfunc = writer;
  ctx.wdata = data;
  ctx.strip = strip;
  ctx.status = 0;
  lj_str_initbuf(&ctx.sb);
  status = lj_vm_cpcall(L, NULL, &ctx, cpwriter);
  if (status == 0) status = ctx.status;
  lj_str_freebuf(G(ctx.L), &ctx.sb);
  return status;
}

