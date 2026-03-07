use crate::ast;
use crate::encode;
use crate::encode::EncodeChunk;
use crate::generics::{self, generic_to_concrete};
use crate::Diagnostic;
use proc_macro2::{Ident, Span, TokenStream};
use quote::format_ident;
use quote::quote_spanned;
use quote::{quote, ToTokens};
use std::borrow::Cow;
use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use syn::parse_quote;
use syn::spanned::Spanned;
use syn::{Attribute, Meta, MetaList};
use wasm_bindgen_shared as shared;

/// A trait for converting AST structs into Tokens and adding them to a TokenStream,
/// or providing a diagnostic if conversion fails.
pub trait TryToTokens {
    /// Attempt to convert a `Self` into tokens and add it to the `TokenStream`
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic>;

    /// Attempt to convert a `Self` into a new `TokenStream`
    fn try_to_token_stream(&self) -> Result<TokenStream, Diagnostic> {
        let mut tokens = TokenStream::new();
        self.try_to_tokens(&mut tokens)?;
        Ok(tokens)
    }
}

impl TryToTokens for ast::Program {
    // Generate wrappers for all the items that we've found
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
        let mut errors = Vec::new();
        for export in self.exports.iter() {
            if let Err(e) = export.try_to_tokens(tokens) {
                errors.push(e);
            }
        }
        for s in self.structs.iter() {
            s.to_tokens(tokens);
        }
        let mut types = HashMap::new();
        for i in self.imports.iter() {
            if let ast::ImportKind::Type(t) = &i.kind {
                types.insert(t.rust_name.to_string(), t.rust_name.clone());
            }
        }
        for i in self.imports.iter() {
            DescribeImport {
                kind: &i.kind,
                wasm_bindgen: &self.wasm_bindgen,
            }
            .try_to_tokens(tokens)?;

            // If there is a js namespace, check that name isn't a type. If it is,
            // this import might be a method on that type.
            if let Some(nss) = &i.js_namespace {
                // When the namespace is `A.B`, the type name should be `B`.
                if let Some(ns) = nss.last().and_then(|t| types.get(t)) {
                    if i.kind.fits_on_impl() {
                        let kind = match i.kind.try_to_token_stream() {
                            Ok(kind) => kind,
                            Err(e) => {
                                errors.push(e);
                                continue;
                            }
                        };
                        (quote! {
                            #[automatically_derived]
                            impl #ns { #kind }
                        })
                        .to_tokens(tokens);
                        continue;
                    }
                }
            }

            if let Err(e) = i.kind.try_to_tokens(tokens) {
                errors.push(e);
            }
        }
        for e in self.enums.iter() {
            e.to_tokens(tokens);
        }

        Diagnostic::from_vec(errors)?;

        // Generate a static which will eventually be what lives in a custom section
        // of the Wasm executable. For now it's just a plain old static, but we'll
        // eventually have it actually in its own section.

        // See comments in `crates/cli-support/src/lib.rs` about what this
        // `schema_version` is.
        let prefix_json = format!(
            r#"{{"schema_version":"{}","version":"{}"}}"#,
            shared::SCHEMA_VERSION,
            shared::version()
        );

        let wasm_bindgen = &self.wasm_bindgen;

        let encoded = encode::encode(self)?;

        let encoded_chunks: Vec<_> = encoded
            .custom_section
            .iter()
            .map(|chunk| match chunk {
                EncodeChunk::EncodedBuf(buf) => {
                    let buf = syn::LitByteStr::new(buf.as_slice(), Span::call_site());
                    quote!(#buf)
                }
                EncodeChunk::StrExpr(expr) => {
                    // encode expr as str
                    quote!({
                        use #wasm_bindgen::__rt::{encode_u32_to_fixed_len_bytes};
                        const _STR_EXPR: &str = #expr;
                        const _STR_EXPR_BYTES: &[u8] = _STR_EXPR.as_bytes();
                        const _STR_EXPR_BYTES_LEN: usize = _STR_EXPR_BYTES.len() + 5;
                        const _ENCODED_BYTES: [u8; _STR_EXPR_BYTES_LEN] = flat_byte_slices([
                            &encode_u32_to_fixed_len_bytes(_STR_EXPR_BYTES.len() as u32),
                            _STR_EXPR_BYTES,
                        ]);
                        &_ENCODED_BYTES
                    })
                }
            })
            .collect();

        let chunk_len = encoded_chunks.len();

        // concatenate all encoded chunks and write the length in front of the chunk;
        let encode_bytes = quote!({
            const _CHUNK_SLICES: [&[u8]; #chunk_len] = [
                #(#encoded_chunks,)*
            ];
            #[allow(long_running_const_eval)]
            const _CHUNK_LEN: usize = flat_len(_CHUNK_SLICES);
            #[allow(long_running_const_eval)]
            const _CHUNKS: [u8; _CHUNK_LEN] = flat_byte_slices(_CHUNK_SLICES);

            const _LEN_BYTES: [u8; 4] = (_CHUNK_LEN as u32).to_le_bytes();
            const _ENCODED_BYTES_LEN: usize = _CHUNK_LEN + 4;
            #[allow(long_running_const_eval)]
            const _ENCODED_BYTES: [u8; _ENCODED_BYTES_LEN] = flat_byte_slices([&_LEN_BYTES, &_CHUNKS]);
            &_ENCODED_BYTES
        });

        // We already consumed the contents of included files when generating
        // the custom section, but we want to make sure that updates to the
        // generated files will cause this macro to rerun incrementally. To do
        // that we use `include_str!` to force rustc to think it has a
        // dependency on these files. That way when the file changes Cargo will
        // automatically rerun rustc which will rerun this macro. Other than
        // this we don't actually need the results of the `include_str!`, so
        // it's just shoved into an anonymous static.
        let file_dependencies = encoded.included_files.iter().map(|file| {
            let file = file.to_str().unwrap();
            quote! { include_str!(#file) }
        });

        let len = prefix_json.len() as u32;
        let prefix_json_bytes = [&len.to_le_bytes()[..], prefix_json.as_bytes()].concat();
        let prefix_json_bytes = syn::LitByteStr::new(&prefix_json_bytes, Span::call_site());

        (quote! {
            #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
            #[automatically_derived]
            const _: () = {
                use #wasm_bindgen::__rt::{flat_len, flat_byte_slices};

                static _INCLUDED_FILES: &[&str] = &[#(#file_dependencies),*];

                const _ENCODED_BYTES: &[u8] = #encode_bytes;
                const _PREFIX_JSON_BYTES: &[u8] = #prefix_json_bytes;
                const _ENCODED_BYTES_LEN: usize  = _ENCODED_BYTES.len();
                const _PREFIX_JSON_BYTES_LEN: usize =  _PREFIX_JSON_BYTES.len();
                const _LEN: usize = _PREFIX_JSON_BYTES_LEN + _ENCODED_BYTES_LEN;

                #[link_section = "__wasm_bindgen_unstable"]
                #[allow(long_running_const_eval)]
                static _GENERATED: [u8; _LEN] = flat_byte_slices([_PREFIX_JSON_BYTES, _ENCODED_BYTES]);
            };
        })
        .to_tokens(tokens);

        Ok(())
    }
}

impl TryToTokens for ast::LinkToModule {
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
        let mut program_tokens = TokenStream::new();
        self.0.try_to_tokens(&mut program_tokens)?;
        let link_function_name = self.0.link_function_name(0);
        let name = Ident::new(&link_function_name, Span::call_site());
        let wasm_bindgen = &self.0.wasm_bindgen;
        let abi_ret = quote! { #wasm_bindgen::convert::WasmRet<<#wasm_bindgen::__rt::alloc::string::String as #wasm_bindgen::convert::FromWasmAbi>::Abi> };
        let extern_fn = extern_fn(&name, &[], &[], &[], abi_ret);
        (quote! {
            {
                #program_tokens
                #extern_fn

                static __VAL: #wasm_bindgen::__rt::LazyLock<#wasm_bindgen::__rt::alloc::string::String> =
                    #wasm_bindgen::__rt::LazyLock::new(|| unsafe {
                        <#wasm_bindgen::__rt::alloc::string::String as #wasm_bindgen::convert::FromWasmAbi>::from_abi(#name().join())
                    });

                #wasm_bindgen::__rt::alloc::string::String::clone(&__VAL)
            }
        })
        .to_tokens(tokens);
        Ok(())
    }
}

impl ToTokens for ast::Struct {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let name = &self.rust_name;
        let name_str = self.qualified_name.to_string();
        let name_len = name_str.len() as u32;
        let name_chars: Vec<u32> = name_str.chars().map(|c| c as u32).collect();
        let new_fn = Ident::new(&shared::new_function(&name_str), Span::call_site());
        let free_fn = Ident::new(&shared::free_function(&name_str), Span::call_site());
        let unwrap_fn = Ident::new(&shared::unwrap_function(&name_str), Span::call_site());
        let wasm_bindgen = &self.wasm_bindgen;
        (quote! {
            #[automatically_derived]
            impl #wasm_bindgen::__rt::marker::SupportsConstructor for #name {}
            #[automatically_derived]
            impl #wasm_bindgen::__rt::marker::SupportsInstanceProperty for #name {}
            #[automatically_derived]
            impl #wasm_bindgen::__rt::marker::SupportsStaticProperty for #name {}

            #[automatically_derived]
            impl #wasm_bindgen::describe::WasmDescribe for #name {
                fn describe() {
                    use #wasm_bindgen::describe::*;
                    inform(RUST_STRUCT);
                    inform(#name_len);
                    #(inform(#name_chars);)*
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::IntoWasmAbi for #name {
                type Abi = u32;

                fn into_abi(self) -> u32 {
                    use #wasm_bindgen::__rt::alloc::rc::Rc;
                    use #wasm_bindgen::__rt::WasmRefCell;
                    Rc::into_raw(Rc::new(WasmRefCell::new(self))) as u32
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::FromWasmAbi for #name {
                type Abi = u32;

                unsafe fn from_abi(js: u32) -> Self {
                    use #wasm_bindgen::__rt::alloc::rc::Rc;
                    use #wasm_bindgen::__rt::core::result::Result::{Ok, Err};
                    use #wasm_bindgen::__rt::{assert_not_null, WasmRefCell};

                    let ptr = js as *mut WasmRefCell<#name>;
                    assert_not_null(ptr);
                    let rc = Rc::from_raw(ptr);
                    match Rc::try_unwrap(rc) {
                        Ok(cell) => cell.into_inner(),
                        Err(_) => #wasm_bindgen::throw_str(
                            "attempted to take ownership of Rust value while it was borrowed"
                        ),
                    }
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::__rt::core::convert::From<#name> for
                #wasm_bindgen::JsValue
            {
                fn from(value: #name) -> Self {
                    let ptr = #wasm_bindgen::convert::IntoWasmAbi::into_abi(value);

                    #[link(wasm_import_module = "__wbindgen_placeholder__")]
                    #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
                    extern "C" {
                        fn #new_fn(ptr: u32) -> u32;
                    }

                    #[cfg(not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none"))))]
                    unsafe fn #new_fn(_: u32) -> u32 {
                        panic!("cannot convert to JsValue outside of the Wasm target")
                    }

                    unsafe {
                        <#wasm_bindgen::JsValue as #wasm_bindgen::convert::FromWasmAbi>
                            ::from_abi(#new_fn(ptr))
                    }
                }
            }

            #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
            #[automatically_derived]
            const _: () = {
                #wasm_bindgen::__wbindgen_coverage! {
                #[no_mangle]
                #[doc(hidden)]
                // `allow_delayed` is whether it's ok to not actually free the `ptr` immediately
                // if it's still borrowed.
                pub unsafe extern "C-unwind" fn #free_fn(ptr: u32, allow_delayed: u32) {
                    use #wasm_bindgen::__rt::alloc::rc::Rc;

                    if allow_delayed != 0 {
                        // Just drop the implicit `Rc` owned by JS, and then if the value is still
                        // referenced it'll be kept alive by its other `Rc`s.
                        let ptr = ptr as *mut #wasm_bindgen::__rt::WasmRefCell<#name>;
                        #wasm_bindgen::__rt::assert_not_null(ptr);
                        drop(Rc::from_raw(ptr));
                    } else {
                        // Claim ownership of the value, which will panic if it's borrowed.
                        let _ = <#name as #wasm_bindgen::convert::FromWasmAbi>::from_abi(ptr);
                    }
                }
                }
            };

            #[automatically_derived]
            impl #wasm_bindgen::convert::RefFromWasmAbi for #name {
                type Abi = u32;
                type Anchor = #wasm_bindgen::__rt::RcRef<#name>;

                unsafe fn ref_from_abi(js: Self::Abi) -> Self::Anchor {
                    use #wasm_bindgen::__rt::alloc::rc::Rc;

                    let js = js as *mut #wasm_bindgen::__rt::WasmRefCell<#name>;
                    #wasm_bindgen::__rt::assert_not_null(js);

                    Rc::increment_strong_count(js);
                    let rc = Rc::from_raw(js);
                    #wasm_bindgen::__rt::RcRef::new(rc)
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::RefMutFromWasmAbi for #name {
                type Abi = u32;
                type Anchor = #wasm_bindgen::__rt::RcRefMut<#name>;

                unsafe fn ref_mut_from_abi(js: Self::Abi) -> Self::Anchor {
                    use #wasm_bindgen::__rt::alloc::rc::Rc;

                    let js = js as *mut #wasm_bindgen::__rt::WasmRefCell<#name>;
                    #wasm_bindgen::__rt::assert_not_null(js);

                    Rc::increment_strong_count(js);
                    let rc = Rc::from_raw(js);
                    #wasm_bindgen::__rt::RcRefMut::new(rc)
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::LongRefFromWasmAbi for #name {
                type Abi = u32;
                type Anchor = #wasm_bindgen::__rt::RcRef<#name>;

                unsafe fn long_ref_from_abi(js: Self::Abi) -> Self::Anchor {
                    <Self as #wasm_bindgen::convert::RefFromWasmAbi>::ref_from_abi(js)
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::OptionIntoWasmAbi for #name {
                #[inline]
                fn none() -> Self::Abi { 0 }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::OptionFromWasmAbi for #name {
                #[inline]
                fn is_none(abi: &Self::Abi) -> bool { *abi == 0 }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::TryFromJsValue for #name {
                fn try_from_js_value(value: #wasm_bindgen::JsValue) -> #wasm_bindgen::__rt::core::result::Result<Self, #wasm_bindgen::JsValue> {
                    Self::try_from_js_value_ref(&value).ok_or(value)
                }
                fn try_from_js_value_ref(value: &#wasm_bindgen::JsValue) -> #wasm_bindgen::__rt::core::option::Option<Self> {
                    let idx = #wasm_bindgen::convert::IntoWasmAbi::into_abi(value);

                    #[link(wasm_import_module = "__wbindgen_placeholder__")]
                    #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
                    extern "C" {
                        fn #unwrap_fn(ptr: u32) -> u32;
                    }

                    #[cfg(not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none"))))]
                    unsafe fn #unwrap_fn(_: u32) -> u32 {
                        panic!("cannot convert from JsValue outside of the Wasm target")
                    }

                    let ptr = unsafe { #unwrap_fn(idx) };
                    if ptr == 0 {
                        #wasm_bindgen::__rt::core::option::Option::None
                    } else {
                        unsafe {
                            #wasm_bindgen::__rt::core::option::Option::Some(
                                <Self as #wasm_bindgen::convert::FromWasmAbi>::from_abi(ptr)
                            )
                        }
                    }
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::describe::WasmDescribeVector for #name {
                fn describe_vector() {
                    use #wasm_bindgen::describe::*;
                    inform(VECTOR);
                    inform(NAMED_EXTERNREF);
                    inform(#name_len);
                    #(inform(#name_chars);)*
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::VectorIntoWasmAbi for #name {
                type Abi = <
                    #wasm_bindgen::__rt::alloc::boxed::Box<[#wasm_bindgen::JsValue]>
                    as #wasm_bindgen::convert::IntoWasmAbi
                >::Abi;

                fn vector_into_abi(
                    vector: #wasm_bindgen::__rt::alloc::boxed::Box<[#name]>
                ) -> Self::Abi {
                    #wasm_bindgen::convert::js_value_vector_into_abi(vector)
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::VectorFromWasmAbi for #name {
                type Abi = <
                    #wasm_bindgen::__rt::alloc::boxed::Box<[#wasm_bindgen::JsValue]>
                    as #wasm_bindgen::convert::FromWasmAbi
                >::Abi;

                unsafe fn vector_from_abi(
                    js: Self::Abi
                ) -> #wasm_bindgen::__rt::alloc::boxed::Box<[#name]> {
                    #wasm_bindgen::convert::js_value_vector_from_abi(js)
                }
            }
        })
        .to_tokens(tokens);

        for field in self.fields.iter() {
            field.to_tokens(tokens);
        }
    }
}

impl ToTokens for ast::StructField {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let rust_name = &self.rust_name;
        let struct_name = &self.struct_name;
        let ty = &self.ty;
        let getter = &self.getter;
        let setter = &self.setter;

        let maybe_assert_copy = if self.getter_with_clone.is_some() {
            quote! {}
        } else {
            quote! { assert_copy::<#ty>() }
        };
        let maybe_assert_copy = respan(maybe_assert_copy, ty);

        // Split this out so that it isn't affected by `quote_spanned!`.
        //
        // If we don't do this, it might end up being unable to reference `js`
        // properly because it doesn't have the same span.
        //
        // See https://github.com/wasm-bindgen/wasm-bindgen/pull/3725.
        let js_token = quote! { js };
        let mut val = quote_spanned!(self.rust_name.span()=> (*#js_token).borrow().#rust_name);
        if let Some(span) = self.getter_with_clone {
            val = quote_spanned!(span=> <#ty as Clone>::clone(&#val) );
        }

        let wasm_bindgen = &self.wasm_bindgen;

        (quote! {
            #[automatically_derived]
            const _: () = {
                #wasm_bindgen::__wbindgen_coverage! {
                #[cfg_attr(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")), no_mangle)]
                #[doc(hidden)]
                pub unsafe extern "C-unwind" fn #getter(js: u32)
                    -> #wasm_bindgen::convert::WasmRet<<#ty as #wasm_bindgen::convert::IntoWasmAbi>::Abi>
                {
                    use #wasm_bindgen::__rt::{WasmRefCell, assert_not_null};
                    use #wasm_bindgen::convert::IntoWasmAbi;

                    fn assert_copy<T: Copy>(){}
                    #maybe_assert_copy;

                    let js = js as *mut WasmRefCell<#struct_name>;
                    assert_not_null(js);
                    let val = #val;
                    <#ty as IntoWasmAbi>::into_abi(val).into()
                }
                }
            };
        })
        .to_tokens(tokens);

        Descriptor {
            ident: getter,
            inner: quote! {
                <#ty as WasmDescribe>::describe();
            },
            attrs: vec![],
            wasm_bindgen: &self.wasm_bindgen,
        }
        .to_tokens(tokens);

        if self.readonly {
            return;
        }

        let abi = quote! { <#ty as #wasm_bindgen::convert::FromWasmAbi>::Abi };
        let (args, names) = splat(wasm_bindgen, &Ident::new("val", rust_name.span()), &abi);

        (quote! {
            #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
            #[automatically_derived]
            const _: () = {
                #wasm_bindgen::__wbindgen_coverage! {
                #[no_mangle]
                #[doc(hidden)]
                pub unsafe extern "C-unwind" fn #setter(
                    js: u32,
                    #(#args,)*
                ) {
                    use #wasm_bindgen::__rt::{WasmRefCell, assert_not_null};
                    use #wasm_bindgen::convert::FromWasmAbi;

                    let js = js as *mut WasmRefCell<#struct_name>;
                    assert_not_null(js);
                    let val = <#abi as #wasm_bindgen::convert::WasmAbi>::join(#(#names),*);
                    let val = <#ty as FromWasmAbi>::from_abi(val);
                    (*js).borrow_mut().#rust_name = val;
                }
                }
            };
        })
        .to_tokens(tokens);
    }
}

impl TryToTokens for ast::Export {
    fn try_to_tokens(self: &ast::Export, into: &mut TokenStream) -> Result<(), Diagnostic> {
        let generated_name = self.rust_symbol();
        let export_name = self.export_name();
        let mut args = vec![];
        let mut arg_conversions = vec![];
        let mut converted_arguments = vec![];
        let ret = Ident::new("_ret", Span::call_site());

        let offset = if self.method_self.is_some() {
            args.push(quote! { me: u32 });
            1
        } else {
            0
        };

        let name = &self.rust_name;
        let wasm_bindgen = &self.wasm_bindgen;
        let wasm_bindgen_futures = &self.wasm_bindgen_futures;
        let receiver = match self.method_self {
            Some(ast::MethodSelf::ByValue) => {
                let class = self.rust_class.as_ref().unwrap();
                arg_conversions.push(quote! {
                    let me = unsafe {
                        <#class as #wasm_bindgen::convert::FromWasmAbi>::from_abi(me)
                    };
                });
                quote! { me.#name }
            }
            Some(ast::MethodSelf::RefMutable) => {
                let class = self.rust_class.as_ref().unwrap();
                arg_conversions.push(quote! {
                    let mut me = unsafe {
                        <#class as #wasm_bindgen::convert::RefMutFromWasmAbi>
                            ::ref_mut_from_abi(me)
                    };
                    let me = &mut *me;
                });
                quote! { me.#name }
            }
            Some(ast::MethodSelf::RefShared) => {
                let class = self.rust_class.as_ref().unwrap();
                let (trait_, func, borrow) = if self.function.r#async {
                    (
                        quote!(LongRefFromWasmAbi),
                        quote!(long_ref_from_abi),
                        quote!(
                            <<#class as #wasm_bindgen::convert::LongRefFromWasmAbi>
                                ::Anchor as #wasm_bindgen::__rt::core::borrow::Borrow<#class>>
                                ::borrow(&me)
                        ),
                    )
                } else {
                    (quote!(RefFromWasmAbi), quote!(ref_from_abi), quote!(&*me))
                };
                arg_conversions.push(quote! {
                    let me = unsafe {
                        <#class as #wasm_bindgen::convert::#trait_>::#func(me)
                    };
                    let me = #borrow;
                });
                quote! { me.#name }
            }
            None => match &self.rust_class {
                Some(class) => quote! { #class::#name },
                None => quote! { #name },
            },
        };

        let mut argtys = Vec::new();
        for (i, arg) in self.function.arguments.iter().enumerate() {
            argtys.push(&*arg.pat_type.ty);
            let i = i + offset;
            let ident = Ident::new(&format!("arg{i}"), Span::call_site());
            fn unwrap_nested_types(ty: &syn::Type) -> &syn::Type {
                match &ty {
                    syn::Type::Group(syn::TypeGroup { ref elem, .. }) => unwrap_nested_types(elem),
                    syn::Type::Paren(syn::TypeParen { ref elem, .. }) => unwrap_nested_types(elem),
                    _ => ty,
                }
            }
            let ty = unwrap_nested_types(&arg.pat_type.ty);

            match &ty {
                syn::Type::Reference(syn::TypeReference {
                    mutability: Some(_),
                    elem,
                    ..
                }) => {
                    let abi = quote! { <#elem as #wasm_bindgen::convert::RefMutFromWasmAbi>::Abi };
                    let (prim_args, prim_names) = splat(wasm_bindgen, &ident, &abi);
                    args.extend(prim_args);
                    arg_conversions.push(quote! {
                        let mut #ident = unsafe {
                            <#elem as #wasm_bindgen::convert::RefMutFromWasmAbi>
                                ::ref_mut_from_abi(
                                    <#abi as #wasm_bindgen::convert::WasmAbi>::join(#(#prim_names),*)
                                )
                        };
                        let #ident = &mut *#ident;
                    });
                }
                syn::Type::Reference(syn::TypeReference { elem, .. }) => {
                    if self.function.r#async {
                        let abi =
                            quote! { <#elem as #wasm_bindgen::convert::LongRefFromWasmAbi>::Abi };
                        let (prim_args, prim_names) = splat(wasm_bindgen, &ident, &abi);
                        args.extend(prim_args);
                        arg_conversions.push(quote! {
                            let #ident = unsafe {
                                <#elem as #wasm_bindgen::convert::LongRefFromWasmAbi>
                                    ::long_ref_from_abi(
                                        <#abi as #wasm_bindgen::convert::WasmAbi>::join(#(#prim_names),*)
                                    )
                            };
                            let #ident = <<#elem as #wasm_bindgen::convert::LongRefFromWasmAbi>
                                ::Anchor as core::borrow::Borrow<#elem>>
                                ::borrow(&#ident);
                        });
                    } else {
                        let abi = quote! { <#elem as #wasm_bindgen::convert::RefFromWasmAbi>::Abi };
                        let (prim_args, prim_names) = splat(wasm_bindgen, &ident, &abi);
                        args.extend(prim_args);
                        arg_conversions.push(quote! {
                            let #ident = unsafe {
                                <#elem as #wasm_bindgen::convert::RefFromWasmAbi>
                                    ::ref_from_abi(
                                        <#abi as #wasm_bindgen::convert::WasmAbi>::join(#(#prim_names),*)
                                    )
                            };
                            let #ident = &*#ident;
                        });
                    }
                }
                _ => {
                    let abi = quote! { <#ty as #wasm_bindgen::convert::FromWasmAbi>::Abi };
                    let (prim_args, prim_names) = splat(wasm_bindgen, &ident, &abi);
                    args.extend(prim_args);
                    arg_conversions.push(quote! {
                        let #ident = unsafe {
                            <#ty as #wasm_bindgen::convert::FromWasmAbi>
                                ::from_abi(
                                    <#abi as #wasm_bindgen::convert::WasmAbi>::join(#(#prim_names),*)
                                )
                        };
                    });
                }
            }
            converted_arguments.push(quote! { #ident });
        }
        let syn_unit = syn::Type::Tuple(syn::TypeTuple {
            elems: Default::default(),
            paren_token: Default::default(),
        });
        let syn_ret = self
            .function
            .ret
            .as_ref()
            .map(|ret| &ret.r#type)
            .unwrap_or(&syn_unit);
        if let syn::Type::Reference(_) = syn_ret {
            bail_span!(syn_ret, "cannot return a borrowed ref with #[wasm_bindgen]",)
        }

        // For an `async` function we always run it through `future_to_promise`
        // since we're returning a promise to JS, and this will implicitly
        // require that the function returns a `Future<Output = Result<...>>`
        let (ret_ty, inner_ret_ty, ret_expr) = if self.function.r#async {
            if self.start {
                (
                    quote! { () },
                    quote! { () },
                    quote! {
                        <#syn_ret as #wasm_bindgen::__rt::Start>::start(#ret.await)
                    },
                )
            } else {
                (
                    quote! { #wasm_bindgen::JsValue },
                    quote! { #syn_ret },
                    quote! {
                        <#syn_ret as #wasm_bindgen::__rt::IntoJsResult>::into_js_result(#ret.await)
                    },
                )
            }
        } else if self.start {
            (
                quote! { () },
                quote! { () },
                quote! { <#syn_ret as #wasm_bindgen::__rt::Start>::start(#ret) },
            )
        } else {
            (quote! { #syn_ret }, quote! { #syn_ret }, quote! { #ret })
        };

        let mut call = quote! {
            {
                #(#arg_conversions)*
                let #ret = #receiver(#(#converted_arguments),*);
                #ret_expr
            }
        };

        if self.function.r#async {
            if self.start {
                call = quote! {
                    #wasm_bindgen_futures::spawn_local(async move {
                        #call
                    })
                }
            } else {
                call = quote! {
                    #wasm_bindgen_futures::future_to_promise(async move {
                        #call
                    }).into()
                }
            }
        } else {
            call = quote! {
                #wasm_bindgen::__rt::maybe_catch_unwind(|| {
                    #call
                })
            };
        }

        let projection = quote! { <#ret_ty as #wasm_bindgen::convert::ReturnWasmAbi> };
        let convert_ret = quote! { #projection::return_abi(#ret).into() };
        let describe_ret = quote! {
            <#ret_ty as WasmDescribe>::describe();
            <#inner_ret_ty as WasmDescribe>::describe();
        };
        let nargs = self.function.arguments.len() as u32;
        let attrs = self
            .function
            .rust_attrs
            .iter()
            .map(|attr| match &attr.meta {
                Meta::List(list @ MetaList { path, .. }) if path.is_ident("expect") => {
                    let list = MetaList {
                        path: parse_quote!(allow),
                        ..list.clone()
                    };
                    Attribute {
                        meta: Meta::List(list),
                        ..*attr
                    }
                }
                _ => attr.clone(),
            })
            .collect::<Vec<_>>();

        let mut checks = Vec::new();
        if self.start {
            checks.push(quote! { const _ASSERT: fn() = || -> #projection::Abi { loop {} }; });
        };

        if let Some(class) = self.rust_class.as_ref() {
            // little helper function to make sure the check points to the
            // location of the function causing the assert to fail
            let mut add_check = |token_stream| {
                checks.push(respan(token_stream, &self.rust_name));
            };

            match &self.method_kind {
                ast::MethodKind::Constructor => {
                    add_check(quote! {
                        let _: #wasm_bindgen::__rt::marker::CheckSupportsConstructor<#class>;
                    });

                    if self.function.r#async {
                        (quote_spanned! {
                            self.function.name_span =>
                            const _: () = {
                                #[deprecated(note = "async constructors produce invalid TS code and support will be removed in the future")]
                                const fn constructor() {}
                                constructor();
                            };
                        })
                        .to_tokens(into);
                    }
                }
                ast::MethodKind::Operation(operation) => match operation.kind {
                    ast::OperationKind::Getter(_) | ast::OperationKind::Setter(_) => {
                        if operation.is_static {
                            add_check(quote! {
                                let _: #wasm_bindgen::__rt::marker::CheckSupportsStaticProperty<#class>;
                            });
                        } else {
                            add_check(quote! {
                                let _: #wasm_bindgen::__rt::marker::CheckSupportsInstanceProperty<#class>;
                            });
                        }
                    }
                    _ => {}
                },
            }
        }

        (quote! {
            #[automatically_derived]
            const _: () = {
                #wasm_bindgen::__wbindgen_coverage! {
                #(#attrs)*
                #[cfg_attr(
                    all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")),
                    export_name = #export_name,
                )]
                pub unsafe extern "C-unwind" fn #generated_name(#(#args),*) -> #wasm_bindgen::convert::WasmRet<#projection::Abi> {
                    const _: () = {
                        #(#checks)*
                    };

                    let #ret = #call;
                    #convert_ret
                }
                }
            };
        })
        .to_tokens(into);

        let describe_args: TokenStream = argtys
            .iter()
            .map(|ty| match ty {
                syn::Type::Reference(reference)
                    if self.function.r#async && reference.mutability.is_none() =>
                {
                    let inner = &reference.elem;
                    quote! {
                        inform(LONGREF);
                        <#inner as WasmDescribe>::describe();
                    }
                }
                _ => quote! { <#ty as WasmDescribe>::describe(); },
            })
            .collect();

        // In addition to generating the shim function above which is what
        // our generated JS will invoke, we *also* generate a "descriptor"
        // shim. This descriptor shim uses the `WasmDescribe` trait to
        // programmatically describe the type signature of the generated
        // shim above. This in turn is then used to inform the
        // `wasm-bindgen` CLI tool exactly what types and such it should be
        // using in JS.
        //
        // Note that this descriptor function is a purely an internal detail
        // of `#[wasm_bindgen]` and isn't intended to be exported to anyone
        // or actually part of the final was binary. Additionally, this is
        // literally executed when the `wasm-bindgen` tool executes.
        //
        // In any case, there's complications in `wasm-bindgen` to handle
        // this, but the tl;dr; is that this is stripped from the final wasm
        // binary along with anything it references.
        let export = Ident::new(&export_name, Span::call_site());
        Descriptor {
            ident: &export,
            inner: quote! {
                inform(FUNCTION);
                inform(0);
                inform(#nargs);
                #describe_args
                #describe_ret
            },
            attrs,
            wasm_bindgen: &self.wasm_bindgen,
        }
        .to_tokens(into);

        Ok(())
    }
}

impl TryToTokens for ast::ImportKind {
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
        match *self {
            ast::ImportKind::Function(ref f) => f.try_to_tokens(tokens)?,
            ast::ImportKind::Static(ref s) => s.to_tokens(tokens),
            ast::ImportKind::String(ref s) => s.to_tokens(tokens),
            ast::ImportKind::Type(ref t) => t.try_to_tokens(tokens)?,
            ast::ImportKind::Enum(ref e) => e.to_tokens(tokens),
        }

        Ok(())
    }
}

impl TryToTokens for ast::ImportType {
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
        let vis = &self.vis;
        let rust_name = &self.rust_name;
        let attrs = &self.attrs;
        let doc_comment = match &self.doc_comment {
            None => "",
            Some(comment) => comment,
        };
        let instanceof_shim = Ident::new(&self.instanceof_shim, Span::call_site());

        let wasm_bindgen = &self.wasm_bindgen;
        let internal_obj = match self.extends.first() {
            Some(target) => {
                quote! { #target }
            }
            None => {
                quote! { #wasm_bindgen::JsValue }
            }
        };

        let description = if let Some(typescript_type) = &self.typescript_type {
            let typescript_type_len = typescript_type.len() as u32;
            let typescript_type_chars = typescript_type.chars().map(|c| c as u32);
            quote! {
                use #wasm_bindgen::describe::*;
                inform(NAMED_EXTERNREF);
                inform(#typescript_type_len);
                #(inform(#typescript_type_chars);)*
            }
        } else {
            quote! {
                JsValue::describe()
            }
        };

        let is_type_of = self.is_type_of.as_ref().map(|is_type_of| {
            quote! {
                #[inline]
                fn is_type_of(val: &JsValue) -> bool {
                    let is_type_of: fn(&JsValue) -> bool = #is_type_of;
                    is_type_of(val)
                }
            }
        });

        let no_deref = self.no_deref;
        let no_promising = self.no_promising;

        let doc = if doc_comment.is_empty() {
            quote! {}
        } else {
            quote! {
                #[doc = #doc_comment]
            }
        };

        let class_generic_params = generics::generic_params(&self.generics);
        let (impl_generics, ty_generics, where_clause) = self.generics.split_for_impl();

        let type_params_with_bounds = generics::type_params_with_bounds(&self.generics);
        let impl_generics_with_lifetime_a = if type_params_with_bounds.is_empty() {
            quote! { <'a> }
        } else {
            quote! { <'a, #(#type_params_with_bounds),*> }
        };

        // For struct definitions, we need generics with defaults, so use params directly
        let struct_generics = if self.generics.params.is_empty() {
            quote! {}
        } else {
            let params = &self.generics.params;
            quote! { <#params> }
        };

        let phantom;
        let phantom_init;
        if !class_generic_params.is_empty() {
            let generic_param_names = class_generic_params.iter().map(|p| p.0);

            phantom = quote! { generics: ::core::marker::PhantomData<(#(#generic_param_names),*)> };
            phantom_init = quote! { generics: ::core::marker::PhantomData };
        } else {
            phantom = quote! {};
            phantom_init = quote! {};
        }

        (quote! {
            #(#attrs)*
            #doc
            #[repr(transparent)]
            #vis struct #rust_name #struct_generics #where_clause {
                obj: #internal_obj,
                #phantom
            }

            #[automatically_derived]
            const _: () = {
                use #wasm_bindgen::convert::TryFromJsValue;
                use #wasm_bindgen::convert::{IntoWasmAbi, FromWasmAbi};
                use #wasm_bindgen::convert::{OptionIntoWasmAbi, OptionFromWasmAbi};
                use #wasm_bindgen::convert::{RefFromWasmAbi, LongRefFromWasmAbi};
                use #wasm_bindgen::describe::WasmDescribe;
                use #wasm_bindgen::{JsValue, JsCast};
                use #wasm_bindgen::__rt::{core, marker::ErasableGeneric};

                #[automatically_derived]
                impl #impl_generics WasmDescribe for #rust_name #ty_generics #where_clause {
                    fn describe() {
                        #description
                    }
                }

                #[automatically_derived]
                impl #impl_generics IntoWasmAbi for #rust_name #ty_generics #where_clause {
                    type Abi = <JsValue as IntoWasmAbi>::Abi;

                    #[inline]
                    fn into_abi(self) -> Self::Abi {
                        self.obj.into_abi()
                    }
                }

                #[automatically_derived]
                impl #impl_generics OptionIntoWasmAbi for #rust_name #ty_generics #where_clause {
                    #[inline]
                    fn none() -> Self::Abi {
                        0
                    }
                }

                #[automatically_derived]
                impl #impl_generics_with_lifetime_a OptionIntoWasmAbi for &'a #rust_name #ty_generics #where_clause {
                    #[inline]
                    fn none() -> Self::Abi {
                        0
                    }
                }

                #[automatically_derived]
                impl #impl_generics FromWasmAbi for #rust_name #ty_generics #where_clause {
                    type Abi = <JsValue as FromWasmAbi>::Abi;

                    #[inline]
                    unsafe fn from_abi(js: Self::Abi) -> Self {
                        #rust_name {
                            obj: JsValue::from_abi(js).into(),
                            #phantom_init
                        }
                    }
                }

                #[automatically_derived]
                impl #impl_generics OptionFromWasmAbi for #rust_name #ty_generics #where_clause {
                    #[inline]
                    fn is_none(abi: &Self::Abi) -> bool { *abi == 0 }
                }

                #[automatically_derived]
                impl #impl_generics_with_lifetime_a IntoWasmAbi for &'a #rust_name #ty_generics #where_clause {
                    type Abi = <&'a JsValue as IntoWasmAbi>::Abi;

                    #[inline]
                    fn into_abi(self) -> Self::Abi {
                        (&self.obj).into_abi()
                    }
                }

                #[automatically_derived]
                impl #impl_generics RefFromWasmAbi for #rust_name #ty_generics #where_clause {
                    type Abi = <JsValue as RefFromWasmAbi>::Abi;
                    type Anchor = core::mem::ManuallyDrop<#rust_name #ty_generics>;

                    #[inline]
                    unsafe fn ref_from_abi(js: Self::Abi) -> Self::Anchor {
                        let tmp = <JsValue as RefFromWasmAbi>::ref_from_abi(js);
                        core::mem::ManuallyDrop::new(#rust_name {
                            obj: core::mem::ManuallyDrop::into_inner(tmp).into(),
                            #phantom_init
                        })
                    }
                }

                #[automatically_derived]
                impl #impl_generics LongRefFromWasmAbi for #rust_name #ty_generics #where_clause {
                    type Abi = <JsValue as LongRefFromWasmAbi>::Abi;
                    type Anchor = #rust_name #ty_generics;

                    #[inline]
                    unsafe fn long_ref_from_abi(js: Self::Abi) -> Self::Anchor {
                        let tmp = <JsValue as LongRefFromWasmAbi>::long_ref_from_abi(js);
                        #rust_name {
                            obj: tmp.into(),
                            #phantom_init
                        }
                    }
                }

                #[automatically_derived]
                impl #impl_generics AsRef<JsValue> for #rust_name #ty_generics #where_clause {
                    #[inline]
                    fn as_ref(&self) -> &JsValue { self.obj.as_ref() }
                }

                #[automatically_derived]
                impl #impl_generics AsRef<#rust_name #ty_generics> for #rust_name #ty_generics #where_clause {
                    #[inline]
                    fn as_ref(&self) -> &#rust_name #ty_generics { self }
                }

                // TODO: remove this on the next major version
                #[automatically_derived]
                impl From<JsValue> for #rust_name {
                    #[inline]
                    fn from(obj: JsValue) -> Self {
                        #rust_name {
                            obj: obj.into(),
                            #phantom_init
                        }
                    }
                }

                #[automatically_derived]
                impl #impl_generics From<#rust_name #ty_generics> for JsValue #where_clause {
                    #[inline]
                    fn from(obj: #rust_name #ty_generics) -> JsValue {
                        obj.obj.into()
                    }
                }

                #[automatically_derived]
                impl #impl_generics JsCast for #rust_name #ty_generics #where_clause {
                    fn instanceof(val: &JsValue) -> bool {
                        #[link(wasm_import_module = "__wbindgen_placeholder__")]
                        #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
                        extern "C" {
                            fn #instanceof_shim(val: u32) -> u32;
                        }
                        #[cfg(not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none"))))]
                        unsafe fn #instanceof_shim(_: u32) -> u32 {
                            panic!("cannot check instanceof on non-wasm targets");
                        }
                        unsafe {
                            let idx = val.into_abi();
                            #instanceof_shim(idx) != 0
                        }
                    }

                    #is_type_of

                    #[inline]
                    fn unchecked_from_js(val: JsValue) -> Self {
                        #rust_name {
                            obj: val.into(),
                            #phantom_init
                        }
                    }

                    #[inline]
                    fn unchecked_from_js_ref(val: &JsValue) -> &Self {
                        // Should be safe because `#rust_name` is a transparent
                        // wrapper around `val`
                        unsafe { &*(val as *const JsValue as *const Self) }
                    }
                }

                unsafe impl #impl_generics ErasableGeneric for #rust_name #ty_generics #where_clause {
                    type Repr = JsValue;
                }
            };
        })
        .to_tokens(tokens);

        if !no_promising {
            (quote! {
                #[automatically_derived]
                impl #impl_generics #wasm_bindgen::sys::Promising for #rust_name #ty_generics #where_clause {
                    type Resolution = #rust_name #ty_generics;
                }
            })
            .to_tokens(tokens);
        }

        if !no_deref {
            (quote! {
                #[automatically_derived]
                impl #impl_generics #wasm_bindgen::__rt::core::ops::Deref for #rust_name #ty_generics #where_clause {
                    type Target = #internal_obj;

                    #[inline]
                    fn deref(&self) -> &#internal_obj {
                        &self.obj
                    }
                }
            })
            .to_tokens(tokens);
        }

        for superclass in self.extends.iter() {
            (quote! {
                #[automatically_derived]
                impl #impl_generics From<#rust_name #ty_generics> for #superclass #where_clause {
                    #[inline]
                    fn from(obj: #rust_name #ty_generics) -> #superclass {
                        use #wasm_bindgen::JsCast;
                        #superclass::unchecked_from_js(obj.into())
                    }
                }

                #[automatically_derived]
                impl #impl_generics AsRef<#superclass> for #rust_name #ty_generics #where_clause {
                    #[inline]
                    fn as_ref(&self) -> &#superclass {
                        use #wasm_bindgen::JsCast;
                        #superclass::unchecked_from_js_ref(self.as_ref())
                    }
                }
            })
            .to_tokens(tokens);
        }

        // Generate UpcastFrom implementations (unless no_upcast is set)
        if !self.no_upcast {
            // 1. Always generate UpcastFrom<Self> for JsValue
            (quote! {
                #[automatically_derived]
                impl #impl_generics #wasm_bindgen::convert::UpcastFrom<#rust_name #ty_generics>
                    for #wasm_bindgen::JsValue
                #where_clause
                {
                }
            })
            .to_tokens(tokens);

            // 2. For non-generic types: generate identity upcast (UpcastFrom<Self> for Self, UpcastFrom<Self> for JsOption<Self>)
            // 3. For generic types: generate structural covariance
            let type_params: Vec<_> = self.generics.type_params().collect();
            if type_params.is_empty() {
                // Identity impls for non-generic types
                (quote! {
                    #[automatically_derived]
                    impl #impl_generics #wasm_bindgen::convert::UpcastFrom<#rust_name>
                        for #rust_name
                    #where_clause
                    {
                    }
                    #[automatically_derived]
                    impl #impl_generics #wasm_bindgen::convert::UpcastFrom<#rust_name>
                        for #wasm_bindgen::sys::JsOption<#rust_name>
                    #where_clause
                    {
                    }
                })
                .to_tokens(tokens);
            } else {
                // Structural covariance impl for generic types
                // Build impl generics: all original params plus a Target param for each
                let mut impl_generics_extended = self.generics.clone();
                let target_param_names: Vec<syn::Ident> = type_params
                    .iter()
                    .enumerate()
                    .map(|(i, tp)| {
                        let target_name = quote::format_ident!("__UpcastTarget{}", i);
                        // Copy bounds from the original type param to the target param
                        // If no bounds, just add the type param without colon
                        if tp.bounds.is_empty() {
                            impl_generics_extended
                                .params
                                .push(syn::parse_quote!(#target_name));
                        } else {
                            let bounds = &tp.bounds;
                            impl_generics_extended
                                .params
                                .push(syn::parse_quote!(#target_name: #bounds));
                        }
                        target_name
                    })
                    .collect();

                // Build where clause: Target: UpcastFrom<T>
                let mut where_clause_extended =
                    self.generics
                        .where_clause
                        .clone()
                        .unwrap_or_else(|| syn::WhereClause {
                            where_token: Default::default(),
                            predicates: Default::default(),
                        });

                for (type_param, target_name) in type_params.iter().zip(&target_param_names) {
                    let param_ident = &type_param.ident;
                    where_clause_extended.predicates.push(syn::parse_quote!(
                        #target_name: #wasm_bindgen::convert::UpcastFrom<#param_ident>
                    ));
                }

                let (impl_generics_split, _, _) = impl_generics_extended.split_for_impl();

                // Structural covariance - Type<Target0, Target1, ...> can be upcast from Type<T1, T2, ...>
                (quote! {
                    #[automatically_derived]
                    impl #impl_generics_split #wasm_bindgen::convert::UpcastFrom<#rust_name #ty_generics>
                        for #rust_name<#(#target_param_names),*>
                    #where_clause_extended
                    {
                    }
                    #[automatically_derived]
                    impl #impl_generics_split #wasm_bindgen::convert::UpcastFrom<#rust_name #ty_generics>
                        for #wasm_bindgen::sys::JsOption<#rust_name<#(#target_param_names),*>>
                    #where_clause_extended
                    {
                    }
                })
                .to_tokens(tokens);
            }

            // 4. For each superclass in extends, generate UpcastFrom<Self> for superclass
            for superclass in self.extends.iter() {
                (quote! {
                    #[automatically_derived]
                    impl #impl_generics #wasm_bindgen::convert::UpcastFrom<#rust_name #ty_generics>
                        for #superclass
                    #where_clause
                    {
                    }
                    #[automatically_derived]
                    impl #impl_generics #wasm_bindgen::convert::UpcastFrom<#rust_name #ty_generics>
                        for #wasm_bindgen::sys::JsOption<#superclass>
                    #where_clause
                    {
                    }
                })
                .to_tokens(tokens);
            }
        }

        Ok(())
    }
}

impl ToTokens for ast::StringEnum {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        let vis = &self.vis;
        let enum_name = &self.name;
        let name_str = &self.export_name;
        let name_len = name_str.len() as u32;
        let name_chars = name_str.chars().map(u32::from);
        let variants = &self.variants;
        let variant_count = self.variant_values.len() as u32;
        let variant_values = &self.variant_values;
        let variant_indices = (0..variant_count).collect::<Vec<_>>();
        let invalid = variant_count;
        let hole = variant_count + 1;
        let attrs = &self.rust_attrs;

        let invalid_to_str_msg = format!(
            "Converting an invalid string enum ({enum_name}) back to a string is currently not supported"
        );

        // A vector of EnumName::VariantName tokens for this enum
        let variant_paths: Vec<TokenStream> = self
            .variants
            .iter()
            .map(|v| quote!(#enum_name::#v).into_token_stream())
            .collect();

        // Borrow variant_paths because we need to use it multiple times inside the quote! macro
        let variant_paths_ref = &variant_paths;

        let wasm_bindgen = &self.wasm_bindgen;

        (quote! {
            #(#attrs)*
            #[non_exhaustive]
            #[repr(u32)]
            #vis enum #enum_name {
                #(#variants = #variant_indices,)*
                #[automatically_derived]
                #[doc(hidden)]
                __Invalid
            }

            #[automatically_derived]
            impl #enum_name {
                fn from_str(s: &str) -> Option<#enum_name> {
                    match s {
                        #(#variant_values => Some(#variant_paths_ref),)*
                        _ => None,
                    }
                }

                fn to_str(&self) -> &'static str {
                    match self {
                        #(#variant_paths_ref => #variant_values,)*
                        #enum_name::__Invalid => panic!(#invalid_to_str_msg),
                    }
                }

                #vis fn from_js_value(obj: &#wasm_bindgen::JsValue) -> Option<#enum_name> {
                    obj.as_string().and_then(|obj_str| Self::from_str(obj_str.as_str()))
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::IntoWasmAbi for #enum_name {
                type Abi = u32;

                #[inline]
                fn into_abi(self) -> u32 {
                    self as u32
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::FromWasmAbi for #enum_name {
                type Abi = u32;

                unsafe fn from_abi(val: u32) -> Self {
                    match val {
                        #(#variant_indices => #variant_paths_ref,)*
                        #invalid => #enum_name::__Invalid,
                        _ => unreachable!("The JS binding should only ever produce a valid value or the specific 'invalid' value"),
                    }
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::OptionFromWasmAbi for #enum_name {
                #[inline]
                fn is_none(val: &u32) -> bool { *val == #hole }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::OptionIntoWasmAbi for #enum_name {
                #[inline]
                fn none() -> Self::Abi { #hole }
            }

            #[automatically_derived]
            impl #wasm_bindgen::describe::WasmDescribe for #enum_name {
                fn describe() {
                    use #wasm_bindgen::describe::*;
                    inform(STRING_ENUM);
                    inform(#name_len);
                    #(inform(#name_chars);)*
                    inform(#variant_count);
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::__rt::core::convert::From<#enum_name> for
                #wasm_bindgen::JsValue
            {
                fn from(val: #enum_name) -> Self {
                    #wasm_bindgen::JsValue::from_str(val.to_str())
                }
            }
        })
        .to_tokens(tokens);
    }
}

impl TryToTokens for ast::ImportFunction {
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
        let mut class = None;
        let mut is_constructor = false;
        let mut is_method = false;
        let mut is_self_returning_static = false;
        if let ast::ImportFunctionKind::Method {
            class: class_name,
            ty,
            kind,
            ..
        } = &self.kind
        {
            class = Some((class_name, get_ty(ty)));
            match kind {
                ast::MethodKind::Constructor => is_constructor = true,
                ast::MethodKind::Operation(ast::Operation {
                    is_static: false, ..
                }) => is_method = true,
                _ => {}
            };
            // For constructors and static methods whose return type matches the
            // class (e.g. `Array::of<T>() -> Array<T>`), override the class type
            // to use the return type so class-level generics get hoisted.
            if self.class_return_path().is_some() {
                class = Some((class_name, get_ty(self.js_ret.as_ref().unwrap())));
                if !is_constructor {
                    is_self_returning_static = true;
                }
            }
        }

        let vis = &self.function.rust_vis;
        let ret = match self.function.ret.as_ref().map(|ret| &ret.r#type) {
            Some(ty) => quote! { -> #ty },
            None => quote!(),
        };

        let mut abi_argument_names = Vec::new();
        let mut abi_arguments = Vec::new();
        let mut arg_conversions = Vec::new();
        let mut arguments = Vec::new();

        let mut fn_class_generics = self.get_fn_generics()?;
        let (fn_lifetime_param_names, fn_generic_param_names) =
            generics::all_param_names(&self.generics);

        let ret_ident = Ident::new("_ret", Span::call_site());
        let wasm_bindgen = &self.wasm_bindgen;
        let wasm_bindgen_futures = &self.wasm_bindgen_futures;

        for (i, arg) in self.function.arguments.iter().enumerate() {
            let ty = &*arg.pat_type.ty;
            let name = match &*arg.pat_type.pat {
                syn::Pat::Ident(syn::PatIdent {
                    by_ref: None,
                    ident,
                    subpat: None,
                    ..
                }) => ident.clone(),
                syn::Pat::Wild(_) => syn::Ident::new(&format!("__genarg_{i}"), Span::call_site()),
                _ => bail_span!(
                    arg.pat_type.pat,
                    "unsupported pattern in #[wasm_bindgen] imported function",
                ),
            };

            let var = if i == 0 && is_method {
                quote! { self }
            } else {
                quote! { #name }
            };

            let abi_ty;
            let convert_arg;

            if generics::uses_generic_params(ty, &fn_generic_param_names)
                || generics::uses_lifetime_params(ty, &fn_lifetime_param_names)
            {
                let (inner_ty, ref_mut, ref_lifetime) =
                    if let syn::Type::Reference(syn::TypeReference {
                        elem,
                        mutability: mut_,
                        lifetime,
                        ..
                    }) = ty
                    {
                        ((**elem).clone(), Some(mut_), lifetime.clone())
                    } else {
                        (ty.clone(), None, None)
                    };
                let concrete_ty = generic_to_concrete(
                    inner_ty.clone(),
                    &fn_class_generics.concrete_defaults,
                    &fn_lifetime_param_names,
                )?;
                if i > 0 || !is_method {
                    fn_class_generics.add_fn_bound(if let Some(mut_) = ref_mut {
                        arguments.push(quote! { #name: & #ref_lifetime #mut_ #inner_ty });
                        if mut_.is_some() {
                            parse_quote! { #inner_ty: #wasm_bindgen::__rt::marker::ErasableGenericBorrowMut<#concrete_ty> }
                        } else {
                            parse_quote! { #inner_ty: #wasm_bindgen::__rt::marker::ErasableGenericBorrow<#concrete_ty> }
                        }
                    } else {
                        arguments.push(quote! { #name: #ty });
                        parse_quote! { #inner_ty: #wasm_bindgen::__rt::marker::ErasableGenericOwn<#concrete_ty> }
                    });
                }
                // abi_ty is fully concrete with 'static lifetimes (used for both extern block and transmute)
                abi_ty = if let Some(mut_) = ref_mut {
                    quote! { &'static #mut_ #concrete_ty }
                } else {
                    quote! { #concrete_ty }
                };

                convert_arg = quote! { unsafe { core::mem::transmute_copy(&core::mem::ManuallyDrop::new(#var)) } };
            } else {
                if i > 0 || !is_method {
                    arguments.push(quote! { #name: #ty });
                }
                abi_ty = quote! { #ty };

                convert_arg = quote! { #var };
            }

            let abi = quote! { <#abi_ty as #wasm_bindgen::convert::IntoWasmAbi>::Abi };
            let (prim_args, prim_names) = splat(wasm_bindgen, &name, &abi);
            abi_arguments.extend(prim_args);
            abi_argument_names.extend(prim_names.iter().cloned());

            arg_conversions.push(quote! {
                let #name = <#abi_ty as #wasm_bindgen::convert::IntoWasmAbi>
                    ::into_abi(#convert_arg);
                let (#(#prim_names),*) = <#abi as #wasm_bindgen::convert::WasmAbi>::split(#name);
            });
        }
        let abi_ret;
        let mut convert_ret;
        match &self.js_ret {
            Some(syn::Type::Reference(_)) => {
                bail_span!(
                    self.js_ret,
                    "cannot return references in #[wasm_bindgen] imports yet"
                );
            }
            Some(ref original_ty) => {
                let maybe_async_wrapped;
                let ty = if self.function.r#async {
                    maybe_async_wrapped =
                        parse_quote!(wasm_bindgen_futures::js_sys::Promise<#original_ty>);
                    &maybe_async_wrapped
                } else {
                    original_ty
                };
                if generics::uses_generic_params(ty, &fn_generic_param_names)
                    || generics::uses_lifetime_params(ty, &fn_lifetime_param_names)
                {
                    let concrete_ty = generic_to_concrete(
                        ty.clone(),
                        &fn_class_generics.concrete_defaults,
                        &fn_lifetime_param_names,
                    )?;
                    fn_class_generics.add_fn_bound(
                        parse_quote! { #ty: #wasm_bindgen::__rt::marker::ErasableGenericOwn<#concrete_ty> },
                    );
                    convert_ret = quote! { unsafe { core::mem::transmute_copy(&core::mem::ManuallyDrop::new(<#concrete_ty as #wasm_bindgen::convert::FromWasmAbi>::from_abi(#ret_ident.join()))) } };
                    abi_ret = quote! { #wasm_bindgen::convert::WasmRet<<#concrete_ty as #wasm_bindgen::convert::FromWasmAbi>::Abi> };
                } else {
                    convert_ret = quote! { <#ty as #wasm_bindgen::convert::FromWasmAbi>::from_abi(#ret_ident.join()) };
                    abi_ret = quote! { #wasm_bindgen::convert::WasmRet<<#ty as #wasm_bindgen::convert::FromWasmAbi>::Abi> };
                }
                if self.function.r#async {
                    convert_ret = quote! {
                        #wasm_bindgen_futures::JsFuture::from(
                            <#wasm_bindgen_futures::js_sys::Promise<#original_ty> as #wasm_bindgen::convert::FromWasmAbi>
                                ::from_abi(#ret_ident.join())
                        ).await
                    };
                    if self.catch {
                        convert_ret = quote! { Ok(#convert_ret?) };
                    } else {
                        convert_ret = quote! { #convert_ret.expect("uncaught exception") };
                    };
                }
            }
            None => {
                if self.function.r#async {
                    abi_ret = quote! {
                        #wasm_bindgen::convert::WasmRet<<#wasm_bindgen_futures::js_sys::Promise as #wasm_bindgen::convert::FromWasmAbi>::Abi>
                    };
                    let future = quote! {
                        #wasm_bindgen_futures::JsFuture::from(
                            <#wasm_bindgen_futures::js_sys::Promise as #wasm_bindgen::convert::FromWasmAbi>
                                ::from_abi(#ret_ident.join())
                        ).await
                    };
                    convert_ret = if self.catch {
                        quote! { #future?; Ok(()) }
                    } else {
                        quote! { #future.expect("uncaught exception"); }
                    };
                } else {
                    abi_ret = quote! { () };
                    convert_ret = quote! { () };
                }
            }
        }

        let mut exceptional_ret = quote!();
        if self.catch && !self.function.r#async {
            convert_ret = quote! { Ok(#convert_ret) };
            exceptional_ret = quote! {
                #wasm_bindgen::__rt::take_last_exception()?;
            };
        }

        let rust_name = &self.rust_name;
        let import_name = &self.shim;
        let attrs = &self.function.rust_attrs;
        let arguments = &arguments;
        let abi_arguments = &abi_arguments[..];
        let abi_argument_names = &abi_argument_names[..];

        let doc = if self.doc_comment.is_empty() {
            quote! {}
        } else {
            let doc_comment = &self.doc_comment;
            quote! { #[doc = #doc_comment] }
        };

        let me = if is_method {
            quote! { &self, }
        } else {
            quote!()
        };

        // Route any errors pointing to this imported function to the identifier
        // of the function we're imported from so we at least know what function
        // is causing issues.
        //
        // Note that this is where type errors like "doesn't implement
        // FromWasmAbi" or "doesn't implement IntoWasmAbi" currently get routed.
        // I suspect that's because they show up in the signature via trait
        // projections as types of arguments, and all that needs to typecheck
        // before the body can be typechecked. Due to rust-lang/rust#60980 (and
        // probably related issues) we can't really get a precise span.
        //
        // Ideally what we want is to point errors for particular types back to
        // the specific argument/type that generated the error, but it looks
        // like rustc itself doesn't do great in that regard so let's just do
        // the best we can in the meantime.
        let extern_fn = respan(
            extern_fn(
                import_name,
                attrs,
                abi_arguments,
                abi_argument_names,
                abi_ret,
            ),
            &self.rust_name,
        );

        let maybe_unsafe = if self.function.r#unsafe {
            Some(quote! { unsafe })
        } else {
            None
        };
        let maybe_async = if self.function.r#async {
            Some(quote! { async })
        } else {
            None
        };

        let mut class_impl_def = None;
        if let Some((_, class)) = class {
            let mut class = class.clone();
            if let syn::Type::Path(syn::TypePath {
                qself: None,
                ref mut path,
            }) = class
            {
                if let Some(segment) = path.segments.last_mut() {
                    segment.arguments = syn::PathArguments::None;
                }
            }
            let has_class_generics = !fn_class_generics.class_generic_params.is_empty()
                || !fn_class_generics.class_lifetime_params.is_empty()
                || !fn_class_generics.class_bound_lifetime_params.is_empty();
            if (!is_method && !is_constructor && !is_self_returning_static) || !has_class_generics {
                // For static functions not the constructor/self-returning, we impl on generic default
                class_impl_def = Some(quote! { impl #class });
            } else {
                // Type lifetimes: appear on impl AND passed to type
                let class_lifetime_params = &fn_class_generics.class_lifetime_params;
                // Bound-only lifetimes: appear on impl but NOT passed to type
                let class_bound_lifetime_params = &fn_class_generics.class_bound_lifetime_params;
                let class_generic_params = &fn_class_generics.class_generic_params;
                let class_generic_exprs = &fn_class_generics.class_generic_exprs;
                let impl_where_clause = if !fn_class_generics.class_bounds.is_empty() {
                    let class_bounds = fn_class_generics.class_bounds.iter();
                    quote! { where #(#class_bounds),* }
                } else {
                    quote! {}
                };
                class_impl_def = Some(
                    quote! { impl<#(#class_lifetime_params,)* #(#class_bound_lifetime_params,)* #(#class_generic_params),*> #class <#(#class_lifetime_params,)* #(#class_generic_exprs),*> #impl_where_clause },
                );
            }
        };

        // Function-level lifetime params
        let fn_lifetime_params = &fn_class_generics.fn_lifetime_params;
        let impl_generics =
            if fn_class_generics.fn_generic_params.is_empty() && fn_lifetime_params.is_empty() {
                quote! {}
            } else {
                let fn_generic_params = fn_class_generics.fn_generic_params;
                quote! { <#(#fn_lifetime_params,)* #(#fn_generic_params),*> }
            };
        let where_clause = if fn_class_generics.fn_bounds.is_empty() {
            quote! {}
        } else {
            let fn_bounds = fn_class_generics.fn_bounds;
            quote! { where #(#fn_bounds),* }
        };

        let invocation = quote! {
            // This is due to `#[automatically_derived]` attribute cannot be
            // placed onto bare functions.
            #[allow(nonstandard_style)]
            #[allow(clippy::all, clippy::nursery, clippy::pedantic, clippy::restriction)]
            #(#attrs)*
            #doc
            #vis #maybe_async #maybe_unsafe fn #rust_name #impl_generics (#me #(#arguments),*) #ret #where_clause {
                #extern_fn

                unsafe {
                    let #ret_ident = {
                        #(#arg_conversions)*
                        #import_name(#(#abi_argument_names),*)
                    };
                    #exceptional_ret
                    #convert_ret
                }
            }
        };

        if let Some(class_impl_def) = class_impl_def {
            quote! {
                #[automatically_derived]
                #class_impl_def {
                    #invocation
                }
            }
            .to_tokens(tokens);
        } else {
            invocation.to_tokens(tokens);
        }

        Ok(())
    }
}

// See comment above in ast::Export for what's going on here.
struct DescribeImport<'a> {
    kind: &'a ast::ImportKind,
    wasm_bindgen: &'a syn::Path,
}

// Extracted impl block info given class generics and function-level method generics
struct FnClassGenerics<'a> {
    // the hoisted class-level param idents used, with identifiers renamed to use function generic identifier names
    class_generic_params: BTreeSet<syn::Ident>,
    // the struct generic expressions on those params
    class_generic_exprs: Vec<&'a syn::Type>,
    // class where bounds including hoisted function bounds
    class_bounds: Vec<Cow<'a, syn::WherePredicate>>,
    // the remaining non-hoisted function-level param idents
    fn_generic_params: Vec<&'a syn::Ident>,
    // function bounds on params which are only specific to the function not hoisted as class bounds
    fn_bounds: Vec<Cow<'a, syn::WherePredicate>>,
    // the union of class-level defaults (for identifier generics) and function defaults
    // this is used to form the concrete type via replacement (using JsValue otherwise)
    concrete_defaults: BTreeMap<&'a syn::Ident, Option<Cow<'a, syn::Type>>>,
    // hoisted class-level lifetime params passed to the type
    class_lifetime_params: Vec<&'a syn::Lifetime>,
    // hoisted class-level lifetime params only used in bounds (not passed to type)
    class_bound_lifetime_params: Vec<syn::Lifetime>,
    // the remaining non-hoisted function-level lifetime params
    fn_lifetime_params: Vec<&'a syn::Lifetime>,
}

impl<'a> FnClassGenerics<'a> {
    /// Adds a new function bound, checking it is not already a bound
    fn add_fn_bound(&mut self, bound: syn::WherePredicate) {
        if !self.fn_bounds.iter().any(|existing| **existing == bound) {
            self.fn_bounds.push(Cow::Owned(bound));
        }
    }
}

impl ast::ImportFunction {
    fn get_fn_generics<'a>(&'a self) -> Result<FnClassGenerics<'a>, Diagnostic> {
        let original_fn_generics = generics::generic_params(&self.generics);
        let mut fn_generic_params: Vec<&syn::Ident> =
            original_fn_generics.iter().map(|p| p.0).collect();
        let concrete_defaults: BTreeMap<_, _> = original_fn_generics
            .into_iter()
            .map(|(i, d)| (i, d.map(Cow::Borrowed)))
            .collect();

        // Extract lifetime parameters
        let all_lifetime_params = generics::lifetime_params(&self.generics);
        let mut fn_lifetime_params: Vec<&syn::Lifetime> = all_lifetime_params.clone();

        let mut where_predicates: Vec<Cow<syn::WherePredicate>> = Vec::new();
        for param in &self.generics.params {
            if let syn::GenericParam::Type(type_param) = param {
                if !type_param.bounds.is_empty() {
                    let ident = &type_param.ident;
                    let bounds = type_param.bounds.clone();
                    let predicate = syn::WherePredicate::Type(syn::PredicateType {
                        lifetimes: None,
                        bounded_ty: syn::parse_quote!(#ident),
                        colon_token: syn::Token![:](proc_macro2::Span::call_site()),
                        bounds,
                    });
                    where_predicates.push(Cow::Owned(predicate));
                }
            }
        }

        let mut class_bounds = Vec::new();
        let mut fn_bounds = generics::generic_bounds(&self.generics);
        let mut class_generic_params = BTreeSet::new();
        let mut class_lifetime_params_set = BTreeSet::new();
        let mut class_bound_lifetime_params_set: BTreeSet<syn::Lifetime> = BTreeSet::new();
        let mut class_generic_exprs = Vec::new();

        let mut class = None;
        if let ast::ImportFunctionKind::Method {
            ty,
            kind:
                ast::MethodKind::Operation(ast::Operation {
                    is_static: false, ..
                }),
            ..
        } = &self.kind
        {
            let syn::Type::Path(syn::TypePath { path, .. }) = ty else {
                unreachable!(); // validated at parse time
            };
            class = Some(path);
        }

        // For constructors and static methods whose return type matches the class
        // (e.g. `Array::of<T>() -> Array<T>`), use the return type path for hoisting
        // since it carries the generic arguments.
        if class.is_none() {
            class = self.class_return_path();
        }

        if let Some(cls_path) = class {
            if let Some(syn::PathSegment {
                arguments: syn::PathArguments::AngleBracketed(gen_args),
                ..
            }) = cls_path.segments.last()
            {
                // Iterate the &self<expr1, expr2, ...> gen args, as the class_generic_exprs Vec
                for gen_arg in gen_args.args.iter() {
                    // Handle lifetime arguments for hoisting
                    if let syn::GenericArgument::Lifetime(lt) = gen_arg {
                        if all_lifetime_params.contains(&lt) {
                            class_lifetime_params_set.insert(lt.clone());
                        }
                        continue;
                    }

                    let syn::GenericArgument::Type(ty) = gen_arg else {
                        bail_span!(gen_arg, "Functions must provide generic arguments");
                    };

                    class_generic_exprs.push(ty);

                    // Visit the generic expression, adding all used function generics to the hoisted class generic params
                    class_generic_params =
                        generics::used_generic_params(ty, &fn_generic_params, class_generic_params);

                    // Also find lifetimes used in class generic expressions
                    let used_lifetimes = generics::used_lifetimes_in_type(ty, &all_lifetime_params);
                    class_lifetime_params_set.extend(used_lifetimes);
                }

                // Transitively hoist generic params and lifetimes that are used in bounds OF already-hoisted params.
                // For example, if F is hoisted and has bound `F: JsFunction<Ret = Ret>`, then Ret
                // must also be hoisted since it appears in a bound on F. Same for lifetimes.
                // We only hoist from bounds where the bounded_ty IS the class param (not just mentions it).
                loop {
                    let remaining_fn_params: Vec<&Ident> = fn_generic_params
                        .iter()
                        .filter(|p| !class_generic_params.contains(*p))
                        .copied()
                        .collect();

                    let remaining_fn_lifetimes: Vec<&syn::Lifetime> = fn_lifetime_params
                        .iter()
                        .filter(|lt| {
                            !class_lifetime_params_set.contains(*lt)
                                && !class_bound_lifetime_params_set.contains(*lt)
                        })
                        .copied()
                        .collect();

                    let mut params_to_add = Vec::new();
                    let mut lifetimes_to_add = BTreeSet::new();

                    for bound in &fn_bounds {
                        // Only process bounds where the bounded type IS a class param
                        // e.g., for `F: JsFunction<Ret = Ret>`, bounded_ty is `F`
                        if let syn::WherePredicate::Type(pred_type) = bound.as_ref() {
                            if let syn::Type::Path(type_path) = &pred_type.bounded_ty {
                                if type_path.qself.is_none() && type_path.path.segments.len() == 1 {
                                    let bounded_ident = &type_path.path.segments[0].ident;
                                    if class_generic_params.contains(bounded_ident) {
                                        // This bound is ON a class param, check for fn params and lifetimes used in the bounds
                                        let mut found_set = BTreeSet::new();
                                        let mut visitor = generics::GenericNameVisitor::new(
                                            &remaining_fn_params,
                                            &mut found_set,
                                        );
                                        for type_bound in &pred_type.bounds {
                                            syn::visit::Visit::visit_type_param_bound(
                                                &mut visitor,
                                                type_bound,
                                            );
                                        }
                                        params_to_add.extend(found_set);

                                        // Also hoist lifetimes from the same bounds
                                        let used = generics::used_lifetimes_in_bounds(
                                            &pred_type.bounds,
                                            &remaining_fn_lifetimes,
                                        );
                                        lifetimes_to_add.extend(used);
                                    }
                                }
                            }
                        }
                    }

                    if params_to_add.is_empty() && lifetimes_to_add.is_empty() {
                        break;
                    }
                    for param in params_to_add {
                        class_generic_params.insert(param);
                    }
                    for lt in lifetimes_to_add {
                        class_bound_lifetime_params_set.insert(lt);
                    }
                }

                let class_generic_params_refs: Vec<&Ident> = class_generic_params.iter().collect();

                // fn generic params are all params not hoisted as class params
                fn_generic_params = fn_generic_params
                    .iter()
                    .copied()
                    .filter(|&p| !class_generic_params.contains(p))
                    .collect();

                // fn lifetime params are all lifetime params not hoisted as class lifetime params
                fn_lifetime_params.retain(|&lt| {
                    !class_lifetime_params_set.contains(lt)
                        && !class_bound_lifetime_params_set.contains(lt)
                });

                // hoist function where bounds on class generic params
                fn_bounds.retain(|bound| {
                    if generics::generics_predicate_uses(bound, &class_generic_params_refs)
                        && !generics::generics_predicate_uses(bound, &fn_generic_params)
                    {
                        class_bounds.push(bound.clone());
                        false
                    } else {
                        true
                    }
                });
            }
        }

        // Convert class_lifetime_params_set to Vec, maintaining order from original params
        let class_lifetime_params: Vec<&syn::Lifetime> = all_lifetime_params
            .iter()
            .copied()
            .filter(|lt| class_lifetime_params_set.contains(*lt))
            .collect();

        // Convert class_bound_lifetime_params_set to Vec, maintaining order from original params
        let class_bound_lifetime_params: Vec<syn::Lifetime> = all_lifetime_params
            .iter()
            .copied()
            .filter(|lt| class_bound_lifetime_params_set.contains(*lt))
            .cloned()
            .collect();

        Ok(FnClassGenerics {
            class_generic_params,
            class_generic_exprs,
            class_bounds,
            fn_generic_params,
            fn_bounds,
            concrete_defaults,
            class_lifetime_params,
            class_bound_lifetime_params,
            fn_lifetime_params,
        })
    }

    /// For constructors and static methods (via `static_method_of`), checks whether
    /// the return type matches the class name. If so, returns the path from `js_ret`
    /// which carries any generic arguments (e.g. `Array<T>`).
    ///
    /// This is used to determine when class-level generic hoisting should apply:
    ///  - Constructors always return their own class, so this always matches.
    ///  - Static methods like `#[wasm_bindgen(static_method_of = Array, js_name = of)]`
    ///    returning `Array<T>` also match, and need the same hoisting treatment.
    ///
    /// For static methods, since we are *inferring* that hoisting should happen (the
    /// user didn't explicitly opt in like with `constructor`), we only match when all
    /// type generic arguments are bare type parameter idents (e.g. `Array<T>`). Cases
    /// like `Array<I::Item>` or `Promise<U::Resolution>` are left as plain static
    /// methods — the associated type is a function-level concern, not a class property.
    fn class_return_path(&self) -> Option<&syn::Path> {
        let ast::ImportFunctionKind::Method {
            class: class_name,
            kind,
            ..
        } = &self.kind
        else {
            return None;
        };

        let is_constructor = matches!(kind, ast::MethodKind::Constructor);
        let is_static = matches!(
            kind,
            ast::MethodKind::Operation(ast::Operation {
                is_static: true,
                ..
            })
        );

        if !is_constructor && !is_static {
            return None;
        }

        let ret_ty = self.js_ret.as_ref()?;
        let syn::Type::Path(syn::TypePath {
            qself: None,
            ref path,
        }) = get_ty(ret_ty)
        else {
            return None;
        };

        let seg = path.segments.last()?;
        if seg.ident != class_name.as_str() {
            return None;
        }

        // For static methods, only infer class hoisting when all type args are
        // bare generic param idents — not associated types like `I::Item`.
        if is_static {
            if let syn::PathArguments::AngleBracketed(ref gen_args) = seg.arguments {
                let fn_params: Vec<&Ident> = generics::generic_params(&self.generics)
                    .iter()
                    .map(|p| p.0)
                    .collect();
                for arg in &gen_args.args {
                    match arg {
                        syn::GenericArgument::Lifetime(_) => {}
                        syn::GenericArgument::Type(syn::Type::Path(syn::TypePath {
                            qself: None,
                            path: arg_path,
                        })) if arg_path.segments.len() == 1
                            && matches!(
                                arg_path.segments[0].arguments,
                                syn::PathArguments::None
                            )
                            && fn_params.iter().any(|p| *p == &arg_path.segments[0].ident) => {}
                        _ => return None,
                    }
                }
            }
        }

        Some(path)
    }
}

impl TryToTokens for DescribeImport<'_> {
    fn try_to_tokens(&self, tokens: &mut TokenStream) -> Result<(), Diagnostic> {
        let f = match *self.kind {
            ast::ImportKind::Function(ref f) => f,
            ast::ImportKind::Static(_) => return Ok(()),
            ast::ImportKind::String(_) => return Ok(()),
            ast::ImportKind::Type(_) => return Ok(()),
            ast::ImportKind::Enum(_) => return Ok(()),
        };
        let fn_class_generics = f.get_fn_generics()?;
        let fn_lifetime_params = generics::lifetime_params(&f.generics);
        let argtys = f
            .function
            .arguments
            .iter()
            .map(|arg| {
                generics::generic_to_concrete(
                    (*arg.pat_type.ty).clone(),
                    &fn_class_generics.concrete_defaults,
                    &fn_lifetime_params,
                )
            })
            .collect::<Result<Vec<syn::Type>, Diagnostic>>()?;
        let nargs = f.function.arguments.len() as u32;
        let inform_ret = match &f.js_ret {
            Some(ref t) => {
                let t = generics::generic_to_concrete(
                    t.clone(),
                    &fn_class_generics.concrete_defaults,
                    &fn_lifetime_params,
                )?;
                quote! { <#t as WasmDescribe>::describe(); }
            }
            // async functions always return a JsValue, even if they say to return ()
            None if f.function.r#async => quote! { <JsValue as WasmDescribe>::describe(); },
            None => quote! { <() as WasmDescribe>::describe(); },
        };

        Descriptor {
            ident: &f.shim,
            inner: quote! {
                inform(FUNCTION);
                inform(0);
                inform(#nargs);
                #(<#argtys as WasmDescribe>::describe();)*
                #inform_ret
                #inform_ret
            },
            attrs: f.function.rust_attrs.clone(),
            wasm_bindgen: self.wasm_bindgen,
        }
        .to_tokens(tokens);
        Ok(())
    }
}

impl ToTokens for ast::Enum {
    fn to_tokens(&self, into: &mut TokenStream) {
        let enum_name = &self.rust_name;
        let name_str = self.js_name.to_string();
        let name_len = name_str.len() as u32;
        let name_chars = name_str.chars().map(|c| c as u32);
        let hole = &self.hole;
        let underlying = if self.signed {
            quote! { i32 }
        } else {
            quote! { u32 }
        };
        let cast_clauses = self.variants.iter().map(|variant| {
            let variant_name = &variant.name;
            quote! {
                if js == #enum_name::#variant_name as #underlying {
                    #enum_name::#variant_name
                }
            }
        });
        let try_from_cast_clauses = cast_clauses.clone();
        let wasm_bindgen = &self.wasm_bindgen;
        (quote! {
            #[automatically_derived]
            impl #wasm_bindgen::convert::IntoWasmAbi for #enum_name {
                type Abi = #underlying;

                #[inline]
                fn into_abi(self) -> #underlying {
                    self as #underlying
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::FromWasmAbi for #enum_name {
                type Abi = #underlying;

                #[inline]
                unsafe fn from_abi(js: #underlying) -> Self {
                    #(#cast_clauses else)* {
                        #wasm_bindgen::throw_str("invalid enum value passed")
                    }
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::OptionFromWasmAbi for #enum_name {
                #[inline]
                fn is_none(val: &Self::Abi) -> bool { *val == #hole as #underlying }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::OptionIntoWasmAbi for #enum_name {
                #[inline]
                fn none() -> Self::Abi { #hole as #underlying }
            }

            #[automatically_derived]
            impl #wasm_bindgen::describe::WasmDescribe for #enum_name {
                fn describe() {
                    use #wasm_bindgen::describe::*;
                    inform(ENUM);
                    inform(#name_len);
                    #(inform(#name_chars);)*
                    inform(#hole);
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::__rt::core::convert::From<#enum_name> for
                #wasm_bindgen::JsValue
            {
                fn from(value: #enum_name) -> Self {
                    #wasm_bindgen::JsValue::from_f64((value as #underlying).into())
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::TryFromJsValue for #enum_name {
                fn try_from_js_value_ref(value: &#wasm_bindgen::JsValue) -> #wasm_bindgen::__rt::core::option::Option<Self> {
                    use #wasm_bindgen::__rt::core::convert::TryFrom;
                    let js = f64::try_from(value).ok()? as #underlying;

                    #wasm_bindgen::__rt::core::option::Option::Some(
                        #(#try_from_cast_clauses else)* {
                            return #wasm_bindgen::__rt::core::option::Option::None;
                        }
                    )
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::describe::WasmDescribeVector for #enum_name {
                fn describe_vector() {
                    use #wasm_bindgen::describe::*;
                    inform(VECTOR);
                    <#wasm_bindgen::JsValue as #wasm_bindgen::describe::WasmDescribe>::describe();
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::VectorIntoWasmAbi for #enum_name {
                type Abi = <
                    #wasm_bindgen::__rt::alloc::boxed::Box<[#wasm_bindgen::JsValue]>
                    as #wasm_bindgen::convert::IntoWasmAbi
                >::Abi;

                fn vector_into_abi(
                    vector: #wasm_bindgen::__rt::alloc::boxed::Box<[#enum_name]>
                ) -> Self::Abi {
                    #wasm_bindgen::convert::js_value_vector_into_abi(vector)
                }
            }

            #[automatically_derived]
            impl #wasm_bindgen::convert::VectorFromWasmAbi for #enum_name {
                type Abi = <
                    #wasm_bindgen::__rt::alloc::boxed::Box<[#wasm_bindgen::JsValue]>
                    as #wasm_bindgen::convert::FromWasmAbi
                >::Abi;

                unsafe fn vector_from_abi(
                    js: Self::Abi
                ) -> #wasm_bindgen::__rt::alloc::boxed::Box<[#enum_name]> {
                    #wasm_bindgen::convert::js_value_vector_from_abi(js)
                }
            }
        })
        .to_tokens(into);
    }
}

impl ToTokens for ast::ImportStatic {
    fn to_tokens(&self, into: &mut TokenStream) {
        let ty = &self.ty;

        if let Some(thread_local) = self.thread_local {
            thread_local_import(
                &self.vis,
                &self.rust_name,
                &self.wasm_bindgen,
                ty,
                ty,
                &self.shim,
                thread_local,
            )
            .to_tokens(into)
        } else {
            let vis = &self.vis;
            let name = &self.rust_name;
            let wasm_bindgen = &self.wasm_bindgen;
            let ty = &self.ty;
            let shim_name = &self.shim;
            let init = static_init(wasm_bindgen, ty, shim_name);

            into.extend(quote! {
                #[automatically_derived]
                #[deprecated = "use with `#[wasm_bindgen(thread_local_v2)]` instead"]
            });
            into.extend(
                quote_spanned! { name.span() => #vis static #name: #wasm_bindgen::JsStatic<#ty> = {
                        fn init() -> #ty {
                            #init
                        }
                        #wasm_bindgen::__rt::std::thread_local!(static _VAL: #ty = init(););
                        #wasm_bindgen::JsStatic {
                            __inner: &_VAL,
                        }
                    };
                },
            );
        }

        Descriptor {
            ident: &self.shim,
            inner: quote! {
                <#ty as WasmDescribe>::describe();
            },
            attrs: vec![],
            wasm_bindgen: &self.wasm_bindgen,
        }
        .to_tokens(into);
    }
}

impl ToTokens for ast::ImportString {
    fn to_tokens(&self, into: &mut TokenStream) {
        let js_sys = &self.js_sys;
        let actual_ty: syn::Type = parse_quote!(#js_sys::JsString);

        thread_local_import(
            &self.vis,
            &self.rust_name,
            &self.wasm_bindgen,
            &actual_ty,
            &self.ty,
            &self.shim,
            self.thread_local,
        )
        .to_tokens(into);
    }
}

fn thread_local_import(
    vis: &syn::Visibility,
    name: &Ident,
    wasm_bindgen: &syn::Path,
    actual_ty: &syn::Type,
    ty: &syn::Type,
    shim_name: &Ident,
    thread_local: ast::ThreadLocal,
) -> TokenStream {
    let init = static_init(wasm_bindgen, ty, shim_name);

    match thread_local {
        ast::ThreadLocal::V1 => quote! {
            #wasm_bindgen::__rt::std::thread_local! {
                #[automatically_derived]
                #[deprecated = "use with `#[wasm_bindgen(thread_local_v2)]` instead"]
                #vis static #name: #actual_ty = {
                    #init
                };
            }
        },
        ast::ThreadLocal::V2 => {
            quote! {
                #vis static #name: #wasm_bindgen::JsThreadLocal<#actual_ty> = {
                    fn init() -> #actual_ty {
                        #init
                    }
                    #wasm_bindgen::__wbindgen_thread_local!(#wasm_bindgen, #actual_ty)
                };
            }
        }
    }
}

fn static_init(wasm_bindgen: &syn::Path, ty: &syn::Type, shim_name: &Ident) -> TokenStream {
    let abi_ret = quote! {
        #wasm_bindgen::convert::WasmRet<<#ty as #wasm_bindgen::convert::FromWasmAbi>::Abi>
    };
    quote! {
        #[link(wasm_import_module = "__wbindgen_placeholder__")]
        #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
        extern "C" {
            fn #shim_name() -> #abi_ret;
        }

        #[cfg(not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none"))))]
        unsafe fn #shim_name() -> #abi_ret {
            panic!("cannot access imported statics on non-wasm targets")
        }

        unsafe {
            <#ty as #wasm_bindgen::convert::FromWasmAbi>::from_abi(#shim_name().join())
        }
    }
}

/// Emits the necessary glue tokens for "descriptor", generating an appropriate
/// symbol name as well as attributes around the descriptor function itself.
struct Descriptor<'a, T> {
    ident: &'a Ident,
    inner: T,
    attrs: Vec<syn::Attribute>,
    wasm_bindgen: &'a syn::Path,
}

impl<T: ToTokens> ToTokens for Descriptor<'_, T> {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        // It's possible for the same descriptor to be emitted in two different
        // modules (aka a value imported twice in a crate, each in a separate
        // module). In this case no need to emit duplicate descriptors (which
        // leads to duplicate symbol errors), instead just emit one.
        //
        // It's up to the descriptors themselves to ensure they have unique
        // names for unique items imported, currently done via `ShortHash` and
        // hashing appropriate data into the symbol name.
        thread_local! {
            static DESCRIPTORS_EMITTED: RefCell<HashSet<String>> = RefCell::default();
        }

        let ident = self.ident;

        if !DESCRIPTORS_EMITTED.with(|list| list.borrow_mut().insert(ident.to_string())) {
            return;
        }

        let name = Ident::new(&format!("__wbindgen_describe_{ident}"), ident.span());
        let inner = &self.inner;
        let attrs = &self.attrs;
        let wasm_bindgen = &self.wasm_bindgen;
        (quote! {
            #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
            #[automatically_derived]
            const _: () = {
                #wasm_bindgen::__wbindgen_coverage! {
                #(#attrs)*
                #[no_mangle]
                #[doc(hidden)]
                pub extern "C-unwind" fn #name() {
                    use #wasm_bindgen::describe::*;
                    // See definition of `link_mem_intrinsics` for what this is doing
                    #wasm_bindgen::__rt::link_mem_intrinsics();
                    #inner
                }
                }
            };
        })
        .to_tokens(tokens);
    }
}

fn extern_fn(
    import_name: &Ident,
    attrs: &[syn::Attribute],
    abi_arguments: &[TokenStream],
    abi_argument_names: &[Ident],
    abi_ret: TokenStream,
) -> TokenStream {
    quote! {
        #[cfg(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none")))]
        #(#attrs)*
        #[link(wasm_import_module = "__wbindgen_placeholder__")]
        extern "C" {
            fn #import_name(#(#abi_arguments),*) -> #abi_ret;
        }

        #[cfg(not(all(target_arch = "wasm32", any(target_os = "unknown", target_os = "none"))))]
        unsafe fn #import_name(#(#abi_arguments),*) -> #abi_ret {
            #(
                drop(#abi_argument_names);
            )*
            panic!("cannot call wasm-bindgen imported functions on \
                    non-wasm targets");
        }
    }
}

/// Splats an argument with the given name and ABI type into 4 arguments, one
/// for each primitive that the ABI type splits into.
///
/// Returns an `(args, names)` pair, where `args` is the list of arguments to
/// be inserted into the function signature, and `names` is a list of the names
/// of those arguments.
fn splat(
    wasm_bindgen: &syn::Path,
    name: &Ident,
    abi: &TokenStream,
) -> (Vec<TokenStream>, Vec<Ident>) {
    let mut args = Vec::new();
    let mut names = Vec::new();

    for n in 1_u32..=4 {
        let arg_name = format_ident!("{}_{}", name, n);
        let prim_name = format_ident!("Prim{}", n);
        args.push(quote! {
            #arg_name: <#abi as #wasm_bindgen::convert::WasmAbi>::#prim_name
        });
        names.push(arg_name);
    }

    (args, names)
}

/// Converts `span` into a stream of tokens, and attempts to ensure that `input`
/// has all the appropriate span information so errors in it point to `span`.
fn respan(input: TokenStream, span: &dyn ToTokens) -> TokenStream {
    let mut first_span = Span::call_site();
    let mut last_span = Span::call_site();
    let mut spans = TokenStream::new();
    span.to_tokens(&mut spans);

    for (i, token) in spans.into_iter().enumerate() {
        if i == 0 {
            first_span = Span::call_site().located_at(token.span());
        }
        last_span = Span::call_site().located_at(token.span());
    }

    let mut new_tokens = Vec::new();
    for (i, mut token) in input.into_iter().enumerate() {
        if i == 0 {
            token.set_span(first_span);
        } else {
            token.set_span(last_span);
        }
        new_tokens.push(token);
    }
    new_tokens.into_iter().collect()
}

fn get_ty(mut ty: &syn::Type) -> &syn::Type {
    while let syn::Type::Group(g) = ty {
        ty = &g.elem;
    }
    ty
}
