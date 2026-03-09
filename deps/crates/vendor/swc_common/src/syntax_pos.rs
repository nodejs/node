use std::{
    borrow::Cow,
    cmp, fmt,
    hash::{Hash, Hasher},
    ops::{Add, Sub},
    path::PathBuf,
    sync::{atomic::AtomicU32, Mutex},
};

use bytes_str::BytesStr;
use serde::{Deserialize, Serialize};
use url::Url;

use self::hygiene::MarkData;
pub use self::hygiene::{Mark, SyntaxContext};
use crate::{cache::CacheCell, rustc_data_structures::stable_hasher::StableHasher, sync::Lrc};

mod analyze_source_file;
pub mod hygiene;

/// Spans represent a region of code, used for error reporting.
///
/// Positions in
/// spans are *absolute* positions from the beginning of the `source_map`, not
/// positions relative to `SourceFile`s. Methods on the `SourceMap` can be used
/// to relate spans back to the original source.
/// You must be careful if the span crosses more than one file - you will not be
/// able to use many of the functions on spans in `source_map` and you cannot
/// assume that the length of the `span = hi - lo`; there may be space in the
/// `BytePos` range between files.
#[derive(Clone, Copy, Hash, PartialEq, Eq, Ord, PartialOrd, Serialize, Deserialize)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct Span {
    #[serde(rename = "start")]
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub lo: BytePos,
    #[serde(rename = "end")]
    #[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))]
    pub hi: BytePos,
}

impl std::fmt::Debug for Span {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}..{}", self.lo.0, self.hi.0,)
    }
}

impl From<(BytePos, BytePos)> for Span {
    #[inline]
    fn from(sp: (BytePos, BytePos)) -> Self {
        Span::new(sp.0, sp.1)
    }
}

impl From<Span> for (BytePos, BytePos) {
    #[inline]
    fn from(sp: Span) -> Self {
        (sp.lo, sp.hi)
    }
}

#[cfg(feature = "arbitrary")]
#[cfg_attr(docsrs, doc(cfg(feature = "arbitrary")))]
impl<'a> arbitrary::Arbitrary<'a> for Span {
    fn arbitrary(u: &mut arbitrary::Unstructured<'_>) -> arbitrary::Result<Self> {
        let lo = u.arbitrary::<BytePos>()?;
        let hi = u.arbitrary::<BytePos>()?;

        Ok(Self::new(lo, hi))
    }
}

/// Dummy span, both position and length are zero, syntax context is zero as
/// well.
pub const DUMMY_SP: Span = Span {
    lo: BytePos::DUMMY,
    hi: BytePos::DUMMY,
};

/// PURE span, will emit `/* #__PURE__ */` comment in codegen.
pub const PURE_SP: Span = Span {
    lo: BytePos::PURE,
    hi: BytePos::PURE,
};

/// Used for some special cases. e.g. mark the generated AST.
pub const PLACEHOLDER_SP: Span = Span {
    lo: BytePos::PLACEHOLDER,
    hi: BytePos::PLACEHOLDER,
};

pub struct Globals {
    hygiene_data: Mutex<hygiene::HygieneData>,
    #[allow(unused)]
    dummy_cnt: AtomicU32,
    #[allow(unused)]
    marks: Mutex<Vec<MarkData>>,
}

const DUMMY_RESERVE: u32 = u32::MAX - 2_u32.pow(16);

impl Default for Globals {
    fn default() -> Self {
        Self::new()
    }
}

impl Globals {
    pub fn new() -> Globals {
        Globals {
            hygiene_data: Mutex::new(hygiene::HygieneData::new()),
            marks: Mutex::new(vec![MarkData {
                parent: Mark::root(),
            }]),
            dummy_cnt: AtomicU32::new(DUMMY_RESERVE),
        }
    }

    /// Clone the data from the current globals.
    ///
    /// Do not use this unless you know what you are doing.
    pub fn clone_data(&self) -> Self {
        Globals {
            hygiene_data: Mutex::new(self.hygiene_data.lock().unwrap().clone()),
            marks: Mutex::new(self.marks.lock().unwrap().clone()),
            dummy_cnt: AtomicU32::new(self.dummy_cnt.load(std::sync::atomic::Ordering::SeqCst)),
        }
    }
}

better_scoped_tls::scoped_tls!(

    /// Storage for span hygiene data.
    ///
    /// This variable is used to manage identifiers or to identify nodes.
    /// Note that it's stored as a thread-local storage, but actually it's shared
    /// between threads.
    ///
    /// # Usages
    ///
    /// ## Configuring
    ///
    /// ```rust
    /// use swc_common::GLOBALS;
    ///
    /// GLOBALS.set(&Default::default(), || {
    ///     // Do operations that require span hygiene
    /// });
    /// ```
    ///
    /// ## Span hygiene
    ///
    /// [Mark]s are stored in this variable.
    ///
    /// You can see the document how swc uses the span hygiene info at
    /// https://rustdoc.swc.rs/swc_ecma_transforms_base/resolver/fn.resolver_with_mark.html
    pub static GLOBALS: Globals
);

#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[derive(Debug, Eq, PartialEq, Clone, Ord, PartialOrd, Hash)]
pub enum FileName {
    Real(
        #[cfg_attr(
            any(feature = "rkyv-impl"),
            rkyv(with = crate::source_map::EncodePathBuf)
        )]
        PathBuf,
    ),
    /// A macro. This includes the full name of the macro, so that there are no
    /// clashes.
    Macros(String),
    /// call to `quote!`
    QuoteExpansion,
    /// Command line
    Anon,
    /// Hack in src/libsyntax/parse.rs
    MacroExpansion,
    ProcMacroSourceCode,
    Url(#[cfg_attr(any(feature = "rkyv-impl"), rkyv(with = crate::source_map::EncodeUrl))] Url),
    Internal(String),
    /// Custom sources for explicit parser calls from plugins and drivers
    Custom(String),
}

#[cfg(feature = "encoding-impl")]
impl cbor4ii::core::enc::Encode for FileName {
    #[inline]
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        use cbor4ii::core::types::{Array, Nothing, Tag};

        match self {
            FileName::Real(name) => {
                let name = name.to_str().unwrap();
                Tag(1, name).encode(writer)?;
            }
            FileName::Macros(name) => Tag(2, name).encode(writer)?,
            FileName::QuoteExpansion => {
                Tag(3, Nothing).encode(writer)?;
                Array::bounded(0, writer)?;
            }
            FileName::Anon => {
                Tag(4, Nothing).encode(writer)?;
                Array::bounded(0, writer)?;
            }
            FileName::MacroExpansion => {
                Tag(5, Nothing).encode(writer)?;
                Array::bounded(0, writer)?;
            }
            FileName::ProcMacroSourceCode => {
                Tag(6, Nothing).encode(writer)?;
                Array::bounded(0, writer)?;
            }
            FileName::Url(name) => Tag(7, name.as_str()).encode(writer)?,
            FileName::Internal(name) => Tag(8, name).encode(writer)?,
            FileName::Custom(name) => Tag(9, name).encode(writer)?,
        }

        Ok(())
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de> cbor4ii::core::dec::Decode<'de> for FileName {
    #[inline]
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        use cbor4ii::core::types::{Array, Tag};

        let tag = Tag::tag(reader)?;
        match tag {
            1 => {
                let name = String::decode(reader)?;
                Ok(FileName::Real(PathBuf::from(name)))
            }
            2 => {
                let name = String::decode(reader)?;
                Ok(FileName::Macros(name))
            }
            3 => {
                let n = Array::len(reader)?;
                debug_assert_eq!(n, Some(0));
                Ok(FileName::QuoteExpansion)
            }
            4 => {
                let n = Array::len(reader)?;
                debug_assert_eq!(n, Some(0));
                Ok(FileName::Anon)
            }
            5 => {
                let n = Array::len(reader)?;
                debug_assert_eq!(n, Some(0));
                Ok(FileName::MacroExpansion)
            }
            6 => {
                let n = Array::len(reader)?;
                debug_assert_eq!(n, Some(0));
                Ok(FileName::ProcMacroSourceCode)
            }
            7 => {
                let name = <&str>::decode(reader)?;
                Ok(FileName::Url(Url::parse(name).unwrap()))
            }
            8 => {
                let name = String::decode(reader)?;
                Ok(FileName::Internal(name))
            }
            9 => {
                let name = String::decode(reader)?;
                Ok(FileName::Custom(name))
            }
            tag => Err(cbor4ii::core::error::DecodeError::Custom {
                name: &"FileName",
                num: tag as u32,
            }),
        }
    }
}

/// A wrapper that attempts to convert a type to and from UTF-8.
///
/// Types like `OsString` and `PathBuf` aren't guaranteed to be encoded as
/// UTF-8, but they usually are anyway. Using this wrapper will archive them as
/// if they were regular `String`s.
///
/// There is built-in `AsString` supports PathBuf but it requires custom
/// serializer wrapper to handle conversion errors. This wrapper is simplified
/// version accepts errors
#[cfg(feature = "rkyv-impl")]
#[derive(Debug, Clone, Copy)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
pub struct EncodePathBuf;

#[cfg(feature = "rkyv-impl")]
impl rkyv::with::ArchiveWith<PathBuf> for EncodePathBuf {
    type Archived = rkyv::string::ArchivedString;
    type Resolver = rkyv::string::StringResolver;

    #[inline]
    fn resolve_with(field: &PathBuf, resolver: Self::Resolver, out: rkyv::Place<Self::Archived>) {
        // It's safe to unwrap here because if the OsString wasn't valid UTF-8 it would
        // have failed to serialize
        rkyv::string::ArchivedString::resolve_from_str(field.to_str().unwrap(), resolver, out);
    }
}

#[cfg(feature = "rkyv-impl")]
impl<S> rkyv::with::SerializeWith<PathBuf, S> for EncodePathBuf
where
    S: ?Sized + rancor::Fallible + rkyv::ser::Writer,
    S::Error: rancor::Source,
{
    #[inline]
    fn serialize_with(field: &PathBuf, serializer: &mut S) -> Result<Self::Resolver, S::Error> {
        let s = field.to_str().unwrap_or_default();
        rkyv::string::ArchivedString::serialize_from_str(s, serializer)
    }
}

#[cfg(feature = "rkyv-impl")]
impl<D> rkyv::with::DeserializeWith<rkyv::string::ArchivedString, PathBuf, D> for EncodePathBuf
where
    D: ?Sized + rancor::Fallible,
{
    #[inline]
    fn deserialize_with(
        field: &rkyv::string::ArchivedString,
        _: &mut D,
    ) -> Result<PathBuf, D::Error> {
        Ok(<PathBuf as std::str::FromStr>::from_str(field.as_str()).unwrap())
    }
}

/// A wrapper that attempts to convert a Url to and from String.
#[cfg(feature = "rkyv-impl")]
#[derive(Debug, Clone, Copy)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
pub struct EncodeUrl;

#[cfg(feature = "rkyv-impl")]
impl rkyv::with::ArchiveWith<Url> for EncodeUrl {
    type Archived = rkyv::string::ArchivedString;
    type Resolver = rkyv::string::StringResolver;

    #[inline]
    fn resolve_with(field: &Url, resolver: Self::Resolver, out: rkyv::Place<Self::Archived>) {
        rkyv::string::ArchivedString::resolve_from_str(field.as_str(), resolver, out);
    }
}

#[cfg(feature = "rkyv-impl")]
impl<S> rkyv::with::SerializeWith<Url, S> for EncodeUrl
where
    S: ?Sized + rancor::Fallible + rkyv::ser::Writer,
    S::Error: rancor::Source,
{
    #[inline]
    fn serialize_with(field: &Url, serializer: &mut S) -> Result<Self::Resolver, S::Error> {
        let field = field.as_str();
        rkyv::string::ArchivedString::serialize_from_str(field, serializer)
    }
}

#[cfg(feature = "rkyv-impl")]
impl<D> rkyv::with::DeserializeWith<rkyv::Archived<String>, Url, D> for EncodeUrl
where
    D: ?Sized + rancor::Fallible,
{
    #[inline]
    fn deserialize_with(field: &rkyv::string::ArchivedString, _: &mut D) -> Result<Url, D::Error> {
        Ok(Url::parse(field.as_str()).unwrap())
    }
}

impl std::fmt::Display for FileName {
    fn fmt(&self, fmt: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match *self {
            FileName::Real(ref path) => write!(fmt, "{}", path.display()),
            FileName::Macros(ref name) => write!(fmt, "<{name} macros>"),
            FileName::QuoteExpansion => write!(fmt, "<quote expansion>"),
            FileName::MacroExpansion => write!(fmt, "<macro expansion>"),
            FileName::Anon => write!(fmt, "<anon>"),
            FileName::ProcMacroSourceCode => write!(fmt, "<proc-macro source code>"),
            FileName::Url(ref u) => write!(fmt, "{u}"),
            FileName::Custom(ref s) => {
                write!(fmt, "{s}")
            }
            FileName::Internal(ref s) => write!(fmt, "<{s}>"),
        }
    }
}

impl From<PathBuf> for FileName {
    fn from(p: PathBuf) -> Self {
        assert!(!p.to_string_lossy().ends_with('>'));
        FileName::Real(p)
    }
}

impl From<Url> for FileName {
    fn from(url: Url) -> Self {
        FileName::Url(url)
    }
}

impl FileName {
    pub fn is_real(&self) -> bool {
        match *self {
            FileName::Real(_) => true,
            FileName::Macros(_)
            | FileName::Anon
            | FileName::MacroExpansion
            | FileName::ProcMacroSourceCode
            | FileName::Custom(_)
            | FileName::QuoteExpansion
            | FileName::Internal(_)
            | FileName::Url(_) => false,
        }
    }

    pub fn is_macros(&self) -> bool {
        match *self {
            FileName::Real(_)
            | FileName::Anon
            | FileName::MacroExpansion
            | FileName::ProcMacroSourceCode
            | FileName::Custom(_)
            | FileName::QuoteExpansion
            | FileName::Internal(_)
            | FileName::Url(_) => false,
            FileName::Macros(_) => true,
        }
    }
}

#[derive(Clone, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(
    feature = "diagnostic-serde",
    derive(serde::Serialize, serde::Deserialize)
)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct PrimarySpanLabel(pub Span, pub String);

/// A collection of spans. Spans have two orthogonal attributes:
///
/// - they can be *primary spans*. In this case they are the locus of the error,
///   and would be rendered with `^^^`.
/// - they can have a *label*. In this case, the label is written next to the
///   mark in the snippet when we render.
#[derive(Clone, Debug, Default, Hash, PartialEq, Eq)]
#[cfg_attr(
    feature = "diagnostic-serde",
    derive(serde::Serialize, serde::Deserialize)
)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct MultiSpan {
    primary_spans: Vec<Span>,
    span_labels: Vec<PrimarySpanLabel>,
}

extern "C" {
    fn __span_dummy_with_cmt_proxy() -> u32;
}

impl Span {
    #[inline]
    pub fn lo(self) -> BytePos {
        self.lo
    }

    #[inline]
    pub fn new(mut lo: BytePos, mut hi: BytePos) -> Self {
        if lo > hi {
            std::mem::swap(&mut lo, &mut hi);
        }

        Span { lo, hi }
    }

    #[inline]
    #[track_caller]
    pub fn new_with_checked(lo: BytePos, hi: BytePos) -> Self {
        debug_assert!(lo <= hi, "lo: {lo:#?}, hi: {hi:#?}");
        Span { lo, hi }
    }

    #[inline]
    pub fn with_lo(&self, lo: BytePos) -> Span {
        Span::new(lo, self.hi)
    }

    #[inline(always)]
    pub fn hi(self) -> BytePos {
        self.hi
    }

    #[inline]
    pub fn with_hi(&self, hi: BytePos) -> Span {
        Span::new(self.lo, hi)
    }

    /// Returns `true` if this is a dummy span with any hygienic context.
    #[inline]
    pub fn is_dummy(self) -> bool {
        self.lo.0 == 0 && self.hi.0 == 0 || self.lo.0 >= DUMMY_RESERVE
    }

    #[inline]
    pub fn is_pure(self) -> bool {
        self.lo.is_pure()
    }

    #[inline]
    pub fn is_placeholder(self) -> bool {
        self.lo.is_placeholder()
    }

    /// Returns `true` if this is a dummy span with any hygienic context.
    #[inline]
    pub fn is_dummy_ignoring_cmt(self) -> bool {
        self.lo.0 == 0 && self.hi.0 == 0
    }

    /// Returns a new span representing an empty span at the beginning of this
    /// span
    #[inline]
    pub fn shrink_to_lo(self) -> Span {
        self.with_hi(self.lo)
    }

    /// Returns a new span representing an empty span at the end of this span
    #[inline]
    pub fn shrink_to_hi(self) -> Span {
        self.with_lo(self.hi)
    }

    /// Returns `self` if `self` is not the dummy span, and `other` otherwise.
    pub fn substitute_dummy(self, other: Span) -> Span {
        if self.is_dummy() {
            other
        } else {
            self
        }
    }

    /// Return true if `self` fully encloses `other`.
    pub fn contains(self, other: Span) -> bool {
        self.lo <= other.lo && other.hi <= self.hi
    }

    /// Return true if the spans are equal with regards to the source text.
    ///
    /// Use this instead of `==` when either span could be generated code,
    /// and you only care that they point to the same bytes of source text.
    pub fn source_equal(self, other: Span) -> bool {
        self.lo == other.lo && self.hi == other.hi
    }

    /// Returns `Some(span)`, where the start is trimmed by the end of `other`
    pub fn trim_start(self, other: Span) -> Option<Span> {
        if self.hi > other.hi {
            Some(self.with_lo(cmp::max(self.lo, other.hi)))
        } else {
            None
        }
    }

    /// Return a `Span` that would enclose both `self` and `end`.
    pub fn to(self, end: Span) -> Span {
        let span_data = self;
        let end_data = end;
        // FIXME(jseyfried): self.ctxt should always equal end.ctxt here (c.f. issue
        // #23480) Return the macro span on its own to avoid weird diagnostic
        // output. It is preferable to have an incomplete span than a completely
        // nonsensical one.

        Span::new(
            cmp::min(span_data.lo, end_data.lo),
            cmp::max(span_data.hi, end_data.hi),
        )
    }

    /// Return a `Span` between the end of `self` to the beginning of `end`.
    pub fn between(self, end: Span) -> Span {
        let span = self;
        Span::new(span.hi, end.lo)
    }

    /// Return a `Span` between the beginning of `self` to the beginning of
    /// `end`.
    pub fn until(self, end: Span) -> Span {
        let span = self;
        Span::new(span.lo, end.lo)
    }

    pub fn from_inner_byte_pos(self, start: usize, end: usize) -> Span {
        let span = self;
        Span::new(
            span.lo + BytePos::from_usize(start),
            span.lo + BytePos::from_usize(end),
        )
    }

    /// Dummy span, both position are extremely large numbers so they would be
    /// ignore by sourcemap, but can still have comments
    pub fn dummy_with_cmt() -> Self {
        #[cfg(all(feature = "__plugin_mode", target_arch = "wasm32"))]
        {
            let lo = BytePos(unsafe { __span_dummy_with_cmt_proxy() });

            return Span { lo, hi: lo };
        }

        #[cfg(not(all(any(feature = "__plugin_mode"), target_arch = "wasm32")))]
        return GLOBALS.with(|globals| {
            let lo = BytePos(
                globals
                    .dummy_cnt
                    .fetch_add(1, std::sync::atomic::Ordering::SeqCst),
            );
            Span { lo, hi: lo }
        });
    }
}

#[derive(Clone, Debug)]
pub struct SpanLabel {
    /// The span we are going to include in the final snippet.
    pub span: Span,

    /// Is this a primary span? This is the "locus" of the message,
    /// and is indicated with a `^^^^` underline, versus `----`.
    pub is_primary: bool,

    /// What label should we attach to this span (if any)?
    pub label: Option<String>,
}

impl Default for Span {
    fn default() -> Self {
        DUMMY_SP
    }
}

impl MultiSpan {
    #[inline]
    pub fn new() -> MultiSpan {
        Self::default()
    }

    pub fn from_span(primary_span: Span) -> MultiSpan {
        MultiSpan {
            primary_spans: vec![primary_span],
            span_labels: Vec::new(),
        }
    }

    pub fn from_spans(vec: Vec<Span>) -> MultiSpan {
        MultiSpan {
            primary_spans: vec,
            span_labels: Vec::new(),
        }
    }

    pub fn push_span_label(&mut self, span: Span, label: String) {
        self.span_labels.push(PrimarySpanLabel(span, label));
    }

    /// Selects the first primary span (if any)
    pub fn primary_span(&self) -> Option<Span> {
        self.primary_spans.first().cloned()
    }

    /// Returns all primary spans.
    pub fn primary_spans(&self) -> &[Span] {
        &self.primary_spans
    }

    /// Returns `true` if this contains only a dummy primary span with any
    /// hygienic context.
    pub fn is_dummy(&self) -> bool {
        let mut is_dummy = true;
        for span in &self.primary_spans {
            if !span.is_dummy() {
                is_dummy = false;
            }
        }
        is_dummy
    }

    /// Replaces all occurrences of one Span with another. Used to move Spans in
    /// areas that don't display well (like std macros). Returns true if
    /// replacements occurred.
    pub fn replace(&mut self, before: Span, after: Span) -> bool {
        let mut replacements_occurred = false;
        for primary_span in &mut self.primary_spans {
            if *primary_span == before {
                *primary_span = after;
                replacements_occurred = true;
            }
        }
        for span_label in &mut self.span_labels {
            if span_label.0 == before {
                span_label.0 = after;
                replacements_occurred = true;
            }
        }
        replacements_occurred
    }

    /// Returns the strings to highlight. We always ensure that there
    /// is an entry for each of the primary spans -- for each primary
    /// span P, if there is at least one label with span P, we return
    /// those labels (marked as primary). But otherwise we return
    /// `SpanLabel` instances with empty labels.
    pub fn span_labels(&self) -> Vec<SpanLabel> {
        let is_primary = |span| self.primary_spans.contains(&span);

        let mut span_labels = self
            .span_labels
            .iter()
            .map(|&PrimarySpanLabel(span, ref label)| SpanLabel {
                span,
                is_primary: is_primary(span),
                label: Some(label.clone()),
            })
            .collect::<Vec<_>>();

        for &span in &self.primary_spans {
            if !span_labels.iter().any(|sl| sl.span == span) {
                span_labels.push(SpanLabel {
                    span,
                    is_primary: true,
                    label: None,
                });
            }
        }

        span_labels
    }
}

impl From<Span> for MultiSpan {
    fn from(span: Span) -> MultiSpan {
        MultiSpan::from_span(span)
    }
}

impl From<Vec<Span>> for MultiSpan {
    fn from(spans: Vec<Span>) -> MultiSpan {
        MultiSpan::from_spans(spans)
    }
}

pub const NO_EXPANSION: SyntaxContext = SyntaxContext::empty();

/// Identifies an offset of a multi-byte character in a SourceFile
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub struct MultiByteChar {
    /// The absolute offset of the character in the SourceMap
    pub pos: BytePos,
    /// The number of bytes, >=2
    pub bytes: u8,
}

impl MultiByteChar {
    /// Computes the extra number of UTF-8 bytes necessary to encode a code
    /// point, compared to UTF-16 encoding.
    ///
    /// 1, 2, and 3 UTF-8 bytes encode into 1 UTF-16 char, but 4 UTF-8 bytes
    /// encode into 2.
    pub fn byte_to_char_diff(&self) -> u8 {
        if self.bytes == 4 {
            2
        } else {
            self.bytes - 1
        }
    }
}

/// Identifies an offset of a non-narrow character in a SourceFile
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[derive(Copy, Clone, Eq, PartialEq, Debug)]
pub enum NonNarrowChar {
    /// Represents a zero-width character
    ZeroWidth(BytePos),
    /// Represents a wide (fullwidth) character
    Wide(BytePos, usize),
    /// Represents a tab character, represented visually with a width of 4
    /// characters
    Tab(BytePos),
}

impl NonNarrowChar {
    fn new(pos: BytePos, width: usize) -> Self {
        match width {
            0 => NonNarrowChar::ZeroWidth(pos),
            4 => NonNarrowChar::Tab(pos),
            w => NonNarrowChar::Wide(pos, w),
        }
    }

    /// Returns the absolute offset of the character in the SourceMap
    pub fn pos(self) -> BytePos {
        match self {
            NonNarrowChar::ZeroWidth(p) | NonNarrowChar::Wide(p, _) | NonNarrowChar::Tab(p) => p,
        }
    }

    /// Returns the width of the character, 0 (zero-width) or 2 (wide)
    pub fn width(self) -> usize {
        match self {
            NonNarrowChar::ZeroWidth(_) => 0,
            NonNarrowChar::Wide(_, width) => width,
            NonNarrowChar::Tab(_) => 4,
        }
    }
}

impl Add<BytePos> for NonNarrowChar {
    type Output = Self;

    fn add(self, rhs: BytePos) -> Self {
        match self {
            NonNarrowChar::ZeroWidth(pos) => NonNarrowChar::ZeroWidth(pos + rhs),
            NonNarrowChar::Wide(pos, width) => NonNarrowChar::Wide(pos + rhs, width),
            NonNarrowChar::Tab(pos) => NonNarrowChar::Tab(pos + rhs),
        }
    }
}

impl Sub<BytePos> for NonNarrowChar {
    type Output = Self;

    fn sub(self, rhs: BytePos) -> Self {
        match self {
            NonNarrowChar::ZeroWidth(pos) => NonNarrowChar::ZeroWidth(pos - rhs),
            NonNarrowChar::Wide(pos, width) => NonNarrowChar::Wide(pos - rhs, width),
            NonNarrowChar::Tab(pos) => NonNarrowChar::Tab(pos - rhs),
        }
    }
}

/// A single source in the SourceMap.
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
#[derive(Clone)]
pub struct SourceFile {
    /// The name of the file that the source came from. Source that doesn't
    /// originate from files has names between angle brackets by convention,
    /// e.g. `<anon>`
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub name: Lrc<FileName>,
    /// True if the `name` field above has been modified by
    /// `--remap-path-prefix`
    pub name_was_remapped: bool,
    /// The unmapped path of the file that the source came from.
    /// Set to `None` if the `SourceFile` was imported from an external crate.
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub unmapped_path: Option<Lrc<FileName>>,
    /// Indicates which crate this `SourceFile` was imported from.
    pub crate_of_origin: u32,
    /// The complete source code
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Str"))]
    pub src: BytesStr,
    /// The source code's hash
    pub src_hash: u128,
    /// The start position of this source in the `SourceMap`
    pub start_pos: BytePos,
    /// The end position of this source in the `SourceMap`
    pub end_pos: BytePos,
    /// A hash of the filename, used for speeding up the incr. comp. hashing.
    pub name_hash: u128,

    #[cfg_attr(feature = "encoding-impl", encoding(ignore))]
    lazy: CacheCell<SourceFileAnalysis>,
}

#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[derive(Clone)]
pub struct SourceFileAnalysis {
    /// Locations of lines beginnings in the source code
    pub lines: Vec<BytePos>,
    /// Locations of multi-byte characters in the source code
    pub multibyte_chars: Vec<MultiByteChar>,
    /// Width of characters that are not narrow in the source code
    pub non_narrow_chars: Vec<NonNarrowChar>,
}

impl fmt::Debug for SourceFile {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(fmt, "SourceFile({})", self.name)
    }
}

impl SourceFile {
    /// `src` should not have UTF8 BOM
    pub fn new(
        name: Lrc<FileName>,
        name_was_remapped: bool,
        unmapped_path: Lrc<FileName>,
        src: BytesStr,
        start_pos: BytePos,
    ) -> SourceFile {
        debug_assert_ne!(
            start_pos,
            BytePos::DUMMY,
            "BytePos::DUMMY is reserved and `SourceFile` should not use it"
        );

        let src_hash = {
            let mut hasher: StableHasher = StableHasher::new();
            hasher.write(src.as_bytes());
            hasher.finish()
        };
        let name_hash = {
            let mut hasher: StableHasher = StableHasher::new();
            name.hash(&mut hasher);
            hasher.finish()
        };
        let end_pos = start_pos.to_usize() + src.len();

        SourceFile {
            name,
            name_was_remapped,
            unmapped_path: Some(unmapped_path),
            crate_of_origin: 0,
            src,
            src_hash,
            start_pos,
            end_pos: SmallPos::from_usize(end_pos),
            name_hash,
            lazy: CacheCell::new(),
        }
    }

    /// Return the BytePos of the beginning of the current line.
    pub fn line_begin_pos(&self, pos: BytePos) -> BytePos {
        let line_index = self.lookup_line(pos).unwrap();
        let analysis = self.analyze();
        analysis.lines[line_index]
    }

    /// Get a line from the list of pre-computed line-beginnings.
    /// The line number here is 0-based.
    pub fn get_line(&self, line_number: usize) -> Option<Cow<'_, str>> {
        fn get_until_newline(src: &str, begin: usize) -> &str {
            // We can't use `lines.get(line_number+1)` because we might
            // be parsing when we call this function and thus the current
            // line is the last one we have line info for.
            let slice = &src[begin..];
            match slice.find('\n') {
                Some(e) => &slice[..e],
                None => slice,
            }
        }

        let begin = {
            let analysis = self.analyze();
            let line = analysis.lines.get(line_number)?;
            let begin: BytePos = *line - self.start_pos;
            begin.to_usize()
        };

        Some(Cow::from(get_until_newline(&self.src, begin)))
    }

    pub fn is_real_file(&self) -> bool {
        self.name.is_real()
    }

    pub fn byte_length(&self) -> u32 {
        self.end_pos.0 - self.start_pos.0
    }

    pub fn count_lines(&self) -> usize {
        let analysis = self.analyze();
        analysis.lines.len()
    }

    /// Find the line containing the given position. The return value is the
    /// index into the `lines` array of this SourceFile, not the 1-based line
    /// number. If the `source_file` is empty or the position is located before
    /// the first line, `None` is returned.
    pub fn lookup_line(&self, pos: BytePos) -> Option<usize> {
        let analysis = self.analyze();
        if analysis.lines.is_empty() {
            return None;
        }

        let line_index = lookup_line(&analysis.lines, pos);
        assert!(line_index < analysis.lines.len() as isize);
        if line_index >= 0 {
            Some(line_index as usize)
        } else {
            None
        }
    }

    pub fn line_bounds(&self, line_index: usize) -> (BytePos, BytePos) {
        if self.start_pos == self.end_pos {
            return (self.start_pos, self.end_pos);
        }

        let analysis = self.analyze();

        assert!(line_index < analysis.lines.len());
        if line_index == (analysis.lines.len() - 1) {
            (analysis.lines[line_index], self.end_pos)
        } else {
            (analysis.lines[line_index], analysis.lines[line_index + 1])
        }
    }

    #[inline]
    pub fn contains(&self, byte_pos: BytePos) -> bool {
        byte_pos >= self.start_pos && byte_pos <= self.end_pos
    }

    pub fn analyze(&self) -> &SourceFileAnalysis {
        self.lazy.get_or_init(|| {
            let (lines, multibyte_chars, non_narrow_chars) =
                analyze_source_file::analyze_source_file(&self.src[..], self.start_pos);
            SourceFileAnalysis {
                lines,
                multibyte_chars,
                non_narrow_chars,
            }
        })
    }
}

// _____________________________________________________________________________
// Pos, BytePos, CharPos
//

pub trait SmallPos {
    fn from_usize(n: usize) -> Self;
    fn to_usize(&self) -> usize;
    fn from_u32(n: u32) -> Self;
    fn to_u32(&self) -> u32;
}

/// A byte offset. Keep this small (currently 32-bits), as AST contains
/// a lot of them.
///
///
/// # Reserved
///
///  - 0 is reserved for dummy spans. It means `BytePos(0)` means the `BytePos`
///    is synthesized by the compiler.
///
///  - Values larger than `u32::MAX - 2^16` are reserved for the comments.
///
/// `u32::MAX` is special value used to generate source map entries.
#[derive(
    Clone, Copy, PartialEq, Eq, Hash, PartialOrd, Ord, Debug, Serialize, Deserialize, Default,
)]
#[serde(transparent)]
#[cfg_attr(feature = "arbitrary", derive(arbitrary::Arbitrary))]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "shrink-to-fit", derive(shrink_to_fit::ShrinkToFit))]
pub struct BytePos(#[cfg_attr(feature = "__rkyv", rkyv(omit_bounds))] pub u32);

#[cfg(feature = "encoding-impl")]
impl cbor4ii::core::enc::Encode for BytePos {
    #[inline]
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        self.0.encode(writer)
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de> cbor4ii::core::dec::Decode<'de> for BytePos {
    #[inline]
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        u32::decode(reader).map(BytePos)
    }
}

impl BytePos {
    /// Dummy position. This is reserved for synthesized spans.
    pub const DUMMY: Self = BytePos(0);
    const MIN_RESERVED: Self = BytePos(DUMMY_RESERVE);
    /// Placeholders, commonly used where names are required, but the names are
    /// not referenced elsewhere.
    pub const PLACEHOLDER: Self = BytePos(u32::MAX - 2);
    /// Reserved for PURE comments. e.g. `/* #__PURE__ */`
    pub const PURE: Self = BytePos(u32::MAX - 1);
    /// Synthesized, but should be stored in a source map.
    pub const SYNTHESIZED: Self = BytePos(u32::MAX);

    pub const fn is_reserved_for_comments(self) -> bool {
        self.0 >= Self::MIN_RESERVED.0 && self.0 != u32::MAX
    }

    /// Returns `true`` if this is synthesized and has no relevant input source
    /// code.
    pub const fn is_dummy(self) -> bool {
        self.0 == 0
    }

    pub const fn is_pure(self) -> bool {
        self.0 == Self::PURE.0
    }

    pub const fn is_placeholder(self) -> bool {
        self.0 == Self::PLACEHOLDER.0
    }

    /// Returns `true`` if this is explicitly synthesized or has relevant input
    /// source so can have a comment.
    pub const fn can_have_comment(self) -> bool {
        self.0 != 0
    }
}

/// A character offset. Because of multibyte utf8 characters, a byte offset
/// is not equivalent to a character offset. The SourceMap will convert BytePos
/// values to CharPos values as necessary.
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
#[derive(Copy, Clone, PartialEq, Eq, Hash, PartialOrd, Ord, Debug)]
pub struct CharPos(
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Usize"))] pub usize,
);

// FIXME: Lots of boilerplate in these impls, but so far my attempts to fix
// have been unsuccessful

impl SmallPos for BytePos {
    #[inline(always)]
    fn from_usize(n: usize) -> BytePos {
        BytePos(n as u32)
    }

    #[inline(always)]
    fn to_usize(&self) -> usize {
        self.0 as usize
    }

    #[inline(always)]
    fn from_u32(n: u32) -> BytePos {
        BytePos(n)
    }

    #[inline(always)]
    fn to_u32(&self) -> u32 {
        self.0
    }
}

impl Add for BytePos {
    type Output = BytePos;

    #[inline(always)]
    fn add(self, rhs: BytePos) -> BytePos {
        BytePos((self.to_usize() + rhs.to_usize()) as u32)
    }
}

impl Sub for BytePos {
    type Output = BytePos;

    #[inline(always)]
    fn sub(self, rhs: BytePos) -> BytePos {
        BytePos((self.to_usize() - rhs.to_usize()) as u32)
    }
}

impl SmallPos for CharPos {
    #[inline(always)]
    fn from_usize(n: usize) -> CharPos {
        CharPos(n)
    }

    #[inline(always)]
    fn to_usize(&self) -> usize {
        self.0
    }

    #[inline(always)]
    fn from_u32(n: u32) -> CharPos {
        CharPos(n as usize)
    }

    #[inline(always)]
    fn to_u32(&self) -> u32 {
        self.0 as u32
    }
}

impl Add for CharPos {
    type Output = CharPos;

    #[inline(always)]
    fn add(self, rhs: CharPos) -> CharPos {
        CharPos(self.to_usize() + rhs.to_usize())
    }
}

impl Sub for CharPos {
    type Output = CharPos;

    #[inline(always)]
    fn sub(self, rhs: CharPos) -> CharPos {
        CharPos(self.to_usize() - rhs.to_usize())
    }
}

// _____________________________________________________________________________
// Loc, LocWithOpt, SourceFileAndLine, SourceFileAndBytePos
//

/// A source code location used for error reporting.
///
/// Note: This struct intentionally does not implement rkyv's archieve
/// to avoid redundant data copy (https://github.com/swc-project/swc/issues/5471)
/// source_map_proxy constructs plugin-side Loc instead with shared SourceFile
/// instance.
#[derive(Debug, Clone)]
pub struct Loc {
    /// Information about the original source
    pub file: Lrc<SourceFile>,
    /// The (1-based) line number
    pub line: usize,
    /// The (0-based) column offset
    pub col: CharPos,
    /// The (0-based) column offset when displayed
    pub col_display: usize,
}

/// A struct to exchange `Loc` with omitting SourceFile as needed.
/// This is internal struct between plugins to the host, not a public interface.
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize, Debug, Clone)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
pub struct PartialLoc {
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub source_file: Option<Lrc<SourceFile>>,
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Usize"))]
    pub line: usize,
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Usize"))]
    pub col: usize,
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Usize"))]
    pub col_display: usize,
}

/// A source code location used as the result of `lookup_char_pos_adj`
// Actually, *none* of the clients use the filename *or* file field;
// perhaps they should just be removed.
#[derive(Debug)]
pub struct LocWithOpt {
    pub filename: Lrc<FileName>,
    pub line: usize,
    pub col: CharPos,
    pub file: Option<Lrc<SourceFile>>,
}

// used to be structural records. Better names, anyone?
#[derive(Debug)]
pub struct SourceFileAndLine {
    pub sf: Lrc<SourceFile>,
    pub line: usize,
}

#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
#[derive(Debug)]
pub struct SourceFileAndBytePos {
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub sf: Lrc<SourceFile>,
    pub pos: BytePos,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
pub struct LineInfo {
    /// Index of line, starting from 0.
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Usize"))]
    pub line_index: usize,

    /// Column in line where span begins, starting from 0.
    pub start_col: CharPos,

    /// Column in line where span ends, starting from 0, exclusive.
    pub end_col: CharPos,
}

/// Used to create a `.map` file.
#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
pub struct LineCol {
    /// Index of line, starting from 0.
    pub line: u32,

    /// UTF-16 column in line, starting from 0.
    pub col: u32,
}

/// A struct to represent lines of a source file.
///
/// Note: This struct intentionally does not implement rkyv's archieve
/// to avoid redundant data copy (https://github.com/swc-project/swc/issues/5471)
/// source_map_proxy constructs plugin-side Loc instead with shared SourceFile
/// instance.
pub struct FileLines {
    pub file: Lrc<SourceFile>,
    pub lines: Vec<LineInfo>,
}

/// A struct to exchange `FileLines` with omitting SourceFile as needed.
/// This is internal struct between plugins to the host, not a public interface.
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize, Debug, Clone)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct PartialFileLines {
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub file: Option<Lrc<SourceFile>>,
    pub lines: Vec<LineInfo>,
}

// _____________________________________________________________________________
// SpanLinesError, SpanSnippetError, DistinctSources,
// MalformedSourceMapPositions
//

pub type FileLinesResult = Result<FileLines, Box<SpanLinesError>>;
#[cfg(feature = "__plugin")]
pub type PartialFileLinesResult = Result<PartialFileLines, Box<SpanLinesError>>;

#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub enum SpanLinesError {
    IllFormedSpan(Span),
    DistinctSources(DistinctSources),
}

#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
pub enum SpanSnippetError {
    DummyBytePos,
    IllFormedSpan(Span),
    DistinctSources(DistinctSources),
    MalformedForSourcemap(MalformedSourceMapPositions),
    SourceNotAvailable { filename: FileName },
    LookupFailed(SourceMapLookupError),
}

#[cfg(feature = "encoding-impl")]
impl cbor4ii::core::enc::Encode for SpanSnippetError {
    #[inline]
    fn encode<W: cbor4ii::core::enc::Write>(
        &self,
        writer: &mut W,
    ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
        use cbor4ii::core::types::{Array, Nothing, Tag};

        match self {
            SpanSnippetError::DummyBytePos => {
                Tag(1, Nothing).encode(writer)?;
                Array::bounded(0, writer)
            }
            SpanSnippetError::IllFormedSpan(span) => Tag(2, span).encode(writer),
            SpanSnippetError::DistinctSources(src) => Tag(3, src).encode(writer),
            SpanSnippetError::MalformedForSourcemap(pos) => Tag(4, pos).encode(writer),
            SpanSnippetError::SourceNotAvailable { filename } => Tag(5, filename).encode(writer),
            SpanSnippetError::LookupFailed(err) => Tag(6, err).encode(writer),
        }
    }
}

#[cfg(feature = "encoding-impl")]
impl<'de> cbor4ii::core::dec::Decode<'de> for SpanSnippetError {
    #[inline]
    fn decode<R: cbor4ii::core::dec::Read<'de>>(
        reader: &mut R,
    ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
        use cbor4ii::core::types::{Array, Tag};

        let tag = Tag::tag(reader)?;
        match tag {
            1 => {
                let n = Array::len(reader)?;
                debug_assert_eq!(n, Some(0));
                Ok(SpanSnippetError::DummyBytePos)
            }
            2 => Span::decode(reader).map(SpanSnippetError::IllFormedSpan),
            3 => DistinctSources::decode(reader).map(SpanSnippetError::DistinctSources),
            4 => MalformedSourceMapPositions::decode(reader)
                .map(SpanSnippetError::MalformedForSourcemap),
            5 => FileName::decode(reader)
                .map(|filename| SpanSnippetError::SourceNotAvailable { filename }),
            6 => SourceMapLookupError::decode(reader).map(SpanSnippetError::LookupFailed),
            tag => Err(cbor4ii::core::error::DecodeError::Custom {
                name: &"SpanSnippetError",
                num: tag as u32,
            }),
        }
    }
}

/// An error type for looking up source maps.
///
///
/// This type is small.
#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub enum SourceMapLookupError {
    NoFileFor(BytePos),
}

#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct FilePos(
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub Lrc<FileName>,
    pub BytePos,
);

#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct DistinctSources {
    pub begin: FilePos,
    pub end: FilePos,
}

#[derive(Clone, PartialEq, Eq, Debug)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct MalformedSourceMapPositions {
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "encoding_helper::LrcHelper")
    )]
    pub name: Lrc<FileName>,
    #[cfg_attr(feature = "encoding-impl", encoding(with = "encoding_helper::Usize"))]
    pub source_len: usize,
    pub begin_pos: BytePos,
    pub end_pos: BytePos,
}

// Given a slice of line start positions and a position, returns the index of
// the line the position is on. Returns -1 if the position is located before
// the first line.
fn lookup_line(lines: &[BytePos], pos: BytePos) -> isize {
    match lines.binary_search(&pos) {
        Ok(line) => line as isize,
        Err(line) => line as isize - 1,
    }
}

impl From<SourceMapLookupError> for Box<SpanSnippetError> {
    #[cold]
    fn from(err: SourceMapLookupError) -> Self {
        Box::new(SpanSnippetError::LookupFailed(err))
    }
}

#[cfg(feature = "encoding-impl")]
mod encoding_helper {
    use super::Lrc;

    pub struct LrcHelper<T>(pub T);

    impl<T: cbor4ii::core::enc::Encode> cbor4ii::core::enc::Encode for LrcHelper<&'_ Lrc<T>> {
        fn encode<W: cbor4ii::core::enc::Write>(
            &self,
            writer: &mut W,
        ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
            self.0.encode(writer)
        }
    }

    impl<'de, T: cbor4ii::core::dec::Decode<'de>> cbor4ii::core::dec::Decode<'de>
        for LrcHelper<Lrc<T>>
    {
        fn decode<R: cbor4ii::core::dec::Read<'de>>(
            reader: &mut R,
        ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
            T::decode(reader).map(Lrc::new).map(LrcHelper)
        }
    }

    impl<T: cbor4ii::core::enc::Encode> cbor4ii::core::enc::Encode for LrcHelper<&'_ Option<Lrc<T>>> {
        fn encode<W: cbor4ii::core::enc::Write>(
            &self,
            writer: &mut W,
        ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
            // when MSRV supports version 1.75.0 and later, `.as_slice()` should be used.
            let v = self.0.as_deref();
            cbor4ii::core::types::Array::bounded(v.is_some() as usize, writer)?;
            if let Some(v) = v {
                v.encode(writer)?;
            }
            Ok(())
        }
    }

    impl<'de, T: cbor4ii::core::dec::Decode<'de>> cbor4ii::core::dec::Decode<'de>
        for LrcHelper<Option<Lrc<T>>>
    {
        fn decode<R: cbor4ii::core::dec::Read<'de>>(
            reader: &mut R,
        ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
            <cbor4ii::core::types::Maybe<Option<T>>>::decode(reader)
                .map(|maybe| maybe.0.map(Lrc::new))
                .map(LrcHelper)
        }
    }

    pub struct Usize<T>(pub T);

    impl cbor4ii::core::enc::Encode for Usize<&'_ usize> {
        fn encode<W: cbor4ii::core::enc::Write>(
            &self,
            writer: &mut W,
        ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
            (*self.0 as u64).encode(writer)
        }
    }

    impl<'de> cbor4ii::core::dec::Decode<'de> for Usize<usize> {
        fn decode<R: cbor4ii::core::dec::Read<'de>>(
            reader: &mut R,
        ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
            <u64>::decode(reader)
                .map(|n| n.try_into().unwrap())
                .map(Usize)
        }
    }

    pub struct Str<T>(pub T);

    impl cbor4ii::core::enc::Encode for Str<&'_ bytes_str::BytesStr> {
        fn encode<W: cbor4ii::core::enc::Write>(
            &self,
            writer: &mut W,
        ) -> Result<(), cbor4ii::core::enc::Error<W::Error>> {
            cbor4ii::core::enc::Encode::encode(&self.0.as_str(), writer)
        }
    }

    impl<'de> cbor4ii::core::dec::Decode<'de> for Str<bytes_str::BytesStr> {
        fn decode<R: cbor4ii::core::dec::Read<'de>>(
            reader: &mut R,
        ) -> Result<Self, cbor4ii::core::dec::Error<R::Error>> {
            String::decode(reader)
                .map(bytes_str::BytesStr::from)
                .map(Str)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{lookup_line, BytePos, Span};

    #[test]
    fn test_lookup_line() {
        let lines = &[BytePos(3), BytePos(17), BytePos(28)];

        assert_eq!(lookup_line(lines, BytePos(0)), -1);
        assert_eq!(lookup_line(lines, BytePos(3)), 0);
        assert_eq!(lookup_line(lines, BytePos(4)), 0);

        assert_eq!(lookup_line(lines, BytePos(16)), 0);
        assert_eq!(lookup_line(lines, BytePos(17)), 1);
        assert_eq!(lookup_line(lines, BytePos(18)), 1);

        assert_eq!(lookup_line(lines, BytePos(28)), 2);
        assert_eq!(lookup_line(lines, BytePos(29)), 2);
    }

    #[test]
    fn size_of_span() {
        assert_eq!(std::mem::size_of::<Span>(), 8);
    }
}
