use std::{borrow::Cow, rc::Rc, sync::Arc};

pub use crate::syntax_pos::{
    hygiene, BytePos, CharPos, FileName, Globals, Loc, LocWithOpt, Mark, MultiSpan, SourceFile,
    SourceFileAndBytePos, SourceFileAndLine, Span, SpanLinesError, SyntaxContext, DUMMY_SP,
    GLOBALS, NO_EXPANSION,
};

///
/// # Derive
/// This trait can be derived with `#[derive(Spanned)]`.
pub trait Spanned {
    /// Get span of `self`.
    fn span(&self) -> Span;

    #[inline]
    fn span_lo(&self) -> BytePos {
        self.span().lo
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        self.span().hi
    }
}

impl<T> Spanned for Cow<'_, T>
where
    T: Spanned + Clone,
{
    #[inline]
    fn span(&self) -> Span {
        (**self).span()
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        (**self).span_lo()
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        (**self).span_hi()
    }
}

impl Spanned for Span {
    #[inline(always)]
    fn span(&self) -> Span {
        *self
    }
}

impl Spanned for BytePos {
    /// Creates a new single-byte span.
    #[inline(always)]
    fn span(&self) -> Span {
        Span::new(*self, *self)
    }
}

impl<S> Spanned for Option<S>
where
    S: Spanned,
{
    #[inline]
    fn span(&self) -> Span {
        match *self {
            Some(ref s) => s.span(),
            None => DUMMY_SP,
        }
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        match *self {
            Some(ref s) => s.span_lo(),
            None => BytePos::DUMMY,
        }
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        match *self {
            Some(ref s) => s.span_hi(),
            None => BytePos::DUMMY,
        }
    }
}

impl<S> Spanned for Rc<S>
where
    S: ?Sized + Spanned,
{
    fn span(&self) -> Span {
        <S as Spanned>::span(self)
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        <S as Spanned>::span_lo(self)
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        <S as Spanned>::span_hi(self)
    }
}

impl<S> Spanned for Arc<S>
where
    S: ?Sized + Spanned,
{
    fn span(&self) -> Span {
        <S as Spanned>::span(self)
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        <S as Spanned>::span_lo(self)
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        <S as Spanned>::span_hi(self)
    }
}

impl<S> Spanned for Box<S>
where
    S: ?Sized + Spanned,
{
    fn span(&self) -> Span {
        <S as Spanned>::span(self)
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        <S as Spanned>::span_lo(self)
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        <S as Spanned>::span_hi(self)
    }
}

impl<S> Spanned for &S
where
    S: ?Sized + Spanned,
{
    fn span(&self) -> Span {
        <S as Spanned>::span(self)
    }

    #[inline]
    fn span_lo(&self) -> BytePos {
        <S as Spanned>::span_lo(self)
    }

    #[inline]
    fn span_hi(&self) -> BytePos {
        <S as Spanned>::span_hi(self)
    }
}

impl<A, B> Spanned for ::either::Either<A, B>
where
    A: Spanned,
    B: Spanned,
{
    fn span(&self) -> Span {
        match *self {
            ::either::Either::Left(ref n) => n.span(),
            ::either::Either::Right(ref n) => n.span(),
        }
    }

    fn span_lo(&self) -> BytePos {
        match *self {
            ::either::Either::Left(ref n) => n.span_lo(),
            ::either::Either::Right(ref n) => n.span_lo(),
        }
    }

    fn span_hi(&self) -> BytePos {
        match *self {
            ::either::Either::Left(ref n) => n.span_hi(),
            ::either::Either::Right(ref n) => n.span_hi(),
        }
    }
}

// swc_allocator::nightly_only!(
//     impl<T> Spanned for swc_allocator::boxed::Box<T>
//     where
//         T: Spanned,
//     {
//         fn span(&self) -> Span {
//             self.as_ref().span()
//         }
//     }
// );
