use serde::Serialize;

use super::docs::Docs;
use super::{Attrs, Ident, LifetimeEnv, Param, PathType, TraitSelfParam, TypeName};

/// A trait declaration in an FFI module.
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Debug)]
#[non_exhaustive]
pub struct Trait {
    pub name: Ident,
    pub lifetimes: LifetimeEnv,
    pub methods: Vec<TraitMethod>,
    pub docs: Docs,
    pub attrs: Attrs,
    pub is_sync: bool,
    pub is_send: bool,
}

#[derive(Clone, PartialEq, Eq, Hash, Serialize, Debug)]
#[non_exhaustive]
pub struct TraitMethod {
    pub name: Ident,
    pub abi_name: Ident,
    pub self_param: Option<TraitSelfParam>,
    // corresponds to the types in Function(Vec<Box<TypeName>>, Box<TypeName>)
    // the callback type; except here the params aren't anonymous
    pub params: Vec<Param>,
    pub output_type: Option<TypeName>,
    pub lifetimes: LifetimeEnv,
    pub attrs: Attrs,
    pub docs: Docs,
}

impl Trait {
    /// Extract a [`Trait`] metadata value from an AST node.
    pub fn new(trt: &syn::ItemTrait, parent_attrs: &Attrs) -> Self {
        let mut attrs = parent_attrs.clone();
        attrs.add_attrs(&trt.attrs);

        let mut trait_fcts = Vec::new();

        let self_ident = &trt.ident;
        // TODO check this
        let self_path_trait = PathType::from(&syn::TraitBound {
            paren_token: None,
            modifier: syn::TraitBoundModifier::None,
            lifetimes: None, // todo this is an assumption
            path: syn::PathSegment {
                ident: self_ident.clone(),
                arguments: syn::PathArguments::None,
            }
            .into(),
        });
        for trait_item in trt.items.iter() {
            if let syn::TraitItem::Fn(fct) = trait_item {
                let mut fct_attrs = attrs.clone();
                fct_attrs.add_attrs(&fct.attrs);
                // copied from the method parsing
                let fct_ident = &fct.sig.ident;
                let concat_fct_ident = format!("{self_ident}_{fct_ident}");
                let extern_ident = syn::Ident::new(
                    &attrs.abi_rename.apply(concat_fct_ident.into()),
                    fct.sig.ident.span(),
                );

                let all_params = fct
                    .sig
                    .inputs
                    .iter()
                    .filter_map(|a| match a {
                        syn::FnArg::Receiver(_) => None,
                        syn::FnArg::Typed(ref t) => {
                            Some(Param::from_syn(t, self_path_trait.clone()))
                        }
                    })
                    .collect::<Vec<_>>();

                let self_param = fct
                    .sig
                    .receiver()
                    .map(|rec| TraitSelfParam::from_syn(rec, self_path_trait.clone()));

                let output_type = match &fct.sig.output {
                    syn::ReturnType::Type(_, return_typ) => Some(TypeName::from_syn(
                        return_typ.as_ref(),
                        Some(self_path_trait.clone()),
                    )),
                    syn::ReturnType::Default => None,
                };

                let lifetimes = LifetimeEnv::from_trait_item(
                    trait_item,
                    self_param.as_ref(),
                    &all_params[..],
                    output_type.as_ref(),
                );

                trait_fcts.push(TraitMethod {
                    name: fct_ident.into(),
                    abi_name: (&extern_ident).into(),
                    self_param,
                    params: all_params,
                    output_type,
                    lifetimes,
                    attrs: fct_attrs,
                    docs: Docs::from_attrs(&fct.attrs),
                });
            }
        }
        let (mut is_sync, mut is_send) = (false, false);
        for supertrait in &trt.supertraits {
            if let syn::TypeParamBound::Trait(syn::TraitBound {
                path: syn::Path { segments, .. },
                ..
            }) = supertrait
            {
                let mut seg_iter = segments.iter();
                if let Some(syn::PathSegment { ident, .. }) = seg_iter.next() {
                    if *ident != "std" {
                        continue;
                    }
                }
                if let Some(syn::PathSegment { ident, .. }) = seg_iter.next() {
                    if *ident != "marker" {
                        continue;
                    }
                }
                if let Some(syn::PathSegment { ident, .. }) = seg_iter.next() {
                    if *ident == "Send" {
                        is_send = true;
                    } else if *ident == "Sync" {
                        is_sync = true;
                    }
                }
            }
        }

        Self {
            name: (&trt.ident).into(),
            methods: trait_fcts,
            docs: Docs::from_attrs(&trt.attrs),
            lifetimes: LifetimeEnv::from_trait(trt), // TODO
            attrs,
            is_send,
            is_sync,
        }
    }
}
