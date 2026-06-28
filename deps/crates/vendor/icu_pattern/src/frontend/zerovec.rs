// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{Pattern, PatternBackend};

use alloc::boxed::Box;
use zerovec::{
    maps::ZeroMapKV,
    ule::{UleError, VarULE},
    VarZeroSlice, VarZeroVec,
};

impl<'a, B: PatternBackend> ZeroMapKV<'a> for Pattern<B>
where
    Pattern<B>: VarULE,
{
    type Container = VarZeroVec<'a, Pattern<B>>;
    type Slice = VarZeroSlice<Pattern<B>>;
    type GetType = Pattern<B>;
    type OwnedType = Box<Pattern<B>>;
}

/// Implement `VarULE` for `Pattern<SinglePlaceholder, str>`.
///
/// # Safety
///
/// Safety checklist for `ULE`:
///
/// 1. `Pattern<B>` does not include any uninitialized or padding bytes.
/// 2. `Pattern<B>` is aligned to 1 byte.
/// 3. The implementation of `validate_bytes()` returns an error
///    if any byte is not valid.
/// 4. The implementation of `validate_bytes()` returns an error
///    if the slice cannot be used to build a `Pattern<B>` in its entirety.
/// 5. The implementation of `from_bytes_unchecked()` returns a reference to the same data.
/// 6. `parse_bytes()` is equivalent to `validate_bytes()` followed by `from_bytes_unchecked()`.
/// 7. `Pattern<B>` byte equality is semantic equality.
unsafe impl<B, S: ?Sized + VarULE> VarULE for Pattern<B>
where
    B: PatternBackend<Store = S>,
{
    fn validate_bytes(bytes: &[u8]) -> Result<(), UleError> {
        let store = S::parse_bytes(bytes)?;
        B::validate_store(store).map_err(|_| UleError::parse::<Self>())
    }

    unsafe fn from_bytes_unchecked(bytes: &[u8]) -> &Self {
        let store = S::from_bytes_unchecked(bytes);
        Self::from_ref_store_unchecked(store)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::SinglePlaceholderPattern;
    use litemap::LiteMap;
    use zerovec::ZeroMap;

    #[test]
    fn test_zeromap() {
        let pattern =
            SinglePlaceholderPattern::try_from_str("Hello, {0}!", Default::default()).unwrap();
        let mut litemap = LiteMap::<u32, Box<SinglePlaceholderPattern>>::new_vec();
        litemap.insert(0, pattern.clone());
        let zeromap = ZeroMap::<u32, SinglePlaceholderPattern>::from_iter(litemap);
        let recovered_pattern = zeromap.get(&0).unwrap();
        assert_eq!(&*pattern, recovered_pattern);
    }
}
