#![doc(html_root_url = "https://docs.rs/wasm-bindgen-shared/0.2")]
#![no_std]

extern crate alloc;

use alloc::string::{String, ToString};

pub mod identifier;
#[cfg(test)]
mod schema_hash_approval;
pub mod tys;

// This gets changed whenever our schema changes.
// At this time versions of wasm-bindgen and wasm-bindgen-cli are required to have the exact same
// SCHEMA_VERSION in order to work together.
pub const SCHEMA_VERSION: &str = "0.2.114";

#[macro_export]
macro_rules! shared_api {
    ($mac:ident) => {
        $mac! {
        struct Program<'a> {
            exports: Vec<Export<'a>>,
            enums: Vec<Enum<'a>>,
            imports: Vec<Import<'a>>,
            structs: Vec<Struct<'a>>,
            // NOTE: Originally typescript_custom_sections are just some strings
            // But the expression type can only be parsed into a string during compilation
            // So when encoding, LitOrExpr contains two types, one is that expressions are parsed into strings during compilation, and the other is can be parsed directly.
            // When decoding, LitOrExpr can be decoded as a string.
            typescript_custom_sections: Vec<LitOrExpr<'a>>,
            local_modules: Vec<LocalModule<'a>>,
            inline_js: Vec<&'a str>,
            unique_crate_identifier: &'a str,
            package_json: Option<&'a str>,
            linked_modules: Vec<LinkedModule<'a>>,
        }

        struct Import<'a> {
            module: Option<ImportModule<'a>>,
            js_namespace: Option<Vec<String>>,
            reexport: Option<String>,
            kind: ImportKind<'a>,
        }

        struct LinkedModule<'a> {
            module: ImportModule<'a>,
            link_function_name: &'a str,
        }

        enum ImportModule<'a> {
            Named(&'a str),
            RawNamed(&'a str),
            Inline(u32),
        }

        enum ImportKind<'a> {
            Function(ImportFunction<'a>),
            Static(ImportStatic<'a>),
            String(ImportString<'a>),
            Type(ImportType<'a>),
            Enum(StringEnum<'a>),
        }

        struct ImportFunction<'a> {
            shim: &'a str,
            catch: bool,
            variadic: bool,
            assert_no_shim: bool,
            method: Option<MethodData<'a>>,
            structural: bool,
            function: Function<'a>,
        }

        struct MethodData<'a> {
            class: &'a str,
            kind: MethodKind<'a>,
        }

        enum MethodKind<'a> {
            Constructor,
            Operation(Operation<'a>),
        }

        struct Operation<'a> {
            is_static: bool,
            kind: OperationKind<'a>,
        }

        enum OperationKind<'a> {
            Regular,
            RegularThis,
            Getter(&'a str),
            Setter(&'a str),
            IndexingGetter,
            IndexingSetter,
            IndexingDeleter,
        }

        struct ImportStatic<'a> {
            name: &'a str,
            shim: &'a str,
        }

        struct ImportString<'a> {
            shim: &'a str,
            string: &'a str,
        }

        struct ImportType<'a> {
            name: &'a str,
            instanceof_shim: &'a str,
            vendor_prefixes: Vec<&'a str>,
        }

        struct StringEnum<'a> {
            name: &'a str,
            variant_values: Vec<&'a str>,
            comments: Vec<&'a str>,
            generate_typescript: bool,
            js_namespace: Option<Vec<&'a str>>,
        }

        struct Export<'a> {
            class: Option<&'a str>,
            comments: Vec<&'a str>,
            consumed: bool,
            function: Function<'a>,
            js_namespace: Option<Vec<&'a str>>,
            method_kind: MethodKind<'a>,
            start: bool,
        }

        struct Enum<'a> {
            name: &'a str,
            signed: bool,
            variants: Vec<EnumVariant<'a>>,
            comments: Vec<&'a str>,
            generate_typescript: bool,
            js_namespace: Option<Vec<&'a str>>,
            private: bool,
        }

        struct EnumVariant<'a> {
            name: &'a str,
            value: u32,
            comments: Vec<&'a str>,
        }

        struct Function<'a> {
            args: Vec<FunctionArgumentData<'a>>,
            asyncness: bool,
            name: &'a str,
            generate_typescript: bool,
            generate_jsdoc: bool,
            variadic: bool,
            ret_ty_override: Option<&'a str>,
            ret_desc: Option<&'a str>,
        }

        struct FunctionArgumentData<'a> {
            name: String,
            ty_override: Option<&'a str>,
            optional: bool,
            desc: Option<&'a str>,
        }

        struct Struct<'a> {
            name: &'a str,
            rust_name: &'a str,
            fields: Vec<StructField<'a>>,
            comments: Vec<&'a str>,
            is_inspectable: bool,
            generate_typescript: bool,
            js_namespace: Option<Vec<&'a str>>,
            private: bool,
        }

        struct StructField<'a> {
            name: &'a str,
            readonly: bool,
            comments: Vec<&'a str>,
            generate_typescript: bool,
            generate_jsdoc: bool,
        }

        struct LocalModule<'a> {
            identifier: &'a str,
            contents: &'a str,
            linked_module: bool,
        }
        }
    }; // end of mac case
} // end of mac definition

/// Compute a "qualified name" by prepending the namespace (joined with `__`) to the js_name.
/// When there is no namespace, this returns the js_name unchanged.
/// This is used to disambiguate internal wasm symbols when the same js_name
/// appears in different namespaces. `__` is used as the separator because
/// double underscores are unlikely to appear in user-defined names.
pub fn qualified_name(js_namespace: Option<&[impl AsRef<str>]>, js_name: &str) -> String {
    match js_namespace {
        Some(ns) if !ns.is_empty() => {
            let mut name = ns
                .iter()
                .map(|s| s.as_ref())
                .collect::<alloc::vec::Vec<_>>()
                .join("__");
            name.push_str("__");
            name.push_str(js_name);
            name
        }
        _ => js_name.to_string(),
    }
}

pub fn new_function(struct_name: &str) -> String {
    let mut name = "__wbg_".to_string();
    name.extend(struct_name.chars().flat_map(|s| s.to_lowercase()));
    name.push_str("_new");
    name
}

pub fn free_function(struct_name: &str) -> String {
    let mut name = "__wbg_".to_string();
    name.extend(struct_name.chars().flat_map(|s| s.to_lowercase()));
    name.push_str("_free");
    name
}

pub fn unwrap_function(struct_name: &str) -> String {
    let mut name = "__wbg_".to_string();
    name.extend(struct_name.chars().flat_map(|s| s.to_lowercase()));
    name.push_str("_unwrap");
    name
}

pub fn free_function_export_name(function_name: &str) -> String {
    function_name.to_string()
}

pub fn struct_function_export_name(struct_: &str, f: &str) -> String {
    let mut name = struct_
        .chars()
        .flat_map(|s| s.to_lowercase())
        .collect::<String>();
    name.push('_');
    name.push_str(f);
    name
}

pub fn struct_field_get(struct_: &str, f: &str) -> String {
    let mut name = String::from("__wbg_get_");
    name.extend(struct_.chars().flat_map(|s| s.to_lowercase()));
    name.push('_');
    name.push_str(f);
    name
}

pub fn struct_field_set(struct_: &str, f: &str) -> String {
    let mut name = String::from("__wbg_set_");
    name.extend(struct_.chars().flat_map(|s| s.to_lowercase()));
    name.push('_');
    name.push_str(f);
    name
}

pub fn version() -> String {
    let mut v = env!("CARGO_PKG_VERSION").to_string();
    if let Some(s) = option_env!("WBG_VERSION") {
        v.push_str(" (");
        v.push_str(s);
        v.push(')');
    }
    v
}

pub fn escape_string(s: &str) -> String {
    let mut result = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '\\' => result.push_str("\\\\"),
            '\n' => result.push_str("\\n"),
            '\r' => result.push_str("\\r"),
            '\'' => result.push_str("\\'"),
            '"' => result.push_str("\\\""),
            _ => result.push(c),
        }
    }
    result
}
