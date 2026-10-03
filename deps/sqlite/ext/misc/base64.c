/*
** 2022-11-18
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
**
** This is a SQLite extension for converting in either direction
** between a (binary) blob and base64 text. Base64 can transit a
** sane USASCII channel unmolested. It also plays nicely in CSV or
** written as TCL brace-enclosed literals or SQL string literals,
** and can be used unmodified in XML-like documents.
**
** This is an independent implementation of conversions specified in
** RFC 4648, done on the above date by the author (Larry Brasfield)
** who thereby has the right to put this into the public domain.
**
** The conversions meet RFC 4648 requirements, provided that this
** C source specifies that line-feeds are included in the encoded
** data to limit visible line lengths to 72 characters and to
** terminate any encoded blob having non-zero length.
**
** Length limitations are not imposed except that the runtime
** SQLite string or blob length limits are respected. Otherwise,
** any length binary sequence can be represented and recovered.
** Generated base64 sequences, with their line-feeds included,
** can be concatenated; the result converted back to binary will
** be the concatenation of the represented binary sequences.
**
** This SQLite3 extension creates a function, base64(x), which
** either: converts text x containing base64 to a returned blob;
** or converts a blob x to returned text containing base64. An
** error will be thrown for other input argument types.
**
** This code relies on UTF-8 encoding only with respect to the
** meaning of the first 128 (7-bit) codes matching that of USASCII.
** It will fail miserably if somehow made to try to convert EBCDIC.
** Because it is table-driven, it could be enhanced to handle that,
** but the world and SQLite have moved on from that anachronism.
**
** To build the extension:
** Set shell variable SQDIR=<your favorite SQLite checkout directory>
** *Nix: gcc -O2 -shared -I$SQDIR -fPIC -o base64.so base64.c
** OSX: gcc -O2 -dynamiclib -fPIC -I$SQDIR -o base64.dylib base64.c
** Win32: gcc -O2 -shared -I%SQDIR% -o base64.dll base64.c
** Win32: cl /Os -I%SQDIR% base64.c -link -dll -out:base64.dll
*/

#include <assert.h>

#include "sqlite3ext.h"

#ifndef deliberate_fall_through
/* Quiet some compilers about some of our intentional code. */
# if GCC_VERSION>=7000000
#  define deliberate_fall_through __attribute__((fallthrough));
# else
#  define deliberate_fall_through
# endif
#endif

SQLITE_EXTENSION_INIT1;

#define PC 0x80 /* pad character */
#define WS 0x81 /* whitespace */
#define ND 0x82 /* Not above or digit-value */
#define PAD_CHAR '='

#ifndef U8_TYPEDEF
typedef unsigned char u8;
#define U8_TYPEDEF
#endif

/* Decoding table, ASCII (7-bit) value to base 64 digit value or other */
static const u8 b64DigitValues[128] = {
  /*                             HT LF VT  FF CR       */
    ND,ND,ND,ND, ND,ND,ND,ND, ND,WS,WS,WS, WS,WS,ND,ND,
  /*                                                US */
    ND,ND,ND,ND, ND,ND,ND,ND, ND,ND,ND,ND, ND,ND,ND,ND,
  /*sp                                  +            / */
    WS,ND,ND,ND, ND,ND,ND,ND, ND,ND,ND,62, ND,ND,ND,63,
  /* 0  1            5            9            =       */
    52,53,54,55, 56,57,58,59, 60,61,ND,ND, ND,PC,ND,ND,
  /*    A                                            O */
    ND, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
  /* P                               Z                 */
    15,16,17,18, 19,20,21,22, 23,24,25,ND, ND,ND,ND,ND,
  /*    a                                            o */
    ND,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
  /* p                               z                 */
    41,42,43,44, 45,46,47,48, 49,50,51,ND, ND,ND,ND,ND
};

static const char b64Numerals[64+1]
= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define BX_DV_PROTO(c) \
  ((((u8)(c))<0x80)? (u8)(b64DigitValues[(u8)(c)]) : 0x80)
#define IS_BX_DIGIT(bdp) (((u8)(bdp))<0x80)
#define IS_BX_WS(bdp) ((bdp)==WS)
#define IS_BX_PAD(bdp) ((bdp)==PC)
#define BX_NUMERAL(dv) (b64Numerals[(u8)(dv)])
/* Width of base64 lines. Should be an integer multiple of 4. */
#define B64_DARK_MAX 72

/* Encode a byte buffer into base64 text with linefeeds appended to limit
** encoded group lengths to B64_DARK_MAX or to terminate the last group.
*/
static char* toBase64( u8 *pIn, int nbIn, char *pOut ){
  int nCol = 0;
  while( nbIn >= 3 ){
    /* Do the bit-shuffle, exploiting unsigned input to avoid masking. */
    pOut[0] = BX_NUMERAL(pIn[0]>>2);
    pOut[1] = BX_NUMERAL(((pIn[0]<<4)|(pIn[1]>>4))&0x3f);
    pOut[2] = BX_NUMERAL(((pIn[1]&0xf)<<2)|(pIn[2]>>6));
    pOut[3] = BX_NUMERAL(pIn[2]&0x3f);
    pOut += 4;
    nbIn -= 3;
    pIn += 3;
    if( (nCol += 4)>=B64_DARK_MAX || nbIn<=0 ){
      *pOut++ = '\n';
      nCol = 0;
    }
  }
  if( nbIn > 0 ){
    signed char nco = nbIn+1;
    int nbe;
    unsigned long qv = *pIn++;
    for( nbe=1; nbe<3; ++nbe ){
      qv <<= 8;
      if( nbe<nbIn ) qv |= *pIn++;
    }
    for( nbe=3; nbe>=0; --nbe ){
      char ce = (nbe<nco)? BX_NUMERAL((u8)(qv & 0x3f)) : PAD_CHAR;
      qv >>= 6;
      pOut[nbe] = ce;
    }
    pOut += 4;
    *pOut++ = '\n';
  }
  *pOut = 0;
  return pOut;
}

/* Skip over text which is not base64 numeral(s). */
static char * skipNonB64( char *s, int nc ){
  char c;
  while( nc-- > 0 && (c = *s) && !IS_BX_DIGIT(BX_DV_PROTO(c)) ) ++s;
  return s;
}

/* Decode base64 text into a byte buffer. */
static u8* fromBase64( char *pIn, int ncIn, u8 *pOut ){
  if( ncIn>0 && pIn[ncIn-1]=='\n' ) --ncIn;
  while( ncIn>0 && *pIn!=PAD_CHAR ){
    static signed char nboi[] = { 0, 0, 1, 2, 3 };
    char *pUse = skipNonB64(pIn, ncIn);
    unsigned long qv = 0L;
    int nti, nbo, nac;
    ncIn -= (pUse - pIn);
    pIn = pUse;
    nti = (ncIn>4)? 4 : ncIn;
    ncIn -= nti;
    nbo = nboi[nti];
    if( nbo==0 ) break;
    for( nac=0; nac<4; ++nac ){
      char c = (nac<nti)? *pIn++ : b64Numerals[0];
      u8 bdp = BX_DV_PROTO(c);
      switch( bdp ){
      case ND:
        /*  Treat dark non-digits as pad, but they terminate decode too. */
        ncIn = 0;
        deliberate_fall_through; /* FALLTHRU */
      case WS:
        /* Treat whitespace as pad and terminate this group.*/
        nti = nac;
        deliberate_fall_through; /* FALLTHRU */
      case PC:
        bdp = 0;
        --nbo;
        deliberate_fall_through; /* FALLTHRU */
      default: /* bdp is the digit value. */
        qv = qv<<6 | bdp;
        break;
      }
    }
    switch( nbo ){
    case 3:
      pOut[2] = (qv) & 0xff;
      deliberate_fall_through; /* FALLTHRU */
    case 2:
      pOut[1] = (qv>>8) & 0xff;
      deliberate_fall_through; /* FALLTHRU */
    case 1:
      pOut[0] = (qv>>16) & 0xff;
      break;
    }
    pOut += nbo;
  }
  return pOut;
}

/* This function does the work for the SQLite base64(x) UDF. */
static void base64(sqlite3_context *context, int na, sqlite3_value *av[]){
  sqlite3_int64 nb;
  sqlite3_int64 nv = sqlite3_value_bytes(av[0]);
  sqlite3_int64 nc;
  int nvMax = sqlite3_limit(sqlite3_context_db_handle(context),
                            SQLITE_LIMIT_LENGTH, -1);
  char *cBuf;
  u8 *bBuf;
  assert(na==1);
  switch( sqlite3_value_type(av[0]) ){
  case SQLITE_BLOB:
    nb = nv;
    nc = 4*((nv+2)/3); /* quads needed */
    nc += (nc+(B64_DARK_MAX-1))/B64_DARK_MAX + 1; /* LFs and a 0-terminator */
    if( nvMax < nc ){
      sqlite3_result_error(context, "blob expanded to base64 too big", -1);
      return;
    }
    bBuf = (u8*)sqlite3_value_blob(av[0]);
    if( !bBuf ){
      if( SQLITE_NOMEM==sqlite3_errcode(sqlite3_context_db_handle(context)) ){
        goto memFail;
      }
      sqlite3_result_text(context,"",-1,SQLITE_STATIC);
      break;
    }
    cBuf = sqlite3_malloc64(nc);
    if( !cBuf ) goto memFail;
    nc = (int)(toBase64(bBuf, nb, cBuf) - cBuf);
    sqlite3_result_text(context, cBuf, nc, sqlite3_free);
    break;
  case SQLITE_TEXT:
    nc = nv;
    nb = 3*((nv+3)/4); /* may overestimate due to LF and padding */
    if( nvMax < nb ){
      sqlite3_result_error(context, "blob from base64 may be too big", -1);
      return;
    }else if( nb<1 ){
      nb = 1;
    }
    cBuf = (char *)sqlite3_value_text(av[0]);
    if( !cBuf ){
      if( SQLITE_NOMEM==sqlite3_errcode(sqlite3_context_db_handle(context)) ){
        goto memFail;
      }
      sqlite3_result_zeroblob(context, 0);
      break;
    }
    bBuf = sqlite3_malloc64(nb);
    if( !bBuf ) goto memFail;
    nb = (int)(fromBase64(cBuf, nc, bBuf) - bBuf);
    sqlite3_result_blob(context, bBuf, nb, sqlite3_free);
    break;
  default:
    sqlite3_result_error(context, "base64 accepts only blob or text", -1);
    return;
  }
  return;
 memFail:
  sqlite3_result_error(context, "base64 OOM", -1);
}

/*
** Establish linkage to running SQLite library.
*/
#ifndef SQLITE_SHELL_EXTFUNCS
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_base64_init
#else
static int sqlite3_base64_init
#endif
(sqlite3 *db, char **pzErr, const sqlite3_api_routines *pApi){
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErr;
  return sqlite3_create_function
    (db, "base64", 1,
     SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS|SQLITE_DIRECTONLY|SQLITE_UTF8,
     0, base64, 0, 0);
}

/*
** Define some macros to allow this extension to be built into the shell
** conveniently, in conjunction with use of SQLITE_SHELL_EXTFUNCS. This
** allows shell.c, as distributed, to have this extension built in.
*/
#define BASE64_INIT(db) sqlite3_base64_init(db, 0, 0)
#define BASE64_EXPOSE(db, pzErr) /* Not needed, ..._init() does this. */
