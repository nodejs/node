use std::collections::{BTreeMap, HashSet};
use std::fmt::Write as _;

use quote::ToTokens;
use serde::Serialize;
use syn::{ImplItem, Item, ItemMod, UseTree, Visibility};

use super::{
    AttrInheritContext, Attrs, CustomType, Enum, Ident, Macros, Method, ModSymbol, Mutability,
    OpaqueType, Path, PathType, RustLink, Struct, Trait,
};
use crate::ast::Function;
use crate::environment::*;

/// Custom Diplomat attribute that can be placed on a struct definition.
#[derive(Debug)]
enum DiplomatStructAttribute {
    /// The `#[diplomat::out]` attribute, used for non-opaque structs that
    /// contain an owned opaque in the form of a `Box`.
    Out,
    /// An attribute that can correspond to a type (struct or enum).
    TypeAttr(DiplomatTypeAttribute),
}

/// Custom Diplomat attribute that can be placed on an enum or struct definition.
#[derive(Debug)]
enum DiplomatTypeAttribute {
    /// The `#[diplomat::opaque]` attribute, used for marking a type as opaque.
    /// Note that opaque structs can be borrowed in return types, but cannot
    /// be passed into a function behind a mutable reference.
    Opaque,
    /// The `#[diplomat::opaque_mut]` attribute, used for marking a type as
    /// opaque and mutable.
    /// Note that mutable opaque types can never be borrowed in return types
    /// (even immutably!), but can be passed into a function behind a mutable
    /// reference.
    OpaqueMut,
}

impl DiplomatStructAttribute {
    /// Parses a [`DiplomatStructAttribute`] from an array of [`syn::Attribute`]s.
    /// If more than one kind is found, an error is returned containing all the
    /// ones encountered, since all the current attributes are disjoint.
    fn parse(attrs: &[syn::Attribute]) -> Result<Option<Self>, Vec<Self>> {
        let mut buf = String::with_capacity(32);
        let mut res = Ok(None);
        for attr in attrs {
            buf.clear();
            write!(&mut buf, "{}", attr.path().to_token_stream()).unwrap();
            let parsed = match buf.as_str() {
                "diplomat :: out" => Some(Self::Out),
                "diplomat :: opaque" => Some(Self::TypeAttr(DiplomatTypeAttribute::Opaque)),
                "diplomat :: opaque_mut" => Some(Self::TypeAttr(DiplomatTypeAttribute::OpaqueMut)),
                _ => None,
            };

            if let Some(parsed) = parsed {
                match res {
                    Ok(None) => res = Ok(Some(parsed)),
                    Ok(Some(first)) => res = Err(vec![first, parsed]),
                    Err(ref mut errors) => errors.push(parsed),
                }
            }
        }

        res
    }
}

impl DiplomatTypeAttribute {
    /// Parses a [`DiplomatTypeAttribute`] from an array of [`syn::Attribute`]s.
    /// If more than one kind is found, an error is returned containing all the
    /// ones encountered, since all the current attributes are disjoint.
    fn parse(attrs: &[syn::Attribute]) -> Result<Option<Self>, Vec<Self>> {
        let mut buf = String::with_capacity(32);
        let mut res = Ok(None);
        for attr in attrs {
            buf.clear();
            write!(&mut buf, "{}", attr.path().to_token_stream()).unwrap();
            let parsed = match buf.as_str() {
                "diplomat :: opaque" => Some(Self::Opaque),
                "diplomat :: opaque_mut" => Some(Self::OpaqueMut),
                _ => None,
            };

            if let Some(parsed) = parsed {
                match res {
                    Ok(None) => res = Ok(Some(parsed)),
                    Ok(Some(first)) => res = Err(vec![first, parsed]),
                    Err(ref mut errors) => errors.push(parsed),
                }
            }
        }

        res
    }
}

#[derive(Clone, Serialize, Debug)]
#[non_exhaustive]
pub struct Module {
    pub name: Ident,
    pub imports: Vec<(Path, Ident)>,
    pub declared_types: BTreeMap<Ident, CustomType>,
    pub declared_traits: BTreeMap<Ident, Trait>,
    pub declared_functions: BTreeMap<Ident, Function>,
    pub sub_modules: Vec<Module>,
    pub attrs: Attrs,
}

/// Contains all items needed to build an AST representation of a given [`Module`],
/// as we traverse through [`syn::ItemMod`]. We build this up in [`ModuleBuilder::add`]
struct ModuleBuilder {
    custom_types_by_name: BTreeMap<Ident, CustomType>,
    custom_traits_by_name: BTreeMap<Ident, Trait>,
    functions_by_name: BTreeMap<Ident, Function>,
    sub_modules: Vec<Module>,
    imports: Vec<(Path, Ident)>,
    analyze_types: bool,
    type_parent_attrs: Attrs,
    impl_parent_attrs: Attrs,
    mod_macros: Macros,
}

impl ModuleBuilder {
    fn add(&mut self, a: &Item) {
        match a {
            Item::Use(u) => {
                if self.analyze_types {
                    extract_imports(&Path::empty(), &u.tree, &mut self.imports);
                }
            }
            Item::Struct(strct) => {
                if self.analyze_types {
                    let custom_type = match DiplomatStructAttribute::parse(&strct.attrs[..]) {
                        Ok(None) => {
                            CustomType::Struct(Struct::new(strct, false, &self.type_parent_attrs))
                        }
                        Ok(Some(DiplomatStructAttribute::Out)) => {
                            CustomType::Struct(Struct::new(strct, true, &self.type_parent_attrs))
                        }
                        Ok(Some(DiplomatStructAttribute::TypeAttr(
                            DiplomatTypeAttribute::Opaque,
                        ))) => CustomType::Opaque(OpaqueType::new_struct(
                            strct,
                            Mutability::Immutable,
                            &self.type_parent_attrs,
                        )),
                        Ok(Some(DiplomatStructAttribute::TypeAttr(
                            DiplomatTypeAttribute::OpaqueMut,
                        ))) => CustomType::Opaque(OpaqueType::new_struct(
                            strct,
                            Mutability::Mutable,
                            &self.type_parent_attrs,
                        )),
                        Err(errors) => {
                            panic!("Multiple conflicting Diplomat struct attributes, there can be at most one: {errors:?}");
                        }
                    };

                    self.custom_types_by_name
                        .insert(Ident::from(&strct.ident), custom_type);
                }
            }

            Item::Enum(enm) => {
                if self.analyze_types {
                    let ident = (&enm.ident).into();
                    let custom_enum = match DiplomatTypeAttribute::parse(&enm.attrs[..]) {
                        Ok(None) => CustomType::Enum(Enum::new(enm, &self.type_parent_attrs)),
                        Ok(Some(DiplomatTypeAttribute::Opaque)) => {
                            CustomType::Opaque(OpaqueType::new_enum(
                                enm,
                                Mutability::Immutable,
                                &self.type_parent_attrs,
                            ))
                        }
                        Ok(Some(DiplomatTypeAttribute::OpaqueMut)) => CustomType::Opaque(
                            OpaqueType::new_enum(enm, Mutability::Mutable, &self.type_parent_attrs),
                        ),
                        Err(errors) => {
                            panic!("Multiple conflicting Diplomat enum attributes, there can be at most one: {errors:?}");
                        }
                    };
                    self.custom_types_by_name.insert(ident, custom_enum);
                }
            }

            Item::Impl(imp) => {
                if self.analyze_types && imp.trait_.is_none() {
                    let self_path = match imp.self_ty.as_ref() {
                        syn::Type::Path(s) => PathType::from(s),
                        _ => panic!("Self type not found"),
                    };
                    let mut impl_attrs = self.impl_parent_attrs.clone();
                    impl_attrs.add_attrs(&imp.attrs);
                    let method_parent_attrs =
                        impl_attrs.attrs_for_inheritance(AttrInheritContext::MethodFromImpl);
                    let self_ident = self_path.path.elements.last().unwrap();

                    // Do a prepass to evaluate macros:
                    let mut impl_item_vec = Vec::new();
                    for i in &imp.items {
                        match i {
                            ImplItem::Fn(f) => {
                                impl_item_vec.push(ImplItem::Fn(f.clone()));
                            }
                            ImplItem::Macro(mac) => {
                                let mut items = self.mod_macros.evaluate_impl_item_macro(mac);
                                impl_item_vec.append(&mut items);
                            }
                            _ => {}
                        }
                    }

                    // Then only add functions to the block:
                    let mut new_methods = impl_item_vec
                        .iter()
                        .filter_map(|i| match i {
                            ImplItem::Fn(m) => Some(m),
                            _ => None,
                        })
                        .filter(|m| {
                            let is_public = matches!(m.vis, Visibility::Public(_));
                            let has_diplomat_attrs = m.attrs.iter().any(|a| {
                                a.path().segments.iter().next().unwrap().ident == "diplomat"
                            });
                            assert!(
                                is_public || !has_diplomat_attrs,
                                "Non-public method with diplomat attrs found: {self_ident}::{}",
                                m.sig.ident
                            );
                            is_public
                        })
                        .map(|m| {
                            Method::from_syn(
                                m,
                                self_path.clone(),
                                Some(&imp.generics),
                                &method_parent_attrs,
                            )
                        })
                        .collect();

                    match self.custom_types_by_name.get_mut(self_ident)
                                                .expect("Diplomat currently requires impls to be in the same module as their self type") {
                        CustomType::Struct(strct) => {
                            strct.methods.append(&mut new_methods);
                        }
                        CustomType::Opaque(strct) => {
                            strct.methods.append(&mut new_methods);
                        }
                        CustomType::Enum(enm) => {
                            enm.methods.append(&mut new_methods);
                        }
                    }
                }
            }
            Item::Mod(item_mod) => {
                self.sub_modules.push(Module::from_syn(item_mod, false));
            }
            Item::Trait(trt) => {
                if self.analyze_types {
                    let ident = (&trt.ident).into();
                    let trt = Trait::new(trt, &self.type_parent_attrs);
                    self.custom_traits_by_name.insert(ident, trt);
                }
            }
            Item::Macro(mac) => {
                if let Some(i) = &mac.ident {
                    let macro_rules_attr = mac.attrs.iter().find(|a| {
                        a.path() == &syn::parse_str::<syn::Path>("diplomat::macro_rules").unwrap()
                    });

                    if macro_rules_attr.is_some() {
                        self.mod_macros.add_item_macro(mac);
                    } else {
                        println!(
                            r#"WARNING: Found macro_rules definition "macro_rules! {i}" with no #[diplomat::macro_rules] attribute. This will not be evaluated in Diplomat bindings."#
                        );
                    }
                } else {
                    let items = self.mod_macros.evaluate_item_macro(mac);
                    for i in items {
                        self.add(&i);
                    }
                }
            }
            Item::Fn(f) => {
                if self.analyze_types {
                    let is_public = matches!(f.vis, Visibility::Public(_));
                    let has_diplomat_attrs = f
                        .attrs
                        .iter()
                        .any(|a| a.path().segments.iter().next().unwrap().ident == "diplomat");
                    assert!(
                        is_public || !has_diplomat_attrs,
                        "Non-public function with diplomat attrs found: {}",
                        f.sig.ident
                    );
                    if is_public {
                        let parent_attrs = self
                            .impl_parent_attrs
                            .attrs_for_inheritance(AttrInheritContext::MethodFromImpl);
                        let out = Function::from_syn(f, &parent_attrs);
                        self.functions_by_name.insert(out.name.clone(), out);
                    }
                }
            }
            _ => {}
        }
    }
}

impl Module {
    pub fn all_rust_links(&self) -> HashSet<&RustLink> {
        let mut rust_links = self
            .declared_types
            .values()
            .flat_map(|t| t.all_rust_links())
            .collect::<HashSet<_>>();

        self.sub_modules.iter().for_each(|m| {
            rust_links.extend(m.all_rust_links().iter());
        });
        rust_links
    }

    pub fn insert_all_types(&self, in_path: Path, out: &mut Env) {
        let mut mod_symbols = ModuleEnv::new(self.attrs.clone());

        self.imports.iter().for_each(|(path, name)| {
            mod_symbols.insert(name.clone(), ModSymbol::Alias(path.clone()));
        });

        self.declared_types.iter().for_each(|(k, v)| {
            if mod_symbols
                .insert(k.clone(), ModSymbol::CustomType(v.clone()))
                .is_some()
            {
                panic!("Two types were declared with the same name, this needs to be implemented (key: {k})");
            }
        });

        self.declared_traits.iter().for_each(|(k, v)| {
            if mod_symbols
                .insert(k.clone(), ModSymbol::Trait(v.clone()))
                .is_some()
            {
                panic!("Two traits were declared with the same name, this needs to be implemented (key: {k})");
            }
        });

        self.declared_functions.iter().for_each(|(k, f)| {
            if mod_symbols.insert(k.clone(), ModSymbol::Function(f.clone())).is_some() {
                panic!("Two functions were declared with the same name, this needs to be implemented (key: {k})")
            }
        });

        let path_to_self = in_path.sub_path(self.name.clone());
        self.sub_modules.iter().for_each(|m| {
            m.insert_all_types(path_to_self.clone(), out);
            mod_symbols.insert(m.name.clone(), ModSymbol::SubModule(m.name.clone()));
        });

        out.insert(path_to_self, mod_symbols);
    }

    pub fn from_syn(input: &ItemMod, force_analyze: bool) -> Module {
        let mod_attrs: Attrs = (&*input.attrs).into();

        let mut mst = ModuleBuilder {
            custom_types_by_name: BTreeMap::new(),
            custom_traits_by_name: BTreeMap::new(),
            functions_by_name: BTreeMap::new(),
            sub_modules: Vec::new(),
            imports: Vec::new(),
            analyze_types: force_analyze
                || input
                    .attrs
                    .iter()
                    .any(|a| a.path().to_token_stream().to_string() == "diplomat :: bridge"),
            impl_parent_attrs: mod_attrs
                .attrs_for_inheritance(AttrInheritContext::MethodOrImplFromModule),
            type_parent_attrs: mod_attrs.attrs_for_inheritance(AttrInheritContext::Type),
            mod_macros: Macros::new(),
        };

        input
            .content
            .as_ref()
            .map(|t| &t.1[..])
            .unwrap_or_default()
            .iter()
            .for_each(|a| {
                mst.add(a);
            });

        Module {
            name: (&input.ident).into(),
            imports: mst.imports,
            declared_types: mst.custom_types_by_name,
            declared_traits: mst.custom_traits_by_name,
            declared_functions: mst.functions_by_name,
            sub_modules: mst.sub_modules,
            attrs: mod_attrs,
        }
    }
}

fn extract_imports(base_path: &Path, use_tree: &UseTree, out: &mut Vec<(Path, Ident)>) {
    match use_tree {
        UseTree::Name(name) => out.push((
            base_path.sub_path((&name.ident).into()),
            (&name.ident).into(),
        )),
        UseTree::Path(path) => {
            extract_imports(&base_path.sub_path((&path.ident).into()), &path.tree, out)
        }
        UseTree::Glob(_) => todo!("Glob imports are not yet supported"),
        UseTree::Group(group) => {
            group
                .items
                .iter()
                .for_each(|i| extract_imports(base_path, i, out));
        }
        UseTree::Rename(rename) => out.push((
            base_path.sub_path((&rename.ident).into()),
            (&rename.rename).into(),
        )),
    }
}

#[derive(Serialize, Clone, Debug)]
#[non_exhaustive]
pub struct File {
    pub modules: BTreeMap<String, Module>,
}

impl File {
    /// Fuses all declared types into a single environment `HashMap`.
    pub fn all_types(&self) -> Env {
        let mut out = Env::default();
        let mut top_symbols = ModuleEnv::new(Default::default());

        self.modules.values().for_each(|m| {
            m.insert_all_types(Path::empty(), &mut out);
            top_symbols.insert(m.name.clone(), ModSymbol::SubModule(m.name.clone()));
        });

        out.insert(Path::empty(), top_symbols);

        out
    }

    pub fn all_rust_links(&self) -> HashSet<&RustLink> {
        self.modules
            .values()
            .flat_map(|m| m.all_rust_links().into_iter())
            .collect()
    }
}

impl From<&syn::File> for File {
    /// Get all custom types across all modules defined in a given file.
    fn from(file: &syn::File) -> File {
        let mut out = BTreeMap::new();
        file.items.iter().for_each(|i| {
            if let Item::Mod(item_mod) = i {
                out.insert(
                    item_mod.ident.to_string(),
                    Module::from_syn(item_mod, false),
                );
            }
        });

        File { modules: out }
    }
}

#[cfg(test)]
mod tests {
    use insta::{self, Settings};

    use syn;

    use crate::ast::{File, Module};

    #[test]
    fn simple_mod() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(Module::from_syn(
                &syn::parse_quote! {
                    mod ffi {
                        struct NonOpaqueStruct {
                            a: i32,
                            b: Box<NonOpaqueStruct>
                        }

                        impl NonOpaqueStruct {
                            pub fn new(x: i32) -> NonOpaqueStruct {
                                unimplemented!();
                            }

                            pub fn set_a(&mut self, new_a: i32) {
                                self.a = new_a;
                            }
                        }

                        #[diplomat::opaque]
                        struct OpaqueStruct {
                            a: SomeExternalType
                        }

                        impl OpaqueStruct {
                            pub fn new() -> Box<OpaqueStruct> {
                                unimplemented!();
                            }

                            pub fn get_string(&self) -> String {
                                unimplemented!()
                            }
                        }

                        pub fn test_function() {}
                        pub fn other_test_function(x : i32) -> NonOpaqueStruct {
                            unimplemented!();
                        }
                    }
                },
                true
            ));
        });
    }

    #[test]
    fn method_visibility() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(Module::from_syn(
                &syn::parse_quote! {
                    #[diplomat::bridge]
                    mod ffi {
                        struct Foo {}

                        impl Foo {
                            pub fn pub_fn() {
                                unimplemented!()
                            }
                            pub(crate) fn pub_crate_fn() {
                                unimplemented!()
                            }
                            pub(super) fn pub_super_fn() {
                                unimplemented!()
                            }
                            fn priv_fn() {
                                unimplemented!()
                            }
                        }
                    }
                },
                true
            ));
        });
    }

    #[test]
    fn import_in_non_diplomat_not_analyzed() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(File::from(&syn::parse_quote! {
                #[diplomat::bridge]
                mod ffi {
                    struct Foo {}
                }

                mod other {
                    use something::*;
                }
            }));
        });
    }
}
