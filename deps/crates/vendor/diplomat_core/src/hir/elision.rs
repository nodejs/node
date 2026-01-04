//! This module provides the functionality for lowering lifetimes from the AST
//! to the HIR, while simultaneously inferencing elided lifetimes.
//!
//! Full elision rules can be found in the [Nomicon].
//!
//! The key factor about lifetime elision is that all elision in the output of
//! the method (if there is any) corresponds to exactly one lifetime in the method
//! arguments, which may or may not be elided. Therefore, our task is to find this
//! potential lifetime first, so that if we encounter an elided lifetime while
//! lowering the output, we know which lifetime it corresponds to.
//!
//! # Unspoken Rules of Elision.
//!
//! Broadly speaking, the Nomicon defines the elision rules are such:
//! 1. If there's a `&self` or `&mut self`, the lifetime of that borrow
//!    corresponds to elision in the output.
//! 2. Otherwise, if there's exactly one lifetime in the input, then that lifetime
//!    corresponds to elision in the output.
//! 3. If neither of these cases hold, then the output cannot contain elision.
//!
//! What the Nomicon doesn't tell you is that there are weird corner cases around
//! using the `Self` type. Specifically, lifetimes in the `Self` type and in the
//! type of the `self` argument (optional) aren't considered when figuring out
//! which lifetime should correspond to elision in the output.
//!
//! Check out the following code:
//! ```compile_fail
//! struct Foo<'a>(&'a str);
//!
//! impl<'a> Foo<'a> {
//!     fn get(self) -> &str { self.0 }
//! }
//! ```
//! This code will fail to compile because it doesn't look at the `'a` in the
//! `Foo<'a>`, which is what the type of `self` expands to. Therefore, it will
//! conclude that there's nothing for the output to borrow from.
//! This can be fixed by returning `&'a str` though. Many of the design
//! decisions in this module were made to be able to replicate this behavior.
//!
//! You may be asking "why would we care about rejecting code that rustc rejects
//! before it reaches us?" And the answer is this:
//! ```rust
//! # struct Foo<'a>(&'a str);
//! impl<'a> Foo<'a> {
//!     fn get(self, s: &str) -> &str { s }
//! }
//! ```
//! This code is accepted by rustc, since it only considers the lifetime of `s`
//! when searching for a lifetime that corresponds to output elision. If we were
//! to naively look at all the lifetimes, we would see the lifetime in the `self`
//! argument and the lifetime of `s`, making us reject this method. Therefore, we
//! have to be extremely careful when traversing lifetimes, and make sure that
//! lifetimes of `Self` are lowered but _not_ considered for elision, while other
//! lifetimes are lowered while also being considered for elision.
//!
//! # Lowering and Inference
//!
//! Lowering and elision inference is broken into three distinct stages:
//! 1. Lowering the borrow in `&self` or `&mut self`, if there is one.
//! 2. Lowering lifetimes of other params.
//! 3. Lowering lifetimes of the output.
//!
//! Although each stage fundementally lowers lifetimes, they behave differently
//! when lowering elided lifetimes. Naturally, this module represents each stage
//! as a state in a state machine.
//!
//! The first state is represented by the [`SelfParamLifetimeLowerer`] type.
//! Since there is either zero or one occurrences of `&self` or `&mut self`, it
//! exposes the `.no_self_ref()` and `.lower_self_ref(lt)` methods respectively,
//! which consume the `SelfParamLifetimeLowerer` and return the next state,
//! [`ParamLifetimeLowerer`], as well as the lowered lifetime. The reason these
//! are two distinct types is that the lifetime in `&self` and `&mut self` takes
//! precedence over any other lifetimes in the input, so `.lower_self_ref(lt)`
//! tells the next state that the candidate lifetime is already found, and to
//! generate fresh anonymous lifetimes for any elided lifetimes.
//!
//! The second state is represented by the [`ParamLifetimeLowerer`] type.
//! It implements a helper trait, [`LifetimeLowerer`], which abstracts the lowering
//! of references and generic lifetimes. Internally, it wraps an [`ElisionSource`],
//! which acts as a state machine for tracking candidate lifetimes to correspond
//! to elision in the output. When a lifetime that's not in the type of the `self`
//! argument or in the expanded generics of the `Self` type is visited, this
//! state machine is potentially updated to another state. If the lifetime is
//! anonymous, it's added to the internal list of nodes that go into the final
//! [`LifetimeEnv`] after lowering. Once all the lifetimes in the input are
//! lowered, the `into_return_ltl()` method is called to transition into the
//! final state.
//!
//! The third and final state is represented by the [`ReturnLifetimeLowerer`] type.
//! Similar to `ParamLifetimeLowerer`, it also implements the [`LifetimeLowerer`]
//! helper trait. However, it differs from `ParamLifetimeLowerer` since instead
//! of potentially updating the internal `ElisionSource` when visiting a lifetime,
//! it instead reads from it when an elided lifetime occurs. Once all the output
//! lifetimes are lowered, `.finish()` is called to return the finalized
//! [`LifetimeEnv`].
//!
//! [Nomicon]: https://doc.rust-lang.org/nomicon/lifetime-elision.html

use super::lifetimes::{BoundedLifetime, Lifetime, LifetimeEnv, Lifetimes, MaybeStatic};
use super::LoweringContext;
use crate::ast;
use smallvec::SmallVec;

/// Lower [`ast::Lifetime`]s to [`Lifetime`]s.
///
/// This helper traits allows the [`lower_type`] and [`lower_out_type`] methods
/// to abstractly lower lifetimes without concern for what sort of tracking
/// goes on. In particular, elision inference requires updating internal state
/// when visiting lifetimes in the input.
pub trait LifetimeLowerer {
    /// Lowers an [`ast::Lifetime`].
    fn lower_lifetime(&mut self, lifetime: &ast::Lifetime) -> MaybeStatic<Lifetime>;

    /// Lowers a slice of [`ast::Lifetime`]s by calling
    /// [`LifetimeLowerer::lower_lifetime`] repeatedly.
    ///
    /// `type_generics` is the full list of generics on the type definition of the type
    /// this lifetimes list is found on (needed for generating anon lifetimes)
    fn lower_lifetimes(
        &mut self,
        lifetimes: &[ast::Lifetime],
        type_generics: &ast::LifetimeEnv,
    ) -> Lifetimes {
        let mut lifetimes = Lifetimes::from_fn(lifetimes, |lifetime| self.lower_lifetime(lifetime));

        for _ in lifetimes.as_slice().len()..type_generics.nodes.len() {
            lifetimes.append_lifetime(self.lower_lifetime(&ast::Lifetime::Anonymous))
        }
        lifetimes
    }

    /// Lowers a slice of [`ast::Lifetime`], where the strategy may vary depending
    /// on whether or not the lifetimes are expanded from the `Self` type.
    ///
    /// The distinction between this and [`LifetimeLowerer::lower_lifetimes`] is
    /// that if `Self` expands to a type with anonymous lifetimes like `Foo<'_>`,
    /// then multiple instances of `Self` should expand to have the same anonymous
    /// lifetime, and this lifetime can be cached inside of the `self` argument.
    /// Additionally, elision inferences knows to not search inside the generics
    /// of `Self` types for candidate lifetimes to correspond to elided lifetimes
    /// in the output.
    ///
    /// `type_generics` is the full list of generics on the type definition of the type
    /// this generics list is found on (needed for generating anon lifetimes)
    fn lower_generics(
        &mut self,
        lifetimes: &[ast::Lifetime],
        type_generics: &ast::LifetimeEnv,
        is_self: bool,
    ) -> Lifetimes;
}

/// A state machine for tracking which lifetime in a function's parameters
/// may correspond to elided lifetimes in the output.
#[derive(Copy, Clone)]
enum ElisionSource {
    /// No borrows in the input, no elision.
    NoBorrows,
    /// `&self` or `&mut self`, elision allowed.
    SelfParam(MaybeStatic<Lifetime>),
    /// One param contains a borrow, elision allowed.
    OneParam(MaybeStatic<Lifetime>),
    /// Multiple borrows and no self borrow, no elision.
    MultipleBorrows,
}

impl ElisionSource {
    /// Potentially transition to a new state.
    fn visit_lifetime(&mut self, lifetime: MaybeStatic<Lifetime>) {
        match self {
            ElisionSource::NoBorrows => *self = ElisionSource::OneParam(lifetime),
            ElisionSource::SelfParam(_) => {
                // References to self have the highest precedence, do nothing.
            }
            ElisionSource::OneParam(_) => *self = ElisionSource::MultipleBorrows,
            ElisionSource::MultipleBorrows => {
                // There's ambiguity. This is valid when there's no elision in
                // the output.
            }
        };
    }
}

/// A type for storing shared information between the different states of elision
/// inference.
///
/// This contains data for generating fresh elided lifetimes, looking up named
/// lifetimes, and caching lifetimes of `Self`.
pub(super) struct BaseLifetimeLowerer<'ast> {
    lifetime_env: &'ast ast::LifetimeEnv,
    self_lifetimes: Option<Lifetimes>,
    nodes: SmallVec<[BoundedLifetime; super::lifetimes::INLINE_NUM_LIFETIMES]>,
    num_lifetimes: usize,
}

/// The first phase of output elision inference.
///
/// In the first phase, the type signature of the `&self` or `&mut self` type
/// is lowered into its HIR representation, if present. According to elision
/// rules, this reference has the highest precedence as the lifetime that
/// goes into elision in the output, and so it's checked first.
pub(super) struct SelfParamLifetimeLowerer<'ast> {
    base: BaseLifetimeLowerer<'ast>,
}

/// The second phase of output elision inference.
///
/// In the second phase, all lifetimes in the parameter type signatures
/// (besides the lifetime of self, if present) are lowered. If a self param
/// didn't claim the potential output elided lifetime, then if there's a
/// single lifetime (elided or not) in the inputs, it will claim the
/// potential output elided lifetime.
pub(super) struct ParamLifetimeLowerer<'ast> {
    elision_source: ElisionSource,
    base: BaseLifetimeLowerer<'ast>,
}

/// The third and final phase of output elision inference.
///
/// In the third phase, the type signature of the output type is lowered into
/// its HIR representation. If one of the input lifetimes were marked as
/// responsible for any elision in the output, then anonymous lifetimes get
/// that lifetime. If none did and there is elision in the output, then
/// rustc should have errored and said the elision was ambiguous, meaning
/// that state should be impossible so it panics.
pub(super) struct ReturnLifetimeLowerer<'ast> {
    elision_source: ElisionSource,
    base: BaseLifetimeLowerer<'ast>,
}

impl<'ast> BaseLifetimeLowerer<'ast> {
    /// Returns a [`Lifetime`] representing a new anonymous lifetime, and
    /// pushes it to the nodes vector.
    fn new_elided(&mut self) -> Lifetime {
        let index = self.num_lifetimes;
        self.num_lifetimes += 1;
        Lifetime::new(index)
    }

    /// Lowers a single [`ast::Lifetime`]. If the lifetime is elided, then a fresh
    /// [`ImplicitLifetime`] is generated.
    fn lower_lifetime(&mut self, lifetime: &ast::Lifetime) -> MaybeStatic<Lifetime> {
        match lifetime {
            ast::Lifetime::Static => MaybeStatic::Static,
            ast::Lifetime::Named(named) => {
                MaybeStatic::NonStatic(Lifetime::from_ast(named, self.lifetime_env))
            }
            ast::Lifetime::Anonymous => MaybeStatic::NonStatic(self.new_elided()),
        }
    }

    /// Retrieves the cached  `Self` lifetimes, or caches newly generated
    /// lifetimes and returns those.
    fn self_lifetimes_or_new(&mut self, ast_lifetimes: &[ast::Lifetime]) -> Lifetimes {
        if let Some(lifetimes) = &self.self_lifetimes {
            lifetimes.clone()
        } else {
            let lifetimes = Lifetimes::from_fn(ast_lifetimes, |lt| self.lower_lifetime(lt));
            self.self_lifetimes = Some(lifetimes.clone());
            lifetimes
        }
    }
}

impl<'ast> SelfParamLifetimeLowerer<'ast> {
    /// Returns a new [`SelfParamLifetimeLowerer`].
    pub fn new(
        lifetime_env: &'ast ast::LifetimeEnv,
        ctx: &mut LoweringContext,
    ) -> Result<Self, ()> {
        let mut hir_nodes = Ok(SmallVec::new());

        for ast_node in lifetime_env.nodes.iter() {
            let lifetime = ctx.lower_ident(ast_node.lifetime.name(), "named lifetime");
            match (lifetime, &mut hir_nodes) {
                (Ok(lifetime), Ok(hir_nodes)) => {
                    hir_nodes.push(BoundedLifetime::new(
                        lifetime,
                        ast_node.longer.iter().map(|i| Lifetime::new(*i)).collect(),
                        ast_node.shorter.iter().map(|i| Lifetime::new(*i)).collect(),
                    ));
                }
                _ => hir_nodes = Err(()),
            }
        }

        hir_nodes.map(|nodes| Self {
            base: BaseLifetimeLowerer {
                lifetime_env,
                self_lifetimes: None,
                num_lifetimes: nodes.len(),
                nodes,
            },
        })
    }

    /// Lowers the lifetime of `&self` or `&mut self`.
    ///
    /// The lifetimes of `&self` and `&mut self` are special, because they
    /// automatically take priority over any other lifetime in the input for
    /// being tied to any elided lifetimes in the output.
    ///
    /// Along with returning the lowered lifetime, this method also returns the
    /// next state in elision inference, the [`ParamLifetimeLowerer`].
    pub fn lower_self_ref(
        mut self,
        lifetime: &ast::Lifetime,
    ) -> (MaybeStatic<Lifetime>, ParamLifetimeLowerer<'ast>) {
        let self_lifetime = self.base.lower_lifetime(lifetime);

        (
            self_lifetime,
            self.into_param_ltl(ElisionSource::SelfParam(self_lifetime)),
        )
    }

    /// Acknowledges that there's no `&self` or `&mut self`, and transitions
    /// to the next state, [`ParamLifetimeLowerer`].
    pub fn no_self_ref(self) -> ParamLifetimeLowerer<'ast> {
        self.into_param_ltl(ElisionSource::NoBorrows)
    }

    /// Transition into the next state, [`ParamLifetimeLowerer`].
    fn into_param_ltl(self, elision_source: ElisionSource) -> ParamLifetimeLowerer<'ast> {
        ParamLifetimeLowerer {
            elision_source,
            base: self.base,
        }
    }
}

impl<'ast> ParamLifetimeLowerer<'ast> {
    /// Once all lifetimes in the parameters are lowered, this function is
    /// called to transition to the next state, [`ReturnLifetimeLowerer`].
    pub fn into_return_ltl(self) -> ReturnLifetimeLowerer<'ast> {
        ReturnLifetimeLowerer {
            elision_source: self.elision_source,
            base: self.base,
        }
    }
}

impl<'ast> LifetimeLowerer for ParamLifetimeLowerer<'ast> {
    fn lower_lifetime(&mut self, borrow: &ast::Lifetime) -> MaybeStatic<Lifetime> {
        let lifetime = self.base.lower_lifetime(borrow);
        self.elision_source.visit_lifetime(lifetime);
        lifetime
    }

    fn lower_generics(
        &mut self,
        lifetimes: &[ast::Lifetime],
        type_generics: &ast::LifetimeEnv,
        is_self: bool,
    ) -> Lifetimes {
        if is_self {
            self.base.self_lifetimes_or_new(lifetimes)
        } else {
            self.lower_lifetimes(lifetimes, type_generics)
        }
    }
}

impl<'ast> ReturnLifetimeLowerer<'ast> {
    /// Finalize the lifetimes in the method, returning the resulting [`LifetimeEnv`].
    pub fn finish(self) -> LifetimeEnv {
        LifetimeEnv::new(self.base.nodes, self.base.num_lifetimes)
    }
}

impl<'ast> LifetimeLowerer for ReturnLifetimeLowerer<'ast> {
    fn lower_lifetime(&mut self, borrow: &ast::Lifetime) -> MaybeStatic<Lifetime> {
        match borrow {
            ast::Lifetime::Static => MaybeStatic::Static,
            ast::Lifetime::Named(named) => {
                MaybeStatic::NonStatic(Lifetime::from_ast(named, self.base.lifetime_env))
            }
            ast::Lifetime::Anonymous => match self.elision_source {
                ElisionSource::SelfParam(lifetime) | ElisionSource::OneParam(lifetime) => lifetime,
                ElisionSource::NoBorrows => {
                    panic!("nothing to borrow from, this shouldn't pass rustc's checks")
                }
                ElisionSource::MultipleBorrows => {
                    panic!("source of elision is ambiguous, this shouldn't pass rustc's checks")
                }
            },
        }
    }

    fn lower_generics(
        &mut self,
        lifetimes: &[ast::Lifetime],
        type_generics: &ast::LifetimeEnv,
        is_self: bool,
    ) -> Lifetimes {
        if is_self {
            self.base.self_lifetimes_or_new(lifetimes)
        } else {
            self.lower_lifetimes(lifetimes, type_generics)
        }
    }
}

impl LifetimeLowerer for &ast::LifetimeEnv {
    fn lower_lifetime(&mut self, lifetime: &ast::Lifetime) -> MaybeStatic<Lifetime> {
        match lifetime {
            ast::Lifetime::Static => MaybeStatic::Static,
            ast::Lifetime::Named(named) => MaybeStatic::NonStatic(Lifetime::from_ast(named, self)),
            ast::Lifetime::Anonymous => {
                panic!("anonymous lifetime inside struct, this shouldn't pass rustc's checks")
            }
        }
    }

    fn lower_generics(
        &mut self,
        lifetimes: &[ast::Lifetime],
        type_generics: &ast::LifetimeEnv,
        _: bool,
    ) -> Lifetimes {
        self.lower_lifetimes(lifetimes, type_generics)
    }
}

// Things to test:
// 1. ensure that if there are multiple inputs that are `Self`, where `Self` has
//    an elided lifetime, all expansions of `Self` have the same anonymous lifetimes.

#[cfg(test)]
mod tests {
    use strck::IntoCk;

    /// Convert a syntax tree into a [`TypeContext`].
    macro_rules! tcx {
        ($($tokens:tt)*) => {{
            let m = crate::ast::Module::from_syn(&syn::parse_quote! { $($tokens)* }, true);

            let mut env = crate::Env::default();
            let mut top_symbols = crate::ModuleEnv::new(Default::default());

            m.insert_all_types(crate::ast::Path::empty(), &mut env);
            top_symbols.insert(m.name.clone(), crate::ast::ModSymbol::SubModule(m.name.clone()));

            env.insert(crate::ast::Path::empty(), top_symbols);

            let mut backend = crate::hir::BasicAttributeValidator::new("test-backend");
            backend.support.static_slices = true;

            // Don't run validation: it will error on elision. We want this code to support
            // elision even if we don't actually allow it, since good diagnostics involve understanding
            // broken code.
            let (_, tcx) = crate::hir::TypeContext::from_ast_without_validation(&env, Default::default(), backend).unwrap();

            tcx
        }}
    }

    macro_rules! do_test {
        ($($tokens:tt)*) => {{
            let mut settings = insta::Settings::new();
            settings.set_sort_maps(true);

            settings.bind(|| {
                let tcx = tcx! { $($tokens)* };

                insta::assert_debug_snapshot!(tcx);
            })
        }}
    }

    #[test]
    fn simple_mod() {
        do_test! {
            mod ffi {
                #[diplomat::opaque]
                struct Opaque<'a> {
                    s: DiplomatStrSlice<'a>,
                }

                struct Struct<'a> {
                    s:  DiplomatStrSlice<'a>,
                }

                #[diplomat::out]
                struct OutStruct<'a> {
                    inner: Box<Opaque<'a>>,
                }

                impl<'a> OutStruct<'a> {
                    pub fn new(s: &'a DiplomatStr) -> Self {
                        Self { inner: Box::new(Opaque { s }) }
                    }

                }

                impl<'a> Struct<'a> {
                    pub fn rustc_elision(self, s: &DiplomatStr) -> &DiplomatStr {
                        s
                    }
                }
            }
        }
    }

    #[test]
    fn test_elision_in_struct() {
        let tcx = tcx! {
            mod ffi {
                #[diplomat::opaque]
                struct Opaque;

                #[diplomat::opaque]
                struct Opaque2<'a>(&'a str);

                impl Opaque {
                    // This should have two elided lifetimes
                    pub fn elided(&self, x: &Opaque2) {

                    }
                }
            }
        };

        let method = &tcx
            .opaques()
            .iter()
            .find(|def| def.name == "Opaque")
            .unwrap()
            .methods[0];

        assert_eq!(
            method.lifetime_env.num_lifetimes(),
            3,
            "elided() must have three anon lifetimes"
        );
        insta::assert_debug_snapshot!(method);
    }

    #[test]
    fn test_borrowing_fields() {
        use std::collections::BTreeMap;
        use std::fmt;

        let tcx = tcx! {
            mod ffi {
                #[diplomat::opaque]
                pub struct Opaque;

                struct Input<'p, 'q> {
                    p_data: &'p Opaque,
                    q_data: &'q Opaque,
                    name: DiplomatStrSlice<'static>,
                    inner: Inner<'q>,
                }

                struct Inner<'a> {
                    more_data: DiplomatStrSlice<'a>,
                }

                struct Output<'p,'q> {
                    p_data: &'p Opaque,
                    q_data: &'q Opaque,
                }

                impl<'a, 'b> Input<'a, 'b> {
                    pub fn as_output(self, _s: &'static DiplomatStr) -> Output<'b, 'a> {
                        Output { data: self.data }
                    }

                }
            }
        };

        let method = &tcx
            .structs()
            .iter()
            .find(|def| def.name == "Input")
            .unwrap()
            .methods[0];

        let visitor = method.borrowing_field_visitor(&tcx, "this".ck().unwrap());
        let mut lt_to_borrowing_fields: BTreeMap<_, Vec<_>> = BTreeMap::new();
        visitor.visit_borrowing_fields(|lt, bf| {
            lt_to_borrowing_fields
                .entry(lt)
                .or_default()
                .push(DebugBorrowingField(bf));
        });

        struct DebugBorrowingField<'m>(crate::hir::borrowing_field::BorrowingField<'m>);

        impl<'m> fmt::Debug for DebugBorrowingField<'m> {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.write_str("\"")?;
                self.0.try_backtrace(|i, ident| {
                    if i != 0 {
                        f.write_str(".")?;
                    }
                    f.write_str(ident.as_str())
                })?;
                f.write_str("\"")
            }
        }

        let mut settings = insta::Settings::new();
        settings.set_sort_maps(true);

        settings.bind(|| {
            insta::assert_debug_snapshot!(lt_to_borrowing_fields);
        })
    }
}
