/*
** 2013-04-17
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
** This SQLite extension implements functions for the exact display
** and input of IEEE754 Binary64 floating-point numbers.
**
**   ieee754(X)
**   ieee754(Y,Z)
**
** In the first form, the value X should be a floating-point number.
** The function will return a string of the form 'ieee754(Y,Z)' where
** Y and Z are integers such that X==Y*pow(2,Z).
**
** In the second form, Y and Z are integers which are the mantissa and
** base-2 exponent of a new floating point number.  The function returns
** a floating-point value equal to Y*pow(2,Z).
**
** Examples:
**
**     ieee754(2.0)             ->     'ieee754(2,0)'
**     ieee754(45.25)           ->     'ieee754(181,-2)'
**     ieee754(2, 0)            ->     2.0
**     ieee754(181, -2)         ->     45.25
**
** Two additional functions break apart the one-argument ieee754()
** result into separate integer values:
**
**     ieee754_mantissa(45.25)  ->     181
**     ieee754_exponent(45.25)  ->     -2
**
** These functions convert binary64 numbers into blobs and back again.
**
**     ieee754_from_blob(x'3ff0000000000000')  ->  1.0
**     ieee754_to_blob(1.0)                    ->  x'3ff0000000000000'
**
** In all single-argument functions, if the argument is an 8-byte blob
** then that blob is interpreted as a big-endian binary64 value.
**
**
** EXACT DECIMAL REPRESENTATION OF BINARY64 VALUES
** -----------------------------------------------
**
** This extension in combination with the separate 'decimal' extension
** can be used to compute the exact decimal representation of binary64
** values.  To begin, first compute a table of exponent values:
**
**    CREATE TABLE pow2(x INTEGER PRIMARY KEY, v TEXT);
**    WITH RECURSIVE c(x,v) AS (
**      VALUES(0,'1')
**      UNION ALL
**      SELECT x+1, decimal_mul(v,'2') FROM c WHERE x+1<=971
**    ) INSERT INTO pow2(x,v) SELECT x, v FROM c;
**    WITH RECURSIVE c(x,v) AS (
**      VALUES(-1,'0.5')
**      UNION ALL
**      SELECT x-1, decimal_mul(v,'0.5') FROM c WHERE x-1>=-1075
**    ) INSERT INTO pow2(x,v) SELECT x, v FROM c;
**
** Then, to compute the exact decimal representation of a floating
** point value (the value 47.49 is used in the example) do:
**
**    WITH c(n) AS (VALUES(47.49))
**          ---------------^^^^^---- Replace with whatever you want
**    SELECT decimal_mul(ieee754_mantissa(c.n),pow2.v)
**      FROM pow2, c WHERE pow2.x=ieee754_exponent(c.n);
**
** Here is a query to show various boundry values for the binary64
** number format:
**
**    WITH c(name,bin) AS (VALUES
**       ('minimum positive value',        x'0000000000000001'),
**       ('maximum subnormal value',       x'000fffffffffffff'),
**       ('minimum positive normal value', x'0010000000000000'),
**       ('maximum value',                 x'7fefffffffffffff'))
**    SELECT c.name, decimal_mul(ieee754_mantissa(c.bin),pow2.v)
**      FROM pow2, c WHERE pow2.x=ieee754_exponent(c.bin);
**
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>

/* Mark a function parameter as unused, to suppress nuisance compiler
** warnings. */
#ifndef UNUSED_PARAMETER
# define UNUSED_PARAMETER(X)  (void)(X)
#endif

/*
** Implementation of the ieee754() function
*/
static void ieee754func(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  if( argc==1 ){
    sqlite3_int64 m, a;
    double r;
    int e;
    int isNeg;
    char zResult[100];
    assert( sizeof(m)==sizeof(r) );
    if( sqlite3_value_type(argv[0])==SQLITE_BLOB
     && sqlite3_value_bytes(argv[0])==sizeof(r)
    ){
      const unsigned char *x = sqlite3_value_blob(argv[0]);
      unsigned int i;
      sqlite3_uint64 v = 0;
      for(i=0; i<sizeof(r); i++){
        v = (v<<8) | x[i];
      }
      memcpy(&r, &v, sizeof(r));
    }else{
      r = sqlite3_value_double(argv[0]);
    }
    if( r<0.0 ){
      isNeg = 1;
      r = -r;
    }else{
      isNeg = 0;
    }
    memcpy(&a,&r,sizeof(a));
    if( a==0 ){
      e = 0;
      m = 0;
    }else if( a==(sqlite3_int64)0x8000000000000000LL ){
      e = -1996;
      m = -1;
    }else{
      e = a>>52;
      m = a & ((((sqlite3_int64)1)<<52)-1);
      if( e==0 ){
        m <<= 1;
      }else{
        m |= ((sqlite3_int64)1)<<52;
      }
      while( e<1075 && m>0 && (m&1)==0 ){
        m >>= 1;
        e++;
      }
      if( isNeg ) m = -m;
    }
    switch( *(int*)sqlite3_user_data(context) ){
      case 0:
        sqlite3_snprintf(sizeof(zResult), zResult, "ieee754(%lld,%d)",
                         m, e-1075);
        sqlite3_result_text(context, zResult, -1, SQLITE_TRANSIENT);
        break;
      case 1:
        sqlite3_result_int64(context, m);
        break;
      case 2:
        sqlite3_result_int(context, e-1075);
        break;
    }
  }else{
    sqlite3_int64 m, e, a;
    double r;
    int isNeg = 0;
    m = sqlite3_value_int64(argv[0]);
    e = sqlite3_value_int64(argv[1]);

    /* Limit the range of e.  Ticket 22dea1cfdb9151e4 2021-03-02 */
    if( e>10000 ){
      e = 10000;
    }else if( e<-10000 ){
      e = -10000;
    }

    if( m<0 ){
      if( m<(-9223372036854775807LL) ) return;
      isNeg = 1;
      m = -m;
    }else if( m==0 && e>-1000 && e<1000 ){
      sqlite3_result_double(context, 0.0);
      return;
    }
    while( (m>>32)&0xffe00000 ){
      m >>= 1;
      e++;
    }
    while( m!=0 && ((m>>32)&0xfff00000)==0 ){
      m <<= 1;
      e--;
    }
    e += 1075;
    if( e<=0 ){
      /* Subnormal */
      if( 1-e >= 64 ){
        m = 0;
      }else{
        m >>= 1-e;
      }
      e = 0;
    }else if( e>0x7ff ){
      e = 0x7ff;
    }
    a = m & ((((sqlite3_int64)1)<<52)-1);
    a |= e<<52;
    if( isNeg ) a |= ((sqlite3_uint64)1)<<63;
    memcpy(&r, &a, sizeof(r));
    sqlite3_result_double(context, r);
  }
}

/*
** Functions to convert between blobs and floats.
*/
static void ieee754func_from_blob(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAMETER(argc);
  if( sqlite3_value_type(argv[0])==SQLITE_BLOB
   && sqlite3_value_bytes(argv[0])==sizeof(double)
  ){
    double r;
    const unsigned char *x = sqlite3_value_blob(argv[0]);
    unsigned int i;
    sqlite3_uint64 v = 0;
    for(i=0; i<sizeof(r); i++){
      v = (v<<8) | x[i];
    }
    memcpy(&r, &v, sizeof(r));
    sqlite3_result_double(context, r);
  }
}
static void ieee754func_to_blob(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAMETER(argc);
  if( sqlite3_value_type(argv[0])==SQLITE_FLOAT
   || sqlite3_value_type(argv[0])==SQLITE_INTEGER
  ){
    double r = sqlite3_value_double(argv[0]);
    sqlite3_uint64 v;
    unsigned char a[sizeof(r)];
    unsigned int i;
    memcpy(&v, &r, sizeof(r));
    for(i=1; i<=sizeof(r); i++){
      a[sizeof(r)-i] = v&0xff;
      v >>= 8;
    }
    sqlite3_result_blob(context, a, sizeof(r), SQLITE_TRANSIENT);
  }
}

/*
** Functions to convert between 64-bit integers and floats.
**
** The bit patterns are copied.  The numeric values are different.
*/
static void ieee754func_from_int(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAMETER(argc);
  if( sqlite3_value_type(argv[0])==SQLITE_INTEGER ){
    double r;
    sqlite3_int64 v = sqlite3_value_int64(argv[0]);
    memcpy(&r, &v, sizeof(r));
    sqlite3_result_double(context, r);
  }
}
static void ieee754func_to_int(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAMETER(argc);
  if( sqlite3_value_type(argv[0])==SQLITE_FLOAT ){
    double r = sqlite3_value_double(argv[0]);
    sqlite3_uint64 v;
    memcpy(&v, &r, sizeof(v));
    sqlite3_result_int64(context, v);
  }
}

/*
** SQL Function:   ieee754_inc(r,N)
**
** Move the floating point value r by N quantums and return the new
** values.
**
** Behind the scenes: this routine merely casts r into a 64-bit unsigned
** integer, adds N, then casts the value back into float.
**
** Example:  To find the smallest positive number:
**
**     SELECT ieee754_inc(0.0,+1);
*/
static void ieee754inc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  double r;
  sqlite3_int64 N;
  sqlite3_uint64 m1, m2;
  double r2;
  UNUSED_PARAMETER(argc);
  r = sqlite3_value_double(argv[0]);
  N = sqlite3_value_int64(argv[1]);
  memcpy(&m1, &r, 8);
  m2 = m1 + N;
  memcpy(&r2, &m2, 8);
  sqlite3_result_double(context, r2);
}


#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_ieee_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  static const struct {
    char *zFName;
    int nArg;
    int iAux;
    void (*xFunc)(sqlite3_context*,int,sqlite3_value**);
  } aFunc[] = {
    { "ieee754",           1,   0, ieee754func },
    { "ieee754",           2,   0, ieee754func },
    { "ieee754_mantissa",  1,   1, ieee754func },
    { "ieee754_exponent",  1,   2, ieee754func },
    { "ieee754_to_blob",   1,   0, ieee754func_to_blob },
    { "ieee754_from_blob", 1,   0, ieee754func_from_blob },
    { "ieee754_to_int",    1,   0, ieee754func_to_int },
    { "ieee754_from_int",  1,   0, ieee754func_from_int },
    { "ieee754_inc",       2,   0, ieee754inc  },
  };
  unsigned int i;
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg;  /* Unused parameter */
  for(i=0; i<sizeof(aFunc)/sizeof(aFunc[0]) && rc==SQLITE_OK; i++){
    rc = sqlite3_create_function(db, aFunc[i].zFName, aFunc[i].nArg,
                               SQLITE_UTF8|SQLITE_INNOCUOUS,
                               (void*)&aFunc[i].iAux,
                               aFunc[i].xFunc, 0, 0);
  }
  return rc;
}
