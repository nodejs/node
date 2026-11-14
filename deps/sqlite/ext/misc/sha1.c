/*
** 2017-01-27
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This SQLite extension implements functions that compute SHA1 hashes.
** Two SQL functions are implemented:
**
**     sha1(X)
**     sha1_query(Y)
**
** The sha1(X) function computes the SHA1 hash of the input X, or NULL if
** X is NULL.
**
** The sha1_query(Y) function evalutes all queries in the SQL statements of Y
** and returns a hash of their results.
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>
#include <stdarg.h>

/******************************************************************************
** The Hash Engine
*/
/* Context for the SHA1 hash */
typedef struct SHA1Context SHA1Context;
struct SHA1Context {
  unsigned int state[5];
  unsigned int count[2];
  unsigned char buffer[64];
};

#define SHA_ROT(x,l,r) ((x) << (l) | (x) >> (r))
#define rol(x,k) SHA_ROT(x,k,32-(k))
#define ror(x,k) SHA_ROT(x,32-(k),k)

#define blk0le(i) (block[i] = (ror(block[i],8)&0xFF00FF00) \
    |(rol(block[i],8)&0x00FF00FF))
#define blk0be(i) block[i]
#define blk(i) (block[i&15] = rol(block[(i+13)&15]^block[(i+8)&15] \
    ^block[(i+2)&15]^block[i&15],1))

/*
 * (R0+R1), R2, R3, R4 are the different operations (rounds) used in SHA1
 *
 * Rl0() for little-endian and Rb0() for big-endian.  Endianness is
 * determined at run-time.
 */
#define Rl0(v,w,x,y,z,i) \
    z+=((w&(x^y))^y)+blk0le(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define Rb0(v,w,x,y,z,i) \
    z+=((w&(x^y))^y)+blk0be(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define R1(v,w,x,y,z,i) \
    z+=((w&(x^y))^y)+blk(i)+0x5A827999+rol(v,5);w=ror(w,2);
#define R2(v,w,x,y,z,i) \
    z+=(w^x^y)+blk(i)+0x6ED9EBA1+rol(v,5);w=ror(w,2);
#define R3(v,w,x,y,z,i) \
    z+=(((w|x)&y)|(w&x))+blk(i)+0x8F1BBCDC+rol(v,5);w=ror(w,2);
#define R4(v,w,x,y,z,i) \
    z+=(w^x^y)+blk(i)+0xCA62C1D6+rol(v,5);w=ror(w,2);

/*
 * Hash a single 512-bit block. This is the core of the algorithm.
 */
static void SHA1Transform(unsigned int state[5], const unsigned char buffer[64]){
  unsigned int qq[5]; /* a, b, c, d, e; */
  static int one = 1;
  unsigned int block[16];
  memcpy(block, buffer, 64);
  memcpy(qq,state,5*sizeof(unsigned int));

#define a qq[0]
#define b qq[1]
#define c qq[2]
#define d qq[3]
#define e qq[4]

  /* Copy p->state[] to working vars */
  /*
  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];
  e = state[4];
  */

  /* 4 rounds of 20 operations each. Loop unrolled. */
  if( 1 == *(unsigned char*)&one ){
    Rl0(a,b,c,d,e, 0); Rl0(e,a,b,c,d, 1); Rl0(d,e,a,b,c, 2); Rl0(c,d,e,a,b, 3);
    Rl0(b,c,d,e,a, 4); Rl0(a,b,c,d,e, 5); Rl0(e,a,b,c,d, 6); Rl0(d,e,a,b,c, 7);
    Rl0(c,d,e,a,b, 8); Rl0(b,c,d,e,a, 9); Rl0(a,b,c,d,e,10); Rl0(e,a,b,c,d,11);
    Rl0(d,e,a,b,c,12); Rl0(c,d,e,a,b,13); Rl0(b,c,d,e,a,14); Rl0(a,b,c,d,e,15);
  }else{
    Rb0(a,b,c,d,e, 0); Rb0(e,a,b,c,d, 1); Rb0(d,e,a,b,c, 2); Rb0(c,d,e,a,b, 3);
    Rb0(b,c,d,e,a, 4); Rb0(a,b,c,d,e, 5); Rb0(e,a,b,c,d, 6); Rb0(d,e,a,b,c, 7);
    Rb0(c,d,e,a,b, 8); Rb0(b,c,d,e,a, 9); Rb0(a,b,c,d,e,10); Rb0(e,a,b,c,d,11);
    Rb0(d,e,a,b,c,12); Rb0(c,d,e,a,b,13); Rb0(b,c,d,e,a,14); Rb0(a,b,c,d,e,15);
  }
  R1(e,a,b,c,d,16); R1(d,e,a,b,c,17); R1(c,d,e,a,b,18); R1(b,c,d,e,a,19);
  R2(a,b,c,d,e,20); R2(e,a,b,c,d,21); R2(d,e,a,b,c,22); R2(c,d,e,a,b,23);
  R2(b,c,d,e,a,24); R2(a,b,c,d,e,25); R2(e,a,b,c,d,26); R2(d,e,a,b,c,27);
  R2(c,d,e,a,b,28); R2(b,c,d,e,a,29); R2(a,b,c,d,e,30); R2(e,a,b,c,d,31);
  R2(d,e,a,b,c,32); R2(c,d,e,a,b,33); R2(b,c,d,e,a,34); R2(a,b,c,d,e,35);
  R2(e,a,b,c,d,36); R2(d,e,a,b,c,37); R2(c,d,e,a,b,38); R2(b,c,d,e,a,39);
  R3(a,b,c,d,e,40); R3(e,a,b,c,d,41); R3(d,e,a,b,c,42); R3(c,d,e,a,b,43);
  R3(b,c,d,e,a,44); R3(a,b,c,d,e,45); R3(e,a,b,c,d,46); R3(d,e,a,b,c,47);
  R3(c,d,e,a,b,48); R3(b,c,d,e,a,49); R3(a,b,c,d,e,50); R3(e,a,b,c,d,51);
  R3(d,e,a,b,c,52); R3(c,d,e,a,b,53); R3(b,c,d,e,a,54); R3(a,b,c,d,e,55);
  R3(e,a,b,c,d,56); R3(d,e,a,b,c,57); R3(c,d,e,a,b,58); R3(b,c,d,e,a,59);
  R4(a,b,c,d,e,60); R4(e,a,b,c,d,61); R4(d,e,a,b,c,62); R4(c,d,e,a,b,63);
  R4(b,c,d,e,a,64); R4(a,b,c,d,e,65); R4(e,a,b,c,d,66); R4(d,e,a,b,c,67);
  R4(c,d,e,a,b,68); R4(b,c,d,e,a,69); R4(a,b,c,d,e,70); R4(e,a,b,c,d,71);
  R4(d,e,a,b,c,72); R4(c,d,e,a,b,73); R4(b,c,d,e,a,74); R4(a,b,c,d,e,75);
  R4(e,a,b,c,d,76); R4(d,e,a,b,c,77); R4(c,d,e,a,b,78); R4(b,c,d,e,a,79);

  /* Add the working vars back into context.state[] */
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;

#undef a
#undef b
#undef c
#undef d
#undef e
}


/* Initialize a SHA1 context */
static void hash_init(SHA1Context *p){
  /* SHA1 initialization constants */
  p->state[0] = 0x67452301;
  p->state[1] = 0xEFCDAB89;
  p->state[2] = 0x98BADCFE;
  p->state[3] = 0x10325476;
  p->state[4] = 0xC3D2E1F0;
  p->count[0] = p->count[1] = 0;
}

/* Add new content to the SHA1 hash */
static void hash_step(
  SHA1Context *p,                 /* Add content to this context */
  const unsigned char *data,      /* Data to be added */
  unsigned int len                /* Number of bytes in data */
){
  unsigned int i, j;

  j = p->count[0];
  if( (p->count[0] += len << 3) < j ){
    p->count[1] += (len>>29)+1;
  }
  j = (j >> 3) & 63;
  if( (j + len) > 63 ){
    (void)memcpy(&p->buffer[j], data, (i = 64-j));
    SHA1Transform(p->state, p->buffer);
    for(; i + 63 < len; i += 64){
      SHA1Transform(p->state, &data[i]);
    }
    j = 0;
  }else{
    i = 0;
  }
  (void)memcpy(&p->buffer[j], &data[i], len - i);
}

/* Compute a string using sqlite3_vsnprintf() and hash it */
static void hash_step_vformat(
  SHA1Context *p,                 /* Add content to this context */
  const char *zFormat,
  ...
){
  va_list ap;
  int n;
  char zBuf[50];
  va_start(ap, zFormat);
  sqlite3_vsnprintf(sizeof(zBuf),zBuf,zFormat,ap);
  va_end(ap);
  n = (int)strlen(zBuf);
  hash_step(p, (unsigned char*)zBuf, n);
}


/* Add padding and compute the message digest.  Render the
** message digest as lower-case hexadecimal and put it into
** zOut[].  zOut[] must be at least 41 bytes long. */
static void hash_finish(
  SHA1Context *p,           /* The SHA1 context to finish and render */
  char *zOut,               /* Store hex or binary hash here */
  int bAsBinary             /* 1 for binary hash, 0 for hex hash */
){
  unsigned int i;
  unsigned char finalcount[8];
  unsigned char digest[20];
  static const char zEncode[] = "0123456789abcdef";

  for (i = 0; i < 8; i++){
    finalcount[i] = (unsigned char)((p->count[(i >= 4 ? 0 : 1)]
       >> ((3-(i & 3)) * 8) ) & 255); /* Endian independent */
  }
  hash_step(p, (const unsigned char *)"\200", 1);
  while ((p->count[0] & 504) != 448){
    hash_step(p, (const unsigned char *)"\0", 1);
  }
  hash_step(p, finalcount, 8);  /* Should cause a SHA1Transform() */
  for (i = 0; i < 20; i++){
    digest[i] = (unsigned char)((p->state[i>>2] >> ((3-(i & 3)) * 8) ) & 255);
  }
  if( bAsBinary ){
    memcpy(zOut, digest, 20);
  }else{
    for(i=0; i<20; i++){
      zOut[i*2] = zEncode[(digest[i]>>4)&0xf];
      zOut[i*2+1] = zEncode[digest[i] & 0xf];
    }
    zOut[i*2]= 0;
  }
}
/* End of the hashing logic
*****************************************************************************/

/*
** Two SQL functions:  sha1(X) and sha1b(X).
**
** sha1(X) returns a lower-case hexadecimal rendering of the SHA1 hash
** of the argument X.  If X is a BLOB, it is hashed as is.  For all other
** types of input, X is converted into a UTF-8 string and the string
** is hashed without the trailing 0x00 terminator.  The hash of a NULL
** value is NULL.
**
** sha1b(X) is the same except that it returns a 20-byte BLOB containing
** the binary hash instead of a hexadecimal string.
*/
static void sha1Func(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  SHA1Context cx;
  int eType = sqlite3_value_type(argv[0]);
  int nByte = sqlite3_value_bytes(argv[0]);
  const unsigned char *pData;
  char zOut[44];

  assert( argc==1 );
  if( eType==SQLITE_NULL ) return;
  hash_init(&cx);
  if( eType==SQLITE_BLOB ){
    pData = (const unsigned char*)sqlite3_value_blob(argv[0]);
  }else{
    pData = (const unsigned char*)sqlite3_value_text(argv[0]);
  }
  if( pData==0 ) return;
  hash_step(&cx, pData, nByte);
  if( sqlite3_user_data(context)!=0 ){
    /* sha1b() - binary result */
    hash_finish(&cx, zOut, 1);
    sqlite3_result_blob(context, zOut, 20, SQLITE_TRANSIENT);
  }else{
    /* sha1() - hexadecimal text result */
    hash_finish(&cx, zOut, 0);
    sqlite3_result_text(context, zOut, 40, SQLITE_TRANSIENT);
  }
}

/*
** Implementation of the sha1_query(SQL) function.
**
** This function compiles and runs the SQL statement(s) given in the
** argument. The results are hashed using SHA1 and that hash is returned.
**
** The original SQL text is included as part of the hash.
**
** The hash is not just a concatenation of the outputs.  Each query
** is delimited and each row and value within the query is delimited,
** with all values being marked with their datatypes.
*/
static void sha1QueryFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  sqlite3 *db = sqlite3_context_db_handle(context);
  const char *zSql = (const char*)sqlite3_value_text(argv[0]);
  sqlite3_stmt *pStmt = 0;
  int nCol;                   /* Number of columns in the result set */
  int i;                      /* Loop counter */
  int rc;
  int n;
  const char *z;
  SHA1Context cx;
  char zOut[44];

  assert( argc==1 );
  if( zSql==0 ) return;
  hash_init(&cx);
  while( zSql[0] ){
    rc = sqlite3_prepare_v2(db, zSql, -1, &pStmt, &zSql);
    if( rc ){
      char *zMsg = sqlite3_mprintf("error SQL statement [%s]: %s",
                                   zSql, sqlite3_errmsg(db));
      sqlite3_finalize(pStmt);
      sqlite3_result_error(context, zMsg, -1);
      sqlite3_free(zMsg);
      return;
    }
    if( !sqlite3_stmt_readonly(pStmt) ){
      char *zMsg = sqlite3_mprintf("non-query: [%s]", sqlite3_sql(pStmt));
      sqlite3_finalize(pStmt);
      sqlite3_result_error(context, zMsg, -1);
      sqlite3_free(zMsg);
      return;
    }
    nCol = sqlite3_column_count(pStmt);
    z = sqlite3_sql(pStmt);
    if( z==0 ) z = "";
    n = (int)strlen(z);
    hash_step_vformat(&cx,"S%d:",n);
    hash_step(&cx,(unsigned char*)z,n);

    /* Compute a hash over the result of the query */
    while( SQLITE_ROW==sqlite3_step(pStmt) ){
      hash_step(&cx,(const unsigned char*)"R",1);
      for(i=0; i<nCol; i++){
        switch( sqlite3_column_type(pStmt,i) ){
          case SQLITE_NULL: {
            hash_step(&cx, (const unsigned char*)"N",1);
            break;
          }
          case SQLITE_INTEGER: {
            sqlite3_uint64 u;
            int j;
            unsigned char x[9];
            sqlite3_int64 v = sqlite3_column_int64(pStmt,i);
            memcpy(&u, &v, 8);
            for(j=8; j>=1; j--){
              x[j] = u & 0xff;
              u >>= 8;
            }
            x[0] = 'I';
            hash_step(&cx, x, 9);
            break;
          }
          case SQLITE_FLOAT: {
            sqlite3_uint64 u;
            int j;
            unsigned char x[9];
            double r = sqlite3_column_double(pStmt,i);
            memcpy(&u, &r, 8);
            for(j=8; j>=1; j--){
              x[j] = u & 0xff;
              u >>= 8;
            }
            x[0] = 'F';
            hash_step(&cx,x,9);
            break;
          }
          case SQLITE_TEXT: {
            int n2 = sqlite3_column_bytes(pStmt, i);
            const unsigned char *z2 = sqlite3_column_text(pStmt, i);
            hash_step_vformat(&cx,"T%d:",n2);
            hash_step(&cx, z2, n2);
            break;
          }
          case SQLITE_BLOB: {
            int n2 = sqlite3_column_bytes(pStmt, i);
            const unsigned char *z2 = sqlite3_column_blob(pStmt, i);
            hash_step_vformat(&cx,"B%d:",n2);
            hash_step(&cx, z2, n2);
            break;
          }
        }
      }
    }
    sqlite3_finalize(pStmt);
  }
  hash_finish(&cx, zOut, 0);
  sqlite3_result_text(context, zOut, 40, SQLITE_TRANSIENT);
}


#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_sha_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  static int one = 1;
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg;  /* Unused parameter */
  rc = sqlite3_create_function(db, "sha1", 1, 
                       SQLITE_UTF8 | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
                                0, sha1Func, 0, 0);
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "sha1b", 1, 
                       SQLITE_UTF8 | SQLITE_INNOCUOUS | SQLITE_DETERMINISTIC,
                          (void*)&one, sha1Func, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_function(db, "sha1_query", 1, 
                                 SQLITE_UTF8|SQLITE_DIRECTONLY, 0,
                                 sha1QueryFunc, 0, 0);
  }
  return rc;
}
