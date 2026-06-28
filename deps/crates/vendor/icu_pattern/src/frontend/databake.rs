// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use core::any::TypeId;

use crate::DoublePlaceholder;
use crate::SinglePlaceholder;

use super::*;
use ::databake::{quote, Bake, BakeSize, CrateEnv, TokenStream};

impl<B> Bake for &Pattern<B>
where
    B: PatternBackend,
    for<'b> &'b B::Store: Bake,
{
    fn bake(&self, ctx: &CrateEnv) -> TokenStream {
        ctx.insert("icu_pattern");
        let store = (&self.store).bake(ctx);
        let b = if TypeId::of::<B>() == TypeId::of::<SinglePlaceholder>() {
            quote!(icu_pattern::SinglePlaceholder)
        } else if TypeId::of::<B>() == TypeId::of::<DoublePlaceholder>() {
            quote!(icu_pattern::DoublePlaceholder)
        } else {
            unreachable!("all impls of sealed trait PatternBackend should be covered")
        };
        quote! {
            icu_pattern::Pattern::<#b>::from_ref_store_unchecked(#store)
        }
    }
}

impl<B> BakeSize for &Pattern<B>
where
    B: PatternBackend,
    for<'b> &'b B::Store: BakeSize,
{
    fn borrows_size(&self) -> usize {
        let s: &B::Store = &self.store;
        s.borrows_size()
    }
}

#[test]
fn test_baked() {
    use ::databake::test_bake;
    test_bake!(
        &Pattern<SinglePlaceholder>,
        const,
        crate::Pattern::<crate::SinglePlaceholder>::from_ref_store_unchecked(""),
        icu_pattern
    );
}
