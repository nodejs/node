/* Area:	ffi_call
   Purpose:	Check int128 call and return.
   Limitations:	none.
   PR:		none. */

/* { dg-do run } */
#include "ffitest.h"

#if defined(FFI_TARGET_HAS_INT128) && defined(__SIZEOF_INT128__)

typedef __int128_t i128;

static const i128 val = ((i128)0x01020304050607ull << 64) | 0x08090a0b0c0d0e0full;
static const int dummy = 0xdeadbeef;

#define D(X) int X __attribute__((unused))

static i128 f0(i128 x)
{
  return x;
}

static i128 f1(D(a), i128 x)
{
  return x;
}

static i128 f2(D(a), D(b), i128 x)
{
  return x;
}

static i128 f3(D(a), D(b), D(c), i128 x)
{
  return x;
}

static i128 f4(D(a), D(b), D(c), D(d), i128 x)
{
  return x;
}

static i128 f5(D(a), D(b), D(c), D(d), D(e), i128 x)
{
  return x;
}

static i128 f6(D(a), D(b), D(c), D(d), D(e), D(f), i128 x)
{
  return x;
}

static i128 f7(D(a), D(b), D(c), D(d), D(e), D(f), D(g), i128 x)
{
  return x;
}

static i128 f8(D(a), D(b), D(c), D(d), D(e), D(f), D(g), D(h), i128 x)
{
  return x;
}

#define N  9

static void * const funcs[N] = {
  f0, f1, f2, f3, f4, f5, f6, f7, f8
};

int main (void)
{
  int i;

  for (i = 0; i < N; i++)
    {
      ffi_cif cif;
      ffi_status s;
      ffi_type *args[N];
      void *values[N];
      i128 ret;
      int j;

      for (j = 0; j < i; j++)
        {
          args[j] = &ffi_type_sint;
          values[j] = (void *)&dummy;
        }
      args[i] = &ffi_type_sint128;
      values[i] = (void *)&val;

      s = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, i + 1,
		       &ffi_type_sint128, args);
      CHECK(s == FFI_OK);

      ffi_call(&cif, FFI_FN(funcs[i]), &ret, values);
      CHECK(ret == val);
    }

  return 0;
}

#else
int main (void) { return 0; }
#endif
