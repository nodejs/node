/*
** 2015-08-18, 2023-04-28
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
** This file demonstrates how to create a table-valued-function using
** a virtual table.  This demo implements the generate_series() function
** which gives the same results as the eponymous function in PostgreSQL,
** within the limitation that its arguments are signed 64-bit integers.
**
** Considering its equivalents to generate_series(start,stop,step): A
** value V[n] sequence is produced for integer n ascending from 0 where
**  ( V[n] == start + n * step  &&  sgn(V[n] - stop) * sgn(step) >= 0 )
** for each produced value (independent of production time ordering.)
**
** All parameters must be either integer or convertable to integer.
** The start parameter is required.
** The stop parameter defaults to (1<<32)-1 (aka 4294967295 or 0xffffffff)
** The step parameter defaults to 1 and 0 is treated as 1.
**
** Examples:
**
**      SELECT * FROM generate_series(0,100,5);
**
** The query above returns integers from 0 through 100 counting by steps
** of 5.  In other words, 0, 5, 10, 15, ..., 90, 95, 100.  There are a total
** of 21 rows.
**
**      SELECT * FROM generate_series(0,100);
**
** Integers from 0 through 100 with a step size of 1.  101 rows.
**
**      SELECT * FROM generate_series(20) LIMIT 10;
**
** Integers 20 through 29.  10 rows.
**
**      SELECT * FROM generate_series(0,-100,-5);
**
** Integers 0 -5 -10 ... -100.  21 rows.
**
**      SELECT * FROM generate_series(0,-1);
**
** Empty sequence.
**
** HOW IT WORKS
**
** The generate_series "function" is really a virtual table with the
** following schema:
**
**     CREATE TABLE generate_series(
**       value,
**       start HIDDEN,
**       stop HIDDEN,
**       step HIDDEN
**     );
**
** The virtual table also has a rowid which is an alias for the value.
**
** Function arguments in queries against this virtual table are translated
** into equality constraints against successive hidden columns.  In other
** words, the following pairs of queries are equivalent to each other:
**
**    SELECT * FROM generate_series(0,100,5);
**    SELECT * FROM generate_series WHERE start=0 AND stop=100 AND step=5;
**
**    SELECT * FROM generate_series(0,100);
**    SELECT * FROM generate_series WHERE start=0 AND stop=100;
**
**    SELECT * FROM generate_series(20) LIMIT 10;
**    SELECT * FROM generate_series WHERE start=20 LIMIT 10;
**
** The generate_series virtual table implementation leaves the xCreate method
** set to NULL.  This means that it is not possible to do a CREATE VIRTUAL
** TABLE command with "generate_series" as the USING argument.  Instead, there
** is a single generate_series virtual table that is always available without
** having to be created first.
**
** The xBestIndex method looks for equality constraints against the hidden
** start, stop, and step columns, and if present, it uses those constraints
** to bound the sequence of generated values.  If the equality constraints
** are missing, it uses 0 for start, 4294967295 for stop, and 1 for step.
** xBestIndex returns a small cost when both start and stop are available,
** and a very large cost if either start or stop are unavailable.  This
** encourages the query planner to order joins such that the bounds of the
** series are well-defined.
**
** Update on 2024-08-22:
** xBestIndex now also looks for equality and inequality constraints against
** the value column and uses those constraints as additional bounds against
** the sequence range.  Thus, a query like this:
**
**     SELECT value FROM generate_series($SA,$EA)
**      WHERE value BETWEEN $SB AND $EB;
**
** Is logically the same as:
**
**     SELECT value FROM generate_series(max($SA,$SB),min($EA,$EB));
**
** Constraints on the value column can server as substitutes for constraints
** on the hidden start and stop columns.  So, the following two queries
** are equivalent:
**
**     SELECT value FROM generate_series($S,$E);
**     SELECT value FROM generate_series WHERE value BETWEEN $S and $E;
**
*/
#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#ifndef SQLITE_OMIT_VIRTUALTABLE

/* series_cursor is a subclass of sqlite3_vtab_cursor which will
** serve as the underlying representation of a cursor that scans
** over rows of the result.
**
** iOBase, iOTerm, and iOStep are the original values of the
** start=, stop=, and step= constraints on the query.  These are
** the values reported by the start, stop, and step columns of the
** virtual table.
**
** iBase, iTerm, iStep, and bDescp are the actual values used to generate
** the sequence.  These might be different from the iOxxxx values.
** For example in
**
**   SELECT value FROM generate_series(1,11,2)
**    WHERE value BETWEEN 4 AND 8;
**
** The iOBase is 1, but the iBase is 5.  iOTerm is 11 but iTerm is 7.
** Another example:
**
**   SELECT value FROM generate_series(1,15,3) ORDER BY value DESC;
**
** The cursor initialization for the above query is:
**
**   iOBase = 1        iBase = 13
**   iOTerm = 15       iTerm = 1
**   iOStep = 3        iStep = 3      bDesc = 1
**
** The actual step size is unsigned so that can have a value of
** +9223372036854775808 which is needed for querys like this:
**
**   SELECT value
**     FROM generate_series(9223372036854775807,
**                          -9223372036854775808,
**                          -9223372036854775808)
**    ORDER BY value ASC;
**
** The setup for the previous query will be:
**
**   iOBase = 9223372036854775807    iBase = -1
**   iOTerm = -9223372036854775808   iTerm = 9223372036854775807
**   iOStep = -9223372036854775808   iStep = 9223372036854775808  bDesc = 0
*/
typedef unsigned char u8;
typedef struct series_cursor series_cursor;
struct series_cursor {
  sqlite3_vtab_cursor base;  /* Base class - must be first */
  sqlite3_int64 iOBase;      /* Original starting value ("start") */
  sqlite3_int64 iOTerm;      /* Original terminal value ("stop") */
  sqlite3_int64 iOStep;      /* Original step value */
  sqlite3_int64 iBase;       /* Starting value to actually use */
  sqlite3_int64 iTerm;       /* Terminal value to actually use */
  sqlite3_uint64 iStep;      /* The step size */
  sqlite3_int64 iValue;      /* Current value */
  u8 bDesc;                  /* iStep is really negative */
  u8 bDone;                  /* True if stepped past last element */
};

/*
** Computed the difference between two 64-bit signed integers using a
** convoluted computation designed to work around the silly restriction
** against signed integer overflow in C.
*/
static sqlite3_uint64 span64(sqlite3_int64 a, sqlite3_int64 b){
  assert( a>=b );
  return (*(sqlite3_uint64*)&a) - (*(sqlite3_uint64*)&b);
}  

/*
** Add or substract an unsigned 64-bit integer from a signed 64-bit integer
** and return the new signed 64-bit integer.
*/
static sqlite3_int64 add64(sqlite3_int64 a, sqlite3_uint64 b){
  sqlite3_uint64 x = *(sqlite3_uint64*)&a;
  x += b;
  return *(sqlite3_int64*)&x;
}
static sqlite3_int64 sub64(sqlite3_int64 a, sqlite3_uint64 b){
  sqlite3_uint64 x = *(sqlite3_uint64*)&a;
  x -= b;
  return *(sqlite3_int64*)&x;
}

/*
** The seriesConnect() method is invoked to create a new
** series_vtab that describes the generate_series virtual table.
**
** Think of this routine as the constructor for series_vtab objects.
**
** All this routine needs to do is:
**
**    (1) Allocate the series_vtab object and initialize all fields.
**
**    (2) Tell SQLite (via the sqlite3_declare_vtab() interface) what the
**        result set of queries against generate_series will look like.
*/
static int seriesConnect(
  sqlite3 *db,
  void *pUnused,
  int argcUnused, const char *const*argvUnused,
  sqlite3_vtab **ppVtab,
  char **pzErrUnused
){
  sqlite3_vtab *pNew;
  int rc;

/* Column numbers */
#define SERIES_COLUMN_ROWID (-1)
#define SERIES_COLUMN_VALUE 0
#define SERIES_COLUMN_START 1
#define SERIES_COLUMN_STOP  2
#define SERIES_COLUMN_STEP  3

  (void)pUnused;
  (void)argcUnused;
  (void)argvUnused;
  (void)pzErrUnused;
  rc = sqlite3_declare_vtab(db,
     "CREATE TABLE x(value,start hidden,stop hidden,step hidden)");
  if( rc==SQLITE_OK ){
    pNew = *ppVtab = sqlite3_malloc64( sizeof(*pNew) );
    if( pNew==0 ) return SQLITE_NOMEM;
    memset(pNew, 0, sizeof(*pNew));
    sqlite3_vtab_config(db, SQLITE_VTAB_INNOCUOUS);
  }
  return rc;
}

/*
** This method is the destructor for series_cursor objects.
*/
static int seriesDisconnect(sqlite3_vtab *pVtab){
  sqlite3_free(pVtab);
  return SQLITE_OK;
}

/*
** Constructor for a new series_cursor object.
*/
static int seriesOpen(sqlite3_vtab *pUnused, sqlite3_vtab_cursor **ppCursor){
  series_cursor *pCur;
  (void)pUnused;
  pCur = sqlite3_malloc64( sizeof(*pCur) );
  if( pCur==0 ) return SQLITE_NOMEM;
  memset(pCur, 0, sizeof(*pCur));
  *ppCursor = &pCur->base;
  return SQLITE_OK;
}

/*
** Destructor for a series_cursor.
*/
static int seriesClose(sqlite3_vtab_cursor *cur){
  sqlite3_free(cur);
  return SQLITE_OK;
}


/*
** Advance a series_cursor to its next row of output.
*/
static int seriesNext(sqlite3_vtab_cursor *cur){
  series_cursor *pCur = (series_cursor*)cur;
  if( pCur->iValue==pCur->iTerm ){
    pCur->bDone = 1;
  }else if( pCur->bDesc ){
    pCur->iValue = sub64(pCur->iValue, pCur->iStep);
    assert( pCur->iValue>=pCur->iTerm );
  }else{
    pCur->iValue = add64(pCur->iValue, pCur->iStep);
    assert( pCur->iValue<=pCur->iTerm );
  }
  return SQLITE_OK;
}

/*
** Return values of columns for the row at which the series_cursor
** is currently pointing.
*/
static int seriesColumn(
  sqlite3_vtab_cursor *cur,   /* The cursor */
  sqlite3_context *ctx,       /* First argument to sqlite3_result_...() */
  int i                       /* Which column to return */
){
  series_cursor *pCur = (series_cursor*)cur;
  sqlite3_int64 x = 0;
  switch( i ){
    case SERIES_COLUMN_START:  x = pCur->iOBase;     break;
    case SERIES_COLUMN_STOP:   x = pCur->iOTerm;     break;
    case SERIES_COLUMN_STEP:   x = pCur->iOStep;     break;
    default:                   x = pCur->iValue;     break;
  }
  sqlite3_result_int64(ctx, x);
  return SQLITE_OK;
}

#ifndef LARGEST_UINT64
#define LARGEST_INT64  ((sqlite3_int64)0x7fffffffffffffffLL)
#define LARGEST_UINT64 ((sqlite3_uint64)0xffffffffffffffffULL)
#define SMALLEST_INT64 ((sqlite3_int64)0x8000000000000000LL)
#endif

/*
** The rowid is the same as the value.
*/
static int seriesRowid(sqlite3_vtab_cursor *cur, sqlite_int64 *pRowid){
  series_cursor *pCur = (series_cursor*)cur;
  *pRowid = pCur->iValue;
  return SQLITE_OK;
}

/*
** Return TRUE if the cursor has been moved off of the last
** row of output.
*/
static int seriesEof(sqlite3_vtab_cursor *cur){
  series_cursor *pCur = (series_cursor*)cur;
  return pCur->bDone;
}

/* True to cause run-time checking of the start=, stop=, and/or step=
** parameters.  The only reason to do this is for testing the
** constraint checking logic for virtual tables in the SQLite core.
*/
#ifndef SQLITE_SERIES_CONSTRAINT_VERIFY
# define SQLITE_SERIES_CONSTRAINT_VERIFY 0
#endif

/*
** Return the number of steps between pCur->iBase and pCur->iTerm if
** the step width is pCur->iStep.
*/
static sqlite3_uint64 seriesSteps(series_cursor *pCur){
  if( pCur->bDesc ){
    assert( pCur->iBase >= pCur->iTerm );
    return span64(pCur->iBase, pCur->iTerm)/pCur->iStep;
  }else{
    assert( pCur->iBase <= pCur->iTerm );
    return span64(pCur->iTerm, pCur->iBase)/pCur->iStep;
  }
}

#if defined(SQLITE_ENABLE_MATH_FUNCTIONS) || defined(_WIN32)
/*
** Case 1 (the most common case):
** The standard math library is available so use ceil() and floor() from there.
*/
static double seriesCeil(double r){ return ceil(r); }
static double seriesFloor(double r){ return floor(r); }
#elif defined(__GNUC__) && !defined(SQLITE_DISABLE_INTRINSIC)
/*
** Case 2 (2nd most common): Use GCC/Clang builtins
*/
static double seriesCeil(double r){ return __builtin_ceil(r); }
static double seriesFloor(double r){ return __builtin_floor(r); }
#else
/*
** Case 3 (rarely happens): Use home-grown ceil() and floor() routines.
*/
static double seriesCeil(double r){
  sqlite3_int64 x;
  if( r!=r ) return r;
  if( r<=(-4503599627370496.0) ) return r;
  if( r>=(+4503599627370496.0) ) return r;
  x = (sqlite3_int64)r;
  if( r==(double)x ) return r;
  if( r>(double)x ) x++;
  return (double)x;
}
static double seriesFloor(double r){
  sqlite3_int64 x;
  if( r!=r ) return r;
  if( r<=(-4503599627370496.0) ) return r;
  if( r>=(+4503599627370496.0) ) return r;
  x = (sqlite3_int64)r;
  if( r==(double)x ) return r;
  if( r<(double)x ) x--;
  return (double)x;
}
#endif

/*
** This method is called to "rewind" the series_cursor object back
** to the first row of output.  This method is always called at least
** once prior to any call to seriesColumn() or seriesRowid() or
** seriesEof().
**
** The query plan selected by seriesBestIndex is passed in the idxNum
** parameter.  (idxStr is not used in this implementation.)  idxNum
** is a bitmask showing which constraints are available:
**
**   0x0001:    start=VALUE
**   0x0002:    stop=VALUE
**   0x0004:    step=VALUE
**   0x0008:    descending order
**   0x0010:    ascending order
**   0x0020:    LIMIT  VALUE
**   0x0040:    OFFSET  VALUE
**   0x0080:    value=VALUE
**   0x0100:    value>=VALUE
**   0x0200:    value>VALUE
**   0x1000:    value<=VALUE
**   0x2000:    value<VALUE
**
** This routine should initialize the cursor and position it so that it
** is pointing at the first row, or pointing off the end of the table
** (so that seriesEof() will return true) if the table is empty.
*/
static int seriesFilter(
  sqlite3_vtab_cursor *pVtabCursor,
  int idxNum, const char *idxStrUnused,
  int argc, sqlite3_value **argv
){
  series_cursor *pCur = (series_cursor *)pVtabCursor;
  int iArg = 0;                         /* Arguments used so far */
  int i;                                /* Loop counter */
  sqlite3_int64 iMin = SMALLEST_INT64;  /* Smallest allowed output value */
  sqlite3_int64 iMax = LARGEST_INT64;   /* Largest allowed output value */
  sqlite3_int64 iLimit = 0;             /* if >0, the value of the LIMIT */
  sqlite3_int64 iOffset = 0;            /* if >0, the value of the OFFSET */

  (void)idxStrUnused;

  /* If any constraints have a NULL value, then return no rows.
  ** See ticket https://sqlite.org/src/info/fac496b61722daf2
  */
  for(i=0; i<argc; i++){
    if( sqlite3_value_type(argv[i])==SQLITE_NULL ){
      goto series_no_rows;
    }
  }

  /* Capture the three HIDDEN parameters to the virtual table and insert
  ** default values for any parameters that are omitted.
  */
  if( idxNum & 0x01 ){
    pCur->iOBase = sqlite3_value_int64(argv[iArg++]);
  }else{
    pCur->iOBase = 0;
  }
  if( idxNum & 0x02 ){
    pCur->iOTerm = sqlite3_value_int64(argv[iArg++]);
  }else{
    pCur->iOTerm = 0xffffffff;
  }
  if( idxNum & 0x04 ){
    pCur->iOStep = sqlite3_value_int64(argv[iArg++]);
    if( pCur->iOStep==0 ) pCur->iOStep = 1;
  }else{
    pCur->iOStep = 1;
  }

  /* If there are constraints on the value column but there are
  ** no constraints on  the start, stop, and step columns, then
  ** initialize the default range to be the entire range of 64-bit signed
  ** integers.  This range will contracted by the value column constraints
  ** further below.
  */
  if( (idxNum & 0x05)==0 && (idxNum & 0x0380)!=0 ){
    pCur->iOBase = SMALLEST_INT64;
  }
  if( (idxNum & 0x06)==0 && (idxNum & 0x3080)!=0 ){
    pCur->iOTerm = LARGEST_INT64;
  }
  pCur->iBase = pCur->iOBase;
  pCur->iTerm = pCur->iOTerm;
  if( pCur->iOStep>0 ){  
    pCur->iStep = pCur->iOStep;
  }else if( pCur->iOStep>SMALLEST_INT64 ){
    pCur->iStep = -pCur->iOStep;
  }else{
    pCur->iStep = LARGEST_INT64;
    pCur->iStep++;
  }
  pCur->bDesc = pCur->iOStep<0;
  if( pCur->bDesc==0 && pCur->iBase>pCur->iTerm ){
    goto series_no_rows;
  }
  if( pCur->bDesc!=0 && pCur->iBase<pCur->iTerm ){
    goto series_no_rows;
  }

  /* Extract the LIMIT and OFFSET values, but do not apply them yet.
  ** The range must first be constrained by the limits on value.
  */
  if( idxNum & 0x20 ){
    iLimit = sqlite3_value_int64(argv[iArg++]);
    if( idxNum & 0x40 ){
      iOffset = sqlite3_value_int64(argv[iArg++]);
    }
  }

  /* Narrow the range of iMin and iMax (the minimum and maximum outputs)
  ** based on equality and inequality constraints on the "value" column.
  */
  if( idxNum & 0x3380 ){
    if( idxNum & 0x0080 ){    /* value=X */
      if( sqlite3_value_numeric_type(argv[iArg])==SQLITE_FLOAT ){
        double r = sqlite3_value_double(argv[iArg++]);
        if( r==seriesCeil(r)
         && r>=(double)SMALLEST_INT64
         && r<=(double)LARGEST_INT64
        ){
          iMin = iMax = (sqlite3_int64)r;
        }else{
          goto series_no_rows;
        }
      }else{
        iMin = iMax = sqlite3_value_int64(argv[iArg++]);
      }
    }else{
      if( idxNum & 0x0300 ){  /* value>X or value>=X */
        if( sqlite3_value_numeric_type(argv[iArg])==SQLITE_FLOAT ){
          double r = sqlite3_value_double(argv[iArg++]);
          if( r<(double)SMALLEST_INT64 ){
            iMin = SMALLEST_INT64;
          }else if( (idxNum & 0x0200)!=0 && r==seriesCeil(r) ){
            iMin = (sqlite3_int64)seriesCeil(r+1.0);
          }else{
            iMin = (sqlite3_int64)seriesCeil(r);
          }
        }else{
          iMin = sqlite3_value_int64(argv[iArg++]);
          if( (idxNum & 0x0200)!=0 ){
            if( iMin==LARGEST_INT64 ){
              goto series_no_rows;
            }else{
              iMin++;
            }
          }
        }
      }
      if( idxNum & 0x3000 ){   /* value<X or value<=X */
        if( sqlite3_value_numeric_type(argv[iArg])==SQLITE_FLOAT ){
          double r = sqlite3_value_double(argv[iArg++]);
          if( r>(double)LARGEST_INT64 ){
            iMax = LARGEST_INT64;
          }else if( (idxNum & 0x2000)!=0 && r==seriesFloor(r) ){
            iMax = (sqlite3_int64)(r-1.0);
          }else{
            iMax = (sqlite3_int64)seriesFloor(r);
          }
        }else{
          iMax = sqlite3_value_int64(argv[iArg++]);
          if( idxNum & 0x2000 ){
            if( iMax==SMALLEST_INT64 ){
              goto series_no_rows;
            }else{
              iMax--;
            }
          }
        }
      }
      if( iMin>iMax ){
        goto series_no_rows;
      }
    }

    /* Try to reduce the range of values to be generated based on
    ** constraints on the "value" column.
    */
    if( pCur->bDesc==0 ){
      if( pCur->iBase<iMin ){
        sqlite3_uint64 span = span64(iMin,pCur->iBase);
        pCur->iBase = add64(pCur->iBase, (span/pCur->iStep)*pCur->iStep);
        if( pCur->iBase<iMin ){
          if( pCur->iBase > sub64(LARGEST_INT64, pCur->iStep) ){
            goto series_no_rows;
          }
          pCur->iBase = add64(pCur->iBase, pCur->iStep);
        }
      }
      if( pCur->iTerm>iMax ){
        pCur->iTerm = iMax;
      }
    }else{
      if( pCur->iBase>iMax ){
        sqlite3_uint64 span = span64(pCur->iBase,iMax);
        pCur->iBase = sub64(pCur->iBase, (span/pCur->iStep)*pCur->iStep);
        if( pCur->iBase>iMax ){
          if( pCur->iBase < add64(SMALLEST_INT64, pCur->iStep) ){
            goto series_no_rows;
          }
          pCur->iBase = sub64(pCur->iBase, pCur->iStep);
        }
      }
      if( pCur->iTerm<iMin ){
        pCur->iTerm = iMin;
      }
    }
  }

  /* Adjust iTerm so that it is exactly the last value of the series.
  */
  if( pCur->bDesc==0 ){
    if( pCur->iBase>pCur->iTerm ){
      goto series_no_rows;
    }
    pCur->iTerm = sub64(pCur->iTerm,
                        span64(pCur->iTerm,pCur->iBase) % pCur->iStep);
  }else{
    if( pCur->iBase<pCur->iTerm ){
      goto series_no_rows;
    }
    pCur->iTerm = add64(pCur->iTerm,
                        span64(pCur->iBase,pCur->iTerm) % pCur->iStep);
  }

  /* Transform the series generator to output values in the requested
  ** order.
  */
  if( ((idxNum & 0x0008)!=0 && pCur->bDesc==0)
   || ((idxNum & 0x0010)!=0 && pCur->bDesc!=0)
  ){
    sqlite3_int64 tmp = pCur->iBase;
    pCur->iBase = pCur->iTerm;
    pCur->iTerm = tmp;
    pCur->bDesc = !pCur->bDesc;
  }

  /* Apply LIMIT and OFFSET constraints, if any */
  assert( pCur->iStep!=0 );
  if( idxNum & 0x20 ){
    if( iOffset>0 ){
      if( seriesSteps(pCur) < (sqlite3_uint64)iOffset ){
        goto series_no_rows;
      }else if( pCur->bDesc ){
        pCur->iBase = sub64(pCur->iBase, pCur->iStep*iOffset);
      }else{
        pCur->iBase = add64(pCur->iBase, pCur->iStep*iOffset);
      }
    }
    if( iLimit>=0 && seriesSteps(pCur) > (sqlite3_uint64)iLimit ){
      pCur->iTerm = add64(pCur->iBase, (iLimit - 1)*pCur->iStep);
    }
  }
  pCur->iValue = pCur->iBase;
  pCur->bDone = 0;
  return SQLITE_OK;

series_no_rows:
  pCur->iBase = 0;
  pCur->iTerm = 0;
  pCur->iStep = 1;
  pCur->bDesc = 0;
  pCur->bDone = 1;
  return SQLITE_OK;  
}

/*
** SQLite will invoke this method one or more times while planning a query
** that uses the generate_series virtual table.  This routine needs to create
** a query plan for each invocation and compute an estimated cost for that
** plan.
**
** In this implementation idxNum is used to represent the
** query plan.  idxStr is unused.
**
** The query plan is represented by bits in idxNum:
**
**   0x0001  start = $num
**   0x0002  stop = $num
**   0x0004  step = $num
**   0x0008  output is in descending order
**   0x0010  output is in ascending order
**   0x0020  LIMIT $num
**   0x0040  OFFSET $num
**   0x0080  value = $num
**   0x0100  value >= $num
**   0x0200  value > $num
**   0x1000  value <= $num
**   0x2000  value < $num
**
** Only one of 0x0100 or 0x0200 will be returned.  Similarly, only
** one of 0x1000 or 0x2000 will be returned.  If the 0x0080 is set, then
** none of the 0xff00 bits will be set.
**
** The order of parameters passed to xFilter is as follows:
**
**    * The argument to start= if bit 0x0001 is in the idxNum mask
**    * The argument to stop= if bit 0x0002 is in the idxNum mask
**    * The argument to step= if bit 0x0004 is in the idxNum mask
**    * The argument to LIMIT if bit 0x0020 is in the idxNum mask
**    * The argument to OFFSET if bit 0x0040 is in the idxNum mask
**    * The argument to value=, or value>= or value> if any of
**      bits 0x0380 are in the idxNum mask
**    * The argument to value<= or value< if either of bits 0x3000
**      are in the mask
**
*/
static int seriesBestIndex(
  sqlite3_vtab *pVTab,
  sqlite3_index_info *pIdxInfo
){
  int i, j;              /* Loop over constraints */
  int idxNum = 0;        /* The query plan bitmask */
#ifndef ZERO_ARGUMENT_GENERATE_SERIES
  int bStartSeen = 0;    /* EQ constraint seen on the START column */
#endif
  int unusableMask = 0;  /* Mask of unusable constraints */
  int nArg = 0;          /* Number of arguments that seriesFilter() expects */
  int aIdx[7];           /* Constraints on start, stop, step, LIMIT, OFFSET,
                         ** and value.  aIdx[5] covers value=, value>=, and
                         ** value>,  aIdx[6] covers value<= and value< */
  const struct sqlite3_index_constraint *pConstraint;

  /* This implementation assumes that the start, stop, and step columns
  ** are the last three columns in the virtual table. */
  assert( SERIES_COLUMN_STOP == SERIES_COLUMN_START+1 );
  assert( SERIES_COLUMN_STEP == SERIES_COLUMN_START+2 );

  aIdx[0] = aIdx[1] = aIdx[2] = aIdx[3] = aIdx[4] = aIdx[5] = aIdx[6] = -1;
  pConstraint = pIdxInfo->aConstraint;
  for(i=0; i<pIdxInfo->nConstraint; i++, pConstraint++){
    int iCol;    /* 0 for start, 1 for stop, 2 for step */
    int iMask;   /* bitmask for those column */
    int op = pConstraint->op;
    if( op>=SQLITE_INDEX_CONSTRAINT_LIMIT
     && op<=SQLITE_INDEX_CONSTRAINT_OFFSET
    ){
      if( pConstraint->usable==0 ){
        /* do nothing */
      }else if( op==SQLITE_INDEX_CONSTRAINT_LIMIT ){
        aIdx[3] = i;
        idxNum |= 0x20;
      }else{
        assert( op==SQLITE_INDEX_CONSTRAINT_OFFSET );
        aIdx[4] = i;
        idxNum |= 0x40;
      }
      continue;
    }
    if( pConstraint->iColumn<SERIES_COLUMN_START ){
      if( (pConstraint->iColumn==SERIES_COLUMN_VALUE ||
           pConstraint->iColumn==SERIES_COLUMN_ROWID)
       && pConstraint->usable
      ){
        switch( op ){
          case SQLITE_INDEX_CONSTRAINT_EQ:
          case SQLITE_INDEX_CONSTRAINT_IS: {
            idxNum |=  0x0080;
            idxNum &= ~0x3300;
            aIdx[5] = i;
            aIdx[6] = -1;
#ifndef ZERO_ARGUMENT_GENERATE_SERIES
            bStartSeen = 1;
#endif
            break;
          }
          case SQLITE_INDEX_CONSTRAINT_GE: {
            if( idxNum & 0x0080 ) break;
            idxNum |=  0x0100;
            idxNum &= ~0x0200;
            aIdx[5] = i;
#ifndef ZERO_ARGUMENT_GENERATE_SERIES
            bStartSeen = 1;
#endif
            break;
          }
          case SQLITE_INDEX_CONSTRAINT_GT: {
            if( idxNum & 0x0080 ) break;
            idxNum |=  0x0200;
            idxNum &= ~0x0100;
            aIdx[5] = i;
#ifndef ZERO_ARGUMENT_GENERATE_SERIES
            bStartSeen = 1;
#endif
            break;
          }
          case SQLITE_INDEX_CONSTRAINT_LE: {
            if( idxNum & 0x0080 ) break;
            idxNum |=  0x1000;
            idxNum &= ~0x2000;
            aIdx[6] = i;
            break;
          }
          case SQLITE_INDEX_CONSTRAINT_LT: {
            if( idxNum & 0x0080 ) break;
            idxNum |=  0x2000;
            idxNum &= ~0x1000;
            aIdx[6] = i;
            break;
          }
        }
      }
      continue;
    }
    iCol = pConstraint->iColumn - SERIES_COLUMN_START;
    assert( iCol>=0 && iCol<=2 );
    iMask = 1 << iCol;
#ifndef ZERO_ARGUMENT_GENERATE_SERIES
    if( iCol==0 && op==SQLITE_INDEX_CONSTRAINT_EQ ){
      bStartSeen = 1;
    }
#endif
    if( pConstraint->usable==0 ){
      unusableMask |=  iMask;
      continue;
    }else if( op==SQLITE_INDEX_CONSTRAINT_EQ ){
      idxNum |= iMask;
      aIdx[iCol] = i;
    }
  }
  if( aIdx[3]==0 ){
    /* Ignore OFFSET if LIMIT is omitted */
    idxNum &= ~0x60;
    aIdx[4] = 0;
  }
  for(i=0; i<7; i++){
    if( (j = aIdx[i])>=0 ){
      pIdxInfo->aConstraintUsage[j].argvIndex = ++nArg;
      pIdxInfo->aConstraintUsage[j].omit =
         !SQLITE_SERIES_CONSTRAINT_VERIFY || i>=3;
    }
  }
  /* The current generate_column() implementation requires at least one
  ** argument (the START value).  Legacy versions assumed START=0 if the
  ** first argument was omitted.  Compile with -DZERO_ARGUMENT_GENERATE_SERIES
  ** to obtain the legacy behavior */
#ifndef ZERO_ARGUMENT_GENERATE_SERIES
  if( !bStartSeen ){
    sqlite3_free(pVTab->zErrMsg);
    pVTab->zErrMsg = sqlite3_mprintf(
        "first argument to \"generate_series()\" missing or unusable");
    return SQLITE_ERROR;
  }
#endif
  if( (unusableMask & ~idxNum)!=0 ){
    /* The start, stop, and step columns are inputs.  Therefore if there
    ** are unusable constraints on any of start, stop, or step then
    ** this plan is unusable */
    return SQLITE_CONSTRAINT;
  }
  if( (idxNum & 0x03)==0x03 ){
    /* Both start= and stop= boundaries are available.  This is the 
    ** the preferred case */
    pIdxInfo->estimatedCost = (double)(2 - ((idxNum&4)!=0));
    pIdxInfo->estimatedRows = 1000;
    if( pIdxInfo->nOrderBy>=1 && pIdxInfo->aOrderBy[0].iColumn==0 ){
      if( pIdxInfo->aOrderBy[0].desc ){
        idxNum |= 0x08;
      }else{
        idxNum |= 0x10;
      }
      pIdxInfo->orderByConsumed = 1;
    }
  }else if( (idxNum & 0x21)==0x21 ){
    /* We have start= and LIMIT */
    pIdxInfo->estimatedRows = 2500;
  }else{
    /* If either boundary is missing, we have to generate a huge span
    ** of numbers.  Make this case very expensive so that the query
    ** planner will work hard to avoid it. */
    pIdxInfo->estimatedRows = 2147483647;
  }
  pIdxInfo->idxNum = idxNum;
#ifdef SQLITE_INDEX_SCAN_HEX
  pIdxInfo->idxFlags = SQLITE_INDEX_SCAN_HEX;
#endif
  return SQLITE_OK;
}

/*
** This following structure defines all the methods for the 
** generate_series virtual table.
*/
static sqlite3_module seriesModule = {
  0,                         /* iVersion */
  0,                         /* xCreate */
  seriesConnect,             /* xConnect */
  seriesBestIndex,           /* xBestIndex */
  seriesDisconnect,          /* xDisconnect */
  0,                         /* xDestroy */
  seriesOpen,                /* xOpen - open a cursor */
  seriesClose,               /* xClose - close a cursor */
  seriesFilter,              /* xFilter - configure scan constraints */
  seriesNext,                /* xNext - advance a cursor */
  seriesEof,                 /* xEof - check for end of scan */
  seriesColumn,              /* xColumn - read data */
  seriesRowid,               /* xRowid - read data */
  0,                         /* xUpdate */
  0,                         /* xBegin */
  0,                         /* xSync */
  0,                         /* xCommit */
  0,                         /* xRollback */
  0,                         /* xFindMethod */
  0,                         /* xRename */
  0,                         /* xSavepoint */
  0,                         /* xRelease */
  0,                         /* xRollbackTo */
  0,                         /* xShadowName */
  0                          /* xIntegrity */
};

#endif /* SQLITE_OMIT_VIRTUALTABLE */

#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_series_init(
  sqlite3 *db, 
  char **pzErrMsg, 
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
#ifndef SQLITE_OMIT_VIRTUALTABLE
  if( sqlite3_libversion_number()<3008012 && pzErrMsg!=0 ){
    *pzErrMsg = sqlite3_mprintf(
        "generate_series() requires SQLite 3.8.12 or later");
    return SQLITE_ERROR;
  }
  rc = sqlite3_create_module(db, "generate_series", &seriesModule, 0);
#endif
  return rc;
}
