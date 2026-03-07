//! A representation of the Abstract Syntax Tree of a Rust program,
//! with all the added metadata necessary to generate Wasm bindings
//! for it.

use crate::{hash::ShortHash, Diagnostic};
use proc_macro2::{Ident, Span};
use std::hash::{Hash, Hasher};
use syn::Path;
use wasm_bindgen_shared as shared;

/// An abstract syntax tree representing a rust program. Contains
/// extra information for joining up this rust code with javascript.
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct Program {
    /// rust -> js interfaces
    pub exports: Vec<Export>,
    /// js -> rust interfaces
    pub imports: Vec<Import>,
    /// linked-to modules
    pub linked_modules: Vec<ImportModule>,
    /// rust enums
    pub enums: Vec<Enum>,
    /// rust structs
    pub structs: Vec<Struct>,
    /// custom typescript sections to be included in the definition file
    pub typescript_custom_sections: Vec<LitOrExpr>,
    /// Inline JS snippets
    pub inline_js: Vec<String>,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
    /// Path to js_sys
    pub js_sys: Path,
    /// Path to wasm_bindgen_futures
    pub wasm_bindgen_futures: Path,
}

impl Default for Program {
    fn default() -> Self {
        Self {
            exports: Default::default(),
            imports: Default::default(),
            linked_modules: Default::default(),
            enums: Default::default(),
            structs: Default::default(),
            typescript_custom_sections: Default::default(),
            inline_js: Default::default(),
            wasm_bindgen: syn::parse_quote! { wasm_bindgen },
            js_sys: syn::parse_quote! { js_sys },
            wasm_bindgen_futures: syn::parse_quote! { wasm_bindgen_futures },
        }
    }
}

impl Program {
    /// Name of the link function for a specific linked module
    pub fn link_function_name(&self, idx: usize) -> String {
        let hash = match &self.linked_modules[idx] {
            ImportModule::Inline(idx) => ShortHash((1, &self.inline_js[*idx])).to_string(),
            other => ShortHash((0, other)).to_string(),
        };
        format!("__wbindgen_link_{hash}")
    }
}

/// An abstract syntax tree representing a link to a module in Rust.
/// In contrast to Program, LinkToModule must expand to an expression.
/// linked_modules of the inner Program must contain exactly one element
/// whose link is produced by the expression.
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct LinkToModule(pub Program);

/// A rust to js interface. Allows interaction with rust objects/functions
/// from javascript.
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct Export {
    /// Comments extracted from the rust source.
    pub comments: Vec<String>,
    /// The rust function
    pub function: Function,
    /// The class name in JS this is attached to
    pub js_class: Option<String>,
    /// The namespace to export the item through, if any
    pub js_namespace: Option<Vec<String>>,
    /// The kind (static, named, regular)
    pub method_kind: MethodKind,
    /// The type of `self` (either `self`, `&self`, or `&mut self`)
    pub method_self: Option<MethodSelf>,
    /// The struct name, in Rust, this is attached to
    pub rust_class: Option<Ident>,
    /// The name of the rust function/method on the rust side.
    pub rust_name: Ident,
    /// Whether or not this function should be flagged as the Wasm start
    /// function.
    pub start: bool,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
    /// Path to wasm_bindgen_futures
    pub wasm_bindgen_futures: Path,
}

/// The 3 types variations of `self`.
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Copy, Clone)]
pub enum MethodSelf {
    /// `self`
    ByValue,
    /// `&mut self`
    RefMutable,
    /// `&self`
    RefShared,
}

/// Things imported from a JS module (in an `extern` block)
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct Import {
    /// The type of module being imported from, if any
    pub module: Option<ImportModule>,
    /// The namespace to access the item through, if any
    pub js_namespace: Option<Vec<String>>,
    /// If Some, this import should be re-exported with the optional given name
    pub reexport: Option<Option<String>>,
    /// The type of item being imported
    pub kind: ImportKind,
}

/// The possible types of module to import from
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub enum ImportModule {
    /// Import from the named module, with relative paths interpreted
    Named(String, Span),
    /// Import from the named module, without interpreting paths
    RawNamed(String, Span),
    /// Import from an inline JS snippet
    Inline(usize),
}

impl Hash for ImportModule {
    fn hash<H: Hasher>(&self, h: &mut H) {
        match self {
            ImportModule::Named(name, _) => (1u8, name).hash(h),
            ImportModule::Inline(idx) => (2u8, idx).hash(h),
            ImportModule::RawNamed(name, _) => (3u8, name).hash(h),
        }
    }
}

/// The type of item being imported
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub enum ImportKind {
    /// Importing a function
    Function(ImportFunction),
    /// Importing a static value
    Static(ImportStatic),
    /// Importing a static string
    String(ImportString),
    /// Importing a type/class
    Type(ImportType),
    /// Importing a JS enum
    Enum(StringEnum),
}

/// A function being imported from JS
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct ImportFunction {
    /// The full signature of the function
    pub function: Function,
    /// The name rust code will use
    pub rust_name: Ident,
    /// The type being returned
    pub js_ret: Option<syn::Type>,
    /// Whether to catch JS exceptions
    pub catch: bool,
    /// Whether the function is variadic on the JS side
    pub variadic: bool,
    /// Whether the function should use structural type checking
    pub structural: bool,
    /// Causes the Builder (See cli-support::js::binding::Builder) to error out if
    /// it finds itself generating code for a function with this signature
    pub assert_no_shim: bool,
    /// The kind of function being imported
    pub kind: ImportFunctionKind,
    /// The shim name to use in the generated code. The 'shim' is a function that appears in
    /// the generated JS as a wrapper around the actual function to import, performing any
    /// necessary conversions (EG adding a try/catch to change a thrown error into a Result)
    pub shim: Ident,
    /// The doc comment on this import, if one is provided
    pub doc_comment: String,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
    /// Path to wasm_bindgen_futures
    pub wasm_bindgen_futures: Path,
    /// Generic parameters as validated simple type parameters for this function
    pub generics: syn::Generics,
}

/// The type of a function being imported
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub enum ImportFunctionKind {
    /// A class method
    Method {
        /// The name of the class for this method, in JS
        class: String,
        /// The type of the class for this method, in Rust
        ty: syn::Type,
        /// The kind of method this is
        kind: MethodKind,
    },
    /// A standard function
    Normal,
}

/// The type of a method
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub enum MethodKind {
    /// A class constructor
    Constructor,
    /// Any other kind of method
    Operation(Operation),
}

/// The operation performed by a class method
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct Operation {
    /// Whether this method is static
    pub is_static: bool,
    /// The internal kind of this Operation
    pub kind: OperationKind,
}

/// The kind of operation performed by a method
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub enum OperationKind {
    /// A standard method, nothing special
    Regular,
    /// A free function that receives JS `this` as its first parameter
    RegularThis,
    /// A method for getting the value of the provided Ident or String
    Getter(Option<String>),
    /// A method for setting the value of the provided Ident or String
    Setter(Option<String>),
    /// A dynamically intercepted getter
    IndexingGetter,
    /// A dynamically intercepted setter
    IndexingSetter,
    /// A dynamically intercepted deleter
    IndexingDeleter,
}

/// The type of a static being imported
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct ImportStatic {
    /// The visibility of this static in Rust
    pub vis: syn::Visibility,
    /// The type of static being imported
    pub ty: syn::Type,
    /// The name of the shim function used to access this static
    pub shim: Ident,
    /// The name of this static on the Rust side
    pub rust_name: Ident,
    /// The name of this static on the JS side
    pub js_name: String,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
    /// Version of `thread_local`, if any.
    pub thread_local: Option<ThreadLocal>,
}

/// Which version of the `thread_local` attribute is enabled.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum ThreadLocal {
    /// V1.
    V1,
    /// V2.
    V2,
}

/// The type of a static string being imported
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct ImportString {
    /// The visibility of this static string in Rust
    pub vis: syn::Visibility,
    /// The type specified by the user, which we only use to show an error if the wrong type is used.
    pub ty: syn::Type,
    /// The name of the shim function used to access this static
    pub shim: Ident,
    /// The name of this static on the Rust side
    pub rust_name: Ident,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
    /// Path to js_sys
    pub js_sys: Path,
    /// The string to export.
    pub string: String,
    /// Version of `thread_local`.
    pub thread_local: ThreadLocal,
}

/// The metadata for a type being imported
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct ImportType {
    /// The visibility of this type in Rust
    pub vis: syn::Visibility,
    /// The name of this type on the Rust side
    pub rust_name: Ident,
    /// The name of this type on the JS side
    pub js_name: String,
    /// The custom attributes to apply to this type
    pub attrs: Vec<syn::Attribute>,
    /// The TS definition to generate for this type
    pub typescript_type: Option<String>,
    /// The doc comment applied to this type, if one exists
    pub doc_comment: Option<String>,
    /// The name of the shim to check instanceof for this type
    pub instanceof_shim: String,
    /// The name of the remote function to use for the generated is_type_of
    pub is_type_of: Option<syn::Expr>,
    /// The list of classes this extends, if any
    pub extends: Vec<syn::Path>,
    /// A custom prefix to add and attempt to fall back to, if the type isn't found
    pub vendor_prefixes: Vec<Ident>,
    /// If present, don't generate a `Deref` impl
    pub no_deref: bool,
    /// If present, don't generate `Upcast` impls
    pub no_upcast: bool,
    /// If present, don't generate a `Promising` impl
    pub no_promising: bool,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
    /// Validated generics
    pub generics: syn::Generics,
}

/// The metadata for a String Enum
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct StringEnum {
    /// The Rust enum's visibility
    pub vis: syn::Visibility,
    /// The Rust enum's identifiers
    pub name: Ident,
    /// The export name of this string enum in JS/TS code
    pub export_name: String,
    /// The Rust identifiers for the variants
    pub variants: Vec<Ident>,
    /// The JS string values of the variants
    pub variant_values: Vec<String>,
    /// The doc comments on this enum, if any
    pub comments: Vec<String>,
    /// Attributes to apply to the Rust enum
    pub rust_attrs: Vec<syn::Attribute>,
    /// Whether to generate a typescript definition for this enum
    pub generate_typescript: bool,
    /// The namespace to export the enum through, if any
    pub js_namespace: Option<Vec<String>>,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
}

/// Information about a function being imported or exported
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct Function {
    /// The exported name of the function
    pub name: String,
    /// The span of the function's name in Rust code
    pub name_span: Span,
    /// The arguments to the function
    pub arguments: Vec<FunctionArgumentData>,
    /// The data of return type of the function
    pub ret: Option<FunctionReturnData>,
    /// Any custom attributes being applied to the function
    pub rust_attrs: Vec<syn::Attribute>,
    /// The visibility of this function in Rust
    pub rust_vis: syn::Visibility,
    /// Whether this is an `unsafe` function
    pub r#unsafe: bool,
    /// Whether this is an `async` function
    pub r#async: bool,
    /// Whether to generate a typescript definition for this function
    pub generate_typescript: bool,
    /// Whether to generate jsdoc documentation for this function
    pub generate_jsdoc: bool,
    /// Whether this is a function with a variadict parameter
    pub variadic: bool,
}

/// Information about a function's return
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct FunctionReturnData {
    /// Specifies the type of the function's return
    pub r#type: syn::Type,
    /// Specifies the JS return type override
    pub js_type: Option<String>,
    /// Specifies the return description
    pub desc: Option<String>,
}

/// Information about a function's argument
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct FunctionArgumentData {
    /// Specifies the type of the function's argument
    pub pat_type: syn::PatType,
    /// Specifies the JS argument name override
    pub js_name: Option<String>,
    /// Specifies the JS function argument type override
    pub js_type: Option<String>,
    /// Specifies whether the parameter is optional
    pub optional: bool,
    /// Specifies the argument description
    pub desc: Option<String>,
}

/// Information about a Struct being exported
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct Struct {
    /// The name of the struct in Rust code
    pub rust_name: Ident,
    /// The export name of the struct in JS code
    pub js_name: String,
    /// The namespace-qualified internal name used for wasm symbol generation.
    /// When a namespace is present, this is `ns1_ns2_JsName`; otherwise it equals `js_name`.
    pub qualified_name: String,
    /// All the fields of this struct to export
    pub fields: Vec<StructField>,
    /// The doc comments on this struct, if provided
    pub comments: Vec<String>,
    /// Whether this struct is inspectable (provides toJSON/toString properties to JS)
    pub is_inspectable: bool,
    /// Whether to generate a typescript definition for this struct
    pub generate_typescript: bool,
    /// Whether to skip exporting this struct from the module exports
    pub private: bool,
    /// The namespace to export the struct through, if any
    pub js_namespace: Option<Vec<String>>,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
}

/// The field of a struct
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub struct StructField {
    /// The name of the field in Rust code
    pub rust_name: syn::Member,
    /// The name of the field in JS code
    pub js_name: String,
    /// The name of the struct this field is part of
    pub struct_name: Ident,
    /// Whether this value is read-only to JS
    pub readonly: bool,
    /// The type of this field
    pub ty: syn::Type,
    /// The name of the getter shim for this field
    pub getter: Ident,
    /// The name of the setter shim for this field
    pub setter: Ident,
    /// The doc comments on this field, if any
    pub comments: Vec<String>,
    /// Whether to generate a typescript definition for this field
    pub generate_typescript: bool,
    /// Whether to generate jsdoc documentation for this field
    pub generate_jsdoc: bool,
    /// The span of the `#[wasm_bindgen(getter_with_clone)]` attribute applied
    /// to this field, if any.
    ///
    /// If this is `Some`, the auto-generated getter for this field must clone
    /// the field instead of copying it.
    pub getter_with_clone: Option<Span>,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
}

/// The metadata for an Enum
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct Enum {
    /// The name of this enum in Rust code
    pub rust_name: Ident,
    /// The export name of this enum in JS code
    pub js_name: String,
    /// Whether the variant values and hole are signed, meaning that they
    /// represent the bits of a `i32` value.
    pub signed: bool,
    /// The variants provided by this enum
    pub variants: Vec<Variant>,
    /// The doc comments on this enum, if any
    pub comments: Vec<String>,
    /// The value to use for a `none` variant of the enum
    pub hole: u32,
    /// Whether to generate a typescript definition for this enum
    pub generate_typescript: bool,
    /// Whether to hide this enum from the module exports
    pub private: bool,
    /// The namespace to export the enum through, if any
    pub js_namespace: Option<Vec<String>>,
    /// Path to wasm_bindgen
    pub wasm_bindgen: Path,
}

/// The variant of an enum
#[cfg_attr(feature = "extra-traits", derive(Debug, PartialEq, Eq))]
#[derive(Clone)]
pub struct Variant {
    /// The name of this variant
    pub name: Ident,
    /// The backing value of this variant
    pub value: u32,
    /// The doc comments on this variant, if any
    pub comments: Vec<String>,
}

/// An enum representing either a literal value (`Lit`) or an expression (`syn::Expr`).
#[cfg_attr(feature = "extra-traits", derive(Debug))]
#[derive(Clone)]
pub enum LitOrExpr {
    /// Represents an expression that needs to be evaluated before it can be encoded
    Expr(syn::Expr),
    /// Represents a literal string that can be directly encoded.
    Lit(String),
}

impl Export {
    /// Mangles a rust -> javascript export, so that the created Ident will be unique over function
    /// name and class name, if the function belongs to a javascript class.
    pub(crate) fn rust_symbol(&self) -> Ident {
        let mut generated_name = String::from("__wasm_bindgen_generated");
        if let Some(class) = &self.js_class {
            generated_name.push('_');
            generated_name.push_str(class);
        }
        generated_name.push('_');
        generated_name.push_str(&self.function.name.to_string());
        Ident::new(&generated_name, Span::call_site())
    }

    /// This is the name of the shim function that gets exported and takes the raw
    /// ABI form of its arguments and converts them back into their normal,
    /// "high level" form before calling the actual function.
    pub(crate) fn export_name(&self) -> String {
        let fn_name = self.function.name.to_string();
        let base_name = match &self.js_class {
            Some(class) => shared::struct_function_export_name(class, &fn_name),
            None => shared::free_function_export_name(&fn_name),
        };

        if let Some(ns) = &self.js_namespace {
            format!("{}__{base_name}", ns.join("__"))
        } else {
            base_name
        }
    }
}

impl ImportKind {
    /// Whether this type can be inside an `impl` block.
    pub fn fits_on_impl(&self) -> bool {
        match *self {
            ImportKind::Function(_) => true,
            ImportKind::Static(_) => false,
            ImportKind::String(_) => false,
            ImportKind::Type(_) => false,
            ImportKind::Enum(_) => false,
        }
    }
}

impl Function {
    /// If the rust object has a `fn xxx(&self) -> MyType` method, get the name for a getter in
    /// javascript (in this case `xxx`, so you can write `val = obj.xxx`)
    pub fn infer_getter_property(&self) -> &str {
        &self.name
    }

    /// If the rust object has a `fn set_xxx(&mut self, MyType)` style method, get the name
    /// for a setter in javascript (in this case `xxx`, so you can write `obj.xxx = val`)
    pub fn infer_setter_property(&self) -> Result<String, Diagnostic> {
        let name = self.name.to_string();

        // Otherwise we infer names based on the Rust function name.
        if !name.starts_with("set_") {
            bail_span!(
                syn::token::Pub(self.name_span),
                "setters must start with `set_`, found: {}",
                name,
            );
        }
        Ok(name[4..].to_string())
    }
}
