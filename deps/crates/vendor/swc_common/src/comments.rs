use std::{
    cell::{Ref, RefCell, RefMut},
    rc::Rc,
    sync::Arc,
};

use rustc_hash::FxHashMap;
use swc_atoms::{atom, Atom};

use crate::{
    pos::Spanned,
    syntax_pos::{BytePos, Span, DUMMY_SP},
};

/// Stores comment.
///
/// ## Implementation notes
///
/// Methods uses `(&self)` instead of `(&mut self)` for some reasons. Firstly,
/// this is similar to the previous api. Secondly, typescript parser requires
/// backtracking, which requires [Clone]. To avoid cloning large vectors, we
/// must use [Rc<RefCell<Comments>>]. We have two option. We may implement it in
/// the parser or in the implementation. If we decide to go with first option,
/// we should pass [Comments] to parser, and as a result we need another method
/// to take comments back. If we decide to go with second way, we can just pass
/// [&Comments] to the parser. Thirdly, `(&self)` allows multi-threaded
/// use-cases such as swc itself.
///
/// We use [Option] instead of no-op Comments implementation to avoid allocation
/// unless required.
pub trait Comments {
    fn add_leading(&self, pos: BytePos, cmt: Comment);
    fn add_leading_comments(&self, pos: BytePos, comments: Vec<Comment>);
    fn has_leading(&self, pos: BytePos) -> bool;
    fn move_leading(&self, from: BytePos, to: BytePos);
    fn take_leading(&self, pos: BytePos) -> Option<Vec<Comment>>;
    fn get_leading(&self, pos: BytePos) -> Option<Vec<Comment>>;

    fn add_trailing(&self, pos: BytePos, cmt: Comment);
    fn add_trailing_comments(&self, pos: BytePos, comments: Vec<Comment>);
    fn has_trailing(&self, pos: BytePos) -> bool;
    fn move_trailing(&self, from: BytePos, to: BytePos);
    fn take_trailing(&self, pos: BytePos) -> Option<Vec<Comment>>;
    fn get_trailing(&self, pos: BytePos) -> Option<Vec<Comment>>;

    fn add_pure_comment(&self, pos: BytePos);

    fn with_leading<F, Ret>(&self, pos: BytePos, f: F) -> Ret
    where
        Self: Sized,
        F: FnOnce(&[Comment]) -> Ret,
    {
        let cmts = self.take_leading(pos);

        let ret = if let Some(cmts) = &cmts {
            f(cmts)
        } else {
            f(&[])
        };

        if let Some(cmts) = cmts {
            self.add_leading_comments(pos, cmts);
        }

        ret
    }

    fn with_trailing<F, Ret>(&self, pos: BytePos, f: F) -> Ret
    where
        Self: Sized,
        F: FnOnce(&[Comment]) -> Ret,
    {
        let cmts = self.take_trailing(pos);

        let ret = if let Some(cmts) = &cmts {
            f(cmts)
        } else {
            f(&[])
        };

        if let Some(cmts) = cmts {
            self.add_trailing_comments(pos, cmts);
        }

        ret
    }

    /// This method is used to check if a comment with the given flag exist.
    ///
    /// If `flag` is `PURE`, this method will look for `@__PURE__` and
    /// `#__PURE__`.
    fn has_flag(&self, lo: BytePos, flag: &str) -> bool {
        let cmts = self.take_leading(lo);

        let ret = if let Some(comments) = &cmts {
            (|| {
                for c in comments {
                    if c.kind == CommentKind::Block {
                        for line in c.text.lines() {
                            // jsdoc
                            let line = line.trim_start_matches(['*', ' ']);
                            let line = line.trim();

                            //
                            if line.len() == (flag.len() + 5)
                                && (line.starts_with("#__") || line.starts_with("@__"))
                                && line.ends_with("__")
                                && flag == &line[3..line.len() - 2]
                            {
                                return true;
                            }
                        }
                    }
                }

                false
            })()
        } else {
            false
        };

        if let Some(cmts) = cmts {
            self.add_trailing_comments(lo, cmts);
        }

        ret
    }
}

macro_rules! delegate {
    () => {
        fn add_leading(&self, pos: BytePos, cmt: Comment) {
            (**self).add_leading(pos, cmt)
        }

        fn add_leading_comments(&self, pos: BytePos, comments: Vec<Comment>) {
            (**self).add_leading_comments(pos, comments)
        }

        fn has_leading(&self, pos: BytePos) -> bool {
            (**self).has_leading(pos)
        }

        fn move_leading(&self, from: BytePos, to: BytePos) {
            (**self).move_leading(from, to)
        }

        fn take_leading(&self, pos: BytePos) -> Option<Vec<Comment>> {
            (**self).take_leading(pos)
        }

        fn get_leading(&self, pos: BytePos) -> Option<Vec<Comment>> {
            (**self).get_leading(pos)
        }

        fn add_trailing(&self, pos: BytePos, cmt: Comment) {
            (**self).add_trailing(pos, cmt)
        }

        fn add_trailing_comments(&self, pos: BytePos, comments: Vec<Comment>) {
            (**self).add_trailing_comments(pos, comments)
        }

        fn has_trailing(&self, pos: BytePos) -> bool {
            (**self).has_trailing(pos)
        }

        fn move_trailing(&self, from: BytePos, to: BytePos) {
            (**self).move_trailing(from, to)
        }

        fn take_trailing(&self, pos: BytePos) -> Option<Vec<Comment>> {
            (**self).take_trailing(pos)
        }

        fn get_trailing(&self, pos: BytePos) -> Option<Vec<Comment>> {
            (**self).get_trailing(pos)
        }

        fn add_pure_comment(&self, pos: BytePos) {
            (**self).add_pure_comment(pos)
        }

        fn has_flag(&self, lo: BytePos, flag: &str) -> bool {
            (**self).has_flag(lo, flag)
        }
    };
}

impl<T> Comments for &'_ T
where
    T: ?Sized + Comments,
{
    delegate!();
}

impl<T> Comments for Arc<T>
where
    T: ?Sized + Comments,
{
    delegate!();
}

impl<T> Comments for Rc<T>
where
    T: ?Sized + Comments,
{
    delegate!();
}

impl<T> Comments for Box<T>
where
    T: ?Sized + Comments,
{
    delegate!();
}

/// Implementation of [Comments] which does not store any comments.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default, Hash)]
pub struct NoopComments;

impl Comments for NoopComments {
    #[cfg_attr(not(debug_assertions), inline(always))]
    fn add_leading(&self, _: BytePos, _: Comment) {}

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn add_leading_comments(&self, _: BytePos, _: Vec<Comment>) {}

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn has_leading(&self, _: BytePos) -> bool {
        false
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn move_leading(&self, _: BytePos, _: BytePos) {}

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn take_leading(&self, _: BytePos) -> Option<Vec<Comment>> {
        None
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn get_leading(&self, _: BytePos) -> Option<Vec<Comment>> {
        None
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn add_trailing(&self, _: BytePos, _: Comment) {}

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn add_trailing_comments(&self, _: BytePos, _: Vec<Comment>) {}

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn has_trailing(&self, _: BytePos) -> bool {
        false
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn move_trailing(&self, _: BytePos, _: BytePos) {}

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn take_trailing(&self, _: BytePos) -> Option<Vec<Comment>> {
        None
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn get_trailing(&self, _: BytePos) -> Option<Vec<Comment>> {
        None
    }

    #[cfg_attr(not(debug_assertions), inline(always))]
    fn add_pure_comment(&self, _: BytePos) {}

    #[inline]
    fn has_flag(&self, _: BytePos, _: &str) -> bool {
        false
    }
}

/// This implementation behaves like [NoopComments] if it's [None].
impl<C> Comments for Option<C>
where
    C: Comments,
{
    fn add_leading(&self, pos: BytePos, cmt: Comment) {
        if let Some(c) = self {
            c.add_leading(pos, cmt)
        }
    }

    fn add_leading_comments(&self, pos: BytePos, comments: Vec<Comment>) {
        if let Some(c) = self {
            c.add_leading_comments(pos, comments)
        }
    }

    fn has_leading(&self, pos: BytePos) -> bool {
        if let Some(c) = self {
            c.has_leading(pos)
        } else {
            false
        }
    }

    fn move_leading(&self, from: BytePos, to: BytePos) {
        if let Some(c) = self {
            c.move_leading(from, to)
        }
    }

    fn take_leading(&self, pos: BytePos) -> Option<Vec<Comment>> {
        if let Some(c) = self {
            c.take_leading(pos)
        } else {
            None
        }
    }

    fn get_leading(&self, pos: BytePos) -> Option<Vec<Comment>> {
        if let Some(c) = self {
            c.get_leading(pos)
        } else {
            None
        }
    }

    fn add_trailing(&self, pos: BytePos, cmt: Comment) {
        if let Some(c) = self {
            c.add_trailing(pos, cmt)
        }
    }

    fn add_trailing_comments(&self, pos: BytePos, comments: Vec<Comment>) {
        if let Some(c) = self {
            c.add_trailing_comments(pos, comments)
        }
    }

    fn has_trailing(&self, pos: BytePos) -> bool {
        if let Some(c) = self {
            c.has_trailing(pos)
        } else {
            false
        }
    }

    fn move_trailing(&self, from: BytePos, to: BytePos) {
        if let Some(c) = self {
            c.move_trailing(from, to)
        }
    }

    fn take_trailing(&self, pos: BytePos) -> Option<Vec<Comment>> {
        if let Some(c) = self {
            c.take_trailing(pos)
        } else {
            None
        }
    }

    fn get_trailing(&self, pos: BytePos) -> Option<Vec<Comment>> {
        if let Some(c) = self {
            c.get_trailing(pos)
        } else {
            None
        }
    }

    fn add_pure_comment(&self, pos: BytePos) {
        assert_ne!(pos, BytePos(0), "cannot add pure comment to zero position");

        if let Some(c) = self {
            c.add_pure_comment(pos)
        }
    }

    fn with_leading<F, Ret>(&self, pos: BytePos, f: F) -> Ret
    where
        Self: Sized,
        F: FnOnce(&[Comment]) -> Ret,
    {
        if let Some(c) = self {
            c.with_leading(pos, f)
        } else {
            f(&[])
        }
    }

    fn with_trailing<F, Ret>(&self, pos: BytePos, f: F) -> Ret
    where
        Self: Sized,
        F: FnOnce(&[Comment]) -> Ret,
    {
        if let Some(c) = self {
            c.with_trailing(pos, f)
        } else {
            f(&[])
        }
    }

    #[inline]
    fn has_flag(&self, lo: BytePos, flag: &str) -> bool {
        if let Some(c) = self {
            c.has_flag(lo, flag)
        } else {
            false
        }
    }
}

pub type SingleThreadedCommentsMapInner = FxHashMap<BytePos, Vec<Comment>>;
pub type SingleThreadedCommentsMap = Rc<RefCell<SingleThreadedCommentsMapInner>>;

/// Single-threaded storage for comments.
#[derive(Debug, Clone, Default)]
pub struct SingleThreadedComments {
    leading: SingleThreadedCommentsMap,
    trailing: SingleThreadedCommentsMap,
}

impl Comments for SingleThreadedComments {
    fn add_leading(&self, pos: BytePos, cmt: Comment) {
        self.leading.borrow_mut().entry(pos).or_default().push(cmt);
    }

    fn add_leading_comments(&self, pos: BytePos, comments: Vec<Comment>) {
        self.leading
            .borrow_mut()
            .entry(pos)
            .or_default()
            .extend(comments);
    }

    fn has_leading(&self, pos: BytePos) -> bool {
        if let Some(v) = self.leading.borrow().get(&pos) {
            !v.is_empty()
        } else {
            false
        }
    }

    fn move_leading(&self, from: BytePos, to: BytePos) {
        let cmt = self.take_leading(from);

        if let Some(mut cmt) = cmt {
            if from < to && self.has_leading(to) {
                cmt.extend(self.take_leading(to).unwrap());
            }

            self.add_leading_comments(to, cmt);
        }
    }

    fn take_leading(&self, pos: BytePos) -> Option<Vec<Comment>> {
        self.leading.borrow_mut().remove(&pos)
    }

    fn get_leading(&self, pos: BytePos) -> Option<Vec<Comment>> {
        self.leading.borrow().get(&pos).map(|c| c.to_owned())
    }

    fn add_trailing(&self, pos: BytePos, cmt: Comment) {
        self.trailing.borrow_mut().entry(pos).or_default().push(cmt);
    }

    fn add_trailing_comments(&self, pos: BytePos, comments: Vec<Comment>) {
        self.trailing
            .borrow_mut()
            .entry(pos)
            .or_default()
            .extend(comments);
    }

    fn has_trailing(&self, pos: BytePos) -> bool {
        if let Some(v) = self.trailing.borrow().get(&pos) {
            !v.is_empty()
        } else {
            false
        }
    }

    fn move_trailing(&self, from: BytePos, to: BytePos) {
        let cmt = self.take_trailing(from);

        if let Some(mut cmt) = cmt {
            if from < to && self.has_trailing(to) {
                cmt.extend(self.take_trailing(to).unwrap());
            }

            self.add_trailing_comments(to, cmt);
        }
    }

    fn take_trailing(&self, pos: BytePos) -> Option<Vec<Comment>> {
        self.trailing.borrow_mut().remove(&pos)
    }

    fn get_trailing(&self, pos: BytePos) -> Option<Vec<Comment>> {
        self.trailing.borrow().get(&pos).map(|c| c.to_owned())
    }

    fn add_pure_comment(&self, pos: BytePos) {
        assert_ne!(pos, BytePos(0), "cannot add pure comment to zero position");

        let mut leading_map = self.leading.borrow_mut();
        let leading = leading_map.entry(pos).or_default();
        let pure_comment = Comment {
            kind: CommentKind::Block,
            span: DUMMY_SP,
            text: atom!("#__PURE__"),
        };

        if !leading.iter().any(|c| c.text == pure_comment.text) {
            leading.push(pure_comment);
        }
    }

    fn with_leading<F, Ret>(&self, pos: BytePos, f: F) -> Ret
    where
        Self: Sized,
        F: FnOnce(&[Comment]) -> Ret,
    {
        let b = self.leading.borrow();
        let cmts = b.get(&pos);

        if let Some(cmts) = &cmts {
            f(cmts)
        } else {
            f(&[])
        }
    }

    fn with_trailing<F, Ret>(&self, pos: BytePos, f: F) -> Ret
    where
        Self: Sized,
        F: FnOnce(&[Comment]) -> Ret,
    {
        let b = self.trailing.borrow();
        let cmts = b.get(&pos);

        if let Some(cmts) = &cmts {
            f(cmts)
        } else {
            f(&[])
        }
    }

    fn has_flag(&self, lo: BytePos, flag: &str) -> bool {
        self.with_leading(lo, |comments| {
            for c in comments {
                if c.kind == CommentKind::Block {
                    for line in c.text.lines() {
                        // jsdoc
                        let line = line.trim_start_matches(['*', ' ']);
                        let line = line.trim();

                        //
                        if line.len() == (flag.len() + 5)
                            && (line.starts_with("#__") || line.starts_with("@__"))
                            && line.ends_with("__")
                            && flag == &line[3..line.len() - 2]
                        {
                            return true;
                        }
                    }
                }
            }

            false
        })
    }
}

impl SingleThreadedComments {
    /// Creates a new `SingleThreadedComments` from the provided leading and
    /// trailing.
    pub fn from_leading_and_trailing(
        leading: SingleThreadedCommentsMap,
        trailing: SingleThreadedCommentsMap,
    ) -> Self {
        SingleThreadedComments { leading, trailing }
    }

    /// Takes all the comments as (leading, trailing).
    pub fn take_all(self) -> (SingleThreadedCommentsMap, SingleThreadedCommentsMap) {
        (self.leading, self.trailing)
    }

    /// Borrows all the comments as (leading, trailing).
    pub fn borrow_all(
        &self,
    ) -> (
        Ref<SingleThreadedCommentsMapInner>,
        Ref<SingleThreadedCommentsMapInner>,
    ) {
        (self.leading.borrow(), self.trailing.borrow())
    }

    /// Borrows all the comments as (leading, trailing).
    pub fn borrow_all_mut(
        &self,
    ) -> (
        RefMut<SingleThreadedCommentsMapInner>,
        RefMut<SingleThreadedCommentsMapInner>,
    ) {
        (self.leading.borrow_mut(), self.trailing.borrow_mut())
    }

    pub fn with_leading<F, Ret>(&self, pos: BytePos, op: F) -> Ret
    where
        F: FnOnce(&[Comment]) -> Ret,
    {
        if let Some(comments) = self.leading.borrow().get(&pos) {
            op(comments)
        } else {
            op(&[])
        }
    }

    pub fn with_trailing<F, Ret>(&self, pos: BytePos, op: F) -> Ret
    where
        F: FnOnce(&[Comment]) -> Ret,
    {
        if let Some(comments) = self.trailing.borrow().get(&pos) {
            op(comments)
        } else {
            op(&[])
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
pub struct Comment {
    pub kind: CommentKind,
    pub span: Span,
    /// [`Atom::new_bad`][] is perfectly fine for this value.
    pub text: Atom,
}

impl Spanned for Comment {
    fn span(&self) -> Span {
        self.span
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[cfg_attr(
    any(feature = "rkyv-impl"),
    derive(rkyv::Archive, rkyv::Serialize, rkyv::Deserialize)
)]
#[cfg_attr(feature = "rkyv-impl", derive(bytecheck::CheckBytes))]
#[cfg_attr(feature = "rkyv-impl", repr(u32))]
#[cfg_attr(feature = "encoding-impl", derive(crate::Encode, crate::Decode))]
pub enum CommentKind {
    Line = 0,
    Block = 1,
}

#[deprecated(
    since = "0.13.5",
    note = "helper methods are merged into Comments itself"
)]
pub trait CommentsExt: Comments {
    fn with_leading<F, Ret>(&self, pos: BytePos, op: F) -> Ret
    where
        F: FnOnce(&[Comment]) -> Ret,
    {
        if let Some(comments) = self.get_leading(pos) {
            op(&comments)
        } else {
            op(&[])
        }
    }

    fn with_trailing<F, Ret>(&self, pos: BytePos, op: F) -> Ret
    where
        F: FnOnce(&[Comment]) -> Ret,
    {
        if let Some(comments) = self.get_trailing(pos) {
            op(&comments)
        } else {
            op(&[])
        }
    }
}

#[allow(deprecated)]
impl<C> CommentsExt for C where C: Comments {}

better_scoped_tls::scoped_tls!(
    /// **This is not a public API**. Used to handle comments while **testing**.
    #[doc(hidden)]
    pub static COMMENTS: Box<dyn Comments>
);
