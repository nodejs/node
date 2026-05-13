//! Architecture-specific support for x86-32 without SSE2
//!
//! We use an alternative implementation on x86, because the
//! main implementation fails with the x87 FPU used by
//! debian i386, probably due to excess precision issues.
//!
//! See https://github.com/rust-lang/compiler-builtins/pull/976 for discussion on why these
//! functions are implemented in this way.

pub fn ceil(mut x: f64) -> f64 {
    unsafe {
        core::arch::asm!(
            "fld qword ptr [{x}]",
            // Save the FPU control word, using `x` as scratch space.
            "fstcw [{x}]",
            // Set rounding control to 0b10 (+∞).
            "mov word ptr [{x} + 2], 0x0b7f",
            "fldcw [{x} + 2]",
            // Round.
            "frndint",
            // Restore FPU control word.
            "fldcw [{x}]",
            // Save rounded value to memory.
            "fstp qword ptr [{x}]",
            x = in(reg) &mut x,
            // All the x87 FPU stack is used, all registers must be clobbered
            out("st(0)") _, out("st(1)") _,
            out("st(2)") _, out("st(3)") _,
            out("st(4)") _, out("st(5)") _,
            out("st(6)") _, out("st(7)") _,
            options(nostack),
        );
    }
    x
}

pub fn floor(mut x: f64) -> f64 {
    unsafe {
        core::arch::asm!(
            "fld qword ptr [{x}]",
            // Save the FPU control word, using `x` as scratch space.
            "fstcw [{x}]",
            // Set rounding control to 0b01 (-∞).
            "mov word ptr [{x} + 2], 0x077f",
            "fldcw [{x} + 2]",
            // Round.
            "frndint",
            // Restore FPU control word.
            "fldcw [{x}]",
            // Save rounded value to memory.
            "fstp qword ptr [{x}]",
            x = in(reg) &mut x,
            // All the x87 FPU stack is used, all registers must be clobbered
            out("st(0)") _, out("st(1)") _,
            out("st(2)") _, out("st(3)") _,
            out("st(4)") _, out("st(5)") _,
            out("st(6)") _, out("st(7)") _,
            options(nostack),
        );
    }
    x
}
/// Implements the exponential functions with `x87` assembly.
///
/// This relies on the instruction `f2xm1`, which computes `2^x - 1` (for
/// |x| < 1). This transcendental instruction is documented to produce results
/// with error below 1ulp (in the native double-extended precision format). This
/// translates to correctly rounded results for f32, but results in f64 may have
/// 1ulp error, which may depend on the hardware.
macro_rules! x87exp {
    ($float_ty:ident, $word_size:literal, $fn_name:ident,  $load_op:literal) => {
        pub fn $fn_name(mut x: $float_ty) -> $float_ty { unsafe {
            core::arch::asm!(
                // Prepare the register stack as
                // ```
                // st(0) = y = x*log2(base)
                // st(1) = 1.0
                // st(2) = round(y)
                // ```
                concat!($load_op, " ", $word_size, " ptr [{x}]"),
                "fld1",
                "fld st(1)",
                "frndint",
                "fxch st(2)",

                // Compare y with round(y) to determine if y is finite and
                // not an integer. If so, compute `exp2(y - round(y))` into
                // st(1). Otherwise skip ahead with `st(1) = 1.0`
                "fucom st(2)",
                "fstsw ax",
                "test ax, 0x4000",
                "jnz 2f",
                "fsub st(0), st(2)", // st(0) = y - round(y)
                "f2xm1",             // st(0) = 2^st(0) - 1.0
                "fadd st(1), st(0)", // st(1) = 1 + st(0) = exp2(y - round(y))
                "2:",

                // Finally, scale by `exp2(round(y))` and clear the stack.
                "fstp st(0)",
                "fscale",
                concat!("fstp ", $word_size, " ptr [{x}]"),
                "fstp st(0)",
                x = in(reg) &mut x,
                out("ax") _,
                out("st(0)") _, out("st(1)") _,
                out("st(2)") _, out("st(3)") _,
                out("st(4)") _, out("st(5)") _,
                out("st(6)") _, out("st(7)") _,
                options(nostack),
            );
            x
        }}
    };
}

x87exp!(f32, "dword", x87_exp2f, "fld");
x87exp!(f64, "qword", x87_exp2, "fld");
x87exp!(f32, "dword", x87_exp10f, "fldl2t\nfmul");
x87exp!(f64, "qword", x87_exp10, "fldl2t\nfmul");
x87exp!(f32, "dword", x87_expf, "fldl2e\nfmul");
x87exp!(f64, "qword", x87_exp, "fldl2e\nfmul");
