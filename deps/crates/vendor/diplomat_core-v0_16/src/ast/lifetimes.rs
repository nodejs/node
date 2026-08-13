use proc_macro2::Span;
use quote::{quote, ToTokens};
use serde::{Deserialize, Serialize};
use std::fmt;
use syn::spanned::Spanned;

use crate::ast::{
    idents::{FromWithSpan, IntoWithSpan},
    logging::{create_report, AstReport, ContextLocation},
    SpanLocation,
};

use super::{Attrs, Docs, Ident, Param, SelfParam, TraitSelfParam, TypeName};

/// A named lifetime, e.g. `'a`.
///
/// # Invariants
///
/// Cannot be `'static` or `'_`, use [`Lifetime`] to represent those instead.
#[derive(Clone, Debug, Hash, Eq, PartialEq, Serialize, PartialOrd, Ord)]
#[serde(transparent)]
pub struct NamedLifetime(Ident);

impl NamedLifetime {
    pub fn name(&self) -> &Ident {
        &self.0
    }
}

impl<'de> Deserialize<'de> for NamedLifetime {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        // Special `Deserialize` impl to ensure invariants.
        let named = Ident::deserialize(deserializer)?;
        if named.as_str() == "static" {
            panic!("cannot be static");
        }
        Ok(NamedLifetime(named))
    }
}

impl FromWithSpan<&syn::Lifetime> for NamedLifetime {
    fn spanned_from(lt: &syn::Lifetime, module_location: &SpanLocation) -> Self {
        Lifetime::spanned_from(lt, module_location)
            .to_named()
            .expect("cannot be static")
    }
}

impl FromWithSpan<&NamedLifetime> for NamedLifetime {
    fn spanned_from(this: &NamedLifetime, _module_location: &SpanLocation) -> Self {
        this.clone()
    }
}

impl PartialEq<syn::Lifetime> for NamedLifetime {
    fn eq(&self, other: &syn::Lifetime) -> bool {
        other.ident == self.0.as_str()
    }
}

impl fmt::Display for NamedLifetime {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "'{}", self.0)
    }
}

impl ToTokens for NamedLifetime {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        use proc_macro2::{Punct, Spacing};
        Punct::new('\'', Spacing::Joint).to_tokens(tokens);
        self.0.to_tokens(tokens);
    }
}

/// A lifetime dependency graph used for tracking which lifetimes outlive,
/// and are outlived by, other lifetimes.
///
/// It is similar to [`syn::LifetimeDef`], except it can also track lifetime
/// bounds defined in the `where` clause.
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub struct LifetimeEnv {
    pub(crate) nodes: Vec<LifetimeNode>,
}

impl LifetimeEnv {
    /// Construct an empty [`LifetimeEnv`].
    ///
    /// To create one outside of this module, use `LifetimeEnv::from_method_item`
    /// or `LifetimeEnv::from` on `&syn::Generics`.
    fn new() -> Self {
        Self { nodes: vec![] }
    }

    /// Iterate through the names of the lifetimes in scope.
    pub fn names(&self) -> impl Iterator<Item = &NamedLifetime> + Clone {
        self.nodes.iter().map(|node| &node.lifetime)
    }

    /// Returns a [`LifetimeEnv`] for a method, accounting for lifetimes and bounds
    /// defined in both the impl block and the method, as well as implicit lifetime
    /// bounds in the optional `self` param, other param, and optional return type.
    /// For example, the type `&'a Foo<'b>` implies `'b: 'a`.
    pub fn from_method_item(
        method: &syn::ImplItemFn,
        impl_generics: Option<&syn::Generics>,
        self_param: Option<&SelfParam>,
        params: &[Param],
        return_type: Option<&TypeName>,
        module_location: &SpanLocation,
    ) -> Self {
        let mut this = LifetimeEnv::new();
        // The impl generics _must_ be loaded into the env first, since the method
        // generics might use lifetimes defined in the impl, and `extend_generics`
        // panics if `'a: 'b` where `'b` isn't declared by the time it finishes.
        if let Some(generics) = impl_generics {
            this.extend_generics(generics, module_location);
        }
        this.extend_generics(&method.sig.generics, module_location);

        if let Some(self_param) = self_param {
            this.extend_implicit_lifetime_bounds(&self_param.to_typename(), None);
        }
        for param in params {
            this.extend_implicit_lifetime_bounds(&param.ty, None);
        }
        if let Some(return_type) = return_type {
            this.extend_implicit_lifetime_bounds(return_type, None);
        }

        this
    }

    pub fn from_trait_item(
        trait_fct_item: &syn::TraitItem,
        self_param: Option<&TraitSelfParam>,
        params: &[Param],
        return_type: Option<&TypeName>,
    ) -> Self {
        let mut this = LifetimeEnv::new();
        if let syn::TraitItem::Fn(_) = trait_fct_item {
            if let Some(self_param) = self_param {
                this.extend_implicit_lifetime_bounds(&self_param.to_typename(), None);
            }
            for param in params {
                this.extend_implicit_lifetime_bounds(&param.ty, None);
            }
            if let Some(return_type) = return_type {
                this.extend_implicit_lifetime_bounds(return_type, None);
            }
        } else {
            // Currently this is impossible to reach,
            // we only call from_trait_item when trait_fct_item is syn::TraitItem::Fn
            panic!(
                "Diplomat traits can only have associated methods and no other associated items."
            )
        }
        this
    }

    pub fn from_trait(trt: &syn::ItemTrait, module_location: &SpanLocation) -> Self {
        if let Some(lt) = trt.generics.lifetimes().next() {
            create_report(AstReport::new(
                "Diplomat traits are not allowed to have any lifetime parameters.".into(),
                Some(trt.ident.span().spanned_into(module_location)),
                "".into(),
                vec![ContextLocation::new(
                    lt.span().spanned_into(module_location),
                    "Remove lifetime annotations".into(),
                )],
            ));
        }
        LifetimeEnv::new()
    }

    pub fn from_enum_item(
        enm: &syn::ItemEnum,
        variant_fields: &[(Option<Ident>, TypeName, Docs, Attrs)],
        module_location: &SpanLocation,
    ) -> Self {
        let mut this = LifetimeEnv::new();
        this.extend_generics(&enm.generics, module_location);
        for (_, typ, _, _) in variant_fields {
            this.extend_implicit_lifetime_bounds(typ, None);
        }
        this
    }

    /// Returns a [`LifetimeEnv`] for a struct, accounding for lifetimes and bounds
    /// defined in the struct generics, as well as implicit lifetime bounds in
    /// the struct's fields. For example, the field `&'a Foo<'b>` implies `'b: 'a`.
    pub fn from_struct_item(
        strct: &syn::ItemStruct,
        fields: &[(Ident, TypeName, Docs, Attrs)],
        module_location: &SpanLocation,
    ) -> Self {
        let mut this = LifetimeEnv::new();
        this.extend_generics(&strct.generics, module_location);
        for (_, typ, _, _) in fields {
            this.extend_implicit_lifetime_bounds(typ, None);
        }
        this
    }

    pub fn from_function_item(
        f: &syn::ItemFn,
        params: &[Param],
        return_type: Option<&TypeName>,
        module_location: &SpanLocation,
    ) -> Self {
        let mut this = LifetimeEnv::new();
        this.extend_generics(&f.sig.generics, module_location);
        for param in params {
            this.extend_implicit_lifetime_bounds(&param.ty, None);
        }
        if let Some(return_type) = return_type {
            this.extend_implicit_lifetime_bounds(return_type, None);
        }
        this
    }

    /// Traverse a type, adding any implicit lifetime bounds that arise from
    /// having a reference to an opaque containing a lifetime.
    /// For example, the type `&'a Foo<'b>` implies `'b: 'a`.
    fn extend_implicit_lifetime_bounds(
        &mut self,
        typ: &TypeName,
        behind_ref: Option<&NamedLifetime>,
    ) {
        match typ {
            TypeName::Named(path_type) => {
                if let Some(borrow_lifetime) = behind_ref {
                    let explicit_longer_than_borrow =
                        LifetimeTransitivity::longer_than(self, borrow_lifetime);
                    let mut implicit_longer_than_borrow = vec![];

                    for path_lifetime in path_type.lifetimes.iter() {
                        if let Lifetime::Named(path_lifetime) = path_lifetime {
                            if !explicit_longer_than_borrow.contains(&path_lifetime) {
                                implicit_longer_than_borrow.push(path_lifetime);
                            }
                        }
                    }

                    self.extend_bounds(
                        implicit_longer_than_borrow
                            .into_iter()
                            .map(|path_lifetime| (path_lifetime, Some(borrow_lifetime))),
                    );
                }
            }
            TypeName::Reference(lifetime, _, typ) => {
                let behind_ref = if let Lifetime::Named(named) = lifetime {
                    Some(named)
                } else {
                    None
                };
                self.extend_implicit_lifetime_bounds(typ, behind_ref);
            }
            TypeName::Option(typ, _) => self.extend_implicit_lifetime_bounds(typ, None),
            TypeName::Result(ok, err, _) => {
                self.extend_implicit_lifetime_bounds(ok, None);
                self.extend_implicit_lifetime_bounds(err, None);
            }
            _ => {}
        }
    }

    /// Add the lifetimes from generic parameters and where bounds.
    fn extend_generics(&mut self, generics: &syn::Generics, module_location: &SpanLocation) {
        let generic_bounds = generics.params.iter().map(|generic| match generic {
            syn::GenericParam::Type(_) => {
                create_report(AstReport::new(
                    "Generic types are unsupported.".into(),
                    Some(generic.span().spanned_into(module_location)),
                    "Suggestion: Remove generic type.".into(),
                    vec![],
                ));
            }
            syn::GenericParam::Lifetime(def) => (&def.lifetime, &def.bounds),
            syn::GenericParam::Const(_) => {
                create_report(AstReport::new(
                    "Const generics are unsupported.".into(),
                    Some(generic.span().spanned_into(module_location)),
                    "Suggestion: Remove const generic.".into(),
                    vec![],
                ));
            }
        });

        let generic_defs = generic_bounds.clone().map(|(lifetime, _)| lifetime);

        self.extend_lifetimes(generic_defs, module_location);
        self.extend_bounds(generic_bounds);

        if let Some(ref where_clause) = generics.where_clause {
            self.extend_bounds(where_clause.predicates.iter().map(|pred| match pred {
                syn::WherePredicate::Type(_) => create_report(AstReport::new(
                    "Trait bounds are unsupported.".into(),
                    Some(pred.span().spanned_into(module_location)),
                    "Suggestion: Remove type predicate.".into(),
                    vec![],
                )),
                syn::WherePredicate::Lifetime(pred) => (&pred.lifetime, &pred.bounds),
                _ => create_report(AstReport::new(
                    "Unrecognized where predicate.".into(),
                    Some(pred.span().spanned_into(module_location)),
                    "".into(),
                    vec![],
                )),
            }));
        }
    }

    /// Returns the number of lifetimes in the graph.
    pub fn len(&self) -> usize {
        self.nodes.len()
    }

    /// Returns `true` if the graph contains no lifetimes.
    pub fn is_empty(&self) -> bool {
        self.nodes.is_empty()
    }

    /// `<'a, 'b, 'c>`
    ///
    /// Write the existing lifetimes, excluding bounds, as generic parameters.
    ///
    /// To include lifetime bounds, use [`LifetimeEnv::lifetime_defs_to_tokens`].
    pub fn lifetimes_to_tokens(&self) -> proc_macro2::TokenStream {
        if self.is_empty() {
            return quote! {};
        }

        let lifetimes = self.nodes.iter().map(|node| &node.lifetime);
        quote! { <#(#lifetimes),*> }
    }

    /// Returns the index of a lifetime in the graph, or `None` if the lifetime
    /// isn't in the graph.
    pub(crate) fn id<L>(&self, lifetime: &L) -> Option<usize>
    where
        NamedLifetime: PartialEq<L>,
    {
        self.nodes
            .iter()
            .position(|node| &node.lifetime == lifetime)
    }

    /// Add isolated lifetimes to the graph.
    fn extend_lifetimes<'a, L, I>(&mut self, iter: I, module_location: &SpanLocation)
    where
        NamedLifetime: PartialEq<L> + FromWithSpan<&'a L>,
        L: 'a,
        I: IntoIterator<Item = &'a L>,
    {
        for lifetime in iter {
            if let Some(idx) = self.id(lifetime) {
                let context_locations = if let Some(sp) = self.nodes[idx].lifetime.0.span() {
                    vec![ContextLocation::new(sp, "Defined first here.".into())]
                } else {
                    vec![]
                };
                let lt = NamedLifetime::spanned_from(lifetime, module_location);
                create_report(AstReport::new(
                    "Lifetime declared twice in the same scope".into(),
                    lt.0.span(),
                    format!("Lifetime {lt} already exists."),
                    context_locations,
                ));
            }

            self.nodes.push(LifetimeNode {
                lifetime: lifetime.spanned_into(module_location),
                shorter: vec![],
                longer: vec![],
            });
        }
    }

    /// Add edges to the lifetime graph.
    ///
    /// This method is decoupled from [`LifetimeEnv::extend_lifetimes`] because
    /// generics can define new lifetimes, while `where` clauses cannot.
    ///
    /// # Panics
    ///
    /// This method panics if any of the lifetime bounds aren't already defined
    /// in the graph. This isn't allowed by rustc in the first place, so it should
    /// only ever occur when deserializing an invalid [`LifetimeEnv`].
    fn extend_bounds<'a, L, B, I>(&mut self, iter: I)
    where
        NamedLifetime: PartialEq<L>,
        L: 'a,
        B: IntoIterator<Item = &'a L>,
        I: IntoIterator<Item = (&'a L, B)>,
    {
        for (lifetime, bounds) in iter {
            let long = self.id(lifetime).expect("use of undeclared lifetime, this is a bug: try calling `LifetimeEnv::extend_lifetimes` first");
            for bound in bounds {
                let short = self
                    .id(bound)
                    .expect("cannot use undeclared lifetime as a bound");
                self.nodes[short].longer.push(long);
                self.nodes[long].shorter.push(short);
            }
        }
    }
}

impl fmt::Display for LifetimeEnv {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.to_token_stream().fmt(f)
    }
}

impl ToTokens for LifetimeEnv {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        for node in self.nodes.iter() {
            let lifetime = &node.lifetime;
            if node.shorter.is_empty() {
                tokens.extend(quote! { #lifetime, });
            } else {
                let bounds = node.shorter.iter().map(|&id| &self.nodes[id].lifetime);
                tokens.extend(quote! { #lifetime: #(#bounds)+*, });
            }
        }
    }
}

/// Serialize a [`LifetimeEnv`] as a map from lifetimes to their bounds.
impl Serialize for LifetimeEnv {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        use serde::ser::SerializeMap;
        let mut seq = serializer.serialize_map(Some(self.len()))?;

        for node in self.nodes.iter() {
            /// Helper type for serializing bounds.
            struct Bounds<'a> {
                ids: &'a [usize],
                nodes: &'a [LifetimeNode],
            }

            impl<'a> Serialize for Bounds<'a> {
                fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
                where
                    S: serde::Serializer,
                {
                    use serde::ser::SerializeSeq;
                    let mut seq = serializer.serialize_seq(Some(self.ids.len()))?;
                    for &id in self.ids {
                        seq.serialize_element(&self.nodes[id].lifetime)?;
                    }
                    seq.end()
                }
            }

            seq.serialize_entry(
                &node.lifetime,
                &Bounds {
                    ids: &node.shorter[..],
                    nodes: &self.nodes,
                },
            )?;
        }
        seq.end()
    }
}

impl<'de> Deserialize<'de> for LifetimeEnv {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use std::collections::BTreeMap;

        let m: BTreeMap<NamedLifetime, Vec<NamedLifetime>> =
            Deserialize::deserialize(deserializer)?;

        let span_loc = if let Some(k) = m.keys().next() {
            if let Some(sp) = k.0.span() {
                sp.span_location
            } else {
                SpanLocation::None
            }
        } else {
            SpanLocation::None
        };

        let mut this = LifetimeEnv::new();
        this.extend_lifetimes(m.keys(), &span_loc);
        this.extend_bounds(m.iter());
        Ok(this)
    }
}

/// A lifetime, along with ptrs to all lifetimes that are explicitly
/// shorter/longer than it.
///
/// This type is internal to [`LifetimeGraph`]- the ptrs are stored as `usize`s,
/// meaning that they may be invalid if a `LifetimeEdges` is created in one
/// `LifetimeGraph` and then used in another.
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub(crate) struct LifetimeNode {
    /// The name of the lifetime.
    pub(crate) lifetime: NamedLifetime,

    /// Pointers to all lifetimes that this lives _at least_ as long as.
    ///
    /// Note: This doesn't account for transitivity.
    pub(crate) shorter: Vec<usize>,

    /// Pointers to all lifetimes that live _at least_ as long as this.
    ///
    /// Note: This doesn't account for transitivity.
    pub(crate) longer: Vec<usize>,
}

/// A lifetime, analogous to [`syn::Lifetime`].
#[derive(Clone, Debug, Hash, Eq, PartialEq, Serialize, Deserialize)]
#[non_exhaustive]
pub enum Lifetime {
    /// The `'static` lifetime.
    Static,

    /// A named lifetime, like `'a`.
    Named(NamedLifetime),

    /// An elided lifetime.
    Anonymous,
}

impl Lifetime {
    /// Returns the inner `NamedLifetime` if the lifetime is the `Named` variant,
    /// otherwise `None`.
    pub fn to_named(self) -> Option<NamedLifetime> {
        if let Lifetime::Named(named) = self {
            return Some(named);
        }
        None
    }

    /// Returns a reference to the inner `NamedLifetime` if the lifetime is the
    /// `Named` variant, otherwise `None`.
    pub fn as_named(&self) -> Option<&NamedLifetime> {
        if let Lifetime::Named(named) = self {
            return Some(named);
        }
        None
    }
}

impl fmt::Display for Lifetime {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            Lifetime::Static => "'static".fmt(f),
            Lifetime::Named(ref named) => named.fmt(f),
            Lifetime::Anonymous => "'_".fmt(f),
        }
    }
}

impl ToTokens for Lifetime {
    fn to_tokens(&self, tokens: &mut proc_macro2::TokenStream) {
        match self {
            Lifetime::Static => syn::Lifetime::new("'static", Span::call_site()).to_tokens(tokens),
            Lifetime::Named(ref s) => s.to_tokens(tokens),
            Lifetime::Anonymous => syn::Lifetime::new("'_", Span::call_site()).to_tokens(tokens),
        };
    }
}

impl FromWithSpan<&syn::Lifetime> for Lifetime {
    fn spanned_from(lt: &syn::Lifetime, module_location: &SpanLocation) -> Self {
        if lt.ident == "static" {
            Self::Static
        } else {
            Self::Named(NamedLifetime((&lt.ident).spanned_into(module_location)))
        }
    }
}

impl FromWithSpan<&Option<syn::Lifetime>> for Lifetime {
    fn spanned_from(lt: &Option<syn::Lifetime>, module_location: &SpanLocation) -> Self {
        lt.as_ref()
            .map(|lt| lt.spanned_into(module_location))
            .unwrap_or(Self::Anonymous)
    }
}

impl Lifetime {
    /// Converts the [`Lifetime`] back into an AST node that can be spliced into a program.
    pub fn to_syn(&self) -> Option<syn::Lifetime> {
        match *self {
            Self::Static => Some(syn::Lifetime::new("'static", Span::call_site())),
            Self::Anonymous => None,
            Self::Named(ref s) => Some(syn::Lifetime::new(&s.to_string(), Span::call_site())),
        }
    }
}

/// Collect all lifetimes that are either longer_or_shorter
pub struct LifetimeTransitivity<'env> {
    env: &'env LifetimeEnv,
    visited: Vec<bool>,
    out: Vec<&'env NamedLifetime>,
    longer_or_shorter: LongerOrShorter,
}

impl<'env> LifetimeTransitivity<'env> {
    /// Returns a new [`LifetimeTransitivity`] that finds all longer lifetimes.
    pub fn longer(env: &'env LifetimeEnv) -> Self {
        Self::new(env, LongerOrShorter::Longer)
    }

    /// Returns a new [`LifetimeTransitivity`] that finds all shorter lifetimes.
    pub fn shorter(env: &'env LifetimeEnv) -> Self {
        Self::new(env, LongerOrShorter::Shorter)
    }

    /// Returns all the lifetimes longer than a provided `NamedLifetime`.
    pub fn longer_than(env: &'env LifetimeEnv, named: &NamedLifetime) -> Vec<&'env NamedLifetime> {
        let mut this = Self::new(env, LongerOrShorter::Longer);
        this.visit(named);
        this.finish()
    }

    /// Returns all the lifetimes shorter than the provided `NamedLifetime`.
    pub fn shorter_than(env: &'env LifetimeEnv, named: &NamedLifetime) -> Vec<&'env NamedLifetime> {
        let mut this = Self::new(env, LongerOrShorter::Shorter);
        this.visit(named);
        this.finish()
    }

    /// Returns a new [`LifetimeTransitivity`].
    fn new(env: &'env LifetimeEnv, longer_or_shorter: LongerOrShorter) -> Self {
        LifetimeTransitivity {
            env,
            visited: vec![false; env.len()],
            out: vec![],
            longer_or_shorter,
        }
    }

    /// Visits a lifetime, as well as all the nodes it's transitively longer or
    /// shorter than, depending on how the `LifetimeTransitivity` was constructed.
    pub fn visit(&mut self, named: &NamedLifetime) {
        if let Some(id) = self
            .env
            .nodes
            .iter()
            .position(|node| node.lifetime == *named)
        {
            self.dfs(id);
        }
    }

    /// Performs depth-first search through the `LifetimeEnv` created at construction
    /// for all nodes longer or shorter than the node at the provided index,
    /// depending on how the `LifetimeTransitivity` was constructed.
    fn dfs(&mut self, index: usize) {
        // Note: all of these indexings SHOULD be valid because
        // `visited.len() == nodes.len()`, and the ids come from
        // calling `Iterator::position` on `nodes`, which never shrinks.
        // So we should be able to change these to `get_unchecked`...
        if !self.visited[index] {
            self.visited[index] = true;

            let node = &self.env.nodes[index];
            self.out.push(&node.lifetime);
            for &edge_index in self.longer_or_shorter.edges(node).iter() {
                self.dfs(edge_index);
            }
        }
    }

    /// Returns the transitively reachable lifetimes.
    pub fn finish(self) -> Vec<&'env NamedLifetime> {
        self.out
    }
}

/// A helper type for [`LifetimeTransitivity`] determining whether to find the
/// transitively longer or transitively shorter lifetimes.
enum LongerOrShorter {
    Longer,
    Shorter,
}

impl LongerOrShorter {
    /// Returns either the indices of the longer or shorter lifetimes, depending
    /// on `self`.
    fn edges<'node>(&self, node: &'node LifetimeNode) -> &'node [usize] {
        match self {
            LongerOrShorter::Longer => &node.longer[..],
            LongerOrShorter::Shorter => &node.shorter[..],
        }
    }
}
