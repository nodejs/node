/*
** 2020-04-14
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
** This SQLite extension implements the UINT collating sequence.
**
** UINT works like BINARY for text, except that embedded strings
** of digits compare in numeric order.
**
**     *   Leading zeros are handled properly, in the sense that
**         they do not mess of the magnitude comparison of embedded
**         strings of digits.  "x00123y" is equal to "x123y".
**
**     *   Only unsigned integers are recognized.  Plus and minus
**         signs are ignored.  Decimal points and exponential notation
**         are ignored.
**
**     *   Embedded integers can be of arbitrary length.  Comparison
**         is *not* limited integers that can be expressed as a
**         64-bit machine integer.
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>
#include <ctype.h>

/*
** Compare text in lexicographic order, except strings of digits
** compare in numeric order.
*/
static int uintCollFunc(
  void *notUsed,
  int nKey1, const void *pKey1,
  int nKey2, const void *pKey2
){
  const unsigned char *zA = (const unsigned char*)pKey1;
  const unsigned char *zB = (const unsigned char*)pKey2;
  int i=0, j=0, x;
  (void)notUsed;
  while( i<nKey1 && j<nKey2 ){
    x = zA[i] - zB[j];
    if( isdigit(zA[i]) ){
      int k;
      if( !isdigit(zB[j]) ) return x;
      while( i<nKey1 && zA[i]=='0' ){ i++; }
      while( j<nKey2 && zB[j]=='0' ){ j++; }
      k = 0;
      while( i+k<nKey1 && isdigit(zA[i+k])
             && j+k<nKey2 && isdigit(zB[j+k]) ){
        k++;
      }
      if( i+k<nKey1 && isdigit(zA[i+k]) ){
        return +1;
      }else if( j+k<nKey2 && isdigit(zB[j+k]) ){
        return -1;
      }else{
        x = memcmp(zA+i, zB+j, k);
        if( x ) return x;
        i += k;
        j += k;
      }
    }else if( x ){
      return x;
    }else{
      i++;
      j++;
    }
  }
  return (nKey1 - i) - (nKey2 - j);
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_uint_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi);
  (void)pzErrMsg;  /* Unused parameter */
  return sqlite3_create_collation(db, "uint", SQLITE_UTF8, 0, uintCollFunc);
}
