use serde::{Deserialize, Serialize};

/// Alternative for https://babeljs.io/docs/en/assumptions
///
/// All fields default to `false`.
#[derive(
    Debug, Default, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize,
)]
#[non_exhaustive]
#[serde(rename_all = "camelCase", deny_unknown_fields)]
pub struct Assumptions {
    /// https://babeljs.io/docs/en/assumptions#arraylikeisiterable
    #[serde(default)]
    pub array_like_is_iterable: bool,

    /// https://babeljs.io/docs/en/assumptions#constantreexports
    #[serde(default)]
    pub constant_reexports: bool,

    /// https://babeljs.io/docs/en/assumptions#constantsuper
    #[serde(default)]
    pub constant_super: bool,

    /// https://babeljs.io/docs/en/assumptions#enumerablemodulemeta
    #[serde(default)]
    pub enumerable_module_meta: bool,

    /// https://babeljs.io/docs/en/assumptions#ignorefunctionlength
    #[serde(default)]
    pub ignore_function_length: bool,

    #[serde(default)]
    pub ignore_function_name: bool,

    /// https://babeljs.io/docs/en/assumptions#ignoretoprimitivehint
    #[serde(default)]
    pub ignore_to_primitive_hint: bool,

    /// https://babeljs.io/docs/en/assumptions#iterableisarray
    #[serde(default)]
    pub iterable_is_array: bool,

    /// https://babeljs.io/docs/en/assumptions#mutabletemplateobject
    #[serde(default)]
    pub mutable_template_object: bool,

    /// https://babeljs.io/docs/en/assumptions#noclasscalls
    #[serde(default)]
    pub no_class_calls: bool,

    /// https://babeljs.io/docs/en/assumptions#nodocumentall
    #[serde(default)]
    pub no_document_all: bool,

    /// https://babeljs.io/docs/en/assumptions#noincompletensimportdetection
    #[serde(default)]
    pub no_incomplete_ns_import_detection: bool,

    /// https://babeljs.io/docs/en/assumptions#nonewarrows
    #[serde(default)]
    pub no_new_arrows: bool,

    /// https://babeljs.io/docs/en/assumptions#objectrestnosymbols
    #[serde(default)]
    pub object_rest_no_symbols: bool,

    /// https://babeljs.io/docs/en/assumptions#privatefieldsasproperties
    #[serde(default)]
    pub private_fields_as_properties: bool,

    /// https://babeljs.io/docs/en/assumptions#puregetters
    #[serde(default)]
    pub pure_getters: bool,

    /// https://babeljs.io/docs/en/assumptions#setclassmethods
    #[serde(default)]
    pub set_class_methods: bool,

    /// https://babeljs.io/docs/en/assumptions#setcomputedproperties
    #[serde(default)]
    pub set_computed_properties: bool,

    /// https://babeljs.io/docs/en/assumptions#setpublicclassfields
    #[serde(default)]
    pub set_public_class_fields: bool,

    /// https://babeljs.io/docs/en/assumptions#setspreadproperties
    #[serde(default)]
    pub set_spread_properties: bool,

    /// https://babeljs.io/docs/en/assumptions#skipforofiteratorclosing
    #[serde(default)]
    pub skip_for_of_iterator_closing: bool,

    /// https://babeljs.io/docs/en/assumptions#superiscallableconstructor
    #[serde(default)]
    pub super_is_callable_constructor: bool,
}

#[allow(deprecated)]
impl Assumptions {
    pub fn all() -> Self {
        Self {
            array_like_is_iterable: true,
            constant_reexports: true,
            constant_super: true,
            enumerable_module_meta: true,
            ignore_function_length: true,
            ignore_function_name: true,
            ignore_to_primitive_hint: true,
            iterable_is_array: true,
            mutable_template_object: true,
            no_class_calls: true,
            no_document_all: true,
            no_incomplete_ns_import_detection: true,
            no_new_arrows: true,
            object_rest_no_symbols: true,
            private_fields_as_properties: true,
            pure_getters: true,
            set_class_methods: true,
            set_computed_properties: true,
            set_public_class_fields: true,
            set_spread_properties: true,
            skip_for_of_iterator_closing: true,
            super_is_callable_constructor: true,
        }
    }
}
