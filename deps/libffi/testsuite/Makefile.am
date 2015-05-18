## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = foreign dejagnu

# Setup the testing framework, if you have one
EXPECT = `if [ -f $(top_builddir)/../expect/expect ] ; then \
            echo $(top_builddir)/../expect/expect ; \
          else echo expect ; fi`

RUNTEST = `if [ -f $(top_srcdir)/../dejagnu/runtest ] ; then \
	       echo $(top_srcdir)/../dejagnu/runtest ; \
	    else echo runtest; fi`

AM_RUNTESTFLAGS =

EXTRA_DEJAGNU_SITE_CONFIG=../local.exp

CLEANFILES = *.exe core* *.log *.sum

EXTRA_DIST = config/default.exp libffi.call/cls_19byte.c		\
libffi.call/cls_align_longdouble_split.c				\
libffi.call/closure_loc_fn0.c libffi.call/cls_schar.c			\
libffi.call/closure_fn1.c libffi.call/many2_win32.c			\
libffi.call/return_ul.c libffi.call/cls_align_double.c			\
libffi.call/return_fl2.c libffi.call/cls_1_1byte.c			\
libffi.call/cls_64byte.c libffi.call/nested_struct7.c			\
libffi.call/cls_align_sint32.c libffi.call/nested_struct2.c		\
libffi.call/ffitest.h libffi.call/nested_struct4.c			\
libffi.call/cls_multi_ushort.c libffi.call/struct3.c			\
libffi.call/cls_3byte1.c libffi.call/cls_16byte.c			\
libffi.call/struct8.c libffi.call/nested_struct8.c			\
libffi.call/cls_multi_sshort.c libffi.call/cls_3byte2.c			\
libffi.call/fastthis2_win32.c libffi.call/cls_pointer.c			\
libffi.call/err_bad_typedef.c libffi.call/cls_4_1byte.c			\
libffi.call/cls_9byte2.c libffi.call/cls_multi_schar.c			\
libffi.call/stret_medium2.c libffi.call/cls_5_1_byte.c			\
libffi.call/call.exp libffi.call/cls_double.c				\
libffi.call/cls_align_sint16.c libffi.call/cls_uint.c			\
libffi.call/return_ll1.c libffi.call/nested_struct3.c			\
libffi.call/cls_20byte1.c libffi.call/closure_fn4.c			\
libffi.call/cls_uchar.c libffi.call/struct2.c libffi.call/cls_7byte.c	\
libffi.call/strlen.c libffi.call/many.c libffi.call/testclosure.c	\
libffi.call/return_fl.c libffi.call/struct5.c				\
libffi.call/cls_12byte.c libffi.call/cls_multi_sshortchar.c		\
libffi.call/cls_align_longdouble_split2.c libffi.call/return_dbl2.c	\
libffi.call/return_fl3.c libffi.call/stret_medium.c			\
libffi.call/nested_struct6.c libffi.call/closure_fn3.c			\
libffi.call/float3.c libffi.call/many2.c				\
libffi.call/closure_stdcall.c libffi.call/cls_align_uint16.c		\
libffi.call/cls_9byte1.c libffi.call/closure_fn6.c			\
libffi.call/cls_double_va.c libffi.call/cls_align_pointer.c		\
libffi.call/cls_align_longdouble.c libffi.call/closure_fn2.c		\
libffi.call/cls_sshort.c libffi.call/many_win32.c			\
libffi.call/nested_struct.c libffi.call/cls_20byte.c			\
libffi.call/cls_longdouble.c libffi.call/cls_multi_uchar.c		\
libffi.call/return_uc.c libffi.call/closure_thiscall.c			\
libffi.call/cls_18byte.c libffi.call/cls_8byte.c			\
libffi.call/promotion.c libffi.call/struct1_win32.c			\
libffi.call/return_dbl.c libffi.call/cls_24byte.c			\
libffi.call/struct4.c libffi.call/cls_6byte.c				\
libffi.call/cls_align_uint32.c libffi.call/float.c			\
libffi.call/float1.c libffi.call/float_va.c libffi.call/negint.c	\
libffi.call/return_dbl1.c libffi.call/cls_3_1byte.c			\
libffi.call/cls_align_float.c libffi.call/return_fl1.c			\
libffi.call/nested_struct10.c libffi.call/nested_struct5.c		\
libffi.call/fastthis1_win32.c libffi.call/cls_align_sint64.c		\
libffi.call/stret_large2.c libffi.call/return_sl.c			\
libffi.call/closure_fn0.c libffi.call/cls_5byte.c			\
libffi.call/cls_2byte.c libffi.call/float2.c				\
libffi.call/cls_dbls_struct.c libffi.call/cls_sint.c			\
libffi.call/stret_large.c libffi.call/cls_ulonglong.c			\
libffi.call/cls_ushort.c libffi.call/nested_struct1.c			\
libffi.call/err_bad_abi.c libffi.call/cls_longdouble_va.c		\
libffi.call/cls_float.c libffi.call/cls_pointer_stack.c		\
libffi.call/pyobjc-tc.c libffi.call/cls_multi_ushortchar.c		\
libffi.call/struct1.c libffi.call/nested_struct9.c			\
libffi.call/huge_struct.c libffi.call/problem1.c			\
libffi.call/float4.c libffi.call/fastthis3_win32.c			\
libffi.call/return_ldl.c libffi.call/strlen2_win32.c			\
libffi.call/closure_fn5.c libffi.call/struct2_win32.c			\
libffi.call/struct6.c libffi.call/return_ll.c libffi.call/struct9.c	\
libffi.call/return_sc.c libffi.call/struct7.c				\
libffi.call/cls_align_uint64.c libffi.call/cls_4byte.c			\
libffi.call/strlen_win32.c libffi.call/cls_6_1_byte.c			\
libffi.call/cls_7_1_byte.c libffi.special/unwindtest.cc			\
libffi.special/special.exp libffi.special/unwindtest_ffi_call.cc	\
libffi.special/ffitestcxx.h lib/wrapper.exp lib/target-libpath.exp	\
lib/libffi.exp libffi.call/cls_struct_va1.c				\
libffi.call/cls_uchar_va.c libffi.call/cls_uint_va.c			\
libffi.call/cls_ulong_va.c libffi.call/cls_ushort_va.c			\
libffi.call/nested_struct11.c libffi.call/uninitialized.c		\
libffi.call/va_1.c libffi.call/va_struct1.c libffi.call/va_struct2.c	\
libffi.call/va_struct3.c

