use std::{
    borrow::Cow,
    fmt::Display,
    ops::{Deref, DerefMut},
};

use once_cell::sync::Lazy;
use phf::phf_set;
use rustc_hash::FxHashSet;
use swc_atoms::{atom, Atom, UnsafeAtom};
use swc_common::{
    ast_node, util::take::Take, BytePos, EqIgnoreSpan, Mark, Span, Spanned, SyntaxContext, DUMMY_SP,
};

use crate::{typescript::TsTypeAnn, Expr};

/// Identifier used as a pattern.
#[derive(Clone, Debug, PartialEq, Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(
    feature = "rkyv-impl",
    rkyv(serialize_bounds(__S: rkyv::ser::Writer + rkyv::ser::Allocator,
        __S::Error: rkyv::rancor::Source))
)]
#[cfg_attr(
    feature = "rkyv-impl",
    rkyv(deserialize_bounds(__D::Error: rkyv::rancor::Source))
)]
#[cfg_attr(
    feature = "rkyv-impl",
    rkyv(bytecheck(bounds(
        __C: rkyv::validation::ArchiveContext,
        __C::Error: rkyv::rancor::Source
    )))
)]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "serde-impl", derive(serde::Serialize, serde::Deserialize))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::swc_common::Encode, ::swc_common::Decode)
)]
pub struct BindingIdent {
    #[cfg_attr(feature = "serde-impl", serde(flatten))]
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub id: Ident,

    #[cfg_attr(feature = "serde-impl", serde(default, rename = "typeAnnotation"))]
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub type_ann: Option<Box<TsTypeAnn>>,
}

impl Spanned for BindingIdent {
    fn span(&self) -> Span {
        match &self.type_ann {
            Some(ann) => Span::new(self.id.span.lo(), ann.span().hi()),
            None => self.id.span,
        }
    }
}

impl Deref for BindingIdent {
    type Target = Ident;

    fn deref(&self) -> &Self::Target {
        &self.id
    }
}

impl DerefMut for BindingIdent {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.id
    }
}

impl AsRef<str> for BindingIdent {
    fn as_ref(&self) -> &str {
        &self.sym
    }
}

impl From<BindingIdent> for Box<Expr> {
    fn from(bi: BindingIdent) -> Self {
        Box::new(Expr::Ident(bi.into()))
    }
}
impl From<&'_ BindingIdent> for Ident {
    fn from(bi: &'_ BindingIdent) -> Self {
        Ident {
            span: bi.span,
            ctxt: bi.ctxt,
            sym: bi.sym.clone(),
            optional: bi.optional,
        }
    }
}

impl BindingIdent {
    /// See [`Ident::to_id`] for documentation.
    pub fn to_id(&self) -> Id {
        (self.sym.clone(), self.ctxt)
    }
}

impl Take for BindingIdent {
    fn dummy() -> Self {
        Default::default()
    }
}

impl From<Ident> for BindingIdent {
    fn from(id: Ident) -> Self {
        BindingIdent {
            id,
            ..Default::default()
        }
    }
}

bridge_from!(BindingIdent, Ident, Id);

/// A complete identifier with span.
///
/// Identifier of swc consists of two parts. The first one is symbol, which is
/// stored using an interned string, [Atom] . The second
/// one is [SyntaxContext][swc_common::SyntaxContext], which can be
/// used to distinguish identifier with same symbol.
///
/// Let me explain this with an example.
///
/// ```ts
/// let a = 5
/// {
///     let a = 3;
/// }
/// ```
/// In the code above, there are two variables with the symbol a.
///
///
/// Other compilers typically uses type like `Scope`, and store them nested, but
/// in rust, type like `Scope`  requires [Arc<Mutex<Scope>>] so swc uses
/// different approach. Instead of passing scopes, swc annotates two variables
/// with different tag, which is named
/// [SyntaxContext]. The notation for the syntax
/// context is #n where n is a number. e.g. `foo#1`
///
/// For the example above, after applying resolver pass, it becomes.
///
/// ```ts
/// let a#1 = 5
/// {
///     let a#2 = 3;
/// }
/// ```
///
/// Thanks to the `tag` we attached, we can now distinguish them.
///
/// ([Atom], [SyntaxContext])
///
/// See [Id], which is a type alias for this.
///
/// This can be used to store all variables in a module to single hash map.
///
/// # Comparison
///
/// While comparing two identifiers, you can use `.to_id()`.
///
/// # HashMap
///
/// There's a type named [Id] which only contains minimal information to
/// distinguish identifiers.
#[ast_node("Identifier")]
#[derive(Eq, Hash, Default)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Ident {
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub span: Span,

    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub ctxt: SyntaxContext,

    #[cfg_attr(feature = "serde-impl", serde(rename = "value"))]
    pub sym: Atom,

    /// TypeScript only. Used in case of an optional parameter.
    #[cfg_attr(feature = "serde-impl", serde(default))]
    pub optional: bool,
}

impl From<BindingIdent> for Ident {
    fn from(bi: BindingIdent) -> Self {
        bi.id
    }
}

impl From<Atom> for Ident {
    fn from(bi: Atom) -> Self {
        Ident::new_no_ctxt(bi, DUMMY_SP)
    }
}
bridge_from!(Ident, Atom, &'_ str);
bridge_from!(Ident, Atom, Cow<'_, str>);
bridge_from!(Ident, Atom, String);

impl From<(Atom, Span)> for Ident {
    fn from((sym, span): (Atom, Span)) -> Self {
        Ident {
            span,
            sym,
            ..Default::default()
        }
    }
}

impl EqIgnoreSpan for Ident {
    fn eq_ignore_span(&self, other: &Self) -> bool {
        if self.sym != other.sym {
            return false;
        }

        self.ctxt.eq_ignore_span(&other.ctxt)
    }
}

impl From<Id> for Ident {
    fn from(id: Id) -> Self {
        Ident::new(id.0, DUMMY_SP, id.1)
    }
}

impl From<Ident> for Id {
    fn from(i: Ident) -> Self {
        (i.sym, i.ctxt)
    }
}

#[repr(C, align(64))]
struct Align64<T>(pub(crate) T);

const T: bool = true;
const F: bool = false;

impl Ident {
    /// In `op`, [EqIgnoreSpan] of [Ident] will ignore the syntax context.
    pub fn within_ignored_ctxt<F, Ret>(op: F) -> Ret
    where
        F: FnOnce() -> Ret,
    {
        SyntaxContext::within_ignored_ctxt(op)
    }

    /// Preserve syntax context while drop `span.lo` and `span.hi`.
    pub fn without_loc(mut self) -> Ident {
        self.span.lo = BytePos::DUMMY;
        self.span.hi = BytePos::DUMMY;
        self
    }

    /// Creates `Id` using `Atom` and `SyntaxContext` of `self`.
    pub fn to_id(&self) -> Id {
        (self.sym.clone(), self.ctxt)
    }

    #[inline]
    pub fn is_valid_ascii_start(c: u8) -> bool {
        debug_assert!(c.is_ascii());
        // This contains `$` (36) and `_` (95)
        const ASCII_START: Align64<[bool; 128]> = Align64([
            F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
            F, F, F, F, F, F, F, T, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
            F, F, F, F, F, F, F, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,
            T, T, T, T, F, F, F, F, T, F, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,
            T, T, T, T, T, T, T, F, F, F, F, F,
        ]);
        ASCII_START.0[c as usize]
    }

    pub fn is_valid_non_ascii_start(c: char) -> bool {
        debug_assert!(!c.is_ascii());
        unicode_id_start::is_id_start_unicode(c)
    }

    /// Returns true if `c` is a valid character for an identifier start.
    #[inline]
    pub fn is_valid_start(c: char) -> bool {
        if c.is_ascii() {
            Self::is_valid_ascii_start(c as u8)
        } else {
            Self::is_valid_non_ascii_start(c)
        }
    }

    #[inline]
    pub fn is_valid_non_ascii_continue(c: char) -> bool {
        debug_assert!(!c.is_ascii());
        unicode_id_start::is_id_continue_unicode(c)
    }

    #[inline]
    pub fn is_valid_ascii_continue(c: u8) -> bool {
        debug_assert!(c.is_ascii());
        // This contains `$` (36)
        const ASCII_CONTINUE: Align64<[bool; 128]> = Align64([
            F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F, F,
            F, F, F, F, F, F, F, T, F, F, F, F, F, F, F, F, F, F, F, T, T, T, T, T, T, T, T, T, T,
            F, F, F, F, F, F, F, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,
            T, T, T, T, F, F, F, F, T, F, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,
            T, T, T, T, T, T, T, F, F, F, F, F,
        ]);
        ASCII_CONTINUE.0[c as usize]
    }

    /// Returns true if `c` is a valid character for an identifier part after
    /// start.
    #[inline]
    pub fn is_valid_continue(c: char) -> bool {
        if c.is_ascii() {
            Self::is_valid_ascii_continue(c as u8)
        } else {
            Self::is_valid_non_ascii_continue(c)
        }
    }

    /// Alternative for `toIdentifier` of babel.
    ///
    /// Returns [Ok] if it's a valid identifier and [Err] if it's not valid.
    /// The returned [Err] contains the valid symbol.
    pub fn verify_symbol(s: &str) -> Result<(), String> {
        fn is_reserved_symbol(s: &str) -> bool {
            s.is_reserved() || s.is_reserved_in_strict_mode(true) || s.is_reserved_in_strict_bind()
        }

        if is_reserved_symbol(s) {
            let mut buf = String::with_capacity(s.len() + 1);
            buf.push('_');
            buf.push_str(s);
            return Err(buf);
        }

        {
            let mut chars = s.chars();

            if let Some(first) = chars.next() {
                if Self::is_valid_start(first) && chars.all(Self::is_valid_continue) {
                    return Ok(());
                }
            }
        }

        let mut buf = String::with_capacity(s.len() + 2);
        let mut has_start = false;

        for c in s.chars() {
            if !has_start && Self::is_valid_start(c) {
                has_start = true;
                buf.push(c);
                continue;
            }

            if Self::is_valid_continue(c) {
                buf.push(c);
            }
        }

        if buf.is_empty() {
            buf.push('_');
        }

        if is_reserved_symbol(&buf) {
            let mut new_buf = String::with_capacity(buf.len() + 1);
            new_buf.push('_');
            new_buf.push_str(&buf);
            buf = new_buf;
        }

        Err(buf)
    }

    /// Create a new identifier with the given prefix.
    pub fn with_prefix(&self, prefix: &str) -> Ident {
        Ident::new(
            format!("{}{}", prefix, self.sym).into(),
            self.span,
            self.ctxt,
        )
    }

    /// Create a private identifier that is unique in the file, but with the
    /// same symbol.
    pub fn into_private(self) -> Ident {
        Self::new(
            self.sym,
            self.span,
            SyntaxContext::empty().apply_mark(Mark::new()),
        )
    }

    #[inline]
    pub fn is_dummy(&self) -> bool {
        self.sym == atom!("") && self.span.is_dummy()
    }

    /// Create a new identifier with the given position.
    pub fn with_pos(mut self, lo: BytePos, hi: BytePos) -> Ident {
        self.span = Span::new(lo, hi);
        self
    }
}

#[ast_node("Identifier")]
#[derive(Eq, Hash, Default, EqIgnoreSpan)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct IdentName {
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub span: Span,

    #[cfg_attr(feature = "serde-impl", serde(rename = "value"))]
    pub sym: Atom,
}

impl From<Atom> for IdentName {
    fn from(sym: Atom) -> Self {
        IdentName {
            span: DUMMY_SP,
            sym,
        }
    }
}

impl From<(Atom, Span)> for IdentName {
    fn from((sym, span): (Atom, Span)) -> Self {
        IdentName { span, sym }
    }
}

bridge_from!(IdentName, Atom, &'_ str);
bridge_from!(IdentName, Atom, Cow<'_, str>);
bridge_from!(IdentName, Atom, String);
bridge_from!(IdentName, Ident, &'_ BindingIdent);
bridge_from!(IdentName, Ident, BindingIdent);

impl AsRef<str> for IdentName {
    fn as_ref(&self) -> &str {
        &self.sym
    }
}

impl IdentName {
    pub const fn new(sym: Atom, span: Span) -> Self {
        Self { span, sym }
    }
}

impl Take for IdentName {
    fn dummy() -> Self {
        Default::default()
    }
}

impl From<Ident> for IdentName {
    fn from(i: Ident) -> Self {
        IdentName {
            span: i.span,
            sym: i.sym,
        }
    }
}

impl From<IdentName> for Ident {
    fn from(i: IdentName) -> Self {
        Ident {
            span: i.span,
            sym: i.sym,
            ..Default::default()
        }
    }
}

bridge_from!(BindingIdent, Ident, Atom);
bridge_from!(BindingIdent, Atom, &'_ str);
bridge_from!(BindingIdent, Atom, Cow<'_, str>);
bridge_from!(BindingIdent, Atom, String);

impl From<IdentName> for BindingIdent {
    fn from(i: IdentName) -> Self {
        BindingIdent {
            id: i.into(),
            ..Default::default()
        }
    }
}

/// UnsafeId is a wrapper around [Id] that does not allocate, but extremely
/// unsafe.
///
/// Do not use this unless you know what you are doing.
///
/// **Currently, it's considered as a unstable API and may be changed in the
/// future without a semver bump.**
pub type UnsafeId = (UnsafeAtom, SyntaxContext);

/// This is extremely unsafe so don't use it unless you know what you are doing.
///
/// # Safety
///
/// See [`UnsafeAtom::new`] for constraints.
///
/// **Currently, it's considered as a unstable API and may be changed in the
/// future without a semver bump.**
pub unsafe fn unsafe_id(id: &Id) -> UnsafeId {
    (UnsafeAtom::new(&id.0), id.1)
}

/// This is extremely unsafe so don't use it unless you know what you are doing.
///
/// # Safety
///
/// See [`UnsafeAtom::new`] for constraints.
///
/// **Currently, it's considered as a unstable API and may be changed in the
/// future without a semver bump.**
pub unsafe fn unsafe_id_from_ident(id: &Ident) -> UnsafeId {
    (UnsafeAtom::new(&id.sym), id.ctxt)
}

/// See [Ident] for documentation.
pub type Id = (Atom, SyntaxContext);

impl Take for Ident {
    fn dummy() -> Self {
        Ident::new_no_ctxt(atom!(""), DUMMY_SP)
    }
}

impl Display for Ident {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}{:?}", self.sym, self.ctxt)
    }
}

impl Display for IdentName {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.sym)
    }
}

impl Display for BindingIdent {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}{:?}", self.sym, self.ctxt)
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Ident {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let span = u.arbitrary()?;
        let sym = u.arbitrary::<Atom>()?;

        let optional = u.arbitrary()?;

        Ok(Self {
            span,
            sym,
            optional,
            ctxt: Default::default(),
        })
    }
}

#[ast_node("PrivateName")]
#[derive(Eq, Hash, EqIgnoreSpan, Default)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct PrivateName {
    pub span: Span,
    #[cfg_attr(feature = "serde-impl", serde(rename = "value"))]
    pub name: Atom,
}

impl AsRef<str> for Ident {
    fn as_ref(&self) -> &str {
        &self.sym
    }
}

impl Ident {
    pub const fn new(sym: Atom, span: Span, ctxt: SyntaxContext) -> Self {
        Ident {
            span,
            ctxt,
            sym,
            optional: false,
        }
    }

    /// Creates a new private identifier. A private identifier is an identifier
    /// that is guaranteed to be unique.
    ///
    /// See https://swc.rs/docs/contributing/es-commons/variable-management for more details.
    ///
    /// Note: This method requires configuring
    /// [GLOBALS](`swc_common::GLOBALS`) because this method use [`Mark::new`]
    /// internally.
    #[inline(never)]
    pub fn new_private(sym: Atom, span: Span) -> Self {
        Self::new(sym, span, SyntaxContext::empty().apply_mark(Mark::new()))
    }

    pub const fn new_no_ctxt(sym: Atom, span: Span) -> Self {
        Self::new(sym, span, SyntaxContext::empty())
    }
}

macro_rules! gen_reserved_set {
    ($set: ident, $set_atoms: ident, [$($item: expr),*]) => {
        static $set: phf::Set<&str> = phf_set!($($item),*);
        static $set_atoms: Lazy<FxHashSet<Atom>> = Lazy::new(|| {
            let mut set = FxHashSet::with_capacity_and_hasher($set.len(), rustc_hash::FxBuildHasher);
            $(
                set.insert(atom!($item));
            )*
            set
        });
    };
}

gen_reserved_set!(
    RESERVED,
    RESERVED_ATOMS,
    [
        "break",
        "case",
        "catch",
        "class",
        "const",
        "continue",
        "debugger",
        "default",
        "delete",
        "do",
        "else",
        "enum",
        "export",
        "extends",
        "false",
        "finally",
        "for",
        "function",
        "if",
        "import",
        "in",
        "instanceof",
        "new",
        "null",
        "package",
        "return",
        "super",
        "switch",
        "this",
        "throw",
        "true",
        "try",
        "typeof",
        "var",
        "void",
        "while",
        "with"
    ]
);

gen_reserved_set!(
    RESSERVED_IN_STRICT_MODE,
    RESSERVED_IN_STRICT_MODE_ATOMS,
    [
        "implements",
        "interface",
        "let",
        "package",
        "private",
        "protected",
        "public",
        "static",
        "yield"
    ]
);

gen_reserved_set!(
    RESSERVED_IN_STRICT_BIND,
    RESSERVED_IN_STRICT_BIND_ATOMS,
    ["eval", "arguments"]
);

gen_reserved_set!(
    RESERVED_IN_ES3,
    RESERVED_IN_ES3_ATOMS,
    [
        "abstract",
        "boolean",
        "byte",
        "char",
        "double",
        "final",
        "float",
        "goto",
        "int",
        "long",
        "native",
        "short",
        "synchronized",
        "throws",
        "transient",
        "volatile"
    ]
);

pub trait EsReserved {
    fn is_reserved(&self) -> bool;
    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool;
    fn is_reserved_in_strict_bind(&self) -> bool;
    fn is_reserved_in_es3(&self) -> bool;
    fn is_reserved_in_any(&self) -> bool;
}

impl EsReserved for Atom {
    fn is_reserved(&self) -> bool {
        is_reserved_for_atom(self)
    }

    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool {
        is_reserved_in_strict_mode_for_atom(self, is_module)
    }

    fn is_reserved_in_strict_bind(&self) -> bool {
        is_reserved_in_strict_bind_for_atom(self)
    }

    fn is_reserved_in_es3(&self) -> bool {
        is_reserved_in_es3_for_atom(self)
    }

    fn is_reserved_in_any(&self) -> bool {
        is_reserved_in_any_for_atom(self)
    }
}
impl EsReserved for IdentName {
    fn is_reserved(&self) -> bool {
        is_reserved_for_atom(&self.sym)
    }

    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool {
        is_reserved_in_strict_mode_for_atom(&self.sym, is_module)
    }

    fn is_reserved_in_strict_bind(&self) -> bool {
        is_reserved_in_strict_bind_for_atom(&self.sym)
    }

    fn is_reserved_in_es3(&self) -> bool {
        is_reserved_in_es3_for_atom(&self.sym)
    }

    fn is_reserved_in_any(&self) -> bool {
        is_reserved_in_any_for_atom(&self.sym)
    }
}
impl EsReserved for Ident {
    fn is_reserved(&self) -> bool {
        is_reserved_for_atom(&self.sym)
    }

    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool {
        is_reserved_in_strict_mode_for_atom(&self.sym, is_module)
    }

    fn is_reserved_in_strict_bind(&self) -> bool {
        is_reserved_in_strict_bind_for_atom(&self.sym)
    }

    fn is_reserved_in_es3(&self) -> bool {
        is_reserved_in_es3_for_atom(&self.sym)
    }

    fn is_reserved_in_any(&self) -> bool {
        is_reserved_in_any_for_atom(&self.sym)
    }
}
impl EsReserved for BindingIdent {
    fn is_reserved(&self) -> bool {
        is_reserved_for_atom(&self.sym)
    }

    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool {
        is_reserved_in_strict_mode_for_atom(&self.sym, is_module)
    }

    fn is_reserved_in_strict_bind(&self) -> bool {
        is_reserved_in_strict_bind_for_atom(&self.sym)
    }

    fn is_reserved_in_es3(&self) -> bool {
        is_reserved_in_es3_for_atom(&self.sym)
    }

    fn is_reserved_in_any(&self) -> bool {
        is_reserved_in_any_for_atom(&self.sym)
    }
}
impl EsReserved for &'_ str {
    fn is_reserved(&self) -> bool {
        is_reserved_for_str(self)
    }

    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool {
        is_reserved_in_strict_mode_for_str(self, is_module)
    }

    fn is_reserved_in_strict_bind(&self) -> bool {
        is_reserved_in_strict_bind_for_str(self)
    }

    fn is_reserved_in_es3(&self) -> bool {
        is_reserved_in_es3_for_str(self)
    }

    fn is_reserved_in_any(&self) -> bool {
        is_reserved_in_any_for_str(self)
    }
}
impl EsReserved for String {
    fn is_reserved(&self) -> bool {
        is_reserved_for_str(self)
    }

    fn is_reserved_in_strict_mode(&self, is_module: bool) -> bool {
        is_reserved_in_strict_mode_for_str(self, is_module)
    }

    fn is_reserved_in_strict_bind(&self) -> bool {
        is_reserved_in_strict_bind_for_str(self)
    }

    fn is_reserved_in_es3(&self) -> bool {
        is_reserved_in_es3_for_str(self)
    }

    fn is_reserved_in_any(&self) -> bool {
        is_reserved_in_any_for_str(self)
    }
}

fn is_reserved_for_str(n: impl AsRef<str>) -> bool {
    RESERVED.contains(n.as_ref())
}

fn is_reserved_in_strict_mode_for_str(n: impl AsRef<str>, is_module: bool) -> bool {
    if is_module && n.as_ref() == "await" {
        return true;
    }
    RESSERVED_IN_STRICT_MODE.contains(n.as_ref())
}

fn is_reserved_in_strict_bind_for_str(n: impl AsRef<str>) -> bool {
    RESSERVED_IN_STRICT_BIND.contains(n.as_ref())
}

fn is_reserved_in_es3_for_str(n: impl AsRef<str>) -> bool {
    RESERVED_IN_ES3.contains(n.as_ref())
}

fn is_reserved_in_any_for_str(n: impl AsRef<str>) -> bool {
    RESERVED.contains(n.as_ref())
        || RESSERVED_IN_STRICT_MODE.contains(n.as_ref())
        || RESSERVED_IN_STRICT_BIND.contains(n.as_ref())
        || RESERVED_IN_ES3.contains(n.as_ref())
}

fn is_reserved_for_atom(n: &Atom) -> bool {
    RESERVED_ATOMS.contains(n)
}

fn is_reserved_in_strict_mode_for_atom(n: &Atom, is_module: bool) -> bool {
    if is_module && *n == atom!("await") {
        return true;
    }
    RESSERVED_IN_STRICT_MODE_ATOMS.contains(n)
}

fn is_reserved_in_strict_bind_for_atom(n: &Atom) -> bool {
    RESSERVED_IN_STRICT_BIND_ATOMS.contains(n)
}

fn is_reserved_in_es3_for_atom(n: &Atom) -> bool {
    RESERVED_IN_ES3_ATOMS.contains(n)
}

fn is_reserved_in_any_for_atom(n: &Atom) -> bool {
    RESERVED_ATOMS.contains(n)
        || RESSERVED_IN_STRICT_MODE_ATOMS.contains(n)
        || RESSERVED_IN_STRICT_BIND_ATOMS.contains(n)
        || RESERVED_IN_ES3_ATOMS.contains(n)
}
