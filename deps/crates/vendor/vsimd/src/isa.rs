use crate::{SIMD128, SIMD256, SIMD64};

pub unsafe trait InstructionSet: Copy + 'static {
    const ID: InstructionSetTypeId;
    const ARCH: bool;

    unsafe fn new() -> Self;

    fn is_enabled() -> bool;
}

#[inline(always)]
#[must_use]
pub fn detect<S: InstructionSet>() -> Option<S> {
    S::is_enabled().then(|| unsafe { S::new() })
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum InstructionSetTypeId {
    Fallback,
    SSE2,
    SSSE3,
    SSE41,
    AVX2,
    NEON,
    WASM128,
}

#[doc(hidden)]
#[inline]
#[must_use]
pub const fn matches_isa_impl<S, U>() -> bool
where
    S: InstructionSet,
    U: InstructionSet,
{
    #[allow(clippy::enum_glob_use)]
    use InstructionSetTypeId::*;

    let (self_ty, super_ty) = (S::ID, U::ID);
    let inherits = match self_ty {
        Fallback => matches!(super_ty, Fallback),
        SSE2 => matches!(super_ty, Fallback | SSE2),
        SSSE3 => matches!(super_ty, Fallback | SSE2 | SSSE3),
        SSE41 => matches!(super_ty, Fallback | SSE2 | SSSE3 | SSE41),
        AVX2 => matches!(super_ty, Fallback | SSE2 | SSSE3 | SSE41 | AVX2),
        NEON => matches!(super_ty, Fallback | NEON),
        WASM128 => matches!(super_ty, Fallback | WASM128),
    };

    S::ARCH && U::ARCH && inherits
}

#[macro_export]
macro_rules! is_isa_type {
    ($self:ident, $isa:ident) => {{
        matches!(
            <$self as $crate::isa::InstructionSet>::ID,
            <$isa as $crate::isa::InstructionSet>::ID
        )
    }};
}

#[macro_export]
macro_rules! matches_isa {
    ($self:ident, $super:ident $(| $other:ident)*) => {{
        // TODO: inline const
        use $crate::isa::InstructionSet;
        struct MatchesISA<S>(S);
        impl<S: InstructionSet> MatchesISA<S> {
            const VALUE: bool = { $crate::isa::matches_isa_impl::<S, $super>() $(||$crate::isa::matches_isa_impl::<S, $other>())* };
        }
        MatchesISA::<$self>::VALUE
    }};
}

#[derive(Debug, Clone, Copy)]
pub struct Fallback(());

unsafe impl InstructionSet for Fallback {
    const ID: InstructionSetTypeId = InstructionSetTypeId::Fallback;
    const ARCH: bool = true;

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        true
    }
}

#[allow(unused_macros)]
macro_rules! is_feature_detected {
    ($feature:tt) => {{
        #[cfg(target_feature = $feature)]
        {
            true
        }
        #[cfg(not(target_feature = $feature))]
        {
            #[cfg(feature = "detect")]
            {
                #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
                {
                    std::arch::is_x86_feature_detected!($feature)
                }
                #[cfg(target_arch = "arm")]
                {
                    std::arch::is_arm_feature_detected!($feature)
                }
                #[cfg(target_arch = "aarch64")]
                {
                    std::arch::is_aarch64_feature_detected!($feature)
                }
                #[cfg(not(any(
                    target_arch = "x86",
                    target_arch = "x86_64",
                    target_arch = "arm",
                    target_arch = "aarch64"
                )))]
                {
                    false
                }
            }
            #[cfg(not(feature = "detect"))]
            {
                false
            }
        }
    }};
}

macro_rules! x86_is_enabled {
    ($feature:tt) => {{
        #[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
        {
            is_feature_detected!($feature)
        }
        #[cfg(not(any(target_arch = "x86", target_arch = "x86_64")))]
        {
            false
        }
    }};
}

#[derive(Debug, Clone, Copy)]
pub struct SSE2(());

unsafe impl InstructionSet for SSE2 {
    const ID: InstructionSetTypeId = InstructionSetTypeId::SSE2;
    const ARCH: bool = cfg!(any(target_arch = "x86", target_arch = "x86_64"));

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        x86_is_enabled!("sse2")
    }
}

unsafe impl SIMD64 for SSE2 {}
unsafe impl SIMD128 for SSE2 {}
unsafe impl SIMD256 for SSE2 {}

#[derive(Debug, Clone, Copy)]
pub struct SSSE3(());

unsafe impl InstructionSet for SSSE3 {
    const ID: InstructionSetTypeId = InstructionSetTypeId::SSSE3;
    const ARCH: bool = cfg!(any(target_arch = "x86", target_arch = "x86_64"));

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        x86_is_enabled!("ssse3")
    }
}

unsafe impl SIMD64 for SSSE3 {}
unsafe impl SIMD128 for SSSE3 {}
unsafe impl SIMD256 for SSSE3 {}

#[derive(Debug, Clone, Copy)]
pub struct SSE41(());

unsafe impl InstructionSet for SSE41 {
    const ID: InstructionSetTypeId = InstructionSetTypeId::SSE41;
    const ARCH: bool = cfg!(any(target_arch = "x86", target_arch = "x86_64"));

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        x86_is_enabled!("sse4.1")
    }
}

unsafe impl SIMD64 for SSE41 {}
unsafe impl SIMD128 for SSE41 {}
unsafe impl SIMD256 for SSE41 {}

#[derive(Debug, Clone, Copy)]
pub struct AVX2(());

unsafe impl InstructionSet for AVX2 {
    const ID: InstructionSetTypeId = InstructionSetTypeId::AVX2;
    const ARCH: bool = cfg!(any(target_arch = "x86", target_arch = "x86_64"));

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        x86_is_enabled!("avx2")
    }
}

unsafe impl SIMD64 for AVX2 {}
unsafe impl SIMD128 for AVX2 {}
unsafe impl SIMD256 for AVX2 {}

#[allow(clippy::upper_case_acronyms)]
#[derive(Debug, Clone, Copy)]
pub struct NEON(());

unsafe impl InstructionSet for NEON {
    const ID: InstructionSetTypeId = InstructionSetTypeId::NEON;
    const ARCH: bool = cfg!(any(target_arch = "arm", target_arch = "aarch64"));

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        #[cfg(target_arch = "arm")]
        {
            #[cfg(feature = "unstable")]
            {
                is_feature_detected!("neon")
            }
            #[cfg(not(feature = "unstable"))]
            {
                false
            }
        }
        #[cfg(target_arch = "aarch64")]
        {
            is_feature_detected!("neon")
        }
        #[cfg(not(any(target_arch = "arm", target_arch = "aarch64")))]
        {
            false
        }
    }
}

unsafe impl SIMD64 for NEON {}
unsafe impl SIMD128 for NEON {}
unsafe impl SIMD256 for NEON {}

#[derive(Debug, Clone, Copy)]
pub struct WASM128(());

unsafe impl InstructionSet for WASM128 {
    const ID: InstructionSetTypeId = InstructionSetTypeId::WASM128;
    const ARCH: bool = cfg!(target_arch = "wasm32");

    #[inline(always)]
    unsafe fn new() -> Self {
        Self(())
    }

    #[inline(always)]
    fn is_enabled() -> bool {
        #[cfg(target_arch = "wasm32")]
        {
            is_feature_detected!("simd128")
        }
        #[cfg(not(target_arch = "wasm32"))]
        {
            false
        }
    }
}

unsafe impl SIMD64 for WASM128 {}
unsafe impl SIMD128 for WASM128 {}
unsafe impl SIMD256 for WASM128 {}
