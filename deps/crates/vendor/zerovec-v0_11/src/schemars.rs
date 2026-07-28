// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{
    map::ZeroMapKV,
    ule::{AsULE, VarULE},
    vecs::VarZeroVecFormat,
    VarZeroVec, ZeroMap, ZeroSlice, ZeroVec,
};
use alloc::{borrow::Cow, format};
use schemars::JsonSchema;

impl<T: VarULE + JsonSchema + ?Sized, F: VarZeroVecFormat> JsonSchema for VarZeroVec<'_, T, F> {
    fn inline_schema() -> bool {
        true
    }

    fn schema_name() -> Cow<'static, str> {
        format!("VarZeroVec<{}>", T::schema_name()).into()
    }

    fn json_schema(generator: &mut schemars::SchemaGenerator) -> schemars::Schema {
        schemars::json_schema!({
            "type": "array",
            "items": generator.subschema_for::<T>(),
        })
    }
}

impl<'a, T: AsULE + JsonSchema> JsonSchema for ZeroVec<'a, T> {
    fn inline_schema() -> bool {
        true
    }

    fn schema_name() -> Cow<'static, str> {
        alloc::format!("ZeroVec<{}>", T::schema_name()).into()
    }

    fn json_schema(generator: &mut schemars::SchemaGenerator) -> schemars::Schema {
        schemars::json_schema!({
            "type": "array",
            "items": generator.subschema_for::<T>(),
        })
    }
}

impl<T: AsULE + JsonSchema> JsonSchema for ZeroSlice<T> {
    fn inline_schema() -> bool {
        true
    }

    fn schema_name() -> Cow<'static, str> {
        format!("ZeroSlice<{}>", T::schema_name()).into()
    }

    fn json_schema(generator: &mut schemars::SchemaGenerator) -> schemars::Schema {
        schemars::json_schema!({
            "type": "array",
            "items": generator.subschema_for::<T>(),
        })
    }
}

impl<'a, K, V> JsonSchema for ZeroMap<'a, K, V>
where
    K: ZeroMapKV<'a> + ?Sized + JsonSchema,
    V: ZeroMapKV<'a> + ?Sized + JsonSchema,
{
    fn inline_schema() -> bool {
        true
    }

    fn schema_name() -> Cow<'static, str> {
        format!("ZeroMap<{}, {}>", K::schema_name(), V::schema_name()).into()
    }

    fn json_schema(generator: &mut schemars::SchemaGenerator) -> schemars::Schema {
        // forward to the BTreeMap impl, as the impl is quite complex
        //
        // as the impl for the BTreeMap requires its arguments to be Sized, i cannot forward
        // K and V directly, but since schemars simply forwards &T impls to T, this is fine
        <alloc::collections::BTreeMap<&K, &V> as JsonSchema>::json_schema(generator)
    }
}

#[cfg(test)]
mod tests {
    use crate::{VarZeroVec, ZeroMap, ZeroSlice, ZeroVec};

    #[test]
    #[cfg(feature = "schemars")]
    fn schema_zerovec_u32() {
        let generator = schemars::SchemaGenerator::default();
        let schema = generator.into_root_schema_for::<ZeroVec<u32>>();

        insta::assert_json_snapshot!(schema);
    }

    #[test]
    #[cfg(feature = "schemars")]
    fn schema_zerovec_char() {
        let generator = schemars::SchemaGenerator::default();
        let schema = generator.into_root_schema_for::<ZeroVec<char>>();

        insta::assert_json_snapshot!(schema);
    }

    #[test]
    #[cfg(feature = "schemars")]
    fn schema_varzerovec_str() {
        let generator = schemars::SchemaGenerator::default();
        let schema = generator.into_root_schema_for::<VarZeroVec<str>>();

        insta::assert_json_snapshot!(schema);
    }

    #[test]
    #[cfg(feature = "schemars")]
    fn schema_varzerovec_zeroslice() {
        let generator = schemars::SchemaGenerator::default();
        let schema = generator.into_root_schema_for::<VarZeroVec<ZeroSlice<u32>>>();

        insta::assert_json_snapshot!(schema);
    }

    #[test]
    fn schema_zeromap_u32_str() {
        let generator = schemars::SchemaGenerator::default();
        let schema = generator.into_root_schema_for::<ZeroMap<u32, str>>();

        insta::assert_json_snapshot!(schema);
    }
}
