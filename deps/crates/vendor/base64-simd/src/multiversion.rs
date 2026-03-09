use crate::{Config, Error};

vsimd::dispatch!(
    name        = {encode},
    signature   = {pub(crate) unsafe fn(src: *const u8, len: usize, dst: *mut u8, config: Config) -> ()},
    fallback    = {crate::encode::encode_fallback},
    simd        = {crate::encode::encode_simd},
    targets     = {"avx2", "ssse3", "neon", "simd128"},
    fastest     = {"avx2", "neon", "simd128"},
);

vsimd::dispatch!(
    name        = {decode},
    signature   = {pub(crate) unsafe fn(src: *const u8, dst: *mut u8, n: usize, config: Config) -> Result<(), Error>},
    fallback    = {crate::decode::decode_fallback},
    simd        = {crate::decode::decode_simd},
    targets     = {"avx2", "ssse3", "neon", "simd128"},
    fastest     = {"avx2", "neon", "simd128"},
);

vsimd::dispatch!(
    name        = {check},
    signature   = {pub(crate) unsafe fn(src: *const u8, n: usize, config: Config) -> Result<(), Error>},
    fallback    = {crate::check::check_fallback},
    simd        = {crate::check::check_simd},
    targets     = {"avx2", "ssse3", "neon", "simd128"},
    fastest     = {"avx2", "neon", "simd128"},
);

vsimd::dispatch!(
    name        = {find_non_ascii_whitespace},
    signature   = {pub unsafe fn(src: *const u8, len: usize) -> usize},
    fallback    = {crate::ascii::find_non_ascii_whitespace_fallback},
    simd        = {crate::ascii::find_non_ascii_whitespace_simd},
    targets     = {"avx2", "sse2", "neon", "simd128"},
    fastest     = {"avx2", "neon", "simd128"},
);
