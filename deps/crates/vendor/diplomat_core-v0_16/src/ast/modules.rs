use std::cell::RefCell;
use std::collections::{BTreeMap, BTreeSet, HashMap, HashSet};
use std::fmt::Write as _;

use quote::ToTokens;
use serde::Serialize;
use syn::spanned::Spanned;
use syn::{ImplItem, Item, ItemMod, UseTree, Visibility};

use super::{
    AttrInheritContext, Attrs, CustomType, Enum, Ident, Macros, Method, ModSymbol, Mutability,
    OpaqueType, Path, PathType, RustLink, Struct, Trait,
};
use crate::ast::idents::{FromWithSpan, IntoWithSpan};
use crate::ast::logging::{create_report, create_simple_report, AstReport};
use crate::ast::{Function, SpanLocation};
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

/// File name -> List of macro defs
type ModuleCacheMap = HashMap<String, BTreeMap<syn::Ident, super::MacroDef>>;

/// Information for how to parse #[diplomat::include].
/// For proc_macro:
/// Only needs to know the `base_path` from which to include files from.
/// Cache should be set to `None` (proc_macro does not like caching).
///
/// For HIR:
/// Holds a reference to the `base_path` from which to include files from.
/// Also holds a reference to a persistent cache (behind a `RefCell`) to store macro information.
/// The cache should be created by the top level HIR function.
#[derive(Clone, Debug)]
pub struct ModuleIncludeInfo<'a> {
    /// Where to parse files from.
    pub(crate) base_path: &'a std::path::Path,
    /// Cache across Module::from_syn calls.
    pub(crate) cache: Option<&'a RefCell<ModuleCacheMap>>,
}

impl<'a> ModuleIncludeInfo<'a> {
    pub fn new(base_path: &'a std::path::Path, cache: Option<&'a RefCell<ModuleCacheMap>>) -> Self {
        Self { base_path, cache }
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
struct ModuleBuilder<'a> {
    custom_types_by_name: BTreeMap<Ident, CustomType>,
    custom_traits_by_name: BTreeMap<Ident, Trait>,
    /// Types that are private (so if we encounter their impl blocks, they can be safely ignored)
    private_types_by_name: BTreeSet<Ident>,
    functions_by_name: BTreeMap<Ident, Function>,
    sub_modules: Vec<Module>,
    imports: Vec<(Path, Ident)>,
    /// As we traverse through the module, are we inside of #[diplomat::bridge]?
    /// If so, then `analyze_types` is set to true, and types, functions, and traits are all updated according to information parsed.
    ///
    /// Otherwise, we traverse through modules until we find a module marked by #[diplomat::bridge]
    analyze_types: bool,
    /// Are we to only analyze public structs or enums?
    skip_private_items: bool,
    type_parent_attrs: Attrs,
    impl_parent_attrs: Attrs,
    mod_macros: Macros,
    include_info: Option<ModuleIncludeInfo<'a>>,
    /// Where the module is stored as a file.
    module_location: &'a SpanLocation,
}

impl<'a> ModuleBuilder<'a> {
    fn add(&mut self, a: &Item) {
        match a {
            Item::Use(u) if self.analyze_types => {
                extract_imports(
                    &Path::empty(),
                    &u.tree,
                    &mut self.imports,
                    self.module_location,
                );
            }
            Item::Struct(strct) if self.analyze_types => {
                if self.skip_private_items && !matches!(strct.vis, syn::Visibility::Public(..)) {
                    self.private_types_by_name
                        .insert((&strct.ident).spanned_into(self.module_location));
                    return;
                }
                let custom_type = match DiplomatStructAttribute::parse(&strct.attrs[..]) {
                    Ok(None) => CustomType::Struct(Struct::new(
                        strct,
                        false,
                        &self.type_parent_attrs,
                        self.module_location,
                    )),
                    Ok(Some(DiplomatStructAttribute::Out)) => CustomType::Struct(Struct::new(
                        strct,
                        true,
                        &self.type_parent_attrs,
                        self.module_location,
                    )),
                    Ok(Some(DiplomatStructAttribute::TypeAttr(DiplomatTypeAttribute::Opaque))) => {
                        CustomType::Opaque(OpaqueType::new_struct(
                            strct,
                            Mutability::Immutable,
                            &self.type_parent_attrs,
                            self.module_location,
                        ))
                    }
                    Ok(Some(DiplomatStructAttribute::TypeAttr(
                        DiplomatTypeAttribute::OpaqueMut,
                    ))) => CustomType::Opaque(OpaqueType::new_struct(
                        strct,
                        Mutability::Mutable,
                        &self.type_parent_attrs,
                        self.module_location,
                    )),
                    Err(errors) => {
                        create_simple_report((&strct.ident).spanned_into(self.module_location), "Multiple conflicting Diplomat struct attributes, there can be at most one.".into(), format!("{errors:?}"));
                    }
                };

                self.custom_types_by_name.insert(
                    (&strct.ident).spanned_into(self.module_location),
                    custom_type,
                );
            }

            Item::Enum(enm) if self.analyze_types => {
                let ident = (&enm.ident).spanned_into(self.module_location);

                if self.skip_private_items && !matches!(enm.vis, syn::Visibility::Public(..)) {
                    self.private_types_by_name.insert(ident);
                    return;
                }

                let custom_enum = match DiplomatTypeAttribute::parse(&enm.attrs[..]) {
                    Ok(None) => CustomType::Enum(Enum::new(
                        enm,
                        &self.type_parent_attrs,
                        self.module_location,
                    )),
                    Ok(Some(DiplomatTypeAttribute::Opaque)) => {
                        CustomType::Opaque(OpaqueType::new_enum(
                            enm,
                            Mutability::Immutable,
                            &self.type_parent_attrs,
                            self.module_location,
                        ))
                    }
                    Ok(Some(DiplomatTypeAttribute::OpaqueMut)) => {
                        CustomType::Opaque(OpaqueType::new_enum(
                            enm,
                            Mutability::Mutable,
                            &self.type_parent_attrs,
                            self.module_location,
                        ))
                    }
                    Err(errors) => {
                        create_simple_report((&enm.ident).spanned_into(self.module_location), "Multiple conflicting Diplomat enum attributes, there can be at most one.".into(), format!("{errors:?}"));
                    }
                };
                self.custom_types_by_name.insert(ident, custom_enum);
            }

            Item::Impl(imp) if self.analyze_types && imp.trait_.is_none() => {
                let self_path = match imp.self_ty.as_ref() {
                    syn::Type::Path(s) => PathType::spanned_from(s, self.module_location),
                    _ => {
                        create_report(AstReport::new(
                            "Self type not found".into(),
                            Some(imp.self_ty.span().spanned_into(self.module_location)),
                            "Expected Path type".into(),
                            vec![],
                        ));
                    }
                };
                let mut impl_attrs = self.impl_parent_attrs.clone();
                impl_attrs.add_attrs(&imp.attrs, self.module_location);
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
                            let mut items = self
                                .mod_macros
                                .evaluate_impl_item_macro(mac, self.module_location);
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
                        let has_diplomat_attrs = m
                            .attrs
                            .iter()
                            .any(|a| a.path().segments.iter().next().unwrap().ident == "diplomat");
                        if !is_public && has_diplomat_attrs {
                            create_simple_report(
                                (&m.sig.ident).spanned_into(self.module_location),
                                "Found non-public method with diplomat attrs".into(),
                                "Remove #[diplomat::*] attributes.".into(),
                            );
                        }
                        is_public
                    })
                    .map(|m| {
                        Method::from_syn(
                            m,
                            self_path.clone(),
                            Some(&imp.generics),
                            &method_parent_attrs,
                            self.module_location,
                        )
                    })
                    .collect();

                if self.skip_private_items && self.private_types_by_name.contains(self_ident) {
                    return;
                }

                match self
                    .custom_types_by_name
                    .get_mut(self_ident)
                    .unwrap_or_else(|| {
                        create_simple_report(
                            self_ident.clone(),
                            "Diplomat requires impls to be in the same module as their type".into(),
                            format!("{self_ident} should be defined in the same module."),
                        );
                    }) {
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
            Item::Mod(item_mod) => {
                self.sub_modules.push(Module::from_syn(
                    item_mod,
                    false,
                    self.include_info.clone(),
                    self.module_location,
                ));
            }
            Item::Trait(trt) if self.analyze_types => {
                let ident = (&trt.ident).spanned_into(self.module_location);
                let trt = Trait::new(trt, &self.type_parent_attrs, self.module_location);
                self.custom_traits_by_name.insert(ident, trt);
            }
            Item::Macro(mac) if self.analyze_types => {
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
                    let items = self
                        .mod_macros
                        .evaluate_item_macro(mac, self.module_location);
                    for i in items {
                        self.add(&i);
                    }
                }
            }
            Item::Fn(f) if self.analyze_types => {
                let is_public = matches!(f.vis, Visibility::Public(_));
                let has_diplomat_attrs = f
                    .attrs
                    .iter()
                    .any(|a| a.path().segments.iter().next().unwrap().ident == "diplomat");
                if !is_public && has_diplomat_attrs {
                    create_simple_report(
                        (&f.sig.ident).spanned_into(self.module_location),
                        "Found non-public method with diplomat attrs".into(),
                        "Remove #[diplomat::*] attributes.".into(),
                    );
                }
                if is_public {
                    let parent_attrs = self
                        .impl_parent_attrs
                        .attrs_for_inheritance(AttrInheritContext::MethodFromImpl);
                    let out = Function::from_syn(f, &parent_attrs, self.module_location);
                    self.functions_by_name.insert(out.name.clone(), out);
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
                create_simple_report(
                    k.clone(),
                    "Two types were declared with the same name (this is currently unsupported)"
                        .into(),
                    "Duplicate type".into(),
                );
            }
        });

        self.declared_traits.iter().for_each(|(k, v)| {
            if mod_symbols
                .insert(k.clone(), ModSymbol::Trait(v.clone()))
                .is_some()
            {
                create_simple_report(
                    k.clone(),
                    "Two traits were declared with the same name (this is currently unsupported)"
                        .into(),
                    "Duplicate trait".into(),
                );
            }
        });

        self.declared_functions.iter().for_each(|(k, f)| {
            if mod_symbols.insert(k.clone(), ModSymbol::Function(f.clone())).is_some() {
                create_simple_report(k.clone(), "Two functions were declared with the same name (this is currently unsupported)".into(), "Duplicate function".into());
            }
        });

        let path_to_self = in_path.sub_path(self.name.clone());
        self.sub_modules.iter().for_each(|m| {
            m.insert_all_types(path_to_self.clone(), out);
            mod_symbols.insert(m.name.clone(), ModSymbol::SubModule(m.name.clone()));
        });

        out.insert(path_to_self, mod_symbols);
    }

    /// Convert an [`ItemMod`] to a [`Module`].
    ///
    /// `force_analyze` is for forcibly parsing the module in the case where we know the `#[diplomat::bridge]` attribute should be present,
    /// but proc_macro (or some other analyzer) has removed the attribute in advance.
    pub fn from_syn<'a>(
        input: &ItemMod,
        force_analyze: bool,
        include_info: Option<ModuleIncludeInfo<'a>>,
        module_location: &SpanLocation,
    ) -> Module {
        let mod_attrs: Attrs = (&*input.attrs).spanned_into(module_location);

        let mod_macros = if let Some(inc) = &include_info {
            let defs = parse_macro_file(input, force_analyze, inc.clone(), module_location)
                .unwrap_or_else(|e| {
                    panic!(
                        "Could not parse macro definitions in {:?}: {e}",
                        inc.base_path
                    )
                });
            Macros { defs }
        } else {
            Macros::new()
        };

        let mut mst = ModuleBuilder {
            custom_types_by_name: BTreeMap::new(),
            custom_traits_by_name: BTreeMap::new(),
            private_types_by_name: BTreeSet::new(),
            functions_by_name: BTreeMap::new(),
            sub_modules: Vec::new(),
            imports: Vec::new(),
            analyze_types: force_analyze
                || input
                    .attrs
                    .iter()
                    .any(|a| a.path().to_token_stream().to_string() == "diplomat :: bridge"),
            skip_private_items: input.attrs.iter().any(|a| {
                a.path().to_token_stream().to_string() == "diplomat :: skip_private_items"
            }),
            impl_parent_attrs: mod_attrs
                .attrs_for_inheritance(AttrInheritContext::MethodOrImplFromModule),
            type_parent_attrs: mod_attrs.attrs_for_inheritance(AttrInheritContext::Type),
            mod_macros,
            include_info,
            module_location,
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
            name: (&input.ident).spanned_into(module_location),
            imports: mst.imports,
            declared_types: mst.custom_types_by_name,
            declared_traits: mst.custom_traits_by_name,
            declared_functions: mst.functions_by_name,
            sub_modules: mst.sub_modules,
            attrs: mod_attrs,
        }
    }
}

fn extract_imports(
    base_path: &Path,
    use_tree: &UseTree,
    out: &mut Vec<(Path, Ident)>,
    module_location: &SpanLocation,
) {
    match use_tree {
        UseTree::Name(name) => out.push((
            base_path.sub_path((&name.ident).spanned_into(module_location)),
            (&name.ident).spanned_into(module_location),
        )),
        UseTree::Path(path) => extract_imports(
            &base_path.sub_path((&path.ident).spanned_into(module_location)),
            &path.tree,
            out,
            module_location,
        ),
        UseTree::Glob(_) => todo!("Glob imports are not yet supported"),
        UseTree::Group(group) => {
            group
                .items
                .iter()
                .for_each(|i| extract_imports(base_path, i, out, module_location));
        }
        UseTree::Rename(rename) => out.push((
            base_path.sub_path((&rename.ident).spanned_into(module_location)),
            (&rename.rename).spanned_into(module_location),
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

    pub fn from_syn(
        file: &syn::File,
        include_info: Option<ModuleIncludeInfo>,
        entry_location: &SpanLocation,
    ) -> File {
        let mut out = BTreeMap::new();
        file.items.iter().for_each(|i| {
            if let Item::Mod(item_mod) = i {
                let module_location = match entry_location {
                    SpanLocation::FilePath(p) => {
                        // Entry location points to a folder, so we can just join to add a subfolder:
                        let pth = std::path::Path::new(p);
                        let new_pth = pth.join(format!("{}.rs", item_mod.ident));
                        if !new_pth.exists() {
                            // We could just be in an item submodule, so we stay in the entry module path:
                            &SpanLocation::FilePath(p.clone())
                        } else {
                            &SpanLocation::FilePath(new_pth.to_string_lossy().into())
                        }
                    }
                    SpanLocation::None => &SpanLocation::None,
                    SpanLocation::LocalSource(..) => unreachable!("Span Location for ast::File should never be LocalSource, we expect a filepath.")
                };
                out.insert(
                    item_mod.ident.to_string(),
                    Module::from_syn(item_mod, false, include_info.clone(), module_location),
                );
            }
        });

        File { modules: out }
    }
}

pub fn parse_macro_file(
    m: &ItemMod,
    force_analyze: bool,
    include_info: ModuleIncludeInfo,
    module_location: &SpanLocation,
) -> Result<BTreeMap<syn::Ident, super::MacroDef>, std::io::Error> {
    let contains_bridge = m
        .attrs
        .iter()
        .any(|a| a.path().to_token_stream().to_string() == "diplomat :: bridge")
        || force_analyze;

    if !contains_bridge {
        return Ok(BTreeMap::new());
    }

    let attrs: Attrs = (*m.attrs).spanned_into(module_location);

    let mut previously_hit = BTreeMap::<syn::Ident, String>::new();

    let mut ret = BTreeMap::new();
    for i in &attrs.includes {
        let mut defs = if let Some(inner) = include_info
            .cache
            .as_ref()
            .and_then(|c| c.borrow().get(&i.path).cloned())
        {
            inner
        } else {
            let inc_loc = include_info.base_path.join(i.path.clone());
            let module_location = &SpanLocation::FilePath(inc_loc.to_string_lossy().into());
            let file_contents = std::fs::read_to_string(inc_loc)?;
            let syn_file = syn::parse_file(&file_contents).unwrap_or_else(|e| {
                create_report(AstReport::new(
                    "Error reading file".into(),
                    Some(e.span().spanned_into(module_location)),
                    e.to_string(),
                    vec![],
                ))
            });
            // Parse the module (we're just interested in the macros, but this is a quick shortcut to do that)
            let mut mst = ModuleBuilder {
                custom_types_by_name: BTreeMap::new(),
                custom_traits_by_name: BTreeMap::new(),
                functions_by_name: BTreeMap::new(),
                sub_modules: vec![],
                imports: vec![],
                analyze_types: true,
                impl_parent_attrs: attrs
                    .attrs_for_inheritance(AttrInheritContext::MethodOrImplFromModule),
                type_parent_attrs: attrs.attrs_for_inheritance(AttrInheritContext::Type),
                mod_macros: Macros::new(),
                include_info: None,
                private_types_by_name: BTreeSet::new(),
                skip_private_items: false,
                module_location,
            };
            for i in syn_file.items {
                mst.add(&i);
            }

            if let Some(c) = &include_info.cache {
                c.borrow_mut()
                    .insert(i.path.clone(), mst.mod_macros.defs.clone());
            }
            mst.mod_macros.defs
        };

        // ModuleBuilder catches redefinitions within a given module, but we want to make sure
        // there are no collisions between multiple #[diplomat::include()] files:
        for id in defs.keys() {
            if let Some(pth) = previously_hit.get(id) {
                create_report(AstReport::new(
                    format!("Duplicate macro definition of {id} found"),
                    Some(id.span().spanned_into(module_location)),
                    format!("Previously defined in {pth}"),
                    vec![],
                ));
            } else {
                previously_hit.insert(id.clone(), i.path.clone());
            }
        }
        ret.append(&mut defs);
    }
    Ok(ret)
}

impl From<&syn::File> for File {
    /// Get all custom types across all modules defined in a given file.
    fn from(file: &syn::File) -> File {
        File::from_syn(file, None, &SpanLocation::None)
    }
}

#[cfg(test)]
mod tests {
    use insta::{self, Settings};

    use syn;

    use crate::ast::{File, Module, SpanLocation};

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
                true,
                None,
                &SpanLocation::None,
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
                true,
                None,
                &SpanLocation::None,
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

    #[test]
    fn struct_visibility() {
        let mut settings = Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_yaml_snapshot!(File::from(&syn::parse_quote! {
                #[diplomat::bridge]
                #[diplomat::skip_private_items]
                mod ffi {
                    struct Foo {}

                    #[diplomat::opaque]
                    pub struct Opaque{
                        foo: Foo,
                    }
                }
            }));
        });
    }
}
