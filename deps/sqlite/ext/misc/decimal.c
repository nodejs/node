/*
** 2020-06-22
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
** Routines to implement arbitrary-precision decimal math.
**
** The focus here is on simplicity and correctness, not performance.
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Mark a function parameter as unused, to suppress nuisance compiler
** warnings. */
#ifndef UNUSED_PARAMETER
# define UNUSED_PARAMETER(X)  (void)(X)
#endif

#ifndef IsSpace
#define IsSpace(X)  isspace((unsigned char)X)
#endif

#ifndef SQLITE_DECIMAL_MAX_DIGIT
# define SQLITE_DECIMAL_MAX_DIGIT 10000000
#endif

/* A decimal object */
typedef struct Decimal Decimal;
struct Decimal {
  char sign;        /* 0 for positive, 1 for negative */
  char oom;         /* True if an OOM is encountered */
  char isNull;      /* True if holds a NULL rather than a number */
  char isInit;      /* True upon initialization */
  int nDigit;       /* Total number of digits */
  int nFrac;        /* Number of digits to the right of the decimal point */
  signed char *a;   /* Array of digits.  Most significant first. */
};

/*
** Release memory held by a Decimal, but do not free the object itself.
*/
static void decimal_clear(Decimal *p){
  sqlite3_free(p->a);
}

/*
** Destroy a Decimal object
*/
static void decimal_free(Decimal *p){
  if( p ){
    decimal_clear(p);
    sqlite3_free(p);
  }
}

/*
** Allocate a new Decimal object initialized to the text in zIn[].
** Return NULL if any kind of error occurs.
*/
static Decimal *decimalNewFromText(const char *zIn, int n){
  Decimal *p = 0;
  int i;
  int iExp = 0;

  if( zIn==0 ) goto new_from_text_failed;
  p = sqlite3_malloc64( sizeof(*p) );
  if( p==0 ) goto new_from_text_failed;
  p->sign = 0;
  p->oom = 0;
  p->isInit = 1;
  p->isNull = 0;
  p->nDigit = 0;
  p->nFrac = 0;
  p->a = sqlite3_malloc64( n+1 );
  if( p->a==0 ) goto new_from_text_failed;
  for(i=0; IsSpace(zIn[i]); i++){}
  if( zIn[i]=='-' ){
    p->sign = 1;
    i++;
  }else if( zIn[i]=='+' ){
    i++;
  }
  while( i<n && zIn[i]=='0' ) i++;
  while( i<n ){
    char c = zIn[i];
    if( c>='0' && c<='9' ){
      p->a[p->nDigit++] = c - '0';
    }else if( c=='.' ){
      p->nFrac = p->nDigit + 1;
    }else if( c=='e' || c=='E' ){
      int j = i+1;
      int neg = 0;
      if( j>=n ) break;
      if( zIn[j]=='-' ){
        neg = 1;
        j++;
      }else if( zIn[j]=='+' ){
        j++;
      }
      while( j<n && iExp<1000000 ){
        if( zIn[j]>='0' && zIn[j]<='9' ){
          iExp = iExp*10 + zIn[j] - '0';
        }
        j++;
      }
      if( neg ) iExp = -iExp;
      break;
    }
    i++;
  }
  if( p->nFrac ){
    p->nFrac = p->nDigit - (p->nFrac - 1);
  }
  if( iExp>0 ){
    if( p->nFrac>0 ){
      if( iExp<=p->nFrac ){
        p->nFrac -= iExp;
        iExp = 0;
      }else{
        iExp -= p->nFrac;
        p->nFrac = 0;
      }
    }
    if( iExp>0 ){   
      signed char *a = sqlite3_realloc64(p->a, (sqlite3_int64)p->nDigit
                                     + (sqlite3_int64)iExp + 1 );
      if( a==0 ) goto new_from_text_failed;
      p->a = a;
      memset(p->a+p->nDigit, 0, iExp);
      p->nDigit += iExp;
    }
  }else if( iExp<0 ){
    int nExtra;
    iExp = -iExp;
    nExtra = p->nDigit - p->nFrac - 1;
    if( nExtra ){
      if( nExtra>=iExp ){
        p->nFrac += iExp;
        iExp  = 0;
      }else{
        iExp -= nExtra;
        p->nFrac = p->nDigit - 1;
      }
    }
    if( iExp>0 ){
      signed char *a = sqlite3_realloc64(p->a, (sqlite3_int64)p->nDigit
                                     + (sqlite3_int64)iExp + 1 );
      if( a==0 ) goto new_from_text_failed;
      p->a = a;
      memmove(p->a+iExp, p->a, p->nDigit);
      memset(p->a, 0, iExp);
      p->nDigit += iExp;
      p->nFrac += iExp;
    }
  }
  if( p->sign ){
    for(i=0; i<p->nDigit && p->a[i]==0; i++){}
    if( i>=p->nDigit ) p->sign = 0;
  }
  if( p->nDigit>SQLITE_DECIMAL_MAX_DIGIT ) goto new_from_text_failed;
  return p;

new_from_text_failed:
  if( p ){
    if( p->a ) sqlite3_free(p->a);
    sqlite3_free(p);
  }
  return 0;
}

/* Forward reference */
static Decimal *decimalFromDouble(double);

/*
** Allocate a new Decimal object from an sqlite3_value.  Return a pointer
** to the new object, or NULL if there is an error.  If the pCtx argument
** is not NULL, then errors are reported on it as well.
**
** If the pIn argument is SQLITE_TEXT or SQLITE_INTEGER, it is converted
** directly into a Decimal.  For SQLITE_FLOAT or for SQLITE_BLOB of length
** 8 bytes, the resulting double value is expanded into its decimal equivalent.
** If pIn is NULL or if it is a BLOB that is not exactly 8 bytes in length,
** then NULL is returned.
*/
static Decimal *decimal_new(
  sqlite3_context *pCtx,       /* Report error here, if not null */
  sqlite3_value *pIn,          /* Construct the decimal object from this */
  int bTextOnly                /* Always interpret pIn as text if true */
){
  Decimal *p = 0;
  int eType = sqlite3_value_type(pIn);
  if( bTextOnly && (eType==SQLITE_FLOAT || eType==SQLITE_BLOB) ){
    eType = SQLITE_TEXT;
  }
  switch( eType ){
    case SQLITE_TEXT:
    case SQLITE_INTEGER: {
      const char *zIn = (const char*)sqlite3_value_text(pIn);
      int n = sqlite3_value_bytes(pIn);
      p = decimalNewFromText(zIn, n);
      if( p==0 ) goto new_failed;
      break;
    }

    case SQLITE_FLOAT: {
      p = decimalFromDouble(sqlite3_value_double(pIn));
      break;
    }

    case SQLITE_BLOB: {
      const unsigned char *x;
      unsigned int i;
      sqlite3_uint64 v = 0;
      double r;

      if( sqlite3_value_bytes(pIn)!=sizeof(r) ) break;
      x = sqlite3_value_blob(pIn);
      for(i=0; i<sizeof(r); i++){
        v = (v<<8) | x[i];
      }
      memcpy(&r, &v, sizeof(r));
      p = decimalFromDouble(r);
      break;
    }

    case SQLITE_NULL: {
      break;
    }
  }
  return p;

new_failed:
  if( pCtx ) sqlite3_result_error_nomem(pCtx);
  sqlite3_free(p);
  return 0;
}

/*
** Make the given Decimal the result.
*/
static void decimal_result(sqlite3_context *pCtx, Decimal *p){
  char *z;
  int i, j;
  int n;
  if( p==0 || p->oom ){
    sqlite3_result_error_nomem(pCtx);
    return;
  }
  if( p->isNull ){
    sqlite3_result_null(pCtx);
    return;
  }
  z = sqlite3_malloc64( (sqlite3_int64)p->nDigit+4 );
  if( z==0 ){
    sqlite3_result_error_nomem(pCtx);
    return;
  }
  i = 0;
  if( p->nDigit==0 || (p->nDigit==1 && p->a[0]==0) ){
    p->sign = 0;
  }
  if( p->sign ){
    z[0] = '-';
    i = 1;
  }
  n = p->nDigit - p->nFrac;
  if( n<=0 ){
    z[i++] = '0';
  }
  j = 0;
  while( n>1 && p->a[j]==0 ){
    j++;
    n--;
  }
  while( n>0  ){
    z[i++] = p->a[j] + '0';
    j++;
    n--;
  }
  if( p->nFrac ){
    z[i++] = '.';
    do{
      z[i++] = p->a[j] + '0';
      j++;
    }while( j<p->nDigit );
  }
  z[i] = 0;
  sqlite3_result_text(pCtx, z, i, sqlite3_free);
}

/*
** Round a decimal value to N significant digits.  N must be positive.
*/
static void decimal_round(Decimal *p, int N){
  int i;
  int nZero;
  if( N<1 ) return;
  if( p==0 ) return;
  if( p->nDigit<=N ) return;
  for(nZero=0; nZero<p->nDigit && p->a[nZero]==0; nZero++){}
  N += nZero;
  if( p->nDigit<=N ) return;
  if( p->a[N]>4 ){
    p->a[N-1]++;
    for(i=N-1; i>0 && p->a[i]>9; i--){
      p->a[i] = 0;
      p->a[i-1]++;
    }
    if( p->a[0]>9 ){
      p->a[0] = 1;
      p->nFrac--;
    }
  }
  memset(&p->a[N], 0, p->nDigit - N);
}

/*
** Make the given Decimal the result in an format similar to  '%+#e'.
** In other words, show exponential notation with leading and trailing
** zeros omitted.
*/
static void decimal_result_sci(sqlite3_context *pCtx, Decimal *p, int N){
  char *z;       /* The output buffer */
  int i;         /* Loop counter */
  int nZero;     /* Number of leading zeros */
  int nDigit;    /* Number of digits not counting trailing zeros */
  int nFrac;     /* Digits to the right of the decimal point */
  int exp;       /* Exponent value */
  signed char zero;     /* Zero value */
  signed char *a;       /* Array of digits */

  if( p==0 || p->oom ){
    sqlite3_result_error_nomem(pCtx);
    return;
  }
  if( p->isNull ){
    sqlite3_result_null(pCtx);
    return;
  }
  if( N<1 ) N = 0;
  for(nDigit=p->nDigit; nDigit>N && p->a[nDigit-1]==0; nDigit--){}
  for(nZero=0; nZero<nDigit && p->a[nZero]==0; nZero++){}
  nFrac = p->nFrac + (nDigit - p->nDigit);
  nDigit -= nZero;
  z = sqlite3_malloc64( (sqlite3_int64)nDigit+20 );
  if( z==0 ){
    sqlite3_result_error_nomem(pCtx);
    return;
  }
  if( nDigit==0 ){
    zero = 0;
    a = &zero;
    nDigit = 1;
    nFrac = 0;
  }else{
    a = &p->a[nZero];
  }
  if( p->sign && nDigit>0 ){
    z[0] = '-';
  }else{
    z[0] = '+';
  }
  z[1] = a[0]+'0';
  z[2] = '.';
  if( nDigit==1 ){
    z[3] = '0';
    i = 4;
  }else{
    for(i=1; i<nDigit; i++){
      z[2+i] = a[i]+'0';
    }
    i = nDigit+2;
  }
  exp = nDigit - nFrac - 1;
  sqlite3_snprintf(nDigit+20-i, &z[i], "e%+03d", exp);
  sqlite3_result_text(pCtx, z, -1, sqlite3_free);
}

/*
** Compare to Decimal objects.  Return negative, 0, or positive if the
** first object is less than, equal to, or greater than the second.
**
** Preconditions for this routine:
**
**    pA!=0
**    pA->isNull==0
**    pB!=0
**    pB->isNull==0
*/
static int decimal_cmp(Decimal *pA, Decimal *pB){
  int nASig, nBSig, rc, n;
  while( pA->nFrac>0 && pA->a[pA->nDigit-1]==0 ){
    pA->nDigit--;
    pA->nFrac--;
  }
  while( pB->nFrac>0 && pB->a[pB->nDigit-1]==0 ){
    pB->nDigit--;
    pB->nFrac--;
  }
  if( pA->sign!=pB->sign ){
    return pA->sign ? -1 : +1;
  }
  if( pA->sign ){
    Decimal *pTemp = pA;
    pA = pB;
    pB = pTemp;
  }
  nASig = pA->nDigit - pA->nFrac;
  nBSig = pB->nDigit - pB->nFrac;
  if( nASig!=nBSig ){
    return nASig - nBSig;
  }
  n = pA->nDigit;
  if( n>pB->nDigit ) n = pB->nDigit;
  rc = memcmp(pA->a, pB->a, n);
  if( rc==0 ){
    rc = pA->nDigit - pB->nDigit;
  }
  return rc;
}

/*
** SQL Function:   decimal_cmp(X, Y)
**
** Return negative, zero, or positive if X is less then, equal to, or
** greater than Y.
*/
static void decimalCmpFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *pA = 0, *pB = 0;
  int rc;

  UNUSED_PARAMETER(argc);
  pA = decimal_new(context, argv[0], 1);
  if( pA==0 || pA->isNull ) goto cmp_done;
  pB = decimal_new(context, argv[1], 1);
  if( pB==0 || pB->isNull ) goto cmp_done;
  rc = decimal_cmp(pA, pB);
  if( rc<0 ) rc = -1;
  else if( rc>0 ) rc = +1;
  sqlite3_result_int(context, rc);
cmp_done:
  decimal_free(pA);
  decimal_free(pB);
}

/*
** Expand the Decimal so that it has a least nDigit digits and nFrac
** digits to the right of the decimal point.
*/
static void decimal_expand(Decimal *p, int nDigit, int nFrac){
  int nAddSig;
  int nAddFrac;
  signed char *a;
  if( p==0 ) return;
  nAddFrac = nFrac - p->nFrac;
  nAddSig = (nDigit - p->nDigit) - nAddFrac;
  if( nAddFrac==0 && nAddSig==0 ) return;
  if( nDigit+1>SQLITE_DECIMAL_MAX_DIGIT ){ p->oom = 1; return; }
  a = sqlite3_realloc64(p->a, nDigit+1);
  if( a==0 ){
    p->oom = 1;
    return;
  }
  p->a = a;
  if( nAddSig ){
    memmove(p->a+nAddSig, p->a, p->nDigit);
    memset(p->a, 0, nAddSig);
    p->nDigit += nAddSig;
  }
  if( nAddFrac ){
    memset(p->a+p->nDigit, 0, nAddFrac);
    p->nDigit += nAddFrac;
    p->nFrac += nAddFrac;
  }
}

/*
** Add the value pB into pA.   A := A + B.
**
** Both pA and pB might become denormalized by this routine.
*/
static void decimal_add(Decimal *pA, Decimal *pB){
  int nSig, nFrac, nDigit;
  int i, rc;
  if( pA==0 ){
    return;
  }
  if( pA->oom || pB==0 || pB->oom ){
    pA->oom = 1;
    return;
  }
  if( pA->isNull || pB->isNull ){
    pA->isNull = 1;
    return;
  }
  nSig = pA->nDigit - pA->nFrac;
  if( nSig && pA->a[0]==0 ) nSig--;
  if( nSig<pB->nDigit-pB->nFrac ){
    nSig = pB->nDigit - pB->nFrac;
  }
  nFrac = pA->nFrac;
  if( nFrac<pB->nFrac ) nFrac = pB->nFrac;
  nDigit = nSig + nFrac + 1;
  decimal_expand(pA, nDigit, nFrac);
  decimal_expand(pB, nDigit, nFrac);
  if( pA->oom || pB->oom ){
    pA->oom = 1;
  }else{
    if( pA->sign==pB->sign ){
      int carry = 0;
      for(i=nDigit-1; i>=0; i--){
        int x = pA->a[i] + pB->a[i] + carry;
        if( x>=10 ){
          carry = 1;
          pA->a[i] = x - 10;
        }else{
          carry = 0;
          pA->a[i] = x;
        }
      }
    }else{
      signed char *aA, *aB;
      int borrow = 0;
      rc = memcmp(pA->a, pB->a, nDigit);
      if( rc<0 ){
        aA = pB->a;
        aB = pA->a;
        pA->sign = !pA->sign;
      }else{
        aA = pA->a;
        aB = pB->a;
      }
      for(i=nDigit-1; i>=0; i--){
        int x = aA[i] - aB[i] - borrow;
        if( x<0 ){
          pA->a[i] = x+10;
          borrow = 1;
        }else{
          pA->a[i] = x;
          borrow = 0;
        }
      }
    }
  }
}

/*
** Multiply A by B.   A := A * B
**
** All significant digits after the decimal point are retained.
** Trailing zeros after the decimal point are omitted as long as
** the number of digits after the decimal point is no less than
** either the number of digits in either input.
*/
static void decimalMul(Decimal *pA, Decimal *pB){
  signed char *acc = 0;
  int i, j, k;
  int minFrac;
  sqlite3_int64 sumDigit;

  if( pA==0 || pA->oom || pA->isNull
   || pB==0 || pB->oom || pB->isNull 
  ){
    goto mul_end;
  }
  sumDigit = pA->nDigit;
  sumDigit += pB->nDigit;
  sumDigit += 2;
  if( sumDigit>SQLITE_DECIMAL_MAX_DIGIT ){ pA->oom = 1; return; }
  acc = sqlite3_malloc64( sumDigit );
  if( acc==0 ){
    pA->oom = 1;
    goto mul_end;
  }
  memset(acc, 0, pA->nDigit + pB->nDigit + 2);
  minFrac = pA->nFrac;
  if( pB->nFrac<minFrac ) minFrac = pB->nFrac;
  for(i=pA->nDigit-1; i>=0; i--){
    signed char f = pA->a[i];
    int carry = 0, x;
    for(j=pB->nDigit-1, k=i+j+3; j>=0; j--, k--){
      x = acc[k] + f*pB->a[j] + carry;
      acc[k] = x%10;
      carry = x/10;
    }
    x = acc[k] + carry;
    acc[k] = x%10;
    acc[k-1] += x/10;
  }
  sqlite3_free(pA->a);
  pA->a = acc;
  acc = 0;
  pA->nDigit += pB->nDigit + 2;
  pA->nFrac += pB->nFrac;
  pA->sign ^= pB->sign;
  while( pA->nFrac>minFrac && pA->a[pA->nDigit-1]==0 ){
    pA->nFrac--;
    pA->nDigit--;
  }

mul_end:
  sqlite3_free(acc);
}

/*
** Create a new Decimal object that contains an integer power of 2.
*/
static Decimal *decimalPow2(int N){
  Decimal *pA = 0;      /* The result to be returned */
  Decimal *pX = 0;      /* Multiplier */
  if( N<-20000 || N>20000 ) goto pow2_fault;
  pA = decimalNewFromText("1.0", 3);
  if( pA==0 || pA->oom ) goto pow2_fault;
  if( N==0 ) return pA;
  if( N>0 ){
    pX = decimalNewFromText("2.0", 3);
  }else{
    N = -N;
    pX = decimalNewFromText("0.5", 3);
  }
  if( pX==0 || pX->oom ) goto pow2_fault;
  while( 1 /* Exit by break */ ){
    if( N & 1 ){
      decimalMul(pA, pX);
      if( pA->oom ) goto pow2_fault;
    }
    N >>= 1;
    if( N==0 ) break;
    decimalMul(pX, pX);
  }
  decimal_free(pX);
  return pA;

pow2_fault:
  decimal_free(pA);
  decimal_free(pX);
  return 0;
}

/*
** Use an IEEE754 binary64 ("double") to generate a new Decimal object.
*/
static Decimal *decimalFromDouble(double r){
  sqlite3_int64 m, a;
  int e;
  int isNeg;
  Decimal *pA;
  Decimal *pX;
  char zNum[100];
  if( r<0.0 ){
    isNeg = 1;
    r = -r;
  }else{
    isNeg = 0;
  }
  memcpy(&a,&r,sizeof(a));
  if( a==0 || a==(sqlite3_int64)0x8000000000000000LL){
    e = 0;
    m = 0;
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
    e = e - 1075;
    if( e>971 ){
      return 0;  /* A NaN or an Infinity */
    }
  }

  /* At this point m is the integer significand and e is the exponent */
  sqlite3_snprintf(sizeof(zNum), zNum, "%lld", m);
  pA = decimalNewFromText(zNum, (int)strlen(zNum));
  pX = decimalPow2(e);
  decimalMul(pA, pX);
  decimal_free(pX);
  return pA;
}

/*
** SQL Function:   decimal(X)
** OR:             decimal_exp(X)
**
** Convert input X into decimal and then back into text.
**
** If X is originally a float, then a full decimal expansion of that floating
** point value is done.  Or if X is an 8-byte blob, it is interpreted
** as a float and similarly expanded.
**
** The decimal_exp(X) function returns the result in exponential notation.
** decimal(X) returns a complete decimal, without the e+NNN at the end.
*/
static void decimalFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *p =  decimal_new(context, argv[0], 0);
  int N;
  if( argc==2 ){
    N = sqlite3_value_int(argv[1]);
    if( N>0 ) decimal_round(p, N);
  }else{
    N = 0;
  }
  if( p ){
    if( sqlite3_user_data(context)!=0 ){
      decimal_result_sci(context, p, N);
    }else{
      decimal_result(context, p);
    }
    decimal_free(p);
  }
}

/*
** Compare text in decimal order.
*/
static int decimalCollFunc(
  void *notUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  const unsigned char *zA = (const unsigned char*)pKey1;
  const unsigned char *zB = (const unsigned char*)pKey2;
  Decimal *pA = decimalNewFromText((const char*)zA, nKey1);
  Decimal *pB = decimalNewFromText((const char*)zB, nKey2);
  int rc;
  UNUSED_PARAMETER(notUsed);
  if( pA==0 || pB==0 ){
    rc = 0;
  }else{
    rc = decimal_cmp(pA, pB);
  }
  decimal_free(pA);
  decimal_free(pB);
  return rc;
}


/*
** SQL Function:   decimal_add(X, Y)
**                 decimal_sub(X, Y)
**
** Return the sum or difference of X and Y.
*/
static void decimalAddFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *pA = decimal_new(context, argv[0], 1);
  Decimal *pB = decimal_new(context, argv[1], 1);
  UNUSED_PARAMETER(argc);
  decimal_add(pA, pB);
  decimal_result(context, pA);
  decimal_free(pA);
  decimal_free(pB);
}
static void decimalSubFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *pA = decimal_new(context, argv[0], 1);
  Decimal *pB = decimal_new(context, argv[1], 1);
  UNUSED_PARAMETER(argc);
  if( pB ){
    pB->sign = !pB->sign;
    decimal_add(pA, pB);
    decimal_result(context, pA);
  }
  decimal_free(pA);
  decimal_free(pB);
}

/* Aggregate function:   decimal_sum(X)
**
** Works like sum() except that it uses decimal arithmetic for unlimited
** precision.
*/
static void decimalSumStep(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *p;
  Decimal *pArg;
  UNUSED_PARAMETER(argc);
  p = sqlite3_aggregate_context(context, sizeof(*p));
  if( p==0 ) return;
  if( !p->isInit ){
    p->isInit = 1;
    p->a = sqlite3_malloc64(2);
    if( p->a==0 ){
      p->oom = 1;
    }else{
      p->a[0] = 0;
    }
    p->nDigit = 1;
    p->nFrac = 0;
  }
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;
  pArg = decimal_new(context, argv[0], 1);
  decimal_add(p, pArg);
  decimal_free(pArg);
}
static void decimalSumInverse(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *p;
  Decimal *pArg;
  UNUSED_PARAMETER(argc);
  p = sqlite3_aggregate_context(context, sizeof(*p));
  if( p==0 ) return;
  if( sqlite3_value_type(argv[0])==SQLITE_NULL ) return;
  pArg = decimal_new(context, argv[0], 1);
  if( pArg ) pArg->sign = !pArg->sign;
  decimal_add(p, pArg);
  decimal_free(pArg);
}
static void decimalSumValue(sqlite3_context *context){
  Decimal *p = sqlite3_aggregate_context(context, 0);
  if( p==0 ) return;
  decimal_result(context, p);
}
static void decimalSumFinalize(sqlite3_context *context){
  Decimal *p = sqlite3_aggregate_context(context, 0);
  if( p==0 ) return;
  decimal_result(context, p);
  decimal_clear(p);
}

/*
** SQL Function:   decimal_mul(X, Y)
**
** Return the product of X and Y.
*/
static void decimalMulFunc(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  Decimal *pA = decimal_new(context, argv[0], 1);
  Decimal *pB = decimal_new(context, argv[1], 1);
  UNUSED_PARAMETER(argc);
  if( pA==0 || pA->oom || pA->isNull
   || pB==0 || pB->oom || pB->isNull 
  ){
    goto mul_end;
  }
  decimalMul(pA, pB);
  if( pA->oom ){
    goto mul_end;
  }
  decimal_result(context, pA);

mul_end:
  decimal_free(pA);
  decimal_free(pB);
}

/*
** SQL Function:   decimal_pow2(N)
**
** Return the N-th power of 2.  N must be an integer.
*/
static void decimalPow2Func(
  sqlite3_context *context,
  int argc,
  sqlite3_value **argv
){
  UNUSED_PARAMETER(argc);
  if( sqlite3_value_type(argv[0])==SQLITE_INTEGER ){
    Decimal *pA = decimalPow2(sqlite3_value_int(argv[0]));
    decimal_result_sci(context, pA, 0);
    decimal_free(pA);
  }
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_decimal_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  static const struct {
    const char *zFuncName;
    int nArg;
    int iArg;
    void (*xFunc)(sqlite3_context*,int,sqlite3_value**);
  } aFunc[] = {
    { "decimal",       1, 0,  decimalFunc        },
    { "decimal",       2, 0,  decimalFunc        },
    { "decimal_exp",   1, 1,  decimalFunc        },
    { "decimal_exp",   2, 1,  decimalFunc        },
    { "decimal_cmp",   2, 0,  decimalCmpFunc     },
    { "decimal_add",   2, 0,  decimalAddFunc     },
    { "decimal_sub",   2, 0,  decimalSubFunc     },
    { "decimal_mul",   2, 0,  decimalMulFunc     },
    { "decimal_pow2",  1, 0,  decimalPow2Func    },
  };
  unsigned int i;
  (void)pzErrMsg;  /* Unused parameter */

  SQLITE_EXTENSION_INIT2(pApi);

  for(i=0; i<(int)(sizeof(aFunc)/sizeof(aFunc[0])) && rc==SQLITE_OK; i++){
    rc = sqlite3_create_function(db, aFunc[i].zFuncName, aFunc[i].nArg,
                   SQLITE_UTF8|SQLITE_INNOCUOUS|SQLITE_DETERMINISTIC,
                   aFunc[i].iArg ? db : 0, aFunc[i].xFunc, 0, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_window_function(db, "decimal_sum", 1,
                   SQLITE_UTF8|SQLITE_INNOCUOUS|SQLITE_DETERMINISTIC, 0,
                   decimalSumStep, decimalSumFinalize,
                   decimalSumValue, decimalSumInverse, 0);
  }
  if( rc==SQLITE_OK ){
    rc = sqlite3_create_collation(db, "decimal", SQLITE_UTF8,
                                  0, decimalCollFunc);
  }
  return rc;
}
