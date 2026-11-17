/* Area:	ffi_call, closure_call
   Purpose:	Check a small struct passed on the stack and the argument
		following it.
   Limitations:	none.
   PR:		none.  */

/* { dg-do run } */
#include "ffitest.h"

/* A two-byte struct after eight ints goes to the stack on targets with
   eight argument registers, followed by a short that must land in the slot
   the ABI assigns after the struct.  */

typedef struct
{
  unsigned char a;
  unsigned char b;
} small_struct;

static int ABI_ATTR
f_stack (int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8,
	 small_struct s, short t)
{
  return i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8
	 + s.a * 10 + s.b * 100 + t * 1000;
}

#if FFI_CLOSURES
static void
cls_stack (ffi_cif *cif __UNUSED__, void *resp, void **args,
	   void *userdata __UNUSED__)
{
  *(ffi_arg *) resp = f_stack (*(int *) args[0], *(int *) args[1],
			       *(int *) args[2], *(int *) args[3],
			       *(int *) args[4], *(int *) args[5],
			       *(int *) args[6], *(int *) args[7],
			       *(small_struct *) args[8],
			       *(short *) args[9]);
}
#endif

int
main (void)
{
  ffi_cif cif;
  ffi_type small_struct_type;
  ffi_type *small_struct_elements[3];
  ffi_type *types[10];
  void *values[10];
#if FFI_CLOSURES
  ffi_closure *pcl;
  void *code;
#endif
  int ints[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  small_struct s = { 3, 5 };
  short t = 7;
  ffi_arg result;
  int i;

  small_struct_elements[0] = &ffi_type_uchar;
  small_struct_elements[1] = &ffi_type_uchar;
  small_struct_elements[2] = NULL;
  small_struct_type.size = 0;
  small_struct_type.alignment = 0;
  small_struct_type.type = FFI_TYPE_STRUCT;
  small_struct_type.elements = small_struct_elements;

  for (i = 0; i < 8; i++)
    {
      types[i] = &ffi_type_sint;
      values[i] = &ints[i];
    }
  types[8] = &small_struct_type;
  values[8] = &s;
  types[9] = &ffi_type_sshort;
  values[9] = &t;

  CHECK (ffi_prep_cif (&cif, ABI_NUM, 10, &ffi_type_sint, types) == FFI_OK);
  ffi_call (&cif, FFI_FN (f_stack), &result, values);
  printf ("stack: %d\n", (int) result);
  CHECK ((int) result == 7566);

#if FFI_CLOSURES
  pcl = ffi_closure_alloc (sizeof (ffi_closure), &code);
  CHECK (pcl != NULL);
  CHECK (ffi_prep_closure_loc (pcl, &cif, cls_stack, NULL, code) == FFI_OK);
  result = ((int (ABI_ATTR *) (int, int, int, int, int, int, int, int,
			       small_struct, short)) code)
    (1, 2, 3, 4, 5, 6, 7, 8, s, t);
  printf ("closure stack: %d\n", (int) result);
  CHECK ((int) result == 7566);
  ffi_closure_free (pcl);
#endif

  exit (0);
}
