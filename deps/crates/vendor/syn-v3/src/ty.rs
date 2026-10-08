use crate::attr::Attribute;
use crate::expr::Expr;
use crate::generics::{BoundLifetimes, TypeParamBound};
use crate::ident::Ident;
use crate::lifetime::Lifetime;
use crate::lit::LitStr;
use crate::mac::Macro;
use crate::path::{Path, QSelf};
use crate::punctuated::Punctuated;
use crate::token;
use alloc::boxed::Box;
use alloc::vec::Vec;
use proc_macro2::TokenStream;

ast_enum_of_structs! {
    /// The possible types that a Rust value could have.
    ///
    /// # Syntax tree enum
    ///
    /// This type is a [syntax tree enum].
    ///
    /// [syntax tree enum]: crate::expr::Expr#syntax-tree-enums
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    #[non_exhaustive]
    pub enum Type {
        /// A fixed size array type: `[T; n]`.
        Array(TypeArray),

        /// A function pointer type: `fn(usize) -> bool`.
        FnPtr(TypeFnPtr),

        /// A type contained within invisible delimiters.
        Group(TypeGroup),

        /// An `impl Bound1 + Bound2 + Bound3` type where `Bound` is a trait or
        /// a lifetime.
        ImplTrait(TypeImplTrait),

        /// Indication that a type should be inferred by the compiler: `_`.
        Infer(TypeInfer),

        /// A macro in the type position.
        Macro(TypeMacro),

        /// The never type: `!`.
        Never(TypeNever),

        /// A parenthesized type equivalent to the inner type.
        Paren(TypeParen),

        /// A path like `core::slice::Iter`, optionally qualified with a
        /// self-type as in `<Vec<T> as SomeTrait>::Associated`.
        Path(TypePath),

        /// A raw pointer type: `*const T` or `*mut T`.
        Ptr(TypePtr),

        /// A reference type: `&'a T` or `&'a mut T`.
        Reference(TypeReference),

        /// A dynamically sized slice type: `[T]`.
        Slice(TypeSlice),

        /// A trait object type `dyn Bound1 + Bound2 + Bound3` where `Bound` is a
        /// trait or a lifetime.
        TraitObject(TypeTraitObject),

        /// A tuple type: `(A, B, C, String)`.
        Tuple(TypeTuple),

        /// Tokens in type position not interpreted by Syn.
        ///
        /// <div class="warning">
        ///
        /// Important: see [Compatibility notes][crate#verbatim-variants].
        ///
        /// </div>
        Verbatim(TokenStream),
    }
}

ast_struct! {
    /// A fixed size array type: `[T; n]`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeArray {
        pub attrs: Vec<Attribute>,
        pub bracket_token: token::Bracket,
        pub elem: Box<Type>,
        pub semi_token: Token![;],
        pub len: Expr,
    }
}

ast_struct! {
    /// A function pointer type: `fn(usize) -> bool`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeFnPtr {
        pub attrs: Vec<Attribute>,
        pub lifetimes: Option<BoundLifetimes>,
        pub unsafety: Option<Token![unsafe]>,
        pub abi: Option<Abi>,
        pub fn_token: Token![fn],
        pub paren_token: token::Paren,
        pub inputs: Punctuated<NamedArg, Token![,]>,
        pub variadic: Option<FnPtrVariadic>,
        pub output: ReturnType,
    }
}

ast_struct! {
    /// A type contained within invisible delimiters.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeGroup {
        pub attrs: Vec<Attribute>,
        pub group_token: token::Group,
        pub elem: Box<Type>,
    }
}

ast_struct! {
    /// An `impl Bound1 + Bound2 + Bound3` type where `Bound` is a trait or
    /// a lifetime.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeImplTrait {
        pub attrs: Vec<Attribute>,
        pub impl_token: Token![impl],
        pub bounds: Punctuated<TypeParamBound, Token![+]>,
    }
}

ast_struct! {
    /// Indication that a type should be inferred by the compiler: `_`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeInfer {
        pub attrs: Vec<Attribute>,
        pub underscore_token: Token![_],
    }
}

ast_struct! {
    /// A macro in the type position.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeMacro {
        pub attrs: Vec<Attribute>,
        pub mac: Macro,
    }
}

ast_struct! {
    /// The never type: `!`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeNever {
        pub attrs: Vec<Attribute>,
        pub bang_token: Token![!],
    }
}

ast_struct! {
    /// A parenthesized type equivalent to the inner type.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeParen {
        pub attrs: Vec<Attribute>,
        pub paren_token: token::Paren,
        pub elem: Box<Type>,
    }
}

ast_struct! {
    /// A path like `core::slice::Iter`, optionally qualified with a
    /// self-type as in `<Vec<T> as SomeTrait>::Associated`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypePath {
        pub attrs: Vec<Attribute>,
        pub qself: Option<QSelf>,
        pub path: Path,
    }
}

ast_struct! {
    /// A raw pointer type: `*const T` or `*mut T`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypePtr {
        pub attrs: Vec<Attribute>,
        pub star_token: Token![*],
        pub mutability: PointerMutability,
        pub elem: Box<Type>,
    }
}

ast_struct! {
    /// A reference type: `&'a T` or `&'a mut T`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeReference {
        pub attrs: Vec<Attribute>,
        pub and_token: Token![&],
        pub lifetime: Option<Lifetime>,
        pub mutability: Option<Token![mut]>,
        pub elem: Box<Type>,
    }
}

ast_struct! {
    /// A dynamically sized slice type: `[T]`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeSlice {
        pub attrs: Vec<Attribute>,
        pub bracket_token: token::Bracket,
        pub elem: Box<Type>,
    }
}

ast_struct! {
    /// A trait object type `dyn Bound1 + Bound2 + Bound3` where `Bound` is a
    /// trait or a lifetime.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeTraitObject {
        pub attrs: Vec<Attribute>,
        /// The `dyn` keyword is required since Rust 2021 edition. In editions
        /// 2015&ndash;2018, trait objects without a `dyn` keyword are allowed
        /// but deprecated.
        pub dyn_token: Option<Token![dyn]>,
        pub bounds: Punctuated<TypeParamBound, Token![+]>,
    }
}

ast_struct! {
    /// A tuple type: `(A, B, C, String)`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct TypeTuple {
        pub attrs: Vec<Attribute>,
        pub paren_token: token::Paren,
        pub elems: Punctuated<Type, Token![,]>,
    }
}

ast_struct! {
    /// The binary interface of a function: `extern "C"`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct Abi {
        pub extern_token: Token![extern],

        /// ABI name is optional, but note that extern blocks and functions with
        /// an omitted ABI name are [deprecated since Rust 1.86.0][deprecated].
        /// Omitting the ABI after the extern keyword has always implicitly
        /// resulted in the "C" ABI. It is now recommended to explicitly specify
        /// the "C" ABI (`extern "C" {}` and `extern "C" fn`).
        ///
        /// [deprecated]: https://blog.rust-lang.org/2025/04/03/Rust-1.86.0/#make-missing-abi-lint-warn-by-default
        pub name: Option<LitStr>,
    }
}

ast_enum! {
    /// Mutability of a raw pointer (`*const T`, `*mut T`), in which non-mutable
    /// isn't the implicit default.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub enum PointerMutability {
        Const(Token![const]),
        Mut(Token![mut]),
    }
}

ast_struct! {
    /// An argument in a function type: the `usize` in `fn(usize) -> bool`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct NamedArg {
        pub attrs: Vec<Attribute>,
        pub name: Option<(Ident, Token![:])>,
        pub ty: Type,
    }
}

ast_struct! {
    /// The variadic argument of a function pointer like `fn(usize, ...)`.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub struct FnPtrVariadic {
        pub attrs: Vec<Attribute>,
        pub name: Option<(Ident, Token![:])>,
        pub dots: Token![...],
        pub comma: Option<Token![,]>,
    }
}

ast_enum! {
    /// Return type of a function signature.
    #[cfg_attr(docsrs, doc(cfg(any(feature = "full", feature = "derive"))))]
    pub enum ReturnType {
        /// Return type is not specified.
        ///
        /// Functions default to `()` and closures default to type inference.
        Default,
        /// A particular type is returned.
        Type(Token![->], Box<Type>),
    }
}

#[cfg(feature = "parsing")]
pub(crate) mod parsing {
    use crate::attr::Attribute;
    use crate::buffer::Cursor;
    use crate::error::{Error, Result};
    use crate::ext::IdentExt as _;
    use crate::generics::{BoundLifetimes, TraitBound, TraitBoundModifiers, TypeParamBound};
    use crate::ident::Ident;
    use crate::lifetime::Lifetime;
    use crate::mac::{self, Macro};
    use crate::parse::{Parse, ParseStream};
    use crate::path;
    use crate::path::{Path, PathArguments, QSelf};
    use crate::punctuated::Punctuated;
    use crate::token;
    use crate::ty::{
        Abi, FnPtrVariadic, NamedArg, PointerMutability, ReturnType, Type, TypeArray, TypeFnPtr,
        TypeGroup, TypeImplTrait, TypeInfer, TypeMacro, TypeNever, TypeParen, TypePath, TypePtr,
        TypeReference, TypeSlice, TypeTraitObject, TypeTuple,
    };
    use crate::verbatim;
    use alloc::boxed::Box;
    use alloc::vec::Vec;
    use proc_macro2::TokenStream;

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for Type {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_plus = true;
            let allow_group_generic = true;
            ambig_ty(input, allow_plus, allow_group_generic)
        }
    }

    impl Type {
        /// In some positions, types may not contain the `+` character, to
        /// disambiguate them. For example in the expression `1 as T`, T may not
        /// contain a `+` character.
        ///
        /// This parser does not allow a `+`, while the default parser does.
        #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
        pub fn without_plus(input: ParseStream) -> Result<Self> {
            let allow_plus = false;
            let allow_group_generic = true;
            ambig_ty(input, allow_plus, allow_group_generic)
        }
    }

    pub(crate) fn ambig_ty(
        input: ParseStream,
        allow_plus: bool,
        allow_group_generic: bool,
    ) -> Result<Type> {
        let begin = input.cursor();

        if input.peek(token::Group) {
            let mut group: TypeGroup = input.parse()?;
            if input.peek(Token![::]) && input.peek3(Ident::peek_any) {
                if let Type::Path(mut ty) = *group.elem {
                    Path::parse_rest(input, &mut ty.path, false)?;
                    return Ok(Type::Path(ty));
                } else {
                    return Ok(Type::Path(TypePath {
                        attrs: Vec::new(),
                        qself: Some(QSelf {
                            lt_token: Token![<](group.group_token.span),
                            position: 0,
                            as_token: None,
                            gt_token: Token![>](group.group_token.span),
                            ty: group.elem,
                        }),
                        path: Path::parse_helper(input, false)?,
                    }));
                }
            } else if input.peek(Token![<]) && allow_group_generic
                || input.peek(Token![::]) && input.peek3(Token![<])
            {
                if let Type::Path(mut ty) = *group.elem {
                    let arguments = &mut ty.path.segments.last_mut().unwrap().arguments;
                    if arguments.is_none() {
                        *arguments = PathArguments::AngleBracketed(input.parse()?);
                        Path::parse_rest(input, &mut ty.path, false)?;
                        return Ok(Type::Path(ty));
                    } else {
                        *group.elem = Type::Path(ty);
                    }
                }
            }
            return Ok(Type::Group(group));
        }

        let mut lifetimes = None::<BoundLifetimes>;
        let mut lookahead = input.lookahead1();
        if lookahead.peek(Token![for]) {
            lifetimes = input.parse()?;
            lookahead = input.lookahead1();
            if !lookahead.peek(Ident)
                && !lookahead.peek(Token![fn])
                && !lookahead.peek(Token![unsafe])
                && !lookahead.peek(Token![extern])
                && !lookahead.peek(Token![super])
                && !lookahead.peek(Token![self])
                && !lookahead.peek(Token![Self])
                && !lookahead.peek(Token![crate])
                || input.peek(Token![dyn])
            {
                return Err(lookahead.error());
            }
        }

        if lookahead.peek(token::Paren) {
            let content;
            let paren_token = parenthesized!(content in input);
            if content.is_empty() {
                return Ok(Type::Tuple(TypeTuple {
                    attrs: Vec::new(),
                    paren_token,
                    elems: Punctuated::new(),
                }));
            }
            if content.peek(Lifetime) {
                return Ok(Type::Paren(TypeParen {
                    attrs: Vec::new(),
                    paren_token,
                    elem: Box::new(Type::TraitObject(content.parse()?)),
                }));
            }
            if content.peek(Token![?]) {
                return Ok(Type::TraitObject(TypeTraitObject {
                    attrs: Vec::new(),
                    dyn_token: None,
                    bounds: {
                        let mut bounds = Punctuated::new();
                        bounds.push_value(TypeParamBound::Trait(TraitBound {
                            paren_token: Some(paren_token),
                            ..content.parse()?
                        }));
                        while let Some(plus) = input.parse()? {
                            bounds.push_punct(plus);
                            bounds.push_value({
                                let allow_precise_capture = false;
                                let allow_const = false;
                                TypeParamBound::parse_single(
                                    input,
                                    allow_precise_capture,
                                    allow_const,
                                )?
                            });
                        }
                        bounds
                    },
                }));
            }
            let mut first: Type = content.parse()?;
            if content.peek(Token![,]) {
                return Ok(Type::Tuple(TypeTuple {
                    attrs: Vec::new(),
                    paren_token,
                    elems: {
                        let mut elems = Punctuated::new();
                        elems.push_value(first);
                        elems.push_punct(content.parse()?);
                        while !content.is_empty() {
                            elems.push_value(content.parse()?);
                            if content.is_empty() {
                                break;
                            }
                            elems.push_punct(content.parse()?);
                        }
                        elems
                    },
                }));
            }
            if allow_plus && input.peek(Token![+]) {
                loop {
                    let first = match first {
                        Type::Path(TypePath {
                            attrs: _,
                            qself: None,
                            path,
                        }) => TypeParamBound::Trait(TraitBound {
                            paren_token: Some(paren_token),
                            lifetimes: None,
                            modifiers: TraitBoundModifiers {},
                            maybe: None,
                            path,
                        }),
                        Type::TraitObject(TypeTraitObject {
                            attrs: _,
                            dyn_token: None,
                            bounds,
                        }) => {
                            if bounds.len() > 1 || bounds.trailing_punct() {
                                first = Type::TraitObject(TypeTraitObject {
                                    attrs: Vec::new(),
                                    dyn_token: None,
                                    bounds,
                                });
                                break;
                            }
                            match bounds.into_iter().next().unwrap() {
                                TypeParamBound::Trait(trait_bound) => {
                                    TypeParamBound::Trait(TraitBound {
                                        paren_token: Some(paren_token),
                                        ..trait_bound
                                    })
                                }
                                other @ (TypeParamBound::Lifetime(_)
                                | TypeParamBound::PreciseCapture(_)
                                | TypeParamBound::Verbatim(_)) => other,
                            }
                        }
                        _ => break,
                    };
                    return Ok(Type::TraitObject(TypeTraitObject {
                        attrs: Vec::new(),
                        dyn_token: None,
                        bounds: {
                            let mut bounds = Punctuated::new();
                            bounds.push_value(first);
                            while let Some(plus) = input.parse()? {
                                bounds.push_punct(plus);
                                bounds.push_value({
                                    let allow_precise_capture = false;
                                    let allow_const = false;
                                    TypeParamBound::parse_single(
                                        input,
                                        allow_precise_capture,
                                        allow_const,
                                    )?
                                });
                            }
                            bounds
                        },
                    }));
                }
            }
            Ok(Type::Paren(TypeParen {
                attrs: Vec::new(),
                paren_token,
                elem: Box::new(first),
            }))
        } else if lookahead.peek(Token![unsafe]) && input.peek2(Token![<]) {
            input.parse::<Token![unsafe]>()?;
            input.parse::<Token![<]>()?;
            while !input.peek(Token![>]) {
                Lifetime::parse_any(input)?;
                if input.peek(Token![>]) {
                    break;
                }
                input.parse::<Token![,]>()?;
            }
            input.parse::<Token![>]>()?;
            ambig_ty(input, allow_plus, allow_group_generic)?;
            Ok(Type::Verbatim(verbatim::between(begin, input.cursor())))
        } else if lookahead.peek(Token![fn])
            || input.peek(Token![unsafe])
            || lookahead.peek(Token![extern])
        {
            let mut fn_ptr: TypeFnPtr = input.parse()?;
            fn_ptr.lifetimes = lifetimes;
            Ok(Type::FnPtr(fn_ptr))
        } else if cfg!(feature = "full")
            && input.cursor().peek_keyword("builtin")
            && input.peek2(Token![#])
        {
            token::parsing::keyword(input, "builtin")?;
            input.parse::<Token![#]>()?;
            input.parse::<Ident>()?;
            let args;
            parenthesized!(args in input);
            args.parse::<TokenStream>()?;
            Ok(Type::Verbatim(verbatim::between(begin, input.cursor())))
        } else if lookahead.peek(Ident)
            || input.peek(Token![super])
            || input.peek(Token![self])
            || input.peek(Token![Self])
            || input.peek(Token![crate])
            || lookahead.peek(Token![::])
            || lookahead.peek(Token![<])
        {
            let ty: TypePath = input.parse()?;
            if ty.qself.is_some() {
                return Ok(Type::Path(ty));
            }

            if input.peek(Token![!]) && !input.peek(Token![!=]) && ty.path.is_mod_style() {
                let bang_token: Token![!] = input.parse()?;
                let (delimiter, tokens) = mac::parse_delimiter(input)?;
                return Ok(Type::Macro(TypeMacro {
                    attrs: Vec::new(),
                    mac: Macro {
                        path: ty.path,
                        bang_token,
                        delimiter,
                        tokens,
                    },
                }));
            }

            if lifetimes.is_some() || allow_plus && input.peek(Token![+]) {
                let mut bounds = Punctuated::new();
                bounds.push_value(TypeParamBound::Trait(TraitBound {
                    paren_token: None,
                    lifetimes,
                    modifiers: TraitBoundModifiers {},
                    maybe: None,
                    path: ty.path,
                }));
                if allow_plus {
                    while input.peek(Token![+]) {
                        bounds.push_punct(input.parse()?);
                        if !(input.peek(Ident::peek_any)
                            || input.peek(Token![::])
                            || input.peek(Token![?])
                            || input.peek(Lifetime)
                            || input.peek(token::Paren))
                        {
                            break;
                        }
                        bounds.push_value({
                            let allow_precise_capture = false;
                            let allow_const = false;
                            TypeParamBound::parse_single(input, allow_precise_capture, allow_const)?
                        });
                    }
                }
                return Ok(Type::TraitObject(TypeTraitObject {
                    attrs: Vec::new(),
                    dyn_token: None,
                    bounds,
                }));
            }

            Ok(Type::Path(ty))
        } else if lookahead.peek(Token![dyn]) {
            let dyn_begin = input.cursor();
            let dyn_token: Token![dyn] = input.parse()?;
            let star_token: Option<Token![*]> = input.parse()?;
            let bounds = TypeTraitObject::parse_bounds(dyn_begin, input, allow_plus)?;
            Ok(if star_token.is_some() {
                Type::Verbatim(verbatim::between(begin, input.cursor()))
            } else {
                Type::TraitObject(TypeTraitObject {
                    attrs: Vec::new(),
                    dyn_token: Some(dyn_token),
                    bounds,
                })
            })
        } else if lookahead.peek(token::Bracket) {
            let content;
            let bracket_token = bracketed!(content in input);
            let elem: Type = content.parse()?;
            if content.peek(Token![;]) {
                Ok(Type::Array(TypeArray {
                    attrs: Vec::new(),
                    bracket_token,
                    elem: Box::new(elem),
                    semi_token: content.parse()?,
                    len: content.parse()?,
                }))
            } else {
                Ok(Type::Slice(TypeSlice {
                    attrs: Vec::new(),
                    bracket_token,
                    elem: Box::new(elem),
                }))
            }
        } else if lookahead.peek(Token![*]) {
            input.parse().map(Type::Ptr)
        } else if lookahead.peek(Token![&]) {
            input.parse().map(Type::Reference)
        } else if lookahead.peek(Token![!]) && !input.peek(Token![=]) {
            input.parse().map(Type::Never)
        } else if lookahead.peek(Token![impl]) {
            TypeImplTrait::parse(input, allow_plus).map(Type::ImplTrait)
        } else if lookahead.peek(Token![_]) {
            input.parse().map(Type::Infer)
        } else if lookahead.peek(Lifetime) {
            input.parse().map(Type::TraitObject)
        } else {
            Err(lookahead.error())
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeSlice {
        fn parse(input: ParseStream) -> Result<Self> {
            let content;
            Ok(TypeSlice {
                attrs: Vec::new(),
                bracket_token: bracketed!(content in input),
                elem: content.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeArray {
        fn parse(input: ParseStream) -> Result<Self> {
            let content;
            Ok(TypeArray {
                attrs: Vec::new(),
                bracket_token: bracketed!(content in input),
                elem: content.parse()?,
                semi_token: content.parse()?,
                len: content.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypePtr {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(TypePtr {
                attrs: Vec::new(),
                star_token: input.parse()?,
                mutability: input.parse()?,
                elem: Box::new(input.call(Type::without_plus)?),
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeReference {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(TypeReference {
                attrs: Vec::new(),
                and_token: input.parse()?,
                lifetime: Lifetime::parse_optional_any(input),
                mutability: input.parse()?,
                // & binds tighter than +, so we don't allow + here.
                elem: Box::new(input.call(Type::without_plus)?),
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeFnPtr {
        fn parse(input: ParseStream) -> Result<Self> {
            let args;
            let mut variadic = None;

            Ok(TypeFnPtr {
                attrs: Vec::new(),
                lifetimes: input.parse()?,
                unsafety: input.parse()?,
                abi: input.parse()?,
                fn_token: input.parse()?,
                paren_token: parenthesized!(args in input),
                inputs: {
                    let mut inputs = Punctuated::new();

                    while !args.is_empty() {
                        let attrs = args.call(Attribute::parse_outer)?;

                        if inputs.empty_or_trailing()
                            && (args.peek(Token![...])
                                || (args.peek(Ident) || args.peek(Token![_]))
                                    && args.peek2(Token![:])
                                    && args.peek3(Token![...]))
                        {
                            variadic = Some(parse_fn_ptr_variadic(&args, attrs)?);
                            break;
                        }

                        let allow_self = inputs.is_empty();
                        let arg = parse_fn_ptr_arg(&args, allow_self)?;
                        inputs.push_value(NamedArg { attrs, ..arg });
                        if args.is_empty() {
                            break;
                        }

                        let comma = args.parse()?;
                        inputs.push_punct(comma);
                    }

                    inputs
                },
                variadic,
                output: input.call(ReturnType::without_plus)?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeNever {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(TypeNever {
                attrs: Vec::new(),
                bang_token: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeInfer {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(TypeInfer {
                attrs: Vec::new(),
                underscore_token: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeTuple {
        fn parse(input: ParseStream) -> Result<Self> {
            let content;
            let paren_token = parenthesized!(content in input);

            if content.is_empty() {
                return Ok(TypeTuple {
                    attrs: Vec::new(),
                    paren_token,
                    elems: Punctuated::new(),
                });
            }

            let first: Type = content.parse()?;
            Ok(TypeTuple {
                attrs: Vec::new(),
                paren_token,
                elems: {
                    let mut elems = Punctuated::new();
                    elems.push_value(first);
                    elems.push_punct(content.parse()?);
                    while !content.is_empty() {
                        elems.push_value(content.parse()?);
                        if content.is_empty() {
                            break;
                        }
                        elems.push_punct(content.parse()?);
                    }
                    elems
                },
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeMacro {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(TypeMacro {
                attrs: Vec::new(),
                mac: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypePath {
        fn parse(input: ParseStream) -> Result<Self> {
            let expr_style = false;
            let (qself, path) = path::parsing::qpath(input, expr_style)?;
            Ok(TypePath {
                attrs: Vec::new(),
                qself,
                path,
            })
        }
    }

    impl ReturnType {
        #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
        pub fn without_plus(input: ParseStream) -> Result<Self> {
            let allow_plus = false;
            Self::parse(input, allow_plus)
        }

        pub(crate) fn parse(input: ParseStream, allow_plus: bool) -> Result<Self> {
            if input.peek(Token![->]) {
                let arrow = input.parse()?;
                let allow_group_generic = true;
                let ty = ambig_ty(input, allow_plus, allow_group_generic)?;
                Ok(ReturnType::Type(arrow, Box::new(ty)))
            } else {
                Ok(ReturnType::Default)
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for ReturnType {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_plus = true;
            Self::parse(input, allow_plus)
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeTraitObject {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_plus = true;
            Self::parse(input, allow_plus)
        }
    }

    impl TypeTraitObject {
        #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
        pub fn without_plus(input: ParseStream) -> Result<Self> {
            let allow_plus = false;
            Self::parse(input, allow_plus)
        }

        // Only allow multiple trait references if allow_plus is true.
        pub(crate) fn parse(input: ParseStream, allow_plus: bool) -> Result<Self> {
            let dyn_begin = input.cursor();
            let dyn_token: Option<Token![dyn]> = input.parse()?;
            let bounds = Self::parse_bounds(dyn_begin, input, allow_plus)?;
            Ok(TypeTraitObject {
                attrs: Vec::new(),
                dyn_token,
                bounds,
            })
        }

        fn parse_bounds(
            dyn_begin: Cursor,
            input: ParseStream,
            allow_plus: bool,
        ) -> Result<Punctuated<TypeParamBound, Token![+]>> {
            let allow_precise_capture = false;
            let allow_const = false;
            let bounds = TypeParamBound::parse_multiple(
                input,
                allow_plus,
                allow_precise_capture,
                allow_const,
            )?;
            let mut at_least_one_trait = false;
            for bound in &bounds {
                match bound {
                    TypeParamBound::Trait(_) => {
                        at_least_one_trait = true;
                        break;
                    }
                    TypeParamBound::Lifetime(_) => {}
                    TypeParamBound::PreciseCapture(_) | TypeParamBound::Verbatim(_) => {
                        unreachable!()
                    }
                }
            }
            // Just lifetimes like `'a + 'b` is not a TraitObject.
            if !at_least_one_trait {
                let msg = "at least one trait is required for an object type";
                return Err(Error::new_range(dyn_begin..input.cursor(), msg));
            }
            Ok(bounds)
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeImplTrait {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_plus = true;
            Self::parse(input, allow_plus)
        }
    }

    impl TypeImplTrait {
        #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
        pub fn without_plus(input: ParseStream) -> Result<Self> {
            let allow_plus = false;
            Self::parse(input, allow_plus)
        }

        pub(crate) fn parse(input: ParseStream, allow_plus: bool) -> Result<Self> {
            let impl_begin = input.cursor();
            let impl_token: Token![impl] = input.parse()?;
            let allow_precise_capture = true;
            let allow_const = true;
            let bounds = TypeParamBound::parse_multiple(
                input,
                allow_plus,
                allow_precise_capture,
                allow_const,
            )?;
            let mut at_least_one_trait = false;
            for bound in &bounds {
                match bound {
                    TypeParamBound::Trait(_) => {
                        at_least_one_trait = true;
                        break;
                    }
                    TypeParamBound::Lifetime(_) | TypeParamBound::PreciseCapture(_) => {}
                    TypeParamBound::Verbatim(_) => {
                        // `[const] Trait`
                        at_least_one_trait = true;
                        break;
                    }
                }
            }
            if !at_least_one_trait {
                let msg = "at least one trait must be specified";
                return Err(Error::new_range(impl_begin..input.cursor(), msg));
            }
            Ok(TypeImplTrait {
                attrs: Vec::new(),
                impl_token,
                bounds,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeGroup {
        fn parse(input: ParseStream) -> Result<Self> {
            let group = crate::group::parse_group(input)?;
            Ok(TypeGroup {
                attrs: Vec::new(),
                group_token: group.token,
                elem: group.content.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for TypeParen {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_plus = false;
            Self::parse(input, allow_plus)
        }
    }

    impl TypeParen {
        fn parse(input: ParseStream, allow_plus: bool) -> Result<Self> {
            let content;
            Ok(TypeParen {
                attrs: Vec::new(),
                paren_token: parenthesized!(content in input),
                elem: Box::new({
                    let allow_group_generic = true;
                    ambig_ty(&content, allow_plus, allow_group_generic)?
                }),
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for NamedArg {
        fn parse(input: ParseStream) -> Result<Self> {
            let allow_self = false;
            parse_fn_ptr_arg(input, allow_self)
        }
    }

    fn parse_fn_ptr_arg(input: ParseStream, allow_self: bool) -> Result<NamedArg> {
        let attrs = input.call(Attribute::parse_outer)?;

        let begin = input.cursor();

        let has_mut_self = allow_self && input.peek(Token![mut]) && input.peek2(Token![self]);
        if has_mut_self {
            input.parse::<Token![mut]>()?;
        }

        let mut has_self = false;
        let mut name = if (input.peek(Ident) || input.peek(Token![_]) || {
            has_self = allow_self && input.peek(Token![self]);
            has_self
        }) && input.peek2(Token![:])
            && !input.peek2(Token![::])
        {
            let name = input.call(Ident::parse_any)?;
            let colon: Token![:] = input.parse()?;
            Some((name, colon))
        } else {
            has_self = false;
            None
        };

        let ty = if allow_self && !has_self && input.peek(Token![mut]) && input.peek2(Token![self])
        {
            input.parse::<Token![mut]>()?;
            input.parse::<Token![self]>()?;
            None
        } else if has_mut_self && name.is_none() {
            input.parse::<Token![self]>()?;
            None
        } else {
            Some(input.parse()?)
        };

        let ty = match ty {
            Some(ty) if !has_mut_self => ty,
            _ => {
                name = None;
                Type::Verbatim(verbatim::between(begin, input.cursor()))
            }
        };

        Ok(NamedArg { attrs, name, ty })
    }

    fn parse_fn_ptr_variadic(input: ParseStream, attrs: Vec<Attribute>) -> Result<FnPtrVariadic> {
        Ok(FnPtrVariadic {
            attrs,
            name: if input.peek(Ident) || input.peek(Token![_]) {
                let name = input.call(Ident::parse_any)?;
                let colon: Token![:] = input.parse()?;
                Some((name, colon))
            } else {
                None
            },
            dots: input.parse()?,
            comma: input.parse()?,
        })
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for Abi {
        fn parse(input: ParseStream) -> Result<Self> {
            Ok(Abi {
                extern_token: input.parse()?,
                name: input.parse()?,
            })
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for Option<Abi> {
        fn parse(input: ParseStream) -> Result<Self> {
            if input.peek(Token![extern]) {
                input.parse().map(Some)
            } else {
                Ok(None)
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "parsing")))]
    impl Parse for PointerMutability {
        fn parse(input: ParseStream) -> Result<Self> {
            let lookahead = input.lookahead1();
            if lookahead.peek(Token![const]) {
                Ok(PointerMutability::Const(input.parse()?))
            } else if lookahead.peek(Token![mut]) {
                Ok(PointerMutability::Mut(input.parse()?))
            } else {
                Err(lookahead.error())
            }
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use crate::attr::FilterAttrs;
    use crate::path;
    use crate::path::printing::PathStyle;
    use crate::ty::{
        Abi, FnPtrVariadic, NamedArg, PointerMutability, ReturnType, TypeArray, TypeFnPtr,
        TypeGroup, TypeImplTrait, TypeInfer, TypeMacro, TypeNever, TypeParen, TypePath, TypePtr,
        TypeReference, TypeSlice, TypeTraitObject, TypeTuple,
    };
    use proc_macro2::TokenStream;
    use quote::{ToTokens, TokenStreamExt as _};

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeSlice {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.bracket_token.surround(tokens, |tokens| {
                self.elem.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeArray {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.bracket_token.surround(tokens, |tokens| {
                self.elem.to_tokens(tokens);
                self.semi_token.to_tokens(tokens);
                self.len.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypePtr {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.star_token.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.elem.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeReference {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.and_token.to_tokens(tokens);
            self.lifetime.to_tokens(tokens);
            self.mutability.to_tokens(tokens);
            self.elem.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeFnPtr {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.lifetimes.to_tokens(tokens);
            self.unsafety.to_tokens(tokens);
            self.abi.to_tokens(tokens);
            self.fn_token.to_tokens(tokens);
            self.paren_token.surround(tokens, |tokens| {
                self.inputs.to_tokens(tokens);
                if let Some(variadic) = &self.variadic {
                    if !self.inputs.empty_or_trailing() {
                        let span = variadic.dots.spans[0];
                        Token![,](span).to_tokens(tokens);
                    }
                    variadic.to_tokens(tokens);
                }
            });
            self.output.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeNever {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.bang_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeTuple {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.paren_token.surround(tokens, |tokens| {
                self.elems.to_tokens(tokens);
                // If we only have one argument, we need a trailing comma to
                // distinguish TypeTuple from TypeParen.
                if self.elems.len() == 1 && !self.elems.trailing_punct() {
                    <Token![,]>::default().to_tokens(tokens);
                }
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypePath {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            path::printing::print_qpath(tokens, &self.qself, &self.path, PathStyle::AsWritten);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeTraitObject {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.dyn_token.to_tokens(tokens);
            self.bounds.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeImplTrait {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.impl_token.to_tokens(tokens);
            self.bounds.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeGroup {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.group_token.surround(tokens, |tokens| {
                self.elem.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeParen {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.paren_token.surround(tokens, |tokens| {
                self.elem.to_tokens(tokens);
            });
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeInfer {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.underscore_token.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for TypeMacro {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.mac.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for ReturnType {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                ReturnType::Default => {}
                ReturnType::Type(arrow, ty) => {
                    arrow.to_tokens(tokens);
                    ty.to_tokens(tokens);
                }
            }
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for NamedArg {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            if let Some((name, colon)) = &self.name {
                name.to_tokens(tokens);
                colon.to_tokens(tokens);
            }
            self.ty.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for FnPtrVariadic {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            tokens.append_all(self.attrs.outer());
            if let Some((name, colon)) = &self.name {
                name.to_tokens(tokens);
                colon.to_tokens(tokens);
            }
            self.dots.to_tokens(tokens);
            self.comma.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for Abi {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            self.extern_token.to_tokens(tokens);
            self.name.to_tokens(tokens);
        }
    }

    #[cfg_attr(docsrs, doc(cfg(feature = "printing")))]
    impl ToTokens for PointerMutability {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match self {
                PointerMutability::Const(const_token) => const_token.to_tokens(tokens),
                PointerMutability::Mut(mut_token) => mut_token.to_tokens(tokens),
            }
        }
    }
}
