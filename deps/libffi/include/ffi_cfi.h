/* -----------------------------------------------------------------------
   ffi_cfi.h - Copyright (c) 2014  Red Hat, Inc.

   Conditionally assemble cfi directives. Only necessary for building libffi.
   ----------------------------------------------------------------------- */

#ifndef FFI_CFI_H
#define FFI_CFI_H

#ifdef HAVE_AS_CFI_PSEUDO_OP

# define cfi_startproc			.cfi_startproc
# define cfi_endproc			.cfi_endproc
# define cfi_def_cfa(reg, off)		.cfi_def_cfa reg, off
# define cfi_def_cfa_register(reg)	.cfi_def_cfa_register reg
# define cfi_def_cfa_offset(off)	.cfi_def_cfa_offset off
# define cfi_adjust_cfa_offset(off)	.cfi_adjust_cfa_offset off
# define cfi_offset(reg, off)		.cfi_offset reg, off
# define cfi_rel_offset(reg, off)	.cfi_rel_offset reg, off
# define cfi_register(r1, r2)		.cfi_register r1, r2
# define cfi_return_column(reg)		.cfi_return_column reg
# define cfi_restore(reg)		.cfi_restore reg
# define cfi_same_value(reg)		.cfi_same_value reg
# define cfi_undefined(reg)		.cfi_undefined reg
# define cfi_remember_state		.cfi_remember_state
# define cfi_restore_state		.cfi_restore_state
# define cfi_window_save		.cfi_window_save
# define cfi_personality(enc, exp)	.cfi_personality enc, exp
# define cfi_lsda(enc, exp)		.cfi_lsda enc, exp
# define cfi_escape(...)		.cfi_escape __VA_ARGS__

#else

# define cfi_startproc
# define cfi_endproc
# define cfi_def_cfa(reg, off)
# define cfi_def_cfa_register(reg)
# define cfi_def_cfa_offset(off)
# define cfi_adjust_cfa_offset(off)
# define cfi_offset(reg, off)
# define cfi_rel_offset(reg, off)
# define cfi_register(r1, r2)
# define cfi_return_column(reg)
# define cfi_restore(reg)
# define cfi_same_value(reg)
# define cfi_undefined(reg)
# define cfi_remember_state
# define cfi_restore_state
# define cfi_window_save
# define cfi_personality(enc, exp)
# define cfi_lsda(enc, exp)
# define cfi_escape(...)

#endif /* HAVE_AS_CFI_PSEUDO_OP */
#endif /* FFI_CFI_H */
