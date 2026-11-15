/* Area:	ffi_call, closure_call
   Purpose:	Check return values of structs with one or two floating-point
		fields.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run { xfail mips64*-*-* } } */
/* XFAIL: mips N64 ABI bug in complex-float/longdouble args and fp-struct
   returns; see libffi issue #1071.  */

#include "ffitest.h"

/* Several ABIs return these structs in floating-point registers, or with
   soft float in pairs of GPRs that differ from those used for other small
   structs, so they need their own return paths in ffi_call and in
   closures.  */

typedef struct { float a; } s_f;
typedef struct { double a; } s_d;
typedef struct { float a; float b; } s_ff;
typedef struct { double a; double b; } s_dd;
typedef struct { float a; double b; } s_fd;
typedef struct { double a; float b; } s_df;

#define DEFINE(name, eq, ...)						\
static name ABI_ATTR							\
fn_##name (int k)							\
{									\
  name r = { __VA_ARGS__ };						\
  return r;								\
}									\
									\
static void								\
cls_##name (ffi_cif *cif __UNUSED__, void *resp, void **argv,		\
	    void *userdata __UNUSED__)					\
{									\
  *(name *) resp = fn_##name (*(int *) argv[0]);			\
}									\
									\
static int								\
eq_##name (name x, name y)						\
{									\
  return eq;								\
}

#define EQ1 (x.a == y.a)
#define EQ2 (x.a == y.a && x.b == y.b)

DEFINE (s_f, EQ1, 1.5f * k)
DEFINE (s_d, EQ1, 2.5 * k)
DEFINE (s_ff, EQ2, 1.5f * k, -0.25f * k)
DEFINE (s_dd, EQ2, 2.5 * k, -0.75 * k)
DEFINE (s_fd, EQ2, 1.5f * k, -0.75 * k)
DEFINE (s_df, EQ2, 2.5 * k, -0.25f * k)

#define CHECK_ONE(name, type1, type2)					\
  do {									\
    ffi_type *elements[3] = { type1, type2, NULL };			\
    ffi_type type;							\
    ffi_type *args[1] = { &ffi_type_sint };				\
    void *values[1] = { &k };						\
    ffi_cif cif;							\
    ffi_closure *pcl;							\
    void *code;								\
    union { ffi_arg align; name value; } result;				\
    name want = fn_##name (k), got;					\
									\
    type.size = 0;							\
    type.alignment = 0;							\
    type.type = FFI_TYPE_STRUCT;					\
    type.elements = elements;						\
    CHECK (ffi_prep_cif (&cif, ABI_NUM, 1, &type, args) == FFI_OK);	\
									\
    memset (&result, 0, sizeof (result));				\
    ffi_call (&cif, FFI_FN (fn_##name), &result, values);		\
    CHECK (eq_##name (result.value, want));				\
									\
    pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);		\
    CHECK (pcl != NULL);						\
    CHECK (ffi_prep_closure_loc (pcl, &cif, cls_##name, NULL, code)	\
	   == FFI_OK);							\
    memset (&got, 0, sizeof (got));					\
    got = ((name (ABI_ATTR *)(int)) code) (k);				\
    CHECK (eq_##name (got, want));					\
    ffi_closure_free (pcl);						\
    printf ("%s: ok\n", #name);						\
  } while (0)

int
main (void)
{
  int k = 3;

  CHECK_ONE (s_f, &ffi_type_float, NULL);
  CHECK_ONE (s_d, &ffi_type_double, NULL);
  CHECK_ONE (s_ff, &ffi_type_float, &ffi_type_float);
  CHECK_ONE (s_dd, &ffi_type_double, &ffi_type_double);
  CHECK_ONE (s_fd, &ffi_type_float, &ffi_type_double);
  CHECK_ONE (s_df, &ffi_type_double, &ffi_type_float);

  exit (0);
}
