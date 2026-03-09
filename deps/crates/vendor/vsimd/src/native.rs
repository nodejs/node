#[derive(Debug, Clone, Copy)]
pub struct Native(Arch);

#[derive(Debug, Clone, Copy)]
enum Arch {
    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    Avx2,

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    Sse41,

    #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
    Sse2,

    #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
    Neon,

    #[cfg(target_arch = "wasm32")]
    Simd128,

    Fallback,
}

impl Native {
    #[inline]
    #[must_use]
    pub fn detect() -> Self {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        {
            if is_feature_detected!("avx2") {
                return Self(Arch::Avx2);
            }

            if is_feature_detected!("sse4.1") {
                return Self(Arch::Sse41);
            }

            if is_feature_detected!("sse2") {
                return Self(Arch::Sse2);
            }
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        {
            if is_feature_detected!("neon") {
                return Self(Arch::Neon);
            }
        }
        #[cfg(target_arch = "wasm32")]
        {
            if is_feature_detected!("simd128") {
                return Self(Arch::Simd128);
            }
        }
        Self(Arch::Fallback)
    }

    #[inline]
    pub fn exec<F, O>(self, f: F) -> O
    where
        F: FnOnce() -> O,
    {
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        {
            match self.0 {
                Arch::Avx2 => unsafe { x86::avx2(f) },
                Arch::Sse41 => unsafe { x86::sse41(f) },
                Arch::Sse2 => unsafe { x86::sse2(f) },
                Arch::Fallback => f(),
            }
        }
        #[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
        {
            match self.0 {
                Arch::Neon => unsafe { arm::neon(f) },
                Arch::Fallback => f(),
            }
        }
        #[cfg(target_arch = "wasm32")]
        {
            match self.0 {
                Arch::Simd128 => unsafe { wasm::simd128(f) },
                Arch::Fallback => f(),
            }
        }
        #[cfg(not(any( //
            any(target_arch = "x86", target_arch = "x86_64"), //
            any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"), //
            target_arch = "wasm32" //
        )))]
        {
            f()
        }
    }
}

#[allow(unused_macros)]
macro_rules! generic_dispatch {
    ($name: ident, $feature: tt) => {
        #[inline]
        #[target_feature(enable = $feature)]
        pub unsafe fn $name<F, O>(f: F) -> O
        where
            F: FnOnce() -> O,
        {
            f()
        }
    };
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
mod x86 {
    generic_dispatch!(avx2, "avx2");
    generic_dispatch!(sse41, "sse4.1");
    generic_dispatch!(sse2, "sse2");
}

#[cfg(any(all(feature = "unstable", target_arch = "arm"), target_arch = "aarch64"))]
mod arm {
    generic_dispatch!(neon, "neon");
}

#[cfg(target_arch = "wasm32")]
mod wasm {
    generic_dispatch!(simd128, "simd128");
}
