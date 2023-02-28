/* Area:	ffi_call, closure_call
   Purpose:	Check structure alignment of long double.
   Limitations:	none.
   PR:		none.
   Originator:	<hos@tamanegi.org> 20031203	 */

/* { dg-do run { xfail strongarm*-*-* xscale*-*-* } } */
/* { dg-options -mlong-double-128 { target powerpc64*-*-linux* } } */

#include "ffitest.h"

typedef struct cls_struct_align {
  long double a;
  long double b;
  long double c;
  long double d;
  long double e;
  long double f;
  long double g;
} cls_struct_align;

cls_struct_align cls_struct_align_fn(
	cls_struct_align	a1,
	cls_struct_align	a2)
{
	struct cls_struct_align r;

	r.a = a1.a + a2.a;
	r.b = a1.b + a2.b;
	r.c = a1.c + a2.c;
	r.d = a1.d + a2.d;
	r.e = a1.e + a2.e;
	r.f = a1.f + a2.f;
	r.g = a1.g + a2.g;

	printf("%Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg %Lg: "
		"%Lg %Lg %Lg %Lg %Lg %Lg %Lg\n",
		a1.a, a1.b, a1.c, a1.d, a1.e, a1.f, a1.g,
		a2.a, a2.b, a2.c, a2.d, a2.e, a2.f, a2.g,
		r.a, r.b, r.c, r.d, r.e, r.f, r.g);

	return r;
}

cls_struct_align cls_struct_align_fn2(
	cls_struct_align	a1)
{
	struct cls_struct_align r;

	r.a = a1.a + 1;
	r.b = a1.b + 1;
	r.c = a1.c + 1;
	r.d = a1.d + 1;
	r.e = a1.e + 1;
	r.f = a1.f + 1;
	r.g = a1.g + 1;

	printf("%Lg %Lg %Lg %Lg %Lg %Lg %Lg: "
		"%Lg %Lg %Lg %Lg %Lg %Lg %Lg\n",
		a1.a, a1.b, a1.c, a1.d, a1.e, a1.f, a1.g,
		r.a, r.b, r.c, r.d, r.e, r.f, r.g);

	return r;
}

static void
cls_struct_align_gn(ffi_cif* cif __UNUSED__, void* resp, void** args, 
		    void* userdata __UNUSED__)
{
	struct cls_struct_align a1, a2;

	a1 = *(struct cls_struct_align*)(args[0]);
	a2 = *(struct cls_struct_align*)(args[1]);

	*(cls_struct_align*)resp = cls_struct_align_fn(a1, a2);
}

int main (void)
{
	ffi_cif cif;
        void *code;
	ffi_closure *pcl = ffi_closure_alloc(sizeof(ffi_closure), &code);
	void* args_dbl[3];
	ffi_type* cls_struct_fields[8];
	ffi_type cls_struct_type;
	ffi_type* dbl_arg_types[3];

	struct cls_struct_align g_dbl = { 1, 2, 3, 4, 5, 6, 7 };
	struct cls_struct_align f_dbl = { 8, 9, 10, 11, 12, 13, 14 };
	struct cls_struct_align res_dbl;

	cls_struct_type.size = 0;
	cls_struct_type.alignment = 0;
	cls_struct_type.type = FFI_TYPE_STRUCT;
	cls_struct_type.elements = cls_struct_fields;

	cls_struct_fields[0] = &ffi_type_longdouble;
	cls_struct_fields[1] = &ffi_type_longdouble;
	cls_struct_fields[2] = &ffi_type_longdouble;
	cls_struct_fields[3] = &ffi_type_longdouble;
	cls_struct_fields[4] = &ffi_type_longdouble;
	cls_struct_fields[5] = &ffi_type_longdouble;
	cls_struct_fields[6] = &ffi_type_longdouble;
	cls_struct_fields[7] = NULL;

	dbl_arg_types[0] = &cls_struct_type;
	dbl_arg_types[1] = &cls_struct_type;
	dbl_arg_types[2] = NULL;

	CHECK(ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, &cls_struct_type,
		dbl_arg_types) == FFI_OK);

	args_dbl[0] = &g_dbl;
	args_dbl[1] = &f_dbl;
	args_dbl[2] = NULL;

	ffi_call(&cif, FFI_FN(cls_struct_align_fn), &res_dbl, args_dbl);
	/* { dg-output "1 2 3 4 5 6 7 8 9 10 11 12 13 14: 9 11 13 15 17 19 21" } */
	printf("res: %Lg %Lg %Lg %Lg %Lg %Lg %Lg\n", res_dbl.a, res_dbl.b,
		res_dbl.c, res_dbl.d, res_dbl.e, res_dbl.f, res_dbl.g);
	/* { dg-output "\nres: 9 11 13 15 17 19 21" } */

	CHECK(ffi_prep_closure_loc(pcl, &cif, cls_struct_align_gn, NULL, code) == FFI_OK);

	res_dbl = ((cls_struct_align(*)(cls_struct_align, cls_struct_align))(code))(g_dbl, f_dbl);
	/* { dg-output "\n1 2 3 4 5 6 7 8 9 10 11 12 13 14: 9 11 13 15 17 19 21" } */
	printf("res: %Lg %Lg %Lg %Lg %Lg %Lg %Lg\n", res_dbl.a, res_dbl.b,
		res_dbl.c, res_dbl.d, res_dbl.e, res_dbl.f, res_dbl.g);
	/* { dg-output "\nres: 9 11 13 15 17 19 21" } */

  exit(0);
}
