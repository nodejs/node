/* Area:	closure_call (thiscall convention)
   Purpose:	Check handling when caller expects thiscall callee
   Limitations:	none.
   PR:		none.
   Originator:	<ktietz@redhat.com> */

/* { dg-do run { target i?86-*-cygwin* i?86-*-mingw* } } */
#include "ffitest.h"

static void
closure_test_thiscall(ffi_cif* cif __UNUSED__, void* resp, void** args,
		      void* userdata)
{
  *(ffi_arg*)resp =
    (int)*(int *)args[0] + (int)(*(int *)args[1])
    + (int)(*(int *)args[2])  + (int)(*(int *)args[3])
    + (int)(intptr_t)userdata;

  printf("%d %d %d %d: %d\n",
	 (int)*(int *)args[0], (int)(*(int *)args[1]),
	 (int)(*(int *)args[2]), (int)(*(int *)args[3]),
         (int)*(ffi_arg *)resp);

}

typedef int (__thiscall *closure_test_type0)(int, int, int, int);

int main (void)
{
  ffi_cif cif;
  void *code;
  ffi_closure *pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
  ffi_type * cl_arg_types[17];
  int res;
  void* sp_pre;
  void* sp_post;
  char buf[1024];

  cl_arg_types[0] = &ffi_type_uint;
  cl_arg_types[1] = &ffi_type_uint;
  cl_arg_types[2] = &ffi_type_uint;
  cl_arg_types[3] = &ffi_type_uint;
  cl_arg_types[4] = NULL;

  /* Initialize the cif */
  CHECK(ffi_prep_cif(&cif, FFI_THISCALL, 4,
		     &ffi_type_sint, cl_arg_types) == FFI_OK);

  CHECK(ffi_prep_closure_loc(pcl, &cif, closure_test_thiscall,
                             (void *) 3 /* userdata */, code) == FFI_OK);

#ifdef _MSC_VER
  __asm { mov sp_pre, esp }
#else
  asm volatile (" movl %%esp,%0" : "=g" (sp_pre));
#endif
  res = (*(closure_test_type0)code)(0, 1, 2, 3);
#ifdef _MSC_VER
  __asm { mov sp_post, esp }
#else
  asm volatile (" movl %%esp,%0" : "=g" (sp_post));
#endif
  /* { dg-output "0 1 2 3: 9" } */

  printf("res: %d\n",res);
  /* { dg-output "\nres: 9" } */

  sprintf(buf, "mismatch: pre=%p vs post=%p", sp_pre, sp_post);
  printf("stack pointer %s\n", (sp_pre == sp_post ? "match" : buf));
  /* { dg-output "\nstack pointer match" } */
  exit(0);
}
