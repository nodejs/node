/*
** C type management.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_CTYPE_H
#define _LJ_CTYPE_H

#include "lj_obj.h"
#include "lj_gc.h"

#if LJ_HASFFI

/* -- C type definitions -------------------------------------------------- */

/* C type numbers. Highest 4 bits of C type info. ORDER CT. */
enum {
  /* Externally visible types. */
  CT_NUM,		/* Integer or floating-point numbers. */
  CT_STRUCT,		/* Struct or union. */
  CT_PTR,		/* Pointer or reference. */
  CT_ARRAY,		/* Array or complex type. */
  CT_MAYCONVERT = CT_ARRAY,
  CT_VOID,		/* Void type. */
  CT_ENUM,		/* Enumeration. */
  CT_HASSIZE = CT_ENUM,  /* Last type where ct->size holds the actual size. */
  CT_FUNC,		/* Function. */
  CT_TYPEDEF,		/* Typedef. */
  CT_ATTRIB,		/* Miscellaneous attributes. */
  /* Internal element types. */
  CT_FIELD,		/* Struct/union field or function parameter. */
  CT_BITFIELD,		/* Struct/union bitfield. */
  CT_CONSTVAL,		/* Constant value. */
  CT_EXTERN,		/* External reference. */
  CT_KW			/* Keyword. */
};

LJ_STATIC_ASSERT(((int)CT_PTR & (int)CT_ARRAY) == CT_PTR);
LJ_STATIC_ASSERT(((int)CT_STRUCT & (int)CT_ARRAY) == CT_STRUCT);

/*
**  ---------- info ------------
** |type      flags...  A   cid | size   |  sib  | next  | name  |
** +----------------------------+--------+-------+-------+-------+--
** |NUM       BFvcUL..  A       | size   |       | type  |       |
** |STRUCT    ..vcU..V  A       | size   | field | name? | name? |
** |PTR       ..vcR...  A   cid | size   |       | type  |       |
** |ARRAY     VCvc...V  A   cid | size   |       | type  |       |
** |VOID      ..vc....  A       | size   |       | type  |       |
** |ENUM                A   cid | size   | const | name? | name? |
** |FUNC      ....VS.. cc   cid | nargs  | field | name? | name? |
** |TYPEDEF                 cid |        |       | name  | name  |
** |ATTRIB        attrnum   cid | attr   | sib?  | type? |       |
** |FIELD                   cid | offset | field |       | name? |
** |BITFIELD  B.vcU csz bsz pos | offset | field |       | name? |
** |CONSTVAL     c          cid | value  | const | name  | name  |
** |EXTERN                  cid |        | sib?  | name  | name  |
** |KW                      tok | size   |       | name  | name  |
** +----------------------------+--------+-------+-------+-------+--
**        ^^  ^^--- bits used for C type conversion dispatch
*/

/* C type info flags.     TFFArrrr  */
#define CTF_BOOL	0x08000000u	/* Boolean: NUM, BITFIELD. */
#define CTF_FP		0x04000000u	/* Floating-point: NUM. */
#define CTF_CONST	0x02000000u	/* Const qualifier. */
#define CTF_VOLATILE	0x01000000u	/* Volatile qualifier. */
#define CTF_UNSIGNED	0x00800000u	/* Unsigned: NUM, BITFIELD. */
#define CTF_LONG	0x00400000u	/* Long: NUM. */
#define CTF_VLA		0x00100000u	/* Variable-length: ARRAY, STRUCT. */
#define CTF_REF		0x00800000u	/* Reference: PTR. */
#define CTF_VECTOR	0x08000000u	/* Vector: ARRAY. */
#define CTF_COMPLEX	0x04000000u	/* Complex: ARRAY. */
#define CTF_UNION	0x00800000u	/* Union: STRUCT. */
#define CTF_VARARG	0x00800000u	/* Vararg: FUNC. */
#define CTF_SSEREGPARM	0x00400000u	/* SSE register parameters: FUNC. */

#define CTF_QUAL	(CTF_CONST|CTF_VOLATILE)
#define CTF_ALIGN	(CTMASK_ALIGN<<CTSHIFT_ALIGN)
#define CTF_UCHAR	((char)-1 > 0 ? CTF_UNSIGNED : 0)

/* Flags used in parser.  .F.Ammvf   cp->attr  */
#define CTFP_ALIGNED	0x00000001u	/* cp->attr + ALIGN */
#define CTFP_PACKED	0x00000002u	/* cp->attr */
/*                        ...C...f   cp->fattr */
#define CTFP_CCONV	0x00000001u	/* cp->fattr + CCONV/[SSE]REGPARM */

/* C type info bitfields. */
#define CTMASK_CID	0x0000ffffu	/* Max. 65536 type IDs. */
#define CTMASK_NUM	0xf0000000u	/* Max. 16 type numbers. */
#define CTSHIFT_NUM	28
#define CTMASK_ALIGN	15		/* Max. alignment is 2^15. */
#define CTSHIFT_ALIGN	16
#define CTMASK_ATTRIB	255		/* Max. 256 attributes. */
#define CTSHIFT_ATTRIB	16
#define CTMASK_CCONV	3		/* Max. 4 calling conventions. */
#define CTSHIFT_CCONV	16
#define CTMASK_REGPARM	3		/* Max. 0-3 regparms. */
#define CTSHIFT_REGPARM	18
/* Bitfields only used in parser. */
#define CTMASK_VSIZEP	15		/* Max. vector size is 2^15. */
#define CTSHIFT_VSIZEP	4
#define CTMASK_MSIZEP	255		/* Max. type size (via mode) is 128. */
#define CTSHIFT_MSIZEP	8

/* Info bits for BITFIELD. Max. size of bitfield is 64 bits. */
#define CTBSZ_MAX	32		/* Max. size of bitfield is 32 bit. */
#define CTBSZ_FIELD	127		/* Temp. marker for regular field. */
#define CTMASK_BITPOS	127
#define CTMASK_BITBSZ	127
#define CTMASK_BITCSZ	127
#define CTSHIFT_BITPOS	0
#define CTSHIFT_BITBSZ	8
#define CTSHIFT_BITCSZ	16

#define CTF_INSERT(info, field, val) \
  info = (info & ~(CTMASK_##field<<CTSHIFT_##field)) | \
	  (((CTSize)(val) & CTMASK_##field) << CTSHIFT_##field)

/* Calling conventions. ORDER CC */
enum { CTCC_CDECL, CTCC_THISCALL, CTCC_FASTCALL, CTCC_STDCALL };

/* Attribute numbers. */
enum {
  CTA_NONE,		/* Ignored attribute. Must be zero. */
  CTA_QUAL,		/* Unmerged qualifiers. */
  CTA_ALIGN,		/* Alignment override. */
  CTA_SUBTYPE,		/* Transparent sub-type. */
  CTA_REDIR,		/* Redirected symbol name. */
  CTA_BAD,		/* To catch bad IDs. */
  CTA__MAX
};

/* Special sizes. */
#define CTSIZE_INVALID	0xffffffffu

typedef uint32_t CTInfo;	/* Type info. */
typedef uint32_t CTSize;	/* Type size. */
typedef uint32_t CTypeID;	/* Type ID. */
typedef uint16_t CTypeID1;	/* Minimum-sized type ID. */

/* C type table element. */
typedef struct CType {
  CTInfo info;		/* Type info. */
  CTSize size;		/* Type size or other info. */
  CTypeID1 sib;		/* Sibling element. */
  CTypeID1 next;	/* Next element in hash chain. */
  GCRef name;		/* Element name (GCstr). */
} CType;

#define CTHASH_SIZE	128	/* Number of hash anchors. */
#define CTHASH_MASK	(CTHASH_SIZE-1)

/* Simplify target-specific configuration. Checked in lj_ccall.h. */
#define CCALL_MAX_GPR		8
#define CCALL_MAX_FPR		8

typedef LJ_ALIGN(8) union FPRCBArg { double d; float f[2]; } FPRCBArg;

/* C callback state. Defined here, to avoid dragging in lj_ccall.h. */

typedef LJ_ALIGN(8) struct CCallback {
  FPRCBArg fpr[CCALL_MAX_FPR];	/* Arguments/results in FPRs. */
  intptr_t gpr[CCALL_MAX_GPR];	/* Arguments/results in GPRs. */
  intptr_t *stack;		/* Pointer to arguments on stack. */
  void *mcode;			/* Machine code for callback func. pointers. */
  CTypeID1 *cbid;		/* Callback type table. */
  MSize sizeid;			/* Size of callback type table. */
  MSize topid;			/* Highest unused callback type table slot. */
  MSize slot;			/* Current callback slot. */
} CCallback;

/* C type state. */
typedef struct CTState {
  CType *tab;		/* C type table. */
  CTypeID top;		/* Current top of C type table. */
  MSize sizetab;	/* Size of C type table. */
  lua_State *L;		/* Lua state (needed for errors and allocations). */
  global_State *g;	/* Global state. */
  GCtab *finalizer;	/* Map of cdata to finalizer. */
  GCtab *miscmap;	/* Map of -CTypeID to metatable and cb slot to func. */
  CCallback cb;		/* Temporary callback state. */
  CTypeID1 hash[CTHASH_SIZE];  /* Hash anchors for C type table. */
} CTState;

#define CTINFO(ct, flags)	(((CTInfo)(ct) << CTSHIFT_NUM) + (flags))
#define CTALIGN(al)		((CTSize)(al) << CTSHIFT_ALIGN)
#define CTATTRIB(at)		((CTInfo)(at) << CTSHIFT_ATTRIB)

#define ctype_type(info)	((info) >> CTSHIFT_NUM)
#define ctype_cid(info)		((CTypeID)((info) & CTMASK_CID))
#define ctype_align(info)	(((info) >> CTSHIFT_ALIGN) & CTMASK_ALIGN)
#define ctype_attrib(info)	(((info) >> CTSHIFT_ATTRIB) & CTMASK_ATTRIB)
#define ctype_bitpos(info)	(((info) >> CTSHIFT_BITPOS) & CTMASK_BITPOS)
#define ctype_bitbsz(info)	(((info) >> CTSHIFT_BITBSZ) & CTMASK_BITBSZ)
#define ctype_bitcsz(info)	(((info) >> CTSHIFT_BITCSZ) & CTMASK_BITCSZ)
#define ctype_vsizeP(info)	(((info) >> CTSHIFT_VSIZEP) & CTMASK_VSIZEP)
#define ctype_msizeP(info)	(((info) >> CTSHIFT_MSIZEP) & CTMASK_MSIZEP)
#define ctype_cconv(info)	(((info) >> CTSHIFT_CCONV) & CTMASK_CCONV)

/* Simple type checks. */
#define ctype_isnum(info)	(ctype_type((info)) == CT_NUM)
#define ctype_isvoid(info)	(ctype_type((info)) == CT_VOID)
#define ctype_isptr(info)	(ctype_type((info)) == CT_PTR)
#define ctype_isarray(info)	(ctype_type((info)) == CT_ARRAY)
#define ctype_isstruct(info)	(ctype_type((info)) == CT_STRUCT)
#define ctype_isfunc(info)	(ctype_type((info)) == CT_FUNC)
#define ctype_isenum(info)	(ctype_type((info)) == CT_ENUM)
#define ctype_istypedef(info)	(ctype_type((info)) == CT_TYPEDEF)
#define ctype_isattrib(info)	(ctype_type((info)) == CT_ATTRIB)
#define ctype_isfield(info)	(ctype_type((info)) == CT_FIELD)
#define ctype_isbitfield(info)	(ctype_type((info)) == CT_BITFIELD)
#define ctype_isconstval(info)	(ctype_type((info)) == CT_CONSTVAL)
#define ctype_isextern(info)	(ctype_type((info)) == CT_EXTERN)
#define ctype_hassize(info)	(ctype_type((info)) <= CT_HASSIZE)

/* Combined type and flag checks. */
#define ctype_isinteger(info) \
  (((info) & (CTMASK_NUM|CTF_BOOL|CTF_FP)) == CTINFO(CT_NUM, 0))
#define ctype_isinteger_or_bool(info) \
  (((info) & (CTMASK_NUM|CTF_FP)) == CTINFO(CT_NUM, 0))
#define ctype_isbool(info) \
  (((info) & (CTMASK_NUM|CTF_BOOL)) == CTINFO(CT_NUM, CTF_BOOL))
#define ctype_isfp(info) \
  (((info) & (CTMASK_NUM|CTF_FP)) == CTINFO(CT_NUM, CTF_FP))

#define ctype_ispointer(info) \
  ((ctype_type(info) >> 1) == (CT_PTR >> 1))  /* Pointer or array. */
#define ctype_isref(info) \
  (((info) & (CTMASK_NUM|CTF_REF)) == CTINFO(CT_PTR, CTF_REF))

#define ctype_isrefarray(info) \
  (((info) & (CTMASK_NUM|CTF_VECTOR|CTF_COMPLEX)) == CTINFO(CT_ARRAY, 0))
#define ctype_isvector(info) \
  (((info) & (CTMASK_NUM|CTF_VECTOR)) == CTINFO(CT_ARRAY, CTF_VECTOR))
#define ctype_iscomplex(info) \
  (((info) & (CTMASK_NUM|CTF_COMPLEX)) == CTINFO(CT_ARRAY, CTF_COMPLEX))

#define ctype_isvltype(info) \
  (((info) & ((CTMASK_NUM|CTF_VLA) - (2u<<CTSHIFT_NUM))) == \
   CTINFO(CT_STRUCT, CTF_VLA))  /* VL array or VL struct. */
#define ctype_isvlarray(info) \
  (((info) & (CTMASK_NUM|CTF_VLA)) == CTINFO(CT_ARRAY, CTF_VLA))

#define ctype_isxattrib(info, at) \
  (((info) & (CTMASK_NUM|CTATTRIB(CTMASK_ATTRIB))) == \
   CTINFO(CT_ATTRIB, CTATTRIB(at)))

/* Target-dependent sizes and alignments. */
#if LJ_64
#define CTSIZE_PTR	8
#define CTALIGN_PTR	CTALIGN(3)
#else
#define CTSIZE_PTR	4
#define CTALIGN_PTR	CTALIGN(2)
#endif

#define CTINFO_REF(ref) \
  CTINFO(CT_PTR, (CTF_CONST|CTF_REF|CTALIGN_PTR) + (ref))

#define CT_MEMALIGN	3	/* Alignment guaranteed by memory allocator. */

/* -- Predefined types ---------------------------------------------------- */

/* Target-dependent types. */
#if LJ_TARGET_PPC || LJ_TARGET_PPCSPE
#define CTTYDEFP(_) \
  _(LINT32,		4,	CT_NUM, CTF_LONG|CTALIGN(2))
#else
#define CTTYDEFP(_)
#endif

/* Common types. */
#define CTTYDEF(_) \
  _(NONE,		0,	CT_ATTRIB, CTATTRIB(CTA_BAD)) \
  _(VOID,		-1,	CT_VOID, CTALIGN(0)) \
  _(CVOID,		-1,	CT_VOID, CTF_CONST|CTALIGN(0)) \
  _(BOOL,		1,	CT_NUM, CTF_BOOL|CTF_UNSIGNED|CTALIGN(0)) \
  _(CCHAR,		1,	CT_NUM, CTF_CONST|CTF_UCHAR|CTALIGN(0)) \
  _(INT8,		1,	CT_NUM, CTALIGN(0)) \
  _(UINT8,		1,	CT_NUM, CTF_UNSIGNED|CTALIGN(0)) \
  _(INT16,		2,	CT_NUM, CTALIGN(1)) \
  _(UINT16,		2,	CT_NUM, CTF_UNSIGNED|CTALIGN(1)) \
  _(INT32,		4,	CT_NUM, CTALIGN(2)) \
  _(UINT32,		4,	CT_NUM, CTF_UNSIGNED|CTALIGN(2)) \
  _(INT64,		8,	CT_NUM, CTF_LONG|CTALIGN(3)) \
  _(UINT64,		8,	CT_NUM, CTF_UNSIGNED|CTF_LONG|CTALIGN(3)) \
  _(FLOAT,		4,	CT_NUM, CTF_FP|CTALIGN(2)) \
  _(DOUBLE,		8,	CT_NUM, CTF_FP|CTALIGN(3)) \
  _(COMPLEX_FLOAT,	8,	CT_ARRAY, CTF_COMPLEX|CTALIGN(2)|CTID_FLOAT) \
  _(COMPLEX_DOUBLE,	16,	CT_ARRAY, CTF_COMPLEX|CTALIGN(3)|CTID_DOUBLE) \
  _(P_VOID,	CTSIZE_PTR,	CT_PTR, CTALIGN_PTR|CTID_VOID) \
  _(P_CVOID,	CTSIZE_PTR,	CT_PTR, CTALIGN_PTR|CTID_CVOID) \
  _(P_CCHAR,	CTSIZE_PTR,	CT_PTR, CTALIGN_PTR|CTID_CCHAR) \
  _(A_CCHAR,		-1,	CT_ARRAY, CTF_CONST|CTALIGN(0)|CTID_CCHAR) \
  _(CTYPEID,		4,	CT_ENUM, CTALIGN(2)|CTID_INT32) \
  CTTYDEFP(_) \
  /* End of type list. */

/* Public predefined type IDs. */
enum {
#define CTTYIDDEF(id, sz, ct, info)	CTID_##id,
CTTYDEF(CTTYIDDEF)
#undef CTTYIDDEF
  /* Predefined typedefs and keywords follow. */
  CTID_MAX = 65536
};

/* Target-dependent type IDs. */
#if LJ_64
#define CTID_INT_PSZ	CTID_INT64
#define CTID_UINT_PSZ	CTID_UINT64
#else
#define CTID_INT_PSZ	CTID_INT32
#define CTID_UINT_PSZ	CTID_UINT32
#endif

#if LJ_ABI_WIN
#define CTID_WCHAR	CTID_UINT16
#elif LJ_TARGET_PPC
#define CTID_WCHAR	CTID_LINT32
#else
#define CTID_WCHAR	CTID_INT32
#endif

/* -- C tokens and keywords ----------------------------------------------- */

/* C lexer keywords. */
#define CTOKDEF(_) \
  _(IDENT, "<identifier>") _(STRING, "<string>") \
  _(INTEGER, "<integer>") _(EOF, "<eof>") \
  _(OROR, "||") _(ANDAND, "&&") _(EQ, "==") _(NE, "!=") \
  _(LE, "<=") _(GE, ">=") _(SHL, "<<") _(SHR, ">>") _(DEREF, "->")

/* Simple declaration specifiers. */
#define CDSDEF(_) \
  _(VOID) _(BOOL) _(CHAR) _(INT) _(FP) \
  _(LONG) _(LONGLONG) _(SHORT) _(COMPLEX) _(SIGNED) _(UNSIGNED) \
  _(CONST) _(VOLATILE) _(RESTRICT) _(INLINE) \
  _(TYPEDEF) _(EXTERN) _(STATIC) _(AUTO) _(REGISTER)

/* C keywords. */
#define CKWDEF(_) \
  CDSDEF(_) _(EXTENSION) _(ASM) _(ATTRIBUTE) \
  _(DECLSPEC) _(CCDECL) _(PTRSZ) \
  _(STRUCT) _(UNION) _(ENUM) \
  _(SIZEOF) _(ALIGNOF)

/* C token numbers. */
enum {
  CTOK_OFS = 255,
#define CTOKNUM(name, sym)	CTOK_##name,
#define CKWNUM(name)		CTOK_##name,
CTOKDEF(CTOKNUM)
CKWDEF(CKWNUM)
#undef CTOKNUM
#undef CKWNUM
  CTOK_FIRSTDECL = CTOK_VOID,
  CTOK_FIRSTSCL = CTOK_TYPEDEF,
  CTOK_LASTDECLFLAG = CTOK_REGISTER,
  CTOK_LASTDECL = CTOK_ENUM
};

/* Declaration specifier flags. */
enum {
#define CDSFLAG(name)	CDF_##name = (1u << (CTOK_##name - CTOK_FIRSTDECL)),
CDSDEF(CDSFLAG)
#undef CDSFLAG
  CDF__END
};

#define CDF_SCL  (CDF_TYPEDEF|CDF_EXTERN|CDF_STATIC|CDF_AUTO|CDF_REGISTER)

/* -- C type management --------------------------------------------------- */

#define ctype_ctsG(g)		(mref((g)->ctype_state, CTState))

/* Get C type state. */
static LJ_AINLINE CTState *ctype_cts(lua_State *L)
{
  CTState *cts = ctype_ctsG(G(L));
  cts->L = L;  /* Save L for errors and allocations. */
  return cts;
}

/* Save and restore state of C type table. */
#define LJ_CTYPE_SAVE(cts)	CTState savects_ = *(cts)
#define LJ_CTYPE_RESTORE(cts) \
  ((cts)->top = savects_.top, \
   memcpy((cts)->hash, savects_.hash, sizeof(savects_.hash)))

/* Check C type ID for validity when assertions are enabled. */
static LJ_AINLINE CTypeID ctype_check(CTState *cts, CTypeID id)
{
  lua_assert(id > 0 && id < cts->top); UNUSED(cts);
  return id;
}

/* Get C type for C type ID. */
static LJ_AINLINE CType *ctype_get(CTState *cts, CTypeID id)
{
  return &cts->tab[ctype_check(cts, id)];
}

/* Get C type ID for a C type. */
#define ctype_typeid(cts, ct)	((CTypeID)((ct) - (cts)->tab))

/* Get child C type. */
static LJ_AINLINE CType *ctype_child(CTState *cts, CType *ct)
{
  lua_assert(!(ctype_isvoid(ct->info) || ctype_isstruct(ct->info) ||
	     ctype_isbitfield(ct->info)));  /* These don't have children. */
  return ctype_get(cts, ctype_cid(ct->info));
}

/* Get raw type for a C type ID. */
static LJ_AINLINE CType *ctype_raw(CTState *cts, CTypeID id)
{
  CType *ct = ctype_get(cts, id);
  while (ctype_isattrib(ct->info)) ct = ctype_child(cts, ct);
  return ct;
}

/* Get raw type of the child of a C type. */
static LJ_AINLINE CType *ctype_rawchild(CTState *cts, CType *ct)
{
  do { ct = ctype_child(cts, ct); } while (ctype_isattrib(ct->info));
  return ct;
}

/* Set the name of a C type table element. */
static LJ_AINLINE void ctype_setname(CType *ct, GCstr *s)
{
  /* NOBARRIER: mark string as fixed -- the C type table is never collected. */
  fixstring(s);
  setgcref(ct->name, obj2gco(s));
}

LJ_FUNC CTypeID lj_ctype_new(CTState *cts, CType **ctp);
LJ_FUNC CTypeID lj_ctype_intern(CTState *cts, CTInfo info, CTSize size);
LJ_FUNC void lj_ctype_addname(CTState *cts, CType *ct, CTypeID id);
LJ_FUNC CTypeID lj_ctype_getname(CTState *cts, CType **ctp, GCstr *name,
				 uint32_t tmask);
LJ_FUNC CType *lj_ctype_getfieldq(CTState *cts, CType *ct, GCstr *name,
				  CTSize *ofs, CTInfo *qual);
#define lj_ctype_getfield(cts, ct, name, ofs) \
  lj_ctype_getfieldq((cts), (ct), (name), (ofs), NULL)
LJ_FUNC CType *lj_ctype_rawref(CTState *cts, CTypeID id);
LJ_FUNC CTSize lj_ctype_size(CTState *cts, CTypeID id);
LJ_FUNC CTSize lj_ctype_vlsize(CTState *cts, CType *ct, CTSize nelem);
LJ_FUNC CTInfo lj_ctype_info(CTState *cts, CTypeID id, CTSize *szp);
LJ_FUNC cTValue *lj_ctype_meta(CTState *cts, CTypeID id, MMS mm);
LJ_FUNC GCstr *lj_ctype_repr(lua_State *L, CTypeID id, GCstr *name);
LJ_FUNC GCstr *lj_ctype_repr_int64(lua_State *L, uint64_t n, int isunsigned);
LJ_FUNC GCstr *lj_ctype_repr_complex(lua_State *L, void *sp, CTSize size);
LJ_FUNC CTState *lj_ctype_init(lua_State *L);
LJ_FUNC void lj_ctype_freestate(global_State *g);

#endif

#endif
