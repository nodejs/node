/*
** C declaration parser.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#include "lj_obj.h"

#if LJ_HASFFI

#include "lj_gc.h"
#include "lj_err.h"
#include "lj_str.h"
#include "lj_ctype.h"
#include "lj_cparse.h"
#include "lj_frame.h"
#include "lj_vm.h"
#include "lj_char.h"
#include "lj_strscan.h"

/*
** Important note: this is NOT a validating C parser! This is a minimal
** C declaration parser, solely for use by the LuaJIT FFI.
**
** It ought to return correct results for properly formed C declarations,
** but it may accept some invalid declarations, too (and return nonsense).
** Also, it shows rather generic error messages to avoid unnecessary bloat.
** If in doubt, please check the input against your favorite C compiler.
*/

/* -- C lexer ------------------------------------------------------------- */

/* C lexer token names. */
static const char *const ctoknames[] = {
#define CTOKSTR(name, str)	str,
CTOKDEF(CTOKSTR)
#undef CTOKSTR
  NULL
};

/* Forward declaration. */
LJ_NORET static void cp_err(CPState *cp, ErrMsg em);

static const char *cp_tok2str(CPState *cp, CPToken tok)
{
  lua_assert(tok < CTOK_FIRSTDECL);
  if (tok > CTOK_OFS)
    return ctoknames[tok-CTOK_OFS-1];
  else if (!lj_char_iscntrl(tok))
    return lj_str_pushf(cp->L, "%c", tok);
  else
    return lj_str_pushf(cp->L, "char(%d)", tok);
}

/* End-of-line? */
static LJ_AINLINE int cp_iseol(CPChar c)
{
  return (c == '\n' || c == '\r');
}

static LJ_AINLINE CPChar cp_get(CPState *cp);

/* Peek next raw character. */
static LJ_AINLINE CPChar cp_rawpeek(CPState *cp)
{
  return (CPChar)(uint8_t)(*cp->p);
}

/* Transparently skip backslash-escaped line breaks. */
static LJ_NOINLINE CPChar cp_get_bs(CPState *cp)
{
  CPChar c2, c = cp_rawpeek(cp);
  if (!cp_iseol(c)) return cp->c;
  cp->p++;
  c2 = cp_rawpeek(cp);
  if (cp_iseol(c2) && c2 != c) cp->p++;
  cp->linenumber++;
  return cp_get(cp);
}

/* Get next character. */
static LJ_AINLINE CPChar cp_get(CPState *cp)
{
  cp->c = (CPChar)(uint8_t)(*cp->p++);
  if (LJ_LIKELY(cp->c != '\\')) return cp->c;
  return cp_get_bs(cp);
}

/* Grow save buffer. */
static LJ_NOINLINE void cp_save_grow(CPState *cp, CPChar c)
{
  MSize newsize;
  if (cp->sb.sz >= CPARSE_MAX_BUF/2)
    cp_err(cp, LJ_ERR_XELEM);
  newsize = cp->sb.sz * 2;
  lj_str_resizebuf(cp->L, &cp->sb, newsize);
  cp->sb.buf[cp->sb.n++] = (char)c;
}

/* Save character in buffer. */
static LJ_AINLINE void cp_save(CPState *cp, CPChar c)
{
  if (LJ_UNLIKELY(cp->sb.n + 1 > cp->sb.sz))
    cp_save_grow(cp, c);
  else
    cp->sb.buf[cp->sb.n++] = (char)c;
}

/* Skip line break. Handles "\n", "\r", "\r\n" or "\n\r". */
static void cp_newline(CPState *cp)
{
  CPChar c = cp_rawpeek(cp);
  if (cp_iseol(c) && c != cp->c) cp->p++;
  cp->linenumber++;
}

LJ_NORET static void cp_errmsg(CPState *cp, CPToken tok, ErrMsg em, ...)
{
  const char *msg, *tokstr;
  lua_State *L;
  va_list argp;
  if (tok == 0) {
    tokstr = NULL;
  } else if (tok == CTOK_IDENT || tok == CTOK_INTEGER || tok == CTOK_STRING ||
	     tok >= CTOK_FIRSTDECL) {
    if (cp->sb.n == 0) cp_save(cp, '$');
    cp_save(cp, '\0');
    tokstr = cp->sb.buf;
  } else {
    tokstr = cp_tok2str(cp, tok);
  }
  L = cp->L;
  va_start(argp, em);
  msg = lj_str_pushvf(L, err2msg(em), argp);
  va_end(argp);
  if (tokstr)
    msg = lj_str_pushf(L, err2msg(LJ_ERR_XNEAR), msg, tokstr);
  if (cp->linenumber > 1)
    msg = lj_str_pushf(L, "%s at line %d", msg, cp->linenumber);
  lj_err_callermsg(L, msg);
}

LJ_NORET LJ_NOINLINE static void cp_err_token(CPState *cp, CPToken tok)
{
  cp_errmsg(cp, cp->tok, LJ_ERR_XTOKEN, cp_tok2str(cp, tok));
}

LJ_NORET LJ_NOINLINE static void cp_err_badidx(CPState *cp, CType *ct)
{
  GCstr *s = lj_ctype_repr(cp->cts->L, ctype_typeid(cp->cts, ct), NULL);
  cp_errmsg(cp, 0, LJ_ERR_FFI_BADIDX, strdata(s));
}

LJ_NORET LJ_NOINLINE static void cp_err(CPState *cp, ErrMsg em)
{
  cp_errmsg(cp, 0, em);
}

/* -- Main lexical scanner ------------------------------------------------ */

/* Parse number literal. Only handles int32_t/uint32_t right now. */
static CPToken cp_number(CPState *cp)
{
  StrScanFmt fmt;
  TValue o;
  do { cp_save(cp, cp->c); } while (lj_char_isident(cp_get(cp)));
  cp_save(cp, '\0');
  fmt = lj_strscan_scan((const uint8_t *)cp->sb.buf, &o, STRSCAN_OPT_C);
  if (fmt == STRSCAN_INT) cp->val.id = CTID_INT32;
  else if (fmt == STRSCAN_U32) cp->val.id = CTID_UINT32;
  else if (!(cp->mode & CPARSE_MODE_SKIP))
    cp_errmsg(cp, CTOK_INTEGER, LJ_ERR_XNUMBER);
  cp->val.u32 = (uint32_t)o.i;
  return CTOK_INTEGER;
}

/* Parse identifier or keyword. */
static CPToken cp_ident(CPState *cp)
{
  do { cp_save(cp, cp->c); } while (lj_char_isident(cp_get(cp)));
  cp->str = lj_str_new(cp->L, cp->sb.buf, cp->sb.n);
  cp->val.id = lj_ctype_getname(cp->cts, &cp->ct, cp->str, cp->tmask);
  if (ctype_type(cp->ct->info) == CT_KW)
    return ctype_cid(cp->ct->info);
  return CTOK_IDENT;
}

/* Parse parameter. */
static CPToken cp_param(CPState *cp)
{
  CPChar c = cp_get(cp);
  TValue *o = cp->param;
  if (lj_char_isident(c) || c == '$')  /* Reserve $xyz for future extensions. */
    cp_errmsg(cp, c, LJ_ERR_XSYNTAX);
  if (!o || o >= cp->L->top)
    cp_err(cp, LJ_ERR_FFI_NUMPARAM);
  cp->param = o+1;
  if (tvisstr(o)) {
    cp->str = strV(o);
    cp->val.id = 0;
    cp->ct = &cp->cts->tab[0];
    return CTOK_IDENT;
  } else if (tvisnumber(o)) {
    cp->val.i32 = numberVint(o);
    cp->val.id = CTID_INT32;
    return CTOK_INTEGER;
  } else {
    GCcdata *cd;
    if (!tviscdata(o))
      lj_err_argtype(cp->L, (int)(o-cp->L->base)+1, "type parameter");
    cd = cdataV(o);
    if (cd->ctypeid == CTID_CTYPEID)
      cp->val.id = *(CTypeID *)cdataptr(cd);
    else
      cp->val.id = cd->ctypeid;
    return '$';
  }
}

/* Parse string or character constant. */
static CPToken cp_string(CPState *cp)
{
  CPChar delim = cp->c;
  cp_get(cp);
  while (cp->c != delim) {
    CPChar c = cp->c;
    if (c == '\0') cp_errmsg(cp, CTOK_EOF, LJ_ERR_XSTR);
    if (c == '\\') {
      c = cp_get(cp);
      switch (c) {
      case '\0': cp_errmsg(cp, CTOK_EOF, LJ_ERR_XSTR); break;
      case 'a': c = '\a'; break;
      case 'b': c = '\b'; break;
      case 'f': c = '\f'; break;
      case 'n': c = '\n'; break;
      case 'r': c = '\r'; break;
      case 't': c = '\t'; break;
      case 'v': c = '\v'; break;
      case 'e': c = 27; break;
      case 'x':
	c = 0;
	while (lj_char_isxdigit(cp_get(cp)))
	  c = (c<<4) + (lj_char_isdigit(cp->c) ? cp->c-'0' : (cp->c&15)+9);
	cp_save(cp, (c & 0xff));
	continue;
      default:
	if (lj_char_isdigit(c)) {
	  c -= '0';
	  if (lj_char_isdigit(cp_get(cp))) {
	    c = c*8 + (cp->c - '0');
	    if (lj_char_isdigit(cp_get(cp))) {
	      c = c*8 + (cp->c - '0');
	      cp_get(cp);
	    }
	  }
	  cp_save(cp, (c & 0xff));
	  continue;
	}
	break;
      }
    }
    cp_save(cp, c);
    cp_get(cp);
  }
  cp_get(cp);
  if (delim == '"') {
    cp->str = lj_str_new(cp->L, cp->sb.buf, cp->sb.n);
    return CTOK_STRING;
  } else {
    if (cp->sb.n != 1) cp_err_token(cp, '\'');
    cp->val.i32 = (int32_t)(char)cp->sb.buf[0];
    cp->val.id = CTID_INT32;
    return CTOK_INTEGER;
  }
}

/* Skip C comment. */
static void cp_comment_c(CPState *cp)
{
  do {
    if (cp_get(cp) == '*') {
      do {
	if (cp_get(cp) == '/') { cp_get(cp); return; }
      } while (cp->c == '*');
    }
    if (cp_iseol(cp->c)) cp_newline(cp);
  } while (cp->c != '\0');
}

/* Skip C++ comment. */
static void cp_comment_cpp(CPState *cp)
{
  while (!cp_iseol(cp_get(cp)) && cp->c != '\0')
    ;
}

/* Lexical scanner for C. Only a minimal subset is implemented. */
static CPToken cp_next_(CPState *cp)
{
  lj_str_resetbuf(&cp->sb);
  for (;;) {
    if (lj_char_isident(cp->c))
      return lj_char_isdigit(cp->c) ? cp_number(cp) : cp_ident(cp);
    switch (cp->c) {
    case '\n': case '\r': cp_newline(cp);  /* fallthrough. */
    case ' ': case '\t': case '\v': case '\f': cp_get(cp); break;
    case '"': case '\'': return cp_string(cp);
    case '/':
      if (cp_get(cp) == '*') cp_comment_c(cp);
      else if (cp->c == '/') cp_comment_cpp(cp);
      else return '/';
      break;
    case '|':
      if (cp_get(cp) != '|') return '|'; cp_get(cp); return CTOK_OROR;
    case '&':
      if (cp_get(cp) != '&') return '&'; cp_get(cp); return CTOK_ANDAND;
    case '=':
      if (cp_get(cp) != '=') return '='; cp_get(cp); return CTOK_EQ;
    case '!':
      if (cp_get(cp) != '=') return '!'; cp_get(cp); return CTOK_NE;
    case '<':
      if (cp_get(cp) == '=') { cp_get(cp); return CTOK_LE; }
      else if (cp->c == '<') { cp_get(cp); return CTOK_SHL; }
      return '<';
    case '>':
      if (cp_get(cp) == '=') { cp_get(cp); return CTOK_GE; }
      else if (cp->c == '>') { cp_get(cp); return CTOK_SHR; }
      return '>';
    case '-':
      if (cp_get(cp) != '>') return '-'; cp_get(cp); return CTOK_DEREF;
    case '$':
      return cp_param(cp);
    case '\0': return CTOK_EOF;
    default: { CPToken c = cp->c; cp_get(cp); return c; }
    }
  }
}

static LJ_NOINLINE CPToken cp_next(CPState *cp)
{
  return (cp->tok = cp_next_(cp));
}

/* -- C parser ------------------------------------------------------------ */

/* Namespaces for resolving identifiers. */
#define CPNS_DEFAULT \
  ((1u<<CT_KW)|(1u<<CT_TYPEDEF)|(1u<<CT_FUNC)|(1u<<CT_EXTERN)|(1u<<CT_CONSTVAL))
#define CPNS_STRUCT	((1u<<CT_KW)|(1u<<CT_STRUCT)|(1u<<CT_ENUM))

typedef CTypeID CPDeclIdx;	/* Index into declaration stack. */
typedef uint32_t CPscl;		/* Storage class flags. */

/* Type declaration context. */
typedef struct CPDecl {
  CPDeclIdx top;	/* Top of declaration stack. */
  CPDeclIdx pos;	/* Insertion position in declaration chain. */
  CPDeclIdx specpos;	/* Saved position for declaration specifier. */
  uint32_t mode;	/* Declarator mode. */
  CPState *cp;		/* C parser state. */
  GCstr *name;		/* Name of declared identifier (if direct). */
  GCstr *redir;		/* Redirected symbol name. */
  CTypeID nameid;	/* Existing typedef for declared identifier. */
  CTInfo attr;		/* Attributes. */
  CTInfo fattr;		/* Function attributes. */
  CTInfo specattr;	/* Saved attributes. */
  CTInfo specfattr;	/* Saved function attributes. */
  CTSize bits;		/* Field size in bits (if any). */
  CType stack[CPARSE_MAX_DECLSTACK];  /* Type declaration stack. */
} CPDecl;

/* Forward declarations. */
static CPscl cp_decl_spec(CPState *cp, CPDecl *decl, CPscl scl);
static void cp_declarator(CPState *cp, CPDecl *decl);
static CTypeID cp_decl_abstract(CPState *cp);

/* Initialize C parser state. Caller must set up: L, p, srcname, mode. */
static void cp_init(CPState *cp)
{
  cp->linenumber = 1;
  cp->depth = 0;
  cp->curpack = 0;
  cp->packstack[0] = 255;
  lj_str_initbuf(&cp->sb);
  lj_str_resizebuf(cp->L, &cp->sb, LJ_MIN_SBUF);
  lua_assert(cp->p != NULL);
  cp_get(cp);  /* Read-ahead first char. */
  cp->tok = 0;
  cp->tmask = CPNS_DEFAULT;
  cp_next(cp);  /* Read-ahead first token. */
}

/* Cleanup C parser state. */
static void cp_cleanup(CPState *cp)
{
  global_State *g = G(cp->L);
  lj_str_freebuf(g, &cp->sb);
}

/* Check and consume optional token. */
static int cp_opt(CPState *cp, CPToken tok)
{
  if (cp->tok == tok) { cp_next(cp); return 1; }
  return 0;
}

/* Check and consume token. */
static void cp_check(CPState *cp, CPToken tok)
{
  if (cp->tok != tok) cp_err_token(cp, tok);
  cp_next(cp);
}

/* Check if the next token may start a type declaration. */
static int cp_istypedecl(CPState *cp)
{
  if (cp->tok >= CTOK_FIRSTDECL && cp->tok <= CTOK_LASTDECL) return 1;
  if (cp->tok == CTOK_IDENT && ctype_istypedef(cp->ct->info)) return 1;
  if (cp->tok == '$') return 1;
  return 0;
}

/* -- Constant expression evaluator --------------------------------------- */

/* Forward declarations. */
static void cp_expr_unary(CPState *cp, CPValue *k);
static void cp_expr_sub(CPState *cp, CPValue *k, int pri);

/* Please note that type handling is very weak here. Most ops simply
** assume integer operands. Accessors are only needed to compute types and
** return synthetic values. The only purpose of the expression evaluator
** is to compute the values of constant expressions one would typically
** find in C header files. And again: this is NOT a validating C parser!
*/

/* Parse comma separated expression and return last result. */
static void cp_expr_comma(CPState *cp, CPValue *k)
{
  do { cp_expr_sub(cp, k, 0); } while (cp_opt(cp, ','));
}

/* Parse sizeof/alignof operator. */
static void cp_expr_sizeof(CPState *cp, CPValue *k, int wantsz)
{
  CTSize sz;
  CTInfo info;
  if (cp_opt(cp, '(')) {
    if (cp_istypedecl(cp))
      k->id = cp_decl_abstract(cp);
    else
      cp_expr_comma(cp, k);
    cp_check(cp, ')');
  } else {
    cp_expr_unary(cp, k);
  }
  info = lj_ctype_info(cp->cts, k->id, &sz);
  if (wantsz) {
    if (sz != CTSIZE_INVALID)
      k->u32 = sz;
    else if (k->id != CTID_A_CCHAR)  /* Special case for sizeof("string"). */
      cp_err(cp, LJ_ERR_FFI_INVSIZE);
  } else {
    k->u32 = 1u << ctype_align(info);
  }
  k->id = CTID_UINT32;  /* Really size_t. */
}

/* Parse prefix operators. */
static void cp_expr_prefix(CPState *cp, CPValue *k)
{
  if (cp->tok == CTOK_INTEGER) {
    *k = cp->val; cp_next(cp);
  } else if (cp_opt(cp, '+')) {
    cp_expr_unary(cp, k);  /* Nothing to do (well, integer promotion). */
  } else if (cp_opt(cp, '-')) {
    cp_expr_unary(cp, k); k->i32 = -k->i32;
  } else if (cp_opt(cp, '~')) {
    cp_expr_unary(cp, k); k->i32 = ~k->i32;
  } else if (cp_opt(cp, '!')) {
    cp_expr_unary(cp, k); k->i32 = !k->i32; k->id = CTID_INT32;
  } else if (cp_opt(cp, '(')) {
    if (cp_istypedecl(cp)) {  /* Cast operator. */
      CTypeID id = cp_decl_abstract(cp);
      cp_check(cp, ')');
      cp_expr_unary(cp, k);
      k->id = id;  /* No conversion performed. */
    } else {  /* Sub-expression. */
      cp_expr_comma(cp, k);
      cp_check(cp, ')');
    }
  } else if (cp_opt(cp, '*')) {  /* Indirection. */
    CType *ct;
    cp_expr_unary(cp, k);
    ct = lj_ctype_rawref(cp->cts, k->id);
    if (!ctype_ispointer(ct->info))
      cp_err_badidx(cp, ct);
    k->u32 = 0; k->id = ctype_cid(ct->info);
  } else if (cp_opt(cp, '&')) {  /* Address operator. */
    cp_expr_unary(cp, k);
    k->id = lj_ctype_intern(cp->cts, CTINFO(CT_PTR, CTALIGN_PTR+k->id),
			    CTSIZE_PTR);
  } else if (cp_opt(cp, CTOK_SIZEOF)) {
    cp_expr_sizeof(cp, k, 1);
  } else if (cp_opt(cp, CTOK_ALIGNOF)) {
    cp_expr_sizeof(cp, k, 0);
  } else if (cp->tok == CTOK_IDENT) {
    if (ctype_type(cp->ct->info) == CT_CONSTVAL) {
      k->u32 = cp->ct->size; k->id = ctype_cid(cp->ct->info);
    } else if (ctype_type(cp->ct->info) == CT_EXTERN) {
      k->u32 = cp->val.id; k->id = ctype_cid(cp->ct->info);
    } else if (ctype_type(cp->ct->info) == CT_FUNC) {
      k->u32 = cp->val.id; k->id = cp->val.id;
    } else {
      goto err_expr;
    }
    cp_next(cp);
  } else if (cp->tok == CTOK_STRING) {
    CTSize sz = cp->str->len;
    while (cp_next(cp) == CTOK_STRING)
      sz += cp->str->len;
    k->u32 = sz + 1;
    k->id = CTID_A_CCHAR;
  } else {
  err_expr:
    cp_errmsg(cp, cp->tok, LJ_ERR_XSYMBOL);
  }
}

/* Parse postfix operators. */
static void cp_expr_postfix(CPState *cp, CPValue *k)
{
  for (;;) {
    CType *ct;
    if (cp_opt(cp, '[')) {  /* Array/pointer index. */
      CPValue k2;
      cp_expr_comma(cp, &k2);
      ct = lj_ctype_rawref(cp->cts, k->id);
      if (!ctype_ispointer(ct->info)) {
	ct = lj_ctype_rawref(cp->cts, k2.id);
	if (!ctype_ispointer(ct->info))
	  cp_err_badidx(cp, ct);
      }
      cp_check(cp, ']');
      k->u32 = 0;
    } else if (cp->tok == '.' || cp->tok == CTOK_DEREF) {  /* Struct deref. */
      CTSize ofs;
      CType *fct;
      ct = lj_ctype_rawref(cp->cts, k->id);
      if (cp->tok == CTOK_DEREF) {
	if (!ctype_ispointer(ct->info))
	  cp_err_badidx(cp, ct);
	ct = lj_ctype_rawref(cp->cts, ctype_cid(ct->info));
      }
      cp_next(cp);
      if (cp->tok != CTOK_IDENT) cp_err_token(cp, CTOK_IDENT);
      if (!ctype_isstruct(ct->info) || ct->size == CTSIZE_INVALID ||
	  !(fct = lj_ctype_getfield(cp->cts, ct, cp->str, &ofs)) ||
	  ctype_isbitfield(fct->info)) {
	GCstr *s = lj_ctype_repr(cp->cts->L, ctype_typeid(cp->cts, ct), NULL);
	cp_errmsg(cp, 0, LJ_ERR_FFI_BADMEMBER, strdata(s), strdata(cp->str));
      }
      ct = fct;
      k->u32 = ctype_isconstval(ct->info) ? ct->size : 0;
      cp_next(cp);
    } else {
      return;
    }
    k->id = ctype_cid(ct->info);
  }
}

/* Parse infix operators. */
static void cp_expr_infix(CPState *cp, CPValue *k, int pri)
{
  CPValue k2;
  k2.u32 = 0; k2.id = 0;  /* Silence the compiler. */
  for (;;) {
    switch (pri) {
    case 0:
      if (cp_opt(cp, '?')) {
	CPValue k3;
	cp_expr_comma(cp, &k2);  /* Right-associative. */
	cp_check(cp, ':');
	cp_expr_sub(cp, &k3, 0);
	k->u32 = k->u32 ? k2.u32 : k3.u32;
	k->id = k2.id > k3.id ? k2.id : k3.id;
	continue;
      }
    case 1:
      if (cp_opt(cp, CTOK_OROR)) {
	cp_expr_sub(cp, &k2, 2); k->i32 = k->u32 || k2.u32; k->id = CTID_INT32;
	continue;
      }
    case 2:
      if (cp_opt(cp, CTOK_ANDAND)) {
	cp_expr_sub(cp, &k2, 3); k->i32 = k->u32 && k2.u32; k->id = CTID_INT32;
	continue;
      }
    case 3:
      if (cp_opt(cp, '|')) {
	cp_expr_sub(cp, &k2, 4); k->u32 = k->u32 | k2.u32; goto arith_result;
      }
    case 4:
      if (cp_opt(cp, '^')) {
	cp_expr_sub(cp, &k2, 5); k->u32 = k->u32 ^ k2.u32; goto arith_result;
      }
    case 5:
      if (cp_opt(cp, '&')) {
	cp_expr_sub(cp, &k2, 6); k->u32 = k->u32 & k2.u32; goto arith_result;
      }
    case 6:
      if (cp_opt(cp, CTOK_EQ)) {
	cp_expr_sub(cp, &k2, 7); k->i32 = k->u32 == k2.u32; k->id = CTID_INT32;
	continue;
      } else if (cp_opt(cp, CTOK_NE)) {
	cp_expr_sub(cp, &k2, 7); k->i32 = k->u32 != k2.u32; k->id = CTID_INT32;
	continue;
      }
    case 7:
      if (cp_opt(cp, '<')) {
	cp_expr_sub(cp, &k2, 8);
	if (k->id == CTID_INT32 && k2.id == CTID_INT32)
	  k->i32 = k->i32 < k2.i32;
	else
	  k->i32 = k->u32 < k2.u32;
	k->id = CTID_INT32;
	continue;
      } else if (cp_opt(cp, '>')) {
	cp_expr_sub(cp, &k2, 8);
	if (k->id == CTID_INT32 && k2.id == CTID_INT32)
	  k->i32 = k->i32 > k2.i32;
	else
	  k->i32 = k->u32 > k2.u32;
	k->id = CTID_INT32;
	continue;
      } else if (cp_opt(cp, CTOK_LE)) {
	cp_expr_sub(cp, &k2, 8);
	if (k->id == CTID_INT32 && k2.id == CTID_INT32)
	  k->i32 = k->i32 <= k2.i32;
	else
	  k->i32 = k->u32 <= k2.u32;
	k->id = CTID_INT32;
	continue;
      } else if (cp_opt(cp, CTOK_GE)) {
	cp_expr_sub(cp, &k2, 8);
	if (k->id == CTID_INT32 && k2.id == CTID_INT32)
	  k->i32 = k->i32 >= k2.i32;
	else
	  k->i32 = k->u32 >= k2.u32;
	k->id = CTID_INT32;
	continue;
      }
    case 8:
      if (cp_opt(cp, CTOK_SHL)) {
	cp_expr_sub(cp, &k2, 9); k->u32 = k->u32 << k2.u32;
	continue;
      } else if (cp_opt(cp, CTOK_SHR)) {
	cp_expr_sub(cp, &k2, 9);
	if (k->id == CTID_INT32)
	  k->i32 = k->i32 >> k2.i32;
	else
	  k->u32 = k->u32 >> k2.u32;
	continue;
      }
    case 9:
      if (cp_opt(cp, '+')) {
	cp_expr_sub(cp, &k2, 10); k->u32 = k->u32 + k2.u32;
      arith_result:
	if (k2.id > k->id) k->id = k2.id;  /* Trivial promotion to unsigned. */
	continue;
      } else if (cp_opt(cp, '-')) {
	cp_expr_sub(cp, &k2, 10); k->u32 = k->u32 - k2.u32; goto arith_result;
      }
    case 10:
      if (cp_opt(cp, '*')) {
	cp_expr_unary(cp, &k2); k->u32 = k->u32 * k2.u32; goto arith_result;
      } else if (cp_opt(cp, '/')) {
	cp_expr_unary(cp, &k2);
	if (k2.id > k->id) k->id = k2.id;  /* Trivial promotion to unsigned. */
	if (k2.u32 == 0 ||
	    (k->id == CTID_INT32 && k->u32 == 0x80000000u && k2.i32 == -1))
	  cp_err(cp, LJ_ERR_BADVAL);
	if (k->id == CTID_INT32)
	  k->i32 = k->i32 / k2.i32;
	else
	  k->u32 = k->u32 / k2.u32;
	continue;
      } else if (cp_opt(cp, '%')) {
	cp_expr_unary(cp, &k2);
	if (k2.id > k->id) k->id = k2.id;  /* Trivial promotion to unsigned. */
	if (k2.u32 == 0 ||
	    (k->id == CTID_INT32 && k->u32 == 0x80000000u && k2.i32 == -1))
	  cp_err(cp, LJ_ERR_BADVAL);
	if (k->id == CTID_INT32)
	  k->i32 = k->i32 % k2.i32;
	else
	  k->u32 = k->u32 % k2.u32;
	continue;
      }
    default:
      return;
    }
  }
}

/* Parse and evaluate unary expression. */
static void cp_expr_unary(CPState *cp, CPValue *k)
{
  if (++cp->depth > CPARSE_MAX_DECLDEPTH) cp_err(cp, LJ_ERR_XLEVELS);
  cp_expr_prefix(cp, k);
  cp_expr_postfix(cp, k);
  cp->depth--;
}

/* Parse and evaluate sub-expression. */
static void cp_expr_sub(CPState *cp, CPValue *k, int pri)
{
  cp_expr_unary(cp, k);
  cp_expr_infix(cp, k, pri);
}

/* Parse constant integer expression. */
static void cp_expr_kint(CPState *cp, CPValue *k)
{
  CType *ct;
  cp_expr_sub(cp, k, 0);
  ct = ctype_raw(cp->cts, k->id);
  if (!ctype_isinteger(ct->info)) cp_err(cp, LJ_ERR_BADVAL);
}

/* Parse (non-negative) size expression. */
static CTSize cp_expr_ksize(CPState *cp)
{
  CPValue k;
  cp_expr_kint(cp, &k);
  if (k.u32 >= 0x80000000u) cp_err(cp, LJ_ERR_FFI_INVSIZE);
  return k.u32;
}

/* -- Type declaration stack management ----------------------------------- */

/* Add declaration element behind the insertion position. */
static CPDeclIdx cp_add(CPDecl *decl, CTInfo info, CTSize size)
{
  CPDeclIdx top = decl->top;
  if (top >= CPARSE_MAX_DECLSTACK) cp_err(decl->cp, LJ_ERR_XLEVELS);
  decl->stack[top].info = info;
  decl->stack[top].size = size;
  decl->stack[top].sib = 0;
  setgcrefnull(decl->stack[top].name);
  decl->stack[top].next = decl->stack[decl->pos].next;
  decl->stack[decl->pos].next = (CTypeID1)top;
  decl->top = top+1;
  return top;
}

/* Push declaration element before the insertion position. */
static CPDeclIdx cp_push(CPDecl *decl, CTInfo info, CTSize size)
{
  return (decl->pos = cp_add(decl, info, size));
}

/* Push or merge attributes. */
static void cp_push_attributes(CPDecl *decl)
{
  CType *ct = &decl->stack[decl->pos];
  if (ctype_isfunc(ct->info)) {  /* Ok to modify in-place. */
#if LJ_TARGET_X86
    if ((decl->fattr & CTFP_CCONV))
      ct->info = (ct->info & (CTMASK_NUM|CTF_VARARG|CTMASK_CID)) +
		 (decl->fattr & ~CTMASK_CID);
#endif
  } else {
    if ((decl->attr & CTFP_ALIGNED) && !(decl->mode & CPARSE_MODE_FIELD))
      cp_push(decl, CTINFO(CT_ATTRIB, CTATTRIB(CTA_ALIGN)),
	      ctype_align(decl->attr));
  }
}

/* Push unrolled type to declaration stack and merge qualifiers. */
static void cp_push_type(CPDecl *decl, CTypeID id)
{
  CType *ct = ctype_get(decl->cp->cts, id);
  CTInfo info = ct->info;
  CTSize size = ct->size;
  switch (ctype_type(info)) {
  case CT_STRUCT: case CT_ENUM:
    cp_push(decl, CTINFO(CT_TYPEDEF, id), 0);  /* Don't copy unique types. */
    if ((decl->attr & CTF_QUAL)) {  /* Push unmerged qualifiers. */
      cp_push(decl, CTINFO(CT_ATTRIB, CTATTRIB(CTA_QUAL)),
	      (decl->attr & CTF_QUAL));
      decl->attr &= ~CTF_QUAL;
    }
    break;
  case CT_ATTRIB:
    if (ctype_isxattrib(info, CTA_QUAL))
      decl->attr &= ~size;  /* Remove redundant qualifiers. */
    cp_push_type(decl, ctype_cid(info));  /* Unroll. */
    cp_push(decl, info & ~CTMASK_CID, size);  /* Copy type. */
    break;
  case CT_ARRAY:
    cp_push_type(decl, ctype_cid(info));  /* Unroll. */
    cp_push(decl, info & ~CTMASK_CID, size);  /* Copy type. */
    decl->stack[decl->pos].sib = 1;  /* Mark as already checked and sized. */
    /* Note: this is not copied to the ct->sib in the C type table. */
    break;
  case CT_FUNC:
    /* Copy type, link parameters (shared). */
    decl->stack[cp_push(decl, info, size)].sib = ct->sib;
    break;
  default:
    /* Copy type, merge common qualifiers. */
    cp_push(decl, info|(decl->attr & CTF_QUAL), size);
    decl->attr &= ~CTF_QUAL;
    break;
  }
}

/* Consume the declaration element chain and intern the C type. */
static CTypeID cp_decl_intern(CPState *cp, CPDecl *decl)
{
  CTypeID id = 0;
  CPDeclIdx idx = 0;
  CTSize csize = CTSIZE_INVALID;
  CTSize cinfo = 0;
  do {
    CType *ct = &decl->stack[idx];
    CTInfo info = ct->info;
    CTInfo size = ct->size;
    /* The cid is already part of info for copies of pointers/functions. */
    idx = ct->next;
    if (ctype_istypedef(info)) {
      lua_assert(id == 0);
      id = ctype_cid(info);
      /* Always refetch info/size, since struct/enum may have been completed. */
      cinfo = ctype_get(cp->cts, id)->info;
      csize = ctype_get(cp->cts, id)->size;
      lua_assert(ctype_isstruct(cinfo) || ctype_isenum(cinfo));
    } else if (ctype_isfunc(info)) {  /* Intern function. */
      CType *fct;
      CTypeID fid;
      CTypeID sib;
      if (id) {
	CType *refct = ctype_raw(cp->cts, id);
	/* Reject function or refarray return types. */
	if (ctype_isfunc(refct->info) || ctype_isrefarray(refct->info))
	  cp_err(cp, LJ_ERR_FFI_INVTYPE);
      }
      /* No intervening attributes allowed, skip forward. */
      while (idx) {
	CType *ctn = &decl->stack[idx];
	if (!ctype_isattrib(ctn->info)) break;
	idx = ctn->next;  /* Skip attribute. */
      }
      sib = ct->sib;  /* Next line may reallocate the C type table. */
      fid = lj_ctype_new(cp->cts, &fct);
      csize = CTSIZE_INVALID;
      fct->info = cinfo = info + id;
      fct->size = size;
      fct->sib = sib;
      id = fid;
    } else if (ctype_isattrib(info)) {
      if (ctype_isxattrib(info, CTA_QUAL))
	cinfo |= size;
      else if (ctype_isxattrib(info, CTA_ALIGN))
	CTF_INSERT(cinfo, ALIGN, size);
      id = lj_ctype_intern(cp->cts, info+id, size);
      /* Inherit csize/cinfo from original type. */
    } else {
      if (ctype_isnum(info)) {  /* Handle mode/vector-size attributes. */
	lua_assert(id == 0);
	if (!(info & CTF_BOOL)) {
	  CTSize msize = ctype_msizeP(decl->attr);
	  CTSize vsize = ctype_vsizeP(decl->attr);
	  if (msize && (!(info & CTF_FP) || (msize == 4 || msize == 8))) {
	    CTSize malign = lj_fls(msize);
	    if (malign > 4) malign = 4;  /* Limit alignment. */
	    CTF_INSERT(info, ALIGN, malign);
	    size = msize;  /* Override size via mode. */
	  }
	  if (vsize) {  /* Vector size set? */
	    CTSize esize = lj_fls(size);
	    if (vsize >= esize) {
	      /* Intern the element type first. */
	      id = lj_ctype_intern(cp->cts, info, size);
	      /* Then create a vector (array) with vsize alignment. */
	      size = (1u << vsize);
	      if (vsize > 4) vsize = 4;  /* Limit alignment. */
	      if (ctype_align(info) > vsize) vsize = ctype_align(info);
	      info = CTINFO(CT_ARRAY, (info & CTF_QUAL) + CTF_VECTOR +
				      CTALIGN(vsize));
	    }
	  }
	}
      } else if (ctype_isptr(info)) {
	/* Reject pointer/ref to ref. */
	if (id && ctype_isref(ctype_raw(cp->cts, id)->info))
	  cp_err(cp, LJ_ERR_FFI_INVTYPE);
	if (ctype_isref(info)) {
	  info &= ~CTF_VOLATILE;  /* Refs are always const, never volatile. */
	  /* No intervening attributes allowed, skip forward. */
	  while (idx) {
	    CType *ctn = &decl->stack[idx];
	    if (!ctype_isattrib(ctn->info)) break;
	    idx = ctn->next;  /* Skip attribute. */
	  }
	}
      } else if (ctype_isarray(info)) {  /* Check for valid array size etc. */
	if (ct->sib == 0) {  /* Only check/size arrays not copied by unroll. */
	  if (ctype_isref(cinfo))  /* Reject arrays of refs. */
	    cp_err(cp, LJ_ERR_FFI_INVTYPE);
	  /* Reject VLS or unknown-sized types. */
	  if (ctype_isvltype(cinfo) || csize == CTSIZE_INVALID)
	    cp_err(cp, LJ_ERR_FFI_INVSIZE);
	  /* a[] and a[?] keep their invalid size. */
	  if (size != CTSIZE_INVALID) {
	    uint64_t xsz = (uint64_t)size * csize;
	    if (xsz >= 0x80000000u) cp_err(cp, LJ_ERR_FFI_INVSIZE);
	    size = (CTSize)xsz;
	  }
	}
	if ((cinfo & CTF_ALIGN) > (info & CTF_ALIGN))  /* Find max. align. */
	  info = (info & ~CTF_ALIGN) | (cinfo & CTF_ALIGN);
	info |= (cinfo & CTF_QUAL);  /* Inherit qual. */
      } else {
	lua_assert(ctype_isvoid(info));
      }
      csize = size;
      cinfo = info+id;
      id = lj_ctype_intern(cp->cts, info+id, size);
    }
  } while (idx);
  return id;
}

/* -- C declaration parser ------------------------------------------------ */

#define H_(le, be)	LJ_ENDIAN_SELECT(0x##le, 0x##be)

/* Reset declaration state to declaration specifier. */
static void cp_decl_reset(CPDecl *decl)
{
  decl->pos = decl->specpos;
  decl->top = decl->specpos+1;
  decl->stack[decl->specpos].next = 0;
  decl->attr = decl->specattr;
  decl->fattr = decl->specfattr;
  decl->name = NULL;
  decl->redir = NULL;
}

/* Parse constant initializer. */
/* NYI: FP constants and strings as initializers. */
static CTypeID cp_decl_constinit(CPState *cp, CType **ctp, CTypeID ctypeid)
{
  CType *ctt = ctype_get(cp->cts, ctypeid);
  CTInfo info;
  CTSize size;
  CPValue k;
  CTypeID constid;
  while (ctype_isattrib(ctt->info)) {  /* Skip attributes. */
    ctypeid = ctype_cid(ctt->info);  /* Update ID, too. */
    ctt = ctype_get(cp->cts, ctypeid);
  }
  info = ctt->info;
  size = ctt->size;
  if (!ctype_isinteger(info) || !(info & CTF_CONST) || size > 4)
    cp_err(cp, LJ_ERR_FFI_INVTYPE);
  cp_check(cp, '=');
  cp_expr_sub(cp, &k, 0);
  constid = lj_ctype_new(cp->cts, ctp);
  (*ctp)->info = CTINFO(CT_CONSTVAL, CTF_CONST|ctypeid);
  k.u32 <<= 8*(4-size);
  if ((info & CTF_UNSIGNED))
    k.u32 >>= 8*(4-size);
  else
    k.u32 = (uint32_t)((int32_t)k.u32 >> 8*(4-size));
  (*ctp)->size = k.u32;
  return constid;
}

/* Parse size in parentheses as part of attribute. */
static CTSize cp_decl_sizeattr(CPState *cp)
{
  CTSize sz;
  uint32_t oldtmask = cp->tmask;
  cp->tmask = CPNS_DEFAULT;  /* Required for expression evaluator. */
  cp_check(cp, '(');
  sz = cp_expr_ksize(cp);
  cp->tmask = oldtmask;
  cp_check(cp, ')');
  return sz;
}

/* Parse alignment attribute. */
static void cp_decl_align(CPState *cp, CPDecl *decl)
{
  CTSize al = 4;  /* Unspecified alignment is 16 bytes. */
  if (cp->tok == '(') {
    al = cp_decl_sizeattr(cp);
    al = al ? lj_fls(al) : 0;
  }
  CTF_INSERT(decl->attr, ALIGN, al);
  decl->attr |= CTFP_ALIGNED;
}

/* Parse GCC asm("name") redirect. */
static void cp_decl_asm(CPState *cp, CPDecl *decl)
{
  UNUSED(decl);
  cp_next(cp);
  cp_check(cp, '(');
  if (cp->tok == CTOK_STRING) {
    GCstr *str = cp->str;
    while (cp_next(cp) == CTOK_STRING) {
      lj_str_pushf(cp->L, "%s%s", strdata(str), strdata(cp->str));
      cp->L->top--;
      str = strV(cp->L->top);
    }
    decl->redir = str;
  }
  cp_check(cp, ')');
}

/* Parse GCC __attribute__((mode(...))). */
static void cp_decl_mode(CPState *cp, CPDecl *decl)
{
  cp_check(cp, '(');
  if (cp->tok == CTOK_IDENT) {
    const char *s = strdata(cp->str);
    CTSize sz = 0, vlen = 0;
    if (s[0] == '_' && s[1] == '_') s += 2;
    if (*s == 'V') {
      s++;
      vlen = *s++ - '0';
      if (*s >= '0' && *s <= '9')
	vlen = vlen*10 + (*s++ - '0');
    }
    switch (*s++) {
    case 'Q': sz = 1; break;
    case 'H': sz = 2; break;
    case 'S': sz = 4; break;
    case 'D': sz = 8; break;
    case 'T': sz = 16; break;
    case 'O': sz = 32; break;
    default: goto bad_size;
    }
    if (*s == 'I' || *s == 'F') {
      CTF_INSERT(decl->attr, MSIZEP, sz);
      if (vlen) CTF_INSERT(decl->attr, VSIZEP, lj_fls(vlen*sz));
    }
  bad_size:
    cp_next(cp);
  }
  cp_check(cp, ')');
}

/* Parse GCC __attribute__((...)). */
static void cp_decl_gccattribute(CPState *cp, CPDecl *decl)
{
  cp_next(cp);
  cp_check(cp, '(');
  cp_check(cp, '(');
  while (cp->tok != ')') {
    if (cp->tok == CTOK_IDENT) {
      GCstr *attrstr = cp->str;
      cp_next(cp);
      switch (attrstr->hash) {
      case H_(64a9208e,8ce14319): case H_(8e6331b2,95a282af):  /* aligned */
	cp_decl_align(cp, decl);
	break;
      case H_(42eb47de,f0ede26c): case H_(29f48a09,cf383e0c):  /* packed */
	decl->attr |= CTFP_PACKED;
	break;
      case H_(0a84eef6,8dfab04c): case H_(995cf92c,d5696591):  /* mode */
	cp_decl_mode(cp, decl);
	break;
      case H_(0ab31997,2d5213fa): case H_(bf875611,200e9990):  /* vector_size */
	{
	  CTSize vsize = cp_decl_sizeattr(cp);
	  if (vsize) CTF_INSERT(decl->attr, VSIZEP, lj_fls(vsize));
	}
	break;
#if LJ_TARGET_X86
      case H_(5ad22db8,c689b848): case H_(439150fa,65ea78cb):  /* regparm */
	CTF_INSERT(decl->fattr, REGPARM, cp_decl_sizeattr(cp));
	decl->fattr |= CTFP_CCONV;
	break;
      case H_(18fc0b98,7ff4c074): case H_(4e62abed,0a747424):  /* cdecl */
	CTF_INSERT(decl->fattr, CCONV, CTCC_CDECL);
	decl->fattr |= CTFP_CCONV;
	break;
      case H_(72b2e41b,494c5a44): case H_(f2356d59,f25fc9bd):  /* thiscall */
	CTF_INSERT(decl->fattr, CCONV, CTCC_THISCALL);
	decl->fattr |= CTFP_CCONV;
	break;
      case H_(0d0ffc42,ab746f88): case H_(21c54ba1,7f0ca7e3):  /* fastcall */
	CTF_INSERT(decl->fattr, CCONV, CTCC_FASTCALL);
	decl->fattr |= CTFP_CCONV;
	break;
      case H_(ef76b040,9412e06a): case H_(de56697b,c750e6e1):  /* stdcall */
	CTF_INSERT(decl->fattr, CCONV, CTCC_STDCALL);
	decl->fattr |= CTFP_CCONV;
	break;
      case H_(ea78b622,f234bd8e): case H_(252ffb06,8d50f34b):  /* sseregparm */
	decl->fattr |= CTF_SSEREGPARM;
	decl->fattr |= CTFP_CCONV;
	break;
#endif
      default:  /* Skip all other attributes. */
	goto skip_attr;
      }
    } else if (cp->tok >= CTOK_FIRSTDECL) {  /* For __attribute((const)) etc. */
      cp_next(cp);
    skip_attr:
      if (cp_opt(cp, '(')) {
	while (cp->tok != ')' && cp->tok != CTOK_EOF) cp_next(cp);
	cp_check(cp, ')');
      }
    } else {
      break;
    }
    if (!cp_opt(cp, ',')) break;
  }
  cp_check(cp, ')');
  cp_check(cp, ')');
}

/* Parse MSVC __declspec(...). */
static void cp_decl_msvcattribute(CPState *cp, CPDecl *decl)
{
  cp_next(cp);
  cp_check(cp, '(');
  while (cp->tok == CTOK_IDENT) {
    GCstr *attrstr = cp->str;
    cp_next(cp);
    switch (attrstr->hash) {
    case H_(bc2395fa,98f267f8):  /* align */
      cp_decl_align(cp, decl);
      break;
    default:  /* Ignore all other attributes. */
      if (cp_opt(cp, '(')) {
	while (cp->tok != ')' && cp->tok != CTOK_EOF) cp_next(cp);
	cp_check(cp, ')');
      }
      break;
    }
  }
  cp_check(cp, ')');
}

/* Parse declaration attributes (and common qualifiers). */
static void cp_decl_attributes(CPState *cp, CPDecl *decl)
{
  for (;;) {
    switch (cp->tok) {
    case CTOK_CONST: decl->attr |= CTF_CONST; break;
    case CTOK_VOLATILE: decl->attr |= CTF_VOLATILE; break;
    case CTOK_RESTRICT: break;  /* Ignore. */
    case CTOK_EXTENSION: break;  /* Ignore. */
    case CTOK_ATTRIBUTE: cp_decl_gccattribute(cp, decl); continue;
    case CTOK_ASM: cp_decl_asm(cp, decl); continue;
    case CTOK_DECLSPEC: cp_decl_msvcattribute(cp, decl); continue;
    case CTOK_CCDECL:
#if LJ_TARGET_X86
      CTF_INSERT(decl->fattr, CCONV, cp->ct->size);
      decl->fattr |= CTFP_CCONV;
#endif
      break;
    case CTOK_PTRSZ:
#if LJ_64
      CTF_INSERT(decl->attr, MSIZEP, cp->ct->size);
#endif
      break;
    default: return;
    }
    cp_next(cp);
  }
}

/* Parse struct/union/enum name. */
static CTypeID cp_struct_name(CPState *cp, CPDecl *sdecl, CTInfo info)
{
  CTypeID sid;
  CType *ct;
  cp->tmask = CPNS_STRUCT;
  cp_next(cp);
  cp_decl_attributes(cp, sdecl);
  cp->tmask = CPNS_DEFAULT;
  if (cp->tok != '{') {
    if (cp->tok != CTOK_IDENT) cp_err_token(cp, CTOK_IDENT);
    if (cp->val.id) {  /* Name of existing struct/union/enum. */
      sid = cp->val.id;
      ct = cp->ct;
      if ((ct->info ^ info) & (CTMASK_NUM|CTF_UNION))  /* Wrong type. */
	cp_errmsg(cp, 0, LJ_ERR_FFI_REDEF, strdata(gco2str(gcref(ct->name))));
    } else {  /* Create named, incomplete struct/union/enum. */
      if ((cp->mode & CPARSE_MODE_NOIMPLICIT))
	cp_errmsg(cp, 0, LJ_ERR_FFI_BADTAG, strdata(cp->str));
      sid = lj_ctype_new(cp->cts, &ct);
      ct->info = info;
      ct->size = CTSIZE_INVALID;
      ctype_setname(ct, cp->str);
      lj_ctype_addname(cp->cts, ct, sid);
    }
    cp_next(cp);
  } else {  /* Create anonymous, incomplete struct/union/enum. */
    sid = lj_ctype_new(cp->cts, &ct);
    ct->info = info;
    ct->size = CTSIZE_INVALID;
  }
  if (cp->tok == '{') {
    if (ct->size != CTSIZE_INVALID || ct->sib)
      cp_errmsg(cp, 0, LJ_ERR_FFI_REDEF, strdata(gco2str(gcref(ct->name))));
    ct->sib = 1;  /* Indicate the type is currently being defined. */
  }
  return sid;
}

/* Determine field alignment. */
static CTSize cp_field_align(CPState *cp, CType *ct, CTInfo info)
{
  CTSize align = ctype_align(info);
  UNUSED(cp); UNUSED(ct);
#if (LJ_TARGET_X86 && !LJ_ABI_WIN) || (LJ_TARGET_ARM && __APPLE__)
  /* The SYSV i386 and iOS ABIs limit alignment of non-vector fields to 2^2. */
  if (align > 2 && !(info & CTFP_ALIGNED)) {
    if (ctype_isarray(info) && !(info & CTF_VECTOR)) {
      do {
	ct = ctype_rawchild(cp->cts, ct);
	info = ct->info;
      } while (ctype_isarray(info) && !(info & CTF_VECTOR));
    }
    if (ctype_isnum(info) || ctype_isenum(info))
      align = 2;
  }
#endif
  return align;
}

/* Layout struct/union fields. */
static void cp_struct_layout(CPState *cp, CTypeID sid, CTInfo sattr)
{
  CTSize bofs = 0, bmaxofs = 0;  /* Bit offset and max. bit offset. */
  CTSize maxalign = ctype_align(sattr);
  CType *sct = ctype_get(cp->cts, sid);
  CTInfo sinfo = sct->info;
  CTypeID fieldid = sct->sib;
  while (fieldid) {
    CType *ct = ctype_get(cp->cts, fieldid);
    CTInfo attr = ct->size;  /* Field declaration attributes (temp.). */

    if (ctype_isfield(ct->info) ||
	(ctype_isxattrib(ct->info, CTA_SUBTYPE) && attr)) {
      CTSize align, amask;  /* Alignment (pow2) and alignment mask (bits). */
      CTSize sz;
      CTInfo info = lj_ctype_info(cp->cts, ctype_cid(ct->info), &sz);
      CTSize bsz, csz = 8*sz;  /* Field size and container size (in bits). */
      sinfo |= (info & (CTF_QUAL|CTF_VLA));  /* Merge pseudo-qualifiers. */

      /* Check for size overflow and determine alignment. */
      if (sz >= 0x20000000u || bofs + csz < bofs) {
	if (!(sz == CTSIZE_INVALID && ctype_isarray(info) &&
	      !(sinfo & CTF_UNION)))
	  cp_err(cp, LJ_ERR_FFI_INVSIZE);
	csz = sz = 0;  /* Treat a[] and a[?] as zero-sized. */
      }
      align = cp_field_align(cp, ct, info);
      if (((attr|sattr) & CTFP_PACKED) ||
	  ((attr & CTFP_ALIGNED) && ctype_align(attr) > align))
	align = ctype_align(attr);
      if (cp->packstack[cp->curpack] < align)
	align = cp->packstack[cp->curpack];
      if (align > maxalign) maxalign = align;
      amask = (8u << align) - 1;

      bsz = ctype_bitcsz(ct->info);  /* Bitfield size (temp.). */
      if (bsz == CTBSZ_FIELD || !ctype_isfield(ct->info)) {
	bsz = csz;  /* Regular fields or subtypes always fill the container. */
	bofs = (bofs + amask) & ~amask;  /* Start new aligned field. */
	ct->size = (bofs >> 3);  /* Store field offset. */
      } else {  /* Bitfield. */
	if (bsz == 0 || (attr & CTFP_ALIGNED) ||
	    (!((attr|sattr) & CTFP_PACKED) && (bofs & amask) + bsz > csz))
	  bofs = (bofs + amask) & ~amask;  /* Start new aligned field. */

	/* Prefer regular field over bitfield. */
	if (bsz == csz && (bofs & amask) == 0) {
	  ct->info = CTINFO(CT_FIELD, ctype_cid(ct->info));
	  ct->size = (bofs >> 3);  /* Store field offset. */
	} else {
	  ct->info = CTINFO(CT_BITFIELD,
	    (info & (CTF_QUAL|CTF_UNSIGNED|CTF_BOOL)) +
	    (csz << (CTSHIFT_BITCSZ-3)) + (bsz << CTSHIFT_BITBSZ));
#if LJ_BE
	  ct->info += ((csz - (bofs & (csz-1)) - bsz) << CTSHIFT_BITPOS);
#else
	  ct->info += ((bofs & (csz-1)) << CTSHIFT_BITPOS);
#endif
	  ct->size = ((bofs & ~(csz-1)) >> 3);  /* Store container offset. */
	}
      }

      /* Determine next offset or max. offset. */
      if ((sinfo & CTF_UNION)) {
	if (bsz > bmaxofs) bmaxofs = bsz;
      } else {
	bofs += bsz;
      }
    }  /* All other fields in the chain are already set up. */

    fieldid = ct->sib;
  }

  /* Complete struct/union. */
  sct->info = sinfo + CTALIGN(maxalign);
  bofs = (sinfo & CTF_UNION) ? bmaxofs : bofs;
  maxalign = (8u << maxalign) - 1;
  sct->size = (((bofs + maxalign) & ~maxalign) >> 3);
}

/* Parse struct/union declaration. */
static CTypeID cp_decl_struct(CPState *cp, CPDecl *sdecl, CTInfo sinfo)
{
  CTypeID sid = cp_struct_name(cp, sdecl, sinfo);
  if (cp_opt(cp, '{')) {  /* Struct/union definition. */
    CTypeID lastid = sid;
    int lastdecl = 0;
    while (cp->tok != '}') {
      CPDecl decl;
      CPscl scl = cp_decl_spec(cp, &decl, CDF_STATIC);
      decl.mode = scl ? CPARSE_MODE_DIRECT :
	CPARSE_MODE_DIRECT|CPARSE_MODE_ABSTRACT|CPARSE_MODE_FIELD;

      for (;;) {
	CTypeID ctypeid;

	if (lastdecl) cp_err_token(cp, '}');

	/* Parse field declarator. */
	decl.bits = CTSIZE_INVALID;
	cp_declarator(cp, &decl);
	ctypeid = cp_decl_intern(cp, &decl);

	if ((scl & CDF_STATIC)) {  /* Static constant in struct namespace. */
	  CType *ct;
	  CTypeID fieldid = cp_decl_constinit(cp, &ct, ctypeid);
	  ctype_get(cp->cts, lastid)->sib = fieldid;
	  lastid = fieldid;
	  ctype_setname(ct, decl.name);
	} else {
	  CTSize bsz = CTBSZ_FIELD;  /* Temp. for layout phase. */
	  CType *ct;
	  CTypeID fieldid = lj_ctype_new(cp->cts, &ct);  /* Do this first. */
	  CType *tct = ctype_raw(cp->cts, ctypeid);

	  if (decl.bits == CTSIZE_INVALID) {  /* Regular field. */
	    if (ctype_isarray(tct->info) && tct->size == CTSIZE_INVALID)
	      lastdecl = 1;  /* a[] or a[?] must be the last declared field. */

	    /* Accept transparent struct/union/enum. */
	    if (!decl.name) {
	      if (!((ctype_isstruct(tct->info) && !(tct->info & CTF_VLA)) ||
		    ctype_isenum(tct->info)))
		cp_err_token(cp, CTOK_IDENT);
	      ct->info = CTINFO(CT_ATTRIB, CTATTRIB(CTA_SUBTYPE) + ctypeid);
	      ct->size = ctype_isstruct(tct->info) ?
			 (decl.attr|0x80000000u) : 0;  /* For layout phase. */
	      goto add_field;
	    }
	  } else {  /* Bitfield. */
	    bsz = decl.bits;
	    if (!ctype_isinteger_or_bool(tct->info) ||
		(bsz == 0 && decl.name) || 8*tct->size > CTBSZ_MAX ||
		bsz > ((tct->info & CTF_BOOL) ? 1 : 8*tct->size))
	      cp_errmsg(cp, ':', LJ_ERR_BADVAL);
	  }

	  /* Create temporary field for layout phase. */
	  ct->info = CTINFO(CT_FIELD, ctypeid + (bsz << CTSHIFT_BITCSZ));
	  ct->size = decl.attr;
	  if (decl.name) ctype_setname(ct, decl.name);

	add_field:
	  ctype_get(cp->cts, lastid)->sib = fieldid;
	  lastid = fieldid;
	}
	if (!cp_opt(cp, ',')) break;
	cp_decl_reset(&decl);
      }
      cp_check(cp, ';');
    }
    cp_check(cp, '}');
    ctype_get(cp->cts, lastid)->sib = 0;  /* Drop sib = 1 for empty structs. */
    cp_decl_attributes(cp, sdecl);  /* Layout phase needs postfix attributes. */
    cp_struct_layout(cp, sid, sdecl->attr);
  }
  return sid;
}

/* Parse enum declaration. */
static CTypeID cp_decl_enum(CPState *cp, CPDecl *sdecl)
{
  CTypeID eid = cp_struct_name(cp, sdecl, CTINFO(CT_ENUM, CTID_VOID));
  CTInfo einfo = CTINFO(CT_ENUM, CTALIGN(2) + CTID_UINT32);
  CTSize esize = 4;  /* Only 32 bit enums are supported. */
  if (cp_opt(cp, '{')) {  /* Enum definition. */
    CPValue k;
    CTypeID lastid = eid;
    k.u32 = 0;
    k.id = CTID_INT32;
    do {
      GCstr *name = cp->str;
      if (cp->tok != CTOK_IDENT) cp_err_token(cp, CTOK_IDENT);
      if (cp->val.id) cp_errmsg(cp, 0, LJ_ERR_FFI_REDEF, strdata(name));
      cp_next(cp);
      if (cp_opt(cp, '=')) {
	cp_expr_kint(cp, &k);
	if (k.id == CTID_UINT32) {
	  /* C99 says that enum constants are always (signed) integers.
	  ** But since unsigned constants like 0x80000000 are quite common,
	  ** those are left as uint32_t.
	  */
	  if (k.i32 >= 0) k.id = CTID_INT32;
	} else {
	  /* OTOH it's common practice and even mandated by some ABIs
	  ** that the enum type itself is unsigned, unless there are any
	  ** negative constants.
	  */
	  k.id = CTID_INT32;
	  if (k.i32 < 0) einfo = CTINFO(CT_ENUM, CTALIGN(2) + CTID_INT32);
	}
      }
      /* Add named enum constant. */
      {
	CType *ct;
	CTypeID constid = lj_ctype_new(cp->cts, &ct);
	ctype_get(cp->cts, lastid)->sib = constid;
	lastid = constid;
	ctype_setname(ct, name);
	ct->info = CTINFO(CT_CONSTVAL, CTF_CONST|k.id);
	ct->size = k.u32++;
	if (k.u32 == 0x80000000u) k.id = CTID_UINT32;
	lj_ctype_addname(cp->cts, ct, constid);
      }
      if (!cp_opt(cp, ',')) break;
    } while (cp->tok != '}');  /* Trailing ',' is ok. */
    cp_check(cp, '}');
    /* Complete enum. */
    ctype_get(cp->cts, eid)->info = einfo;
    ctype_get(cp->cts, eid)->size = esize;
  }
  return eid;
}

/* Parse declaration specifiers. */
static CPscl cp_decl_spec(CPState *cp, CPDecl *decl, CPscl scl)
{
  uint32_t cds = 0, sz = 0;
  CTypeID tdef = 0;

  decl->cp = cp;
  decl->mode = cp->mode;
  decl->name = NULL;
  decl->redir = NULL;
  decl->attr = 0;
  decl->fattr = 0;
  decl->pos = decl->top = 0;
  decl->stack[0].next = 0;

  for (;;) {  /* Parse basic types. */
    cp_decl_attributes(cp, decl);
    if (cp->tok >= CTOK_FIRSTDECL && cp->tok <= CTOK_LASTDECLFLAG) {
      uint32_t cbit;
      if (cp->ct->size) {
	if (sz) goto end_decl;
	sz = cp->ct->size;
      }
      cbit = (1u << (cp->tok - CTOK_FIRSTDECL));
      cds = cds | cbit | ((cbit & cds & CDF_LONG) << 1);
      if (cp->tok >= CTOK_FIRSTSCL) {
	if (!(scl & cbit)) cp_errmsg(cp, cp->tok, LJ_ERR_FFI_BADSCL);
      } else if (tdef) {
	goto end_decl;
      }
      cp_next(cp);
      continue;
    }
    if (sz || tdef ||
	(cds & (CDF_SHORT|CDF_LONG|CDF_SIGNED|CDF_UNSIGNED|CDF_COMPLEX)))
      break;
    switch (cp->tok) {
    case CTOK_STRUCT:
      tdef = cp_decl_struct(cp, decl, CTINFO(CT_STRUCT, 0));
      continue;
    case CTOK_UNION:
      tdef = cp_decl_struct(cp, decl, CTINFO(CT_STRUCT, CTF_UNION));
      continue;
    case CTOK_ENUM:
      tdef = cp_decl_enum(cp, decl);
      continue;
    case CTOK_IDENT:
      if (ctype_istypedef(cp->ct->info)) {
	tdef = ctype_cid(cp->ct->info);  /* Get typedef. */
	cp_next(cp);
	continue;
      }
      break;
    case '$':
      tdef = cp->val.id;
      cp_next(cp);
      continue;
    default:
      break;
    }
    break;
  }
end_decl:

  if ((cds & CDF_COMPLEX))  /* Use predefined complex types. */
    tdef = sz == 4 ? CTID_COMPLEX_FLOAT : CTID_COMPLEX_DOUBLE;

  if (tdef) {
    cp_push_type(decl, tdef);
  } else if ((cds & CDF_VOID)) {
    cp_push(decl, CTINFO(CT_VOID, (decl->attr & CTF_QUAL)), CTSIZE_INVALID);
    decl->attr &= ~CTF_QUAL;
  } else {
    /* Determine type info and size. */
    CTInfo info = CTINFO(CT_NUM, (cds & CDF_UNSIGNED) ? CTF_UNSIGNED : 0);
    if ((cds & CDF_BOOL)) {
      if ((cds & ~(CDF_SCL|CDF_BOOL|CDF_INT|CDF_SIGNED|CDF_UNSIGNED)))
	cp_errmsg(cp, 0, LJ_ERR_FFI_INVTYPE);
      info |= CTF_BOOL;
      if (!(cds & CDF_SIGNED)) info |= CTF_UNSIGNED;
      if (!sz) {
	sz = 1;
      }
    } else if ((cds & CDF_FP)) {
      info = CTINFO(CT_NUM, CTF_FP);
      if ((cds & CDF_LONG)) sz = sizeof(long double);
    } else if ((cds & CDF_CHAR)) {
      if ((cds & (CDF_CHAR|CDF_SIGNED|CDF_UNSIGNED)) == CDF_CHAR)
	info |= CTF_UCHAR;  /* Handle platforms where char is unsigned. */
    } else if ((cds & CDF_SHORT)) {
      sz = sizeof(short);
    } else if ((cds & CDF_LONGLONG)) {
      sz = 8;
    } else if ((cds & CDF_LONG)) {
      info |= CTF_LONG;
      sz = sizeof(long);
    } else if (!sz) {
      if (!(cds & (CDF_SIGNED|CDF_UNSIGNED)))
	cp_errmsg(cp, cp->tok, LJ_ERR_FFI_DECLSPEC);
      sz = sizeof(int);
    }
    lua_assert(sz != 0);
    info += CTALIGN(lj_fls(sz));  /* Use natural alignment. */
    info += (decl->attr & CTF_QUAL);  /* Merge qualifiers. */
    cp_push(decl, info, sz);
    decl->attr &= ~CTF_QUAL;
  }
  decl->specpos = decl->pos;
  decl->specattr = decl->attr;
  decl->specfattr = decl->fattr;
  return (cds & CDF_SCL);  /* Return storage class. */
}

/* Parse array declaration. */
static void cp_decl_array(CPState *cp, CPDecl *decl)
{
  CTInfo info = CTINFO(CT_ARRAY, 0);
  CTSize nelem = CTSIZE_INVALID;  /* Default size for a[] or a[?]. */
  cp_decl_attributes(cp, decl);
  if (cp_opt(cp, '?'))
    info |= CTF_VLA;  /* Create variable-length array a[?]. */
  else if (cp->tok != ']')
    nelem = cp_expr_ksize(cp);
  cp_check(cp, ']');
  cp_add(decl, info, nelem);
}

/* Parse function declaration. */
static void cp_decl_func(CPState *cp, CPDecl *fdecl)
{
  CTSize nargs = 0;
  CTInfo info = CTINFO(CT_FUNC, 0);
  CTypeID lastid = 0, anchor = 0;
  if (cp->tok != ')') {
    do {
      CPDecl decl;
      CTypeID ctypeid, fieldid;
      CType *ct;
      if (cp_opt(cp, '.')) {  /* Vararg function. */
	cp_check(cp, '.');  /* Workaround for the minimalistic lexer. */
	cp_check(cp, '.');
	info |= CTF_VARARG;
	break;
      }
      cp_decl_spec(cp, &decl, CDF_REGISTER);
      decl.mode = CPARSE_MODE_DIRECT|CPARSE_MODE_ABSTRACT;
      cp_declarator(cp, &decl);
      ctypeid = cp_decl_intern(cp, &decl);
      ct = ctype_raw(cp->cts, ctypeid);
      if (ctype_isvoid(ct->info))
	break;
      else if (ctype_isrefarray(ct->info))
	ctypeid = lj_ctype_intern(cp->cts,
	  CTINFO(CT_PTR, CTALIGN_PTR|ctype_cid(ct->info)), CTSIZE_PTR);
      else if (ctype_isfunc(ct->info))
	ctypeid = lj_ctype_intern(cp->cts,
	  CTINFO(CT_PTR, CTALIGN_PTR|ctypeid), CTSIZE_PTR);
      /* Add new parameter. */
      fieldid = lj_ctype_new(cp->cts, &ct);
      if (anchor)
	ctype_get(cp->cts, lastid)->sib = fieldid;
      else
	anchor = fieldid;
      lastid = fieldid;
      if (decl.name) ctype_setname(ct, decl.name);
      ct->info = CTINFO(CT_FIELD, ctypeid);
      ct->size = nargs++;
    } while (cp_opt(cp, ','));
  }
  cp_check(cp, ')');
  if (cp_opt(cp, '{')) {  /* Skip function definition. */
    int level = 1;
    cp->mode |= CPARSE_MODE_SKIP;
    for (;;) {
      if (cp->tok == '{') level++;
      else if (cp->tok == '}' && --level == 0) break;
      else if (cp->tok == CTOK_EOF) cp_err_token(cp, '}');
      cp_next(cp);
    }
    cp->mode &= ~CPARSE_MODE_SKIP;
    cp->tok = ';';  /* Ok for cp_decl_multi(), error in cp_decl_single(). */
  }
  info |= (fdecl->fattr & ~CTMASK_CID);
  fdecl->fattr = 0;
  fdecl->stack[cp_add(fdecl, info, nargs)].sib = anchor;
}

/* Parse declarator. */
static void cp_declarator(CPState *cp, CPDecl *decl)
{
  if (++cp->depth > CPARSE_MAX_DECLDEPTH) cp_err(cp, LJ_ERR_XLEVELS);

  for (;;) {  /* Head of declarator. */
    if (cp_opt(cp, '*')) {  /* Pointer. */
      CTSize sz;
      CTInfo info;
      cp_decl_attributes(cp, decl);
      sz = CTSIZE_PTR;
      info = CTINFO(CT_PTR, CTALIGN_PTR);
#if LJ_64
      if (ctype_msizeP(decl->attr) == 4) {
	sz = 4;
	info = CTINFO(CT_PTR, CTALIGN(2));
      }
#endif
      info += (decl->attr & (CTF_QUAL|CTF_REF));
      decl->attr &= ~(CTF_QUAL|(CTMASK_MSIZEP<<CTSHIFT_MSIZEP));
      cp_push(decl, info, sz);
    } else if (cp_opt(cp, '&') || cp_opt(cp, CTOK_ANDAND)) {  /* Reference. */
      decl->attr &= ~(CTF_QUAL|(CTMASK_MSIZEP<<CTSHIFT_MSIZEP));
      cp_push(decl, CTINFO_REF(0), CTSIZE_PTR);
    } else {
      break;
    }
  }

  if (cp_opt(cp, '(')) {  /* Inner declarator. */
    CPDeclIdx pos;
    cp_decl_attributes(cp, decl);
    /* Resolve ambiguity between inner declarator and 1st function parameter. */
    if ((decl->mode & CPARSE_MODE_ABSTRACT) &&
	(cp->tok == ')' || cp_istypedecl(cp))) goto func_decl;
    pos = decl->pos;
    cp_declarator(cp, decl);
    cp_check(cp, ')');
    decl->pos = pos;
  } else if (cp->tok == CTOK_IDENT) {  /* Direct declarator. */
    if (!(decl->mode & CPARSE_MODE_DIRECT)) cp_err_token(cp, CTOK_EOF);
    decl->name = cp->str;
    decl->nameid = cp->val.id;
    cp_next(cp);
  } else {  /* Abstract declarator. */
    if (!(decl->mode & CPARSE_MODE_ABSTRACT)) cp_err_token(cp, CTOK_IDENT);
  }

  for (;;) {  /* Tail of declarator. */
    if (cp_opt(cp, '[')) {  /* Array. */
      cp_decl_array(cp, decl);
    } else if (cp_opt(cp, '(')) {  /* Function. */
    func_decl:
      cp_decl_func(cp, decl);
    } else {
      break;
    }
  }

  if ((decl->mode & CPARSE_MODE_FIELD) && cp_opt(cp, ':'))  /* Field width. */
    decl->bits = cp_expr_ksize(cp);

  /* Process postfix attributes. */
  cp_decl_attributes(cp, decl);
  cp_push_attributes(decl);

  cp->depth--;
}

/* Parse an abstract type declaration and return it's C type ID. */
static CTypeID cp_decl_abstract(CPState *cp)
{
  CPDecl decl;
  cp_decl_spec(cp, &decl, 0);
  decl.mode = CPARSE_MODE_ABSTRACT;
  cp_declarator(cp, &decl);
  return cp_decl_intern(cp, &decl);
}

/* Handle pragmas. */
static void cp_pragma(CPState *cp, BCLine pragmaline)
{
  cp_next(cp);
  if (cp->tok == CTOK_IDENT &&
      cp->str->hash == H_(e79b999f,42ca3e85))  {  /* pack */
    cp_next(cp);
    cp_check(cp, '(');
    if (cp->tok == CTOK_IDENT) {
      if (cp->str->hash == H_(738e923c,a1b65954)) {  /* push */
	if (cp->curpack < CPARSE_MAX_PACKSTACK) {
	  cp->packstack[cp->curpack+1] = cp->packstack[cp->curpack];
	  cp->curpack++;
	}
      } else if (cp->str->hash == H_(6c71cf27,6c71cf27)) {  /* pop */
	if (cp->curpack > 0) cp->curpack--;
      } else {
	cp_errmsg(cp, cp->tok, LJ_ERR_XSYMBOL);
      }
      cp_next(cp);
      if (!cp_opt(cp, ',')) goto end_pack;
    }
    if (cp->tok == CTOK_INTEGER) {
      cp->packstack[cp->curpack] = cp->val.u32 ? lj_fls(cp->val.u32) : 0;
      cp_next(cp);
    } else {
      cp->packstack[cp->curpack] = 255;
    }
  end_pack:
    cp_check(cp, ')');
  } else {  /* Ignore all other pragmas. */
    while (cp->tok != CTOK_EOF && cp->linenumber == pragmaline)
      cp_next(cp);
  }
}

/* Parse multiple C declarations of types or extern identifiers. */
static void cp_decl_multi(CPState *cp)
{
  int first = 1;
  while (cp->tok != CTOK_EOF) {
    CPDecl decl;
    CPscl scl;
    if (cp_opt(cp, ';')) {  /* Skip empty statements. */
      first = 0;
      continue;
    }
    if (cp->tok == '#') {  /* Workaround, since we have no preprocessor, yet. */
      BCLine pragmaline = cp->linenumber;
      if (!(cp_next(cp) == CTOK_IDENT &&
	    cp->str->hash == H_(f5e6b4f8,1d509107)))  /* pragma */
	cp_errmsg(cp, cp->tok, LJ_ERR_XSYMBOL);
      cp_pragma(cp, pragmaline);
      continue;
    }
    scl = cp_decl_spec(cp, &decl, CDF_TYPEDEF|CDF_EXTERN|CDF_STATIC);
    if ((cp->tok == ';' || cp->tok == CTOK_EOF) &&
	ctype_istypedef(decl.stack[0].info)) {
      CTInfo info = ctype_rawchild(cp->cts, &decl.stack[0])->info;
      if (ctype_isstruct(info) || ctype_isenum(info))
	goto decl_end;  /* Accept empty declaration of struct/union/enum. */
    }
    for (;;) {
      CTypeID ctypeid;
      cp_declarator(cp, &decl);
      ctypeid = cp_decl_intern(cp, &decl);
      if (decl.name && !decl.nameid) {  /* NYI: redeclarations are ignored. */
	CType *ct;
	CTypeID id;
	if ((scl & CDF_TYPEDEF)) {  /* Create new typedef. */
	  id = lj_ctype_new(cp->cts, &ct);
	  ct->info = CTINFO(CT_TYPEDEF, ctypeid);
	  goto noredir;
	} else if (ctype_isfunc(ctype_get(cp->cts, ctypeid)->info)) {
	  /* Treat both static and extern function declarations as extern. */
	  ct = ctype_get(cp->cts, ctypeid);
	  /* We always get new anonymous functions (typedefs are copied). */
	  lua_assert(gcref(ct->name) == NULL);
	  id = ctypeid;  /* Just name it. */
	} else if ((scl & CDF_STATIC)) {  /* Accept static constants. */
	  id = cp_decl_constinit(cp, &ct, ctypeid);
	  goto noredir;
	} else {  /* External references have extern or no storage class. */
	  id = lj_ctype_new(cp->cts, &ct);
	  ct->info = CTINFO(CT_EXTERN, ctypeid);
	}
	if (decl.redir) {  /* Add attribute for redirected symbol name. */
	  CType *cta;
	  CTypeID aid = lj_ctype_new(cp->cts, &cta);
	  ct = ctype_get(cp->cts, id);  /* Table may have been reallocated. */
	  cta->info = CTINFO(CT_ATTRIB, CTATTRIB(CTA_REDIR));
	  cta->sib = ct->sib;
	  ct->sib = aid;
	  ctype_setname(cta, decl.redir);
	}
      noredir:
	ctype_setname(ct, decl.name);
	lj_ctype_addname(cp->cts, ct, id);
      }
      if (!cp_opt(cp, ',')) break;
      cp_decl_reset(&decl);
    }
  decl_end:
    if (cp->tok == CTOK_EOF && first) break;  /* May omit ';' for 1 decl. */
    first = 0;
    cp_check(cp, ';');
  }
}

/* Parse a single C type declaration. */
static void cp_decl_single(CPState *cp)
{
  CPDecl decl;
  cp_decl_spec(cp, &decl, 0);
  cp_declarator(cp, &decl);
  cp->val.id = cp_decl_intern(cp, &decl);
  if (cp->tok != CTOK_EOF) cp_err_token(cp, CTOK_EOF);
}

#undef H_

/* ------------------------------------------------------------------------ */

/* Protected callback for C parser. */
static TValue *cpcparser(lua_State *L, lua_CFunction dummy, void *ud)
{
  CPState *cp = (CPState *)ud;
  UNUSED(dummy);
  cframe_errfunc(L->cframe) = -1;  /* Inherit error function. */
  cp_init(cp);
  if ((cp->mode & CPARSE_MODE_MULTI))
    cp_decl_multi(cp);
  else
    cp_decl_single(cp);
  if (cp->param && cp->param != cp->L->top)
    cp_err(cp, LJ_ERR_FFI_NUMPARAM);
  lua_assert(cp->depth == 0);
  return NULL;
}

/* C parser. */
int lj_cparse(CPState *cp)
{
  LJ_CTYPE_SAVE(cp->cts);
  int errcode = lj_vm_cpcall(cp->L, NULL, cp, cpcparser);
  if (errcode)
    LJ_CTYPE_RESTORE(cp->cts);
  cp_cleanup(cp);
  return errcode;
}

#endif
