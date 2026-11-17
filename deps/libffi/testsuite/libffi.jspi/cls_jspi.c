/* Area:	closure_call
   Purpose:	Check that a closure handler can suspend with JSPI.
   Limitations:	Emscripten only, needs -sJSPI and a JSPI-enabled engine.
   PR:		none.
   Originator:	Hood Chatham 20260912	 */

/* { dg-do run { target wasm*-*-* } } */
#include "ffitest.h"
#include <emscripten.h>

/* JSPI suspends the whole wasm stack while the promise is pending. That
   only works if every frame between the JSPI entry point (main) and the
   suspending import is a wasm frame, so the closure trampoline must not
   be a JS function. Exercise each kind of trampoline epilogue: void,
   return-by-pointer (struct), i32, i64, f32, f64.  */

EM_ASYNC_JS(int, async_add_one, (int x), {
  await new Promise(resolve => setTimeout(resolve, 1));
  return x + 1;
});

static int n_calls = 0;

typedef struct { double a; int b; long long c; } big_t;

static void int_fn(ffi_cif *cif __UNUSED__, void *ret, void **args,
		   void *userdata)
{
  int x = *(int *)args[0];
  short y = *(short *)args[1];
  n_calls++;
  *(ffi_arg *)ret = async_add_one(x) * 10 + y + *(int *)userdata;
}

static void double_fn(ffi_cif *cif __UNUSED__, void *ret, void **args,
		      void *userdata __UNUSED__)
{
  n_calls++;
  *(double *)ret = async_add_one((int)*(double *)args[0]) + 0.5;
}

static void float_fn(ffi_cif *cif __UNUSED__, void *ret, void **args,
		     void *userdata __UNUSED__)
{
  n_calls++;
  *(float *)ret = async_add_one((int)*(float *)args[0]) + 0.25f;
}

static void i64_fn(ffi_cif *cif __UNUSED__, void *ret, void **args,
		   void *userdata __UNUSED__)
{
  long long x = *(long long *)args[0];
  n_calls++;
  *(long long *)ret = ((long long)async_add_one((int)x) << 40) | 7;
}

static void struct_fn(ffi_cif *cif __UNUSED__, void *ret, void **args,
		      void *userdata __UNUSED__)
{
  big_t in = *(big_t *)args[0];
  big_t *out = ret;
  n_calls++;
  out->a = in.a * 2;
  out->b = async_add_one(in.b);
  out->c = in.c + 1;
}

static void void_fn(ffi_cif *cif __UNUSED__, void *ret __UNUSED__,
		    void **args, void *userdata)
{
  n_calls++;
  *(int *)userdata = async_add_one(*(int *)args[0]);
}

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl;
  int ud = 1000;

  /* int f(int, short) */
  {
    ffi_type *cl_arg_types[] = { &ffi_type_sint, &ffi_type_sshort };
    int (*f)(int, short);
    pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2,
		       &ffi_type_sint, cl_arg_types) == FFI_OK);
    CHECK(ffi_prep_closure_loc(pcl, &cif, int_fn, &ud, code) == FFI_OK);
    f = code;
    CHECK(f(4, 3) == 50 + 3 + 1000);
    CHECK(f(-2, -1) == -10 - 1 + 1000);
    /* Nested: a suspending closure call inside the arguments of another.  */
    CHECK(f(f(0, 0) - 1000, 1) == 110 + 1 + 1000);
    ffi_closure_free(pcl);
  }

  /* double f(double) */
  {
    ffi_type *cl_arg_types[] = { &ffi_type_double };
    double (*f)(double);
    pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
		       &ffi_type_double, cl_arg_types) == FFI_OK);
    CHECK(ffi_prep_closure_loc(pcl, &cif, double_fn, NULL, code) == FFI_OK);
    f = code;
    CHECK(f(2.0) == 3.5);
    ffi_closure_free(pcl);
  }

  /* float f(float) */
  {
    ffi_type *cl_arg_types[] = { &ffi_type_float };
    float (*f)(float);
    pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
		       &ffi_type_float, cl_arg_types) == FFI_OK);
    CHECK(ffi_prep_closure_loc(pcl, &cif, float_fn, NULL, code) == FFI_OK);
    f = code;
    CHECK(f(2.0f) == 3.25f);
    ffi_closure_free(pcl);
  }

  /* long long f(long long) */
  {
    ffi_type *cl_arg_types[] = { &ffi_type_sint64 };
    long long (*f)(long long);
    pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
		       &ffi_type_sint64, cl_arg_types) == FFI_OK);
    CHECK(ffi_prep_closure_loc(pcl, &cif, i64_fn, NULL, code) == FFI_OK);
    f = code;
    CHECK(f(5) == ((6LL << 40) | 7));
    ffi_closure_free(pcl);
  }

  /* big_t f(big_t): returned through a pointer argument.  */
  {
    ffi_type *elements[] = { &ffi_type_double, &ffi_type_sint,
			     &ffi_type_sint64, NULL };
    ffi_type big = { 0, 0, FFI_TYPE_STRUCT, elements };
    ffi_type *cl_arg_types[] = { &big };
    big_t (*f)(big_t);
    big_t in = { 1.5, 10, 1LL << 33 };
    big_t out;
    pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1, &big, cl_arg_types) == FFI_OK);
    CHECK(ffi_prep_closure_loc(pcl, &cif, struct_fn, NULL, code) == FFI_OK);
    f = code;
    out = f(in);
    CHECK(out.a == 3.0);
    CHECK(out.b == 11);
    CHECK(out.c == (1LL << 33) + 1);
    ffi_closure_free(pcl);
  }

  /* void f(int) */
  {
    ffi_type *cl_arg_types[] = { &ffi_type_sint };
    void (*f)(int);
    int result = 0;
    pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
    CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
		       &ffi_type_void, cl_arg_types) == FFI_OK);
    CHECK(ffi_prep_closure_loc(pcl, &cif, void_fn, &result, code) == FFI_OK);
    f = code;
    f(41);
    CHECK(result == 42);
    ffi_closure_free(pcl);
  }

  printf("%d closure calls\n", n_calls);
  /* { dg-output "9 closure calls" } */
  exit(0);
}
