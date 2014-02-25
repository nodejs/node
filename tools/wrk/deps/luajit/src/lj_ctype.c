/*
** C type management.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_tab.h"
#include "lj_ctype.h"
#include "lj_ccallback.h"

/* -- C type definitions -------------------------------------------------- */

/* Predefined typedefs. */
#define CTTDDEF(_) \
  /* Vararg handling. */ \
  _("va_list",			P_VOID) \
  _("__builtin_va_list",	P_VOID) \
  _("__gnuc_va_list",		P_VOID) \
  /* From stddef.h. */ \
  _("ptrdiff_t",		INT_PSZ) \
  _("size_t",			UINT_PSZ) \
  _("wchar_t",			WCHAR) \
  /* Subset of stdint.h. */ \
  _("int8_t",			INT8) \
  _("int16_t",			INT16) \
  _("int32_t",			INT32) \
  _("int64_t",			INT64) \
  _("uint8_t",			UINT8) \
  _("uint16_t",			UINT16) \
  _("uint32_t",			UINT32) \
  _("uint64_t",			UINT64) \
  _("intptr_t",			INT_PSZ) \
  _("uintptr_t",		UINT_PSZ) \
  /* End of typedef list. */

/* Keywords (only the ones we actually care for). */
#define CTKWDEF(_) \
  /* Type specifiers. */ \
  _("void",		-1,	CTOK_VOID) \
  _("_Bool",		0,	CTOK_BOOL) \
  _("bool",		1,	CTOK_BOOL) \
  _("char",		1,	CTOK_CHAR) \
  _("int",		4,	CTOK_INT) \
  _("__int8",		1,	CTOK_INT) \
  _("__int16",		2,	CTOK_INT) \
  _("__int32",		4,	CTOK_INT) \
  _("__int64",		8,	CTOK_INT) \
  _("float",		4,	CTOK_FP) \
  _("double",		8,	CTOK_FP) \
  _("long",		0,	CTOK_LONG) \
  _("short",		0,	CTOK_SHORT) \
  _("_Complex",		0,	CTOK_COMPLEX) \
  _("complex",		0,	CTOK_COMPLEX) \
  _("__complex",	0,	CTOK_COMPLEX) \
  _("__complex__",	0,	CTOK_COMPLEX) \
  _("signed",		0,	CTOK_SIGNED) \
  _("__signed",		0,	CTOK_SIGNED) \
  _("__signed__",	0,	CTOK_SIGNED) \
  _("unsigned",		0,	CTOK_UNSIGNED) \
  /* Type qualifiers. */ \
  _("const",		0,	CTOK_CONST) \
  _("__const",		0,	CTOK_CONST) \
  _("__const__",	0,	CTOK_CONST) \
  _("volatile",		0,	CTOK_VOLATILE) \
  _("__volatile",	0,	CTOK_VOLATILE) \
  _("__volatile__",	0,	CTOK_VOLATILE) \
  _("restrict",		0,	CTOK_RESTRICT) \
  _("__restrict",	0,	CTOK_RESTRICT) \
  _("__restrict__",	0,	CTOK_RESTRICT) \
  _("inline",		0,	CTOK_INLINE) \
  _("__inline",		0,	CTOK_INLINE) \
  _("__inline__",	0,	CTOK_INLINE) \
  /* Storage class specifiers. */ \
  _("typedef",		0,	CTOK_TYPEDEF) \
  _("extern",		0,	CTOK_EXTERN) \
  _("static",		0,	CTOK_STATIC) \
  _("auto",		0,	CTOK_AUTO) \
  _("register",		0,	CTOK_REGISTER) \
  /* GCC Attributes. */ \
  _("__extension__",	0,	CTOK_EXTENSION) \
  _("__attribute",	0,	CTOK_ATTRIBUTE) \
  _("__attribute__",	0,	CTOK_ATTRIBUTE) \
  _("asm",		0,	CTOK_ASM) \
  _("__asm",		0,	CTOK_ASM) \
  _("__asm__",		0,	CTOK_ASM) \
  /* MSVC Attributes. */ \
  _("__declspec",	0,	CTOK_DECLSPEC) \
  _("__cdecl",		CTCC_CDECL,	CTOK_CCDECL) \
  _("__thiscall",	CTCC_THISCALL,	CTOK_CCDECL) \
  _("__fastcall",	CTCC_FASTCALL,	CTOK_CCDECL) \
  _("__stdcall",	CTCC_STDCALL,	CTOK_CCDECL) \
  _("__ptr32",		4,	CTOK_PTRSZ) \
  _("__ptr64",		8,	CTOK_PTRSZ) \
  /* Other type specifiers. */ \
  _("struct",		0,	CTOK_STRUCT) \
  _("union",		0,	CTOK_UNION) \
  _("enum",		0,	CTOK_ENUM) \
  /* Operators. */ \
  _("sizeof",		0,	CTOK_SIZEOF) \
  _("__alignof",	0,	CTOK_ALIGNOF) \
  _("__alignof__",	0,	CTOK_ALIGNOF) \
  /* End of keyword list. */

/* Type info for predefined types. Size merged in. */
static CTInfo lj_ctype_typeinfo[] = {
#define CTTYINFODEF(id, sz, ct, info)	CTINFO((ct),(((sz)&0x3fu)<<10)+(info)),
#define CTTDINFODEF(name, id)		CTINFO(CT_TYPEDEF, CTID_##id),
#define CTKWINFODEF(name, sz, kw)	CTINFO(CT_KW,(((sz)&0x3fu)<<10)+(kw)),
CTTYDEF(CTTYINFODEF)
CTTDDEF(CTTDINFODEF)
CTKWDEF(CTKWINFODEF)
#undef CTTYINFODEF
#undef CTTDINFODEF
#undef CTKWINFODEF
  0
};

/* Predefined type names collected in a single string. */
static const char * const lj_ctype_typenames =
#define CTTDNAMEDEF(name, id)		name "\0"
#define CTKWNAMEDEF(name, sz, cds)	name "\0"
CTTDDEF(CTTDNAMEDEF)
CTKWDEF(CTKWNAMEDEF)
#undef CTTDNAMEDEF
#undef CTKWNAMEDEF
;

#define CTTYPEINFO_NUM		(sizeof(lj_ctype_typeinfo)/sizeof(CTInfo)-1)
#ifdef LUAJIT_CTYPE_CHECK_ANCHOR
#define CTTYPETAB_MIN		CTTYPEINFO_NUM
#else
#define CTTYPETAB_MIN		128
#endif

/* -- C type interning ---------------------------------------------------- */

#define ct_hashtype(info, size)	(hashrot(info, size) & CTHASH_MASK)
#define ct_hashname(name) \
  (hashrot(u32ptr(name), u32ptr(name) + HASH_BIAS) & CTHASH_MASK)

/* Create new type element. */
CTypeID lj_ctype_new(CTState *cts, CType **ctp)
{
  CTypeID id = cts->top;
  CType *ct;
  lua_assert(cts->L);
  if (LJ_UNLIKELY(id >= cts->sizetab)) {
    if (id >= CTID_MAX) lj_err_msg(cts->L, LJ_ERR_TABOV);
#ifdef LUAJIT_CTYPE_CHECK_ANCHOR
    ct = lj_mem_newvec(cts->L, id+1, CType);
    memcpy(ct, cts->tab, id*sizeof(CType));
    memset(cts->tab, 0, id*sizeof(CType));
    lj_mem_freevec(cts->g, cts->tab, cts->sizetab, CType);
    cts->tab = ct;
    cts->sizetab = id+1;
#else
    lj_mem_growvec(cts->L, cts->tab, cts->sizetab, CTID_MAX, CType);
#endif
  }
  cts->top = id+1;
  *ctp = ct = &cts->tab[id];
  ct->info = 0;
  ct->size = 0;
  ct->sib = 0;
  ct->next = 0;
  setgcrefnull(ct->name);
  return id;
}

/* Intern a type element. */
CTypeID lj_ctype_intern(CTState *cts, CTInfo info, CTSize size)
{
  uint32_t h = ct_hashtype(info, size);
  CTypeID id = cts->hash[h];
  lua_assert(cts->L);
  while (id) {
    CType *ct = ctype_get(cts, id);
    if (ct->info == info && ct->size == size)
      return id;
    id = ct->next;
  }
  id = cts->top;
  if (LJ_UNLIKELY(id >= cts->sizetab)) {
    if (id >= CTID_MAX) lj_err_msg(cts->L, LJ_ERR_TABOV);
    lj_mem_growvec(cts->L, cts->tab, cts->sizetab, CTID_MAX, CType);
  }
  cts->top = id+1;
  cts->tab[id].info = info;
  cts->tab[id].size = size;
  cts->tab[id].sib = 0;
  cts->tab[id].next = cts->hash[h];
  setgcrefnull(cts->tab[id].name);
  cts->hash[h] = (CTypeID1)id;
  return id;
}

/* Add type element to hash table. */
static void ctype_addtype(CTState *cts, CType *ct, CTypeID id)
{
  uint32_t h = ct_hashtype(ct->info, ct->size);
  ct->next = cts->hash[h];
  cts->hash[h] = (CTypeID1)id;
}

/* Add named element to hash table. */
void lj_ctype_addname(CTState *cts, CType *ct, CTypeID id)
{
  uint32_t h = ct_hashname(gcref(ct->name));
  ct->next = cts->hash[h];
  cts->hash[h] = (CTypeID1)id;
}

/* Get a C type by name, matching the type mask. */
CTypeID lj_ctype_getname(CTState *cts, CType **ctp, GCstr *name, uint32_t tmask)
{
  CTypeID id = cts->hash[ct_hashname(name)];
  while (id) {
    CType *ct = ctype_get(cts, id);
    if (gcref(ct->name) == obj2gco(name) &&
	((tmask >> ctype_type(ct->info)) & 1)) {
      *ctp = ct;
      return id;
    }
    id = ct->next;
  }
  *ctp = &cts->tab[0];  /* Simplify caller logic. ctype_get() would assert. */
  return 0;
}

/* Get a struct/union/enum/function field by name. */
CType *lj_ctype_getfieldq(CTState *cts, CType *ct, GCstr *name, CTSize *ofs,
			  CTInfo *qual)
{
  while (ct->sib) {
    ct = ctype_get(cts, ct->sib);
    if (gcref(ct->name) == obj2gco(name)) {
      *ofs = ct->size;
      return ct;
    }
    if (ctype_isxattrib(ct->info, CTA_SUBTYPE)) {
      CType *fct, *cct = ctype_child(cts, ct);
      CTInfo q = 0;
      while (ctype_isattrib(cct->info)) {
	if (ctype_attrib(cct->info) == CTA_QUAL) q |= cct->size;
	cct = ctype_child(cts, cct);
      }
      fct = lj_ctype_getfieldq(cts, cct, name, ofs, qual);
      if (fct) {
	if (qual) *qual |= q;
	*ofs += ct->size;
	return fct;
      }
    }
  }
  return NULL;  /* Not found. */
}

/* -- C type information -------------------------------------------------- */

/* Follow references and get raw type for a C type ID. */
CType *lj_ctype_rawref(CTState *cts, CTypeID id)
{
  CType *ct = ctype_get(cts, id);
  while (ctype_isattrib(ct->info) || ctype_isref(ct->info))
    ct = ctype_child(cts, ct);
  return ct;
}

/* Get size for a C type ID. Does NOT support VLA/VLS. */
CTSize lj_ctype_size(CTState *cts, CTypeID id)
{
  CType *ct = ctype_raw(cts, id);
  return ctype_hassize(ct->info) ? ct->size : CTSIZE_INVALID;
}

/* Get size for a variable-length C type. Does NOT support other C types. */
CTSize lj_ctype_vlsize(CTState *cts, CType *ct, CTSize nelem)
{
  uint64_t xsz = 0;
  if (ctype_isstruct(ct->info)) {
    CTypeID arrid = 0, fid = ct->sib;
    xsz = ct->size;  /* Add the struct size. */
    while (fid) {
      CType *ctf = ctype_get(cts, fid);
      if (ctype_type(ctf->info) == CT_FIELD)
	arrid = ctype_cid(ctf->info);  /* Remember last field of VLS. */
      fid = ctf->sib;
    }
    ct = ctype_raw(cts, arrid);
  }
  lua_assert(ctype_isvlarray(ct->info));  /* Must be a VLA. */
  ct = ctype_rawchild(cts, ct);  /* Get array element. */
  lua_assert(ctype_hassize(ct->info));
  /* Calculate actual size of VLA and check for overflow. */
  xsz += (uint64_t)ct->size * nelem;
  return xsz < 0x80000000u ? (CTSize)xsz : CTSIZE_INVALID;
}

/* Get type, qualifiers, size and alignment for a C type ID. */
CTInfo lj_ctype_info(CTState *cts, CTypeID id, CTSize *szp)
{
  CTInfo qual = 0;
  CType *ct = ctype_get(cts, id);
  for (;;) {
    CTInfo info = ct->info;
    if (ctype_isenum(info)) {
      /* Follow child. Need to look at its attributes, too. */
    } else if (ctype_isattrib(info)) {
      if (ctype_isxattrib(info, CTA_QUAL))
	qual |= ct->size;
      else if (ctype_isxattrib(info, CTA_ALIGN) && !(qual & CTFP_ALIGNED))
	qual |= CTFP_ALIGNED + CTALIGN(ct->size);
    } else {
      if (!(qual & CTFP_ALIGNED)) qual |= (info & CTF_ALIGN);
      qual |= (info & ~(CTF_ALIGN|CTMASK_CID));
      lua_assert(ctype_hassize(info) || ctype_isfunc(info));
      *szp = ctype_isfunc(info) ? CTSIZE_INVALID : ct->size;
      break;
    }
    ct = ctype_get(cts, ctype_cid(info));
  }
  return qual;
}

/* Get ctype metamethod. */
cTValue *lj_ctype_meta(CTState *cts, CTypeID id, MMS mm)
{
  CType *ct = ctype_get(cts, id);
  cTValue *tv;
  while (ctype_isattrib(ct->info) || ctype_isref(ct->info)) {
    id = ctype_cid(ct->info);
    ct = ctype_get(cts, id);
  }
  if (ctype_isptr(ct->info) &&
      ctype_isfunc(ctype_get(cts, ctype_cid(ct->info))->info))
    tv = lj_tab_getstr(cts->miscmap, &cts->g->strempty);
  else
    tv = lj_tab_getinth(cts->miscmap, -(int32_t)id);
  if (tv && tvistab(tv) &&
      (tv = lj_tab_getstr(tabV(tv), mmname_str(cts->g, mm))) && !tvisnil(tv))
    return tv;
  return NULL;
}

/* -- C type representation ----------------------------------------------- */

/* Fixed max. length of a C type representation. */
#define CTREPR_MAX		512

typedef struct CTRepr {
  char *pb, *pe;
  CTState *cts;
  lua_State *L;
  int needsp;
  int ok;
  char buf[CTREPR_MAX];
} CTRepr;

/* Prepend string. */
static void ctype_prepstr(CTRepr *ctr, const char *str, MSize len)
{
  char *p = ctr->pb;
  if (ctr->buf + len+1 > p) { ctr->ok = 0; return; }
  if (ctr->needsp) *--p = ' ';
  ctr->needsp = 1;
  p -= len;
  while (len-- > 0) p[len] = str[len];
  ctr->pb = p;
}

#define ctype_preplit(ctr, str)	ctype_prepstr((ctr), "" str, sizeof(str)-1)

/* Prepend char. */
static void ctype_prepc(CTRepr *ctr, int c)
{
  if (ctr->buf >= ctr->pb) { ctr->ok = 0; return; }
  *--ctr->pb = c;
}

/* Prepend number. */
static void ctype_prepnum(CTRepr *ctr, uint32_t n)
{
  char *p = ctr->pb;
  if (ctr->buf + 10+1 > p) { ctr->ok = 0; return; }
  do { *--p = (char)('0' + n % 10); } while (n /= 10);
  ctr->pb = p;
  ctr->needsp = 0;
}

/* Append char. */
static void ctype_appc(CTRepr *ctr, int c)
{
  if (ctr->pe >= ctr->buf + CTREPR_MAX) { ctr->ok = 0; return; }
  *ctr->pe++ = c;
}

/* Append number. */
static void ctype_appnum(CTRepr *ctr, uint32_t n)
{
  char buf[10];
  char *p = buf+sizeof(buf);
  char *q = ctr->pe;
  if (q > ctr->buf + CTREPR_MAX - 10) { ctr->ok = 0; return; }
  do { *--p = (char)('0' + n % 10); } while (n /= 10);
  do { *q++ = *p++; } while (p < buf+sizeof(buf));
  ctr->pe = q;
}

/* Prepend qualifiers. */
static void ctype_prepqual(CTRepr *ctr, CTInfo info)
{
  if ((info & CTF_VOLATILE)) ctype_preplit(ctr, "volatile");
  if ((info & CTF_CONST)) ctype_preplit(ctr, "const");
}

/* Prepend named type. */
static void ctype_preptype(CTRepr *ctr, CType *ct, CTInfo qual, const char *t)
{
  if (gcref(ct->name)) {
    GCstr *str = gco2str(gcref(ct->name));
    ctype_prepstr(ctr, strdata(str), str->len);
  } else {
    if (ctr->needsp) ctype_prepc(ctr, ' ');
    ctype_prepnum(ctr, ctype_typeid(ctr->cts, ct));
    ctr->needsp = 1;
  }
  ctype_prepstr(ctr, t, (MSize)strlen(t));
  ctype_prepqual(ctr, qual);
}

static void ctype_repr(CTRepr *ctr, CTypeID id)
{
  CType *ct = ctype_get(ctr->cts, id);
  CTInfo qual = 0;
  int ptrto = 0;
  for (;;) {
    CTInfo info = ct->info;
    CTSize size = ct->size;
    switch (ctype_type(info)) {
    case CT_NUM:
      if ((info & CTF_BOOL)) {
	ctype_preplit(ctr, "bool");
      } else if ((info & CTF_FP)) {
	if (size == sizeof(double)) ctype_preplit(ctr, "double");
	else if (size == sizeof(float)) ctype_preplit(ctr, "float");
	else ctype_preplit(ctr, "long double");
      } else if (size == 1) {
	if (!((info ^ CTF_UCHAR) & CTF_UNSIGNED)) ctype_preplit(ctr, "char");
	else if (CTF_UCHAR) ctype_preplit(ctr, "signed char");
	else ctype_preplit(ctr, "unsigned char");
      } else if (size < 8) {
	if (size == 4) ctype_preplit(ctr, "int");
	else ctype_preplit(ctr, "short");
	if ((info & CTF_UNSIGNED)) ctype_preplit(ctr, "unsigned");
      } else {
	ctype_preplit(ctr, "_t");
	ctype_prepnum(ctr, size*8);
	ctype_preplit(ctr, "int");
	if ((info & CTF_UNSIGNED)) ctype_prepc(ctr, 'u');
      }
      ctype_prepqual(ctr, (qual|info));
      return;
    case CT_VOID:
      ctype_preplit(ctr, "void");
      ctype_prepqual(ctr, (qual|info));
      return;
    case CT_STRUCT:
      ctype_preptype(ctr, ct, qual, (info & CTF_UNION) ? "union" : "struct");
      return;
    case CT_ENUM:
      if (id == CTID_CTYPEID) {
	ctype_preplit(ctr, "ctype");
	return;
      }
      ctype_preptype(ctr, ct, qual, "enum");
      return;
    case CT_ATTRIB:
      if (ctype_attrib(info) == CTA_QUAL) qual |= size;
      break;
    case CT_PTR:
      if ((info & CTF_REF)) {
	ctype_prepc(ctr, '&');
      } else {
	ctype_prepqual(ctr, (qual|info));
	if (LJ_64 && size == 4) ctype_preplit(ctr, "__ptr32");
	ctype_prepc(ctr, '*');
      }
      qual = 0;
      ptrto = 1;
      ctr->needsp = 1;
      break;
    case CT_ARRAY:
      if (ctype_isrefarray(info)) {
	ctr->needsp = 1;
	if (ptrto) { ptrto = 0; ctype_prepc(ctr, '('); ctype_appc(ctr, ')'); }
	ctype_appc(ctr, '[');
	if (size != CTSIZE_INVALID) {
	  CTSize csize = ctype_child(ctr->cts, ct)->size;
	  ctype_appnum(ctr, csize ? size/csize : 0);
	} else if ((info & CTF_VLA)) {
	  ctype_appc(ctr, '?');
	}
	ctype_appc(ctr, ']');
      } else if ((info & CTF_COMPLEX)) {
	if (size == 2*sizeof(float)) ctype_preplit(ctr, "float");
	ctype_preplit(ctr, "complex");
	return;
      } else {
	ctype_preplit(ctr, ")))");
	ctype_prepnum(ctr, size);
	ctype_preplit(ctr, "__attribute__((vector_size(");
      }
      break;
    case CT_FUNC:
      ctr->needsp = 1;
      if (ptrto) { ptrto = 0; ctype_prepc(ctr, '('); ctype_appc(ctr, ')'); }
      ctype_appc(ctr, '(');
      ctype_appc(ctr, ')');
      break;
    default:
      lua_assert(0);
      break;
    }
    ct = ctype_get(ctr->cts, ctype_cid(info));
  }
}

/* Return a printable representation of a C type. */
GCstr *lj_ctype_repr(lua_State *L, CTypeID id, GCstr *name)
{
  global_State *g = G(L);
  CTRepr ctr;
  ctr.pb = ctr.pe = &ctr.buf[CTREPR_MAX/2];
  ctr.cts = ctype_ctsG(g);
  ctr.L = L;
  ctr.ok = 1;
  ctr.needsp = 0;
  if (name) ctype_prepstr(&ctr, strdata(name), name->len);
  ctype_repr(&ctr, id);
  if (LJ_UNLIKELY(!ctr.ok)) return lj_str_newlit(L, "?");
  return lj_str_new(L, ctr.pb, ctr.pe - ctr.pb);
}

/* Convert int64_t/uint64_t to string with 'LL' or 'ULL' suffix. */
GCstr *lj_ctype_repr_int64(lua_State *L, uint64_t n, int isunsigned)
{
  char buf[1+20+3];
  char *p = buf+sizeof(buf);
  int sign = 0;
  *--p = 'L'; *--p = 'L';
  if (isunsigned) {
    *--p = 'U';
  } else if ((int64_t)n < 0) {
    n = (uint64_t)-(int64_t)n;
    sign = 1;
  }
  do { *--p = (char)('0' + n % 10); } while (n /= 10);
  if (sign) *--p = '-';
  return lj_str_new(L, p, (size_t)(buf+sizeof(buf)-p));
}

/* Convert complex to string with 'i' or 'I' suffix. */
GCstr *lj_ctype_repr_complex(lua_State *L, void *sp, CTSize size)
{
  char buf[2*LJ_STR_NUMBUF+2+1];
  TValue re, im;
  size_t len;
  if (size == 2*sizeof(double)) {
    re.n = *(double *)sp; im.n = ((double *)sp)[1];
  } else {
    re.n = (double)*(float *)sp; im.n = (double)((float *)sp)[1];
  }
  len = lj_str_bufnum(buf, &re);
  if (!(im.u32.hi & 0x80000000u) || im.n != im.n) buf[len++] = '+';
  len += lj_str_bufnum(buf+len, &im);
  buf[len] = buf[len-1] >= 'a' ? 'I' : 'i';
  return lj_str_new(L, buf, len+1);
}

/* -- C type state -------------------------------------------------------- */

/* Initialize C type table and state. */
CTState *lj_ctype_init(lua_State *L)
{
  CTState *cts = lj_mem_newt(L, sizeof(CTState), CTState);
  CType *ct = lj_mem_newvec(L, CTTYPETAB_MIN, CType);
  const char *name = lj_ctype_typenames;
  CTypeID id;
  memset(cts, 0, sizeof(CTState));
  cts->tab = ct;
  cts->sizetab = CTTYPETAB_MIN;
  cts->top = CTTYPEINFO_NUM;
  cts->L = NULL;
  cts->g = G(L);
  for (id = 0; id < CTTYPEINFO_NUM; id++, ct++) {
    CTInfo info = lj_ctype_typeinfo[id];
    ct->size = (CTSize)((int32_t)(info << 16) >> 26);
    ct->info = info & 0xffff03ffu;
    ct->sib = 0;
    if (ctype_type(info) == CT_KW || ctype_istypedef(info)) {
      size_t len = strlen(name);
      GCstr *str = lj_str_new(L, name, len);
      ctype_setname(ct, str);
      name += len+1;
      lj_ctype_addname(cts, ct, id);
    } else {
      setgcrefnull(ct->name);
      ct->next = 0;
      if (!ctype_isenum(info)) ctype_addtype(cts, ct, id);
    }
  }
  setmref(G(L)->ctype_state, cts);
  return cts;
}

/* Free C type table and state. */
void lj_ctype_freestate(global_State *g)
{
  CTState *cts = ctype_ctsG(g);
  if (cts) {
    lj_ccallback_mcode_free(cts);
    lj_mem_freevec(g, cts->tab, cts->sizetab, CType);
    lj_mem_freevec(g, cts->cb.cbid, cts->cb.sizeid, CTypeID1);
    lj_mem_freet(g, cts);
  }
}

#endif
