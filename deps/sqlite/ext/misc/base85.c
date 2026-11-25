/*
** 2022-11-16
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
** This is a utility for converting binary to base85 or vice-versa.
** It can be built as a standalone program or an SQLite3 extension.
**
** Much like base64 representations, base85 can be sent through a
** sane USASCII channel unmolested. It also plays nicely in CSV or
** written as TCL brace-enclosed literals or SQL string literals.
** It is not suited for unmodified use in XML-like documents.
**
** The encoding used resembles Ascii85, but was devised by the author
** (Larry Brasfield) before Mozilla, Adobe, ZMODEM or other Ascii85
** variant sources existed, in the 1984 timeframe on a VAX mainframe.
** Further, this is an independent implementation of a base85 system.
** Hence, the author has rightfully put this into the public domain.
**
** Base85 numerals are taken from the set of 7-bit USASCII codes,
** excluding control characters and Space ! " ' ( ) { | } ~ Del
** in code order representing digit values 0 to 84 (base 10.)
**
** Groups of 4 bytes, interpreted as big-endian 32-bit values,
** are represented as 5-digit base85 numbers with MS to LS digit
** order. Groups of 1-3 bytes are represented with 2-4 digits,
** still big-endian but 8-24 bit values. (Using big-endian yields
** the simplest transition to byte groups smaller than 4 bytes.
** These byte groups can also be considered base-256 numbers.)
** Groups of 0 bytes are represented with 0 digits and vice-versa.
** No pad characters are used; Encoded base85 numeral sequence
** (aka "group") length maps 1-to-1 to the decoded binary length.
**
** Any character not in the base85 numeral set delimits groups.
** When base85 is streamed or stored in containers of indefinite
** size, newline is used to separate it into sub-sequences of no
** more than 80 digits so that fgets() can be used to read it.
**
** Length limitations are not imposed except that the runtime
** SQLite string or blob length limits are respected. Otherwise,
** any length binary sequence can be represented and recovered.
** Base85 sequences can be concatenated by separating them with
** a non-base85 character; the conversion to binary will then
** be the concatenation of the represented binary sequences.

** The standalone program either converts base85 on stdin to create
** a binary file or converts a binary file to base85 on stdout.
** Read or make it blurt its help for invocation details.
**
** The SQLite3 extension creates a function, base85(x), which will
** either convert text base85 to a blob or a blob to text base85
** and return the result (or throw an error for other types.)
** Unless built with OMIT_BASE85_CHECKER defined, it also creates a
** function, is_base85(t), which returns 1 iff the text t contains
** nothing other than base85 numerals and whitespace, or 0 otherwise.
**
** To build the extension:
** Set shell variable SQDIR=<your favorite SQLite checkout directory>
** and variable OPTS to -DOMIT_BASE85_CHECKER if is_base85() unwanted.
** *Nix: gcc -O2 -shared -I$SQDIR $OPTS -fPIC -o base85.so base85.c
** OSX: gcc -O2 -dynamiclib -fPIC -I$SQDIR $OPTS -o base85.dylib base85.c
** Win32: gcc -O2 -shared -I%SQDIR% %OPTS% -o base85.dll base85.c
** Win32: cl /Os -I%SQDIR% %OPTS% base85.c -link -dll -out:base85.dll
**
** To build the standalone program, define PP symbol BASE85_STANDALONE. Eg.
** *Nix or OSX: gcc -O2 -DBASE85_STANDALONE base85.c -o base85
** Win32: gcc -O2 -DBASE85_STANDALONE -o base85.exe base85.c
** Win32: cl /Os /MD -DBASE85_STANDALONE base85.c
*/

#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <assert.h>
#ifndef OMIT_BASE85_CHECKER
# include <ctype.h>
#endif

#ifndef BASE85_STANDALONE

# include "sqlite3ext.h"

SQLITE_EXTENSION_INIT1;

#else

# ifdef _WIN32
#  include <io.h>
#  include <fcntl.h>
# else
#  define setmode(fd,m)
# endif

static char *zHelp =
  "Usage: base85 <dirFlag> <binFile>\n"
  " <dirFlag> is either -r to read or -w to write <binFile>,\n"
  "   content to be converted to/from base85 on stdout/stdin.\n"
  " <binFile> names a binary file to be rendered or created.\n"
  "   Or, the name '-' refers to the stdin or stdout stream.\n"
  ;

static void sayHelp(){
  printf("%s", zHelp);
}
#endif

#ifndef U8_TYPEDEF
typedef unsigned char u8;
#define U8_TYPEDEF
#endif

/* Classify c according to interval within USASCII set w.r.t. base85
 * Values of 1 and 3 are base85 numerals. Values of 0, 2, or 4 are not.
 */
#define B85_CLASS( c ) (((c)>='#')+((c)>'&')+((c)>='*')+((c)>'z'))

/* Provide digitValue to b85Numeral offset as a function of above class. */
static u8 b85_cOffset[] = { 0, '#', 0, '*'-4, 0 };
#define B85_DNOS( c ) b85_cOffset[B85_CLASS(c)]

/* Say whether c is a base85 numeral. */
#define IS_B85( c ) (B85_CLASS(c) & 1)

#if 0 /* Not used, */
static u8 base85DigitValue( char c ){
  u8 dv = (u8)(c - '#');
  if( dv>87 ) return 0xff;
  return (dv > 3)? dv-3 : dv;
}
#endif

/* Width of base64 lines. Should be an integer multiple of 5. */
#define B85_DARK_MAX 80


static char * skipNonB85( char *s, int nc ){
  char c;
  while( nc-- > 0 && (c = *s) && !IS_B85(c) ) ++s;
  return s;
}

/* Convert small integer, known to be in 0..84 inclusive, to base85 numeral.
 * Do not use the macro form with argument expression having a side-effect.*/
#if 0
static char base85Numeral( u8 b ){
  return (b < 4)? (char)(b + '#') : (char)(b - 4 + '*');
}
#else
# define base85Numeral( dn )\
  ((char)(((dn) < 4)? (char)((dn) + '#') : (char)((dn) - 4 + '*')))
#endif

static char *putcs(char *pc, char *s){
  char c;
  while( (c = *s++)!=0 ) *pc++ = c;
  return pc;
}

/* Encode a byte buffer into base85 text. If pSep!=0, it's a C string
** to be appended to encoded groups to limit their length to B85_DARK_MAX
** or to terminate the last group (to aid concatenation.)
*/
static char* toBase85( u8 *pIn, int nbIn, char *pOut, char *pSep ){
  int nCol = 0;
  while( nbIn >= 4 ){
    int nco = 5;
    unsigned long qbv = (((unsigned long)pIn[0])<<24) |
                        (pIn[1]<<16) | (pIn[2]<<8) | pIn[3];
    while( nco > 0 ){
      unsigned nqv = (unsigned)(qbv/85UL);
      unsigned char dv = qbv - 85UL*nqv;
      qbv = nqv;
      pOut[--nco] = base85Numeral(dv);
    }
    nbIn -= 4;
    pIn += 4;
    pOut += 5;
    if( pSep && (nCol += 5)>=B85_DARK_MAX ){
      pOut = putcs(pOut, pSep);
      nCol = 0;
    }
  }
  if( nbIn > 0 ){
    int nco = nbIn + 1;
    unsigned long qv = *pIn++;
    int nbe = 1;
    while( nbe++ < nbIn ){
      qv = (qv<<8) | *pIn++;
    }
    nCol += nco;
    while( nco > 0 ){
      u8 dv = (u8)(qv % 85);
      qv /= 85;
      pOut[--nco] = base85Numeral(dv);
    }
    pOut += (nbIn+1);
  }
  if( pSep && nCol>0 ) pOut = putcs(pOut, pSep);
  *pOut = 0;
  return pOut;
}

/* Decode base85 text into a byte buffer. */
static u8* fromBase85( char *pIn, int ncIn, u8 *pOut ){
  if( ncIn>0 && pIn[ncIn-1]=='\n' ) --ncIn;
  while( ncIn>0 ){
    static signed char nboi[] = { 0, 0, 1, 2, 3, 4 };
    char *pUse = skipNonB85(pIn, ncIn);
    unsigned long qv = 0L;
    int nti, nbo;
    ncIn -= (pUse - pIn);
    pIn = pUse;
    nti = (ncIn>5)? 5 : ncIn;
    nbo = nboi[nti];
    if( nbo==0 ) break;
    while( nti>0 ){
      char c = *pIn++;
      u8 cdo = B85_DNOS(c);
      --ncIn;
      if( cdo==0 ) break;
      qv = 85 * qv + (c - cdo);
      --nti;
    }
    nbo -= nti; /* Adjust for early (non-digit) end of group. */
    switch( nbo ){
    case 4:
      *pOut++ = (qv >> 24)&0xff;
      /* FALLTHRU */
    case 3:
      *pOut++ = (qv >> 16)&0xff;
      /* FALLTHRU */
    case 2:
      *pOut++ = (qv >> 8)&0xff;
      /* FALLTHRU */
    case 1:
      *pOut++ = qv&0xff;
      /* FALLTHRU */
    case 0:
      break;
    }
  }
  return pOut;
}

#ifndef OMIT_BASE85_CHECKER
/* Say whether input char sequence is all (base85 and/or whitespace).*/
static int allBase85( char *p, int len ){
  char c;
  while( len-- > 0 && (c = *p++) != 0 ){
    if( !IS_B85(c) && !isspace(c) ) return 0;
  }
  return 1;
}
#endif

#ifndef BASE85_STANDALONE

#ifndef OMIT_BASE85_CHECKER
/* This function does the work for the SQLite is_base85(t) UDF. */
static void is_base85(sqlite3_context *context, int na, sqlite3_value *av[]){
  assert(na==1);
  switch( sqlite3_value_type(av[0]) ){
  case SQLITE_TEXT:
    {
      int rv = allBase85( (char *)sqlite3_value_text(av[0]),
                          sqlite3_value_bytes(av[0]) );
      sqlite3_result_int(context, rv);
    }
    break;
  case SQLITE_NULL:
    sqlite3_result_null(context);
    break;
  default:
    sqlite3_result_error(context, "is_base85 accepts only text or NULL", -1);
    return;
  }
}
#endif

/* This function does the work for the SQLite base85(x) UDF. */
static void base85(sqlite3_context *context, int na, sqlite3_value *av[]){
  sqlite3_int64 nb, nc, nv = sqlite3_value_bytes(av[0]);
  int nvMax = sqlite3_limit(sqlite3_context_db_handle(context),
                            SQLITE_LIMIT_LENGTH, -1);
  char *cBuf;
  u8 *bBuf;
  assert(na==1);
  switch( sqlite3_value_type(av[0]) ){
  case SQLITE_BLOB:
    nb = nv;
    /*    ulongs    tail   newlines  tailenc+nul*/
    nc = 5*(nv/4) + nv%4 + nv/64+1 + 2;
    if( nvMax < nc ){
      sqlite3_result_error(context, "blob expanded to base85 too big", -1);
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
    nc = (int)(toBase85(bBuf, nb, cBuf, "\n") - cBuf);
    sqlite3_result_text(context, cBuf, nc, sqlite3_free);
    break;
  case SQLITE_TEXT:
    nc = nv;
    nb = 4*(nv/5) + nv%5; /* may overestimate */
    if( nvMax < nb ){
      sqlite3_result_error(context, "blob from base85 may be too big", -1);
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
    nb = (int)(fromBase85(cBuf, nc, bBuf) - bBuf);
    sqlite3_result_blob(context, bBuf, nb, sqlite3_free);
    break;
  default:
    sqlite3_result_error(context, "base85 accepts only blob or text.", -1);
    return;
  }
  return;
 memFail:
  sqlite3_result_error(context, "base85 OOM", -1);
}

/*
** Establish linkage to running SQLite library.
*/
#ifndef SQLITE_SHELL_EXTFUNCS
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_base85_init
#else
static int sqlite3_base85_init
#endif
(sqlite3 *db, char **pzErr, const sqlite3_api_routines *pApi){
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErr;
#ifndef OMIT_BASE85_CHECKER
  {
    int rc = sqlite3_create_function
      (db, "is_base85", 1,
       SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS|SQLITE_UTF8,
       0, is_base85, 0, 0);
    if( rc!=SQLITE_OK ) return rc;
  }
#endif
  return sqlite3_create_function
    (db, "base85", 1,
     SQLITE_DETERMINISTIC|SQLITE_INNOCUOUS|SQLITE_DIRECTONLY|SQLITE_UTF8,
     0, base85, 0, 0);
}

/*
** Define some macros to allow this extension to be built into the shell
** conveniently, in conjunction with use of SQLITE_SHELL_EXTFUNCS. This
** allows shell.c, as distributed, to have this extension built in.
*/
# define BASE85_INIT(db) sqlite3_base85_init(db, 0, 0)
# define BASE85_EXPOSE(db, pzErr) /* Not needed, ..._init() does this. */

#else /* standalone program */

int main(int na, char *av[]){
  int cin;
  int rc = 0;
  u8 bBuf[4*(B85_DARK_MAX/5)];
  char cBuf[5*(sizeof(bBuf)/4)+2];
  size_t nio;
# ifndef OMIT_BASE85_CHECKER
  int b85Clean = 1;
# endif
  char rw;
  FILE *fb = 0, *foc = 0;
  char fmode[3] = "xb";
  if( na < 3 || av[1][0]!='-' || (rw = av[1][1])==0 || (rw!='r' && rw!='w') ){
    sayHelp();
    return 0;
  }
  fmode[0] = rw;
  if( av[2][0]=='-' && av[2][1]==0 ){
    switch( rw ){
    case 'r':
      fb = stdin;
      setmode(fileno(stdin), O_BINARY);
      break;
    case 'w':
      fb = stdout;
      setmode(fileno(stdout), O_BINARY);
      break;
    }
  }else{
    fb = fopen(av[2], fmode);
    foc = fb;
  }
  if( !fb ){
    fprintf(stderr, "Cannot open %s for %c\n", av[2], rw);
    rc = 1;
  }else{
    switch( rw ){
    case 'r':
      while( (nio = fread( bBuf, 1, sizeof(bBuf), fb))>0 ){
        toBase85( bBuf, (int)nio, cBuf, 0 );
        fprintf(stdout, "%s\n", cBuf);
      }
      break;
    case 'w':
      while( 0 != fgets(cBuf, sizeof(cBuf), stdin) ){
        int nc = strlen(cBuf);
        size_t nbo = fromBase85( cBuf, nc, bBuf ) - bBuf;
        if( 1 != fwrite(bBuf, nbo, 1, fb) ) rc = 1;
#ifndef OMIT_BASE85_CHECKER
        b85Clean &= allBase85( cBuf, nc );
#endif
      }
      break;
    default:
      sayHelp();
      rc = 1;
    }
    if( foc ) fclose(foc);
  }
# ifndef OMIT_BASE85_CHECKER
  if( !b85Clean ){
    fprintf(stderr, "Base85 input had non-base85 dark or control content.\n");
  }
# endif
  return rc;
}

#endif
