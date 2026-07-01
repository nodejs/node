// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! APIs to support in-place transliteration.
//!
//! The process will usually look as follows:
//!
//! 1. Create a [`Replaceable`].
//! 2. If there is a filter to be applied, call [`Replaceable::for_each_run`] with the filter.
//! 3. Next, if we are black-box transliterating, call [`Replaceable::replace_modifiable_with_str`],
//!    after which we are done. If we are transliterating UTS #35 Transform Rules, continue:
//! 4. Create a [`RepMatcher`] with [`Replaceable::start_match`].
//! 5. Match a rule with the `RepMatcher`.
//! 6. Assuming the rule matches, call [`RepMatcher::finish_match`] to obtain an [`Insertable`].
//! 7. Walk through the rule's replacement string, and use `Insertable`'s methods to insert
//!    valid UTF-8 content. If there are recursive function calls, use the entry point
//!    [`Insertable::start_replaceable_adapter`] to obtain a child `Replaceable` that can be
//!    used by going back to step 2.
//! 8. Once the replacement string has been fully processed, the `Insertable`'s `Drop`
//!    implementation will adjust the `Replaceable`'s cursor to reflect the applied replacement.
//! 9. The rule is done, continue with the next rule and step 4 until [`Replaceable::is_finished`]
//!    returns true.
//!
//! This module uses a lot of unsafe code to work with UTF-8 in an allocation-efficient manner and to
//! avoid a lot of UTF-8 validity checks.
// note: this mod could be moved into a utility crate, the only thing
// quite closely coupled to Transform Rules is `RepMatcher` and its explicit `ante`, `post`, `key`
// handling.

// NOTE: Panics (e.g., from CustomTransliterator impls) during transliteration could unwind through
//  Drop impls (Insertable, InsertableGuard, etc.) that may not fully restore UTF-8 validity.
//  As a mitigation, TransliteratorBuffer::into_string() uses checked UTF-8 conversion so that
//  any such corruption results in a clean panic rather than undefined behavior.

use super::Filter;
use alloc::string::String;
use alloc::vec::Vec;
use core::fmt::{Debug, Formatter};
use core::mem::ManuallyDrop;
use core::ops::Range;
use core::ops::{Deref, DerefMut};

/// The backing buffer for the API provided by this module.
///
/// # Safety
/// Exclusive access to a buffer implies it contains valid UTF-8.
pub(crate) struct TransliteratorBuffer(Vec<u8>);

impl TransliteratorBuffer {
    pub(crate) fn from_string(s: String) -> Self {
        Self(s.into_bytes())
    }

    #[allow(clippy::expect_used)] // panic is strictly better than UB from from_utf8_unchecked
    pub(crate) fn into_string(self) -> String {
        // Using checked conversion: if a panic during transliteration unwinds through
        // Drop impls that fail to fully restore UTF-8 validity, this will panic cleanly
        // instead of producing undefined behavior.
        String::from_utf8(self.0)
            .expect("TransliteratorBuffer must contain valid UTF-8 after transliteration")
    }
}

/// A wrapper over `Vec<u8>` that only allows access (and modification) to a certain range.
///
/// This is useful for other types that might only need a part of a `Vec<u8>` to be of a certain
/// structure, such as UTF-8. With this wrapper, they can assert their invariants only for the
/// accessible portion.
///
/// All methods that take indices as arguments expect them to be relative to the *visible* part.
struct Hide<'a> {
    raw: &'a mut Vec<u8>,
    hide_pre_len: usize,
    hide_post_len: usize,
}

impl<'a> Hide<'a> {
    fn new(raw: &'a mut Vec<u8>) -> Self {
        Self {
            raw,
            hide_pre_len: 0,
            hide_post_len: 0,
        }
    }

    fn splice(&mut self, range: Range<usize>, replace_with: impl IntoIterator<Item = u8>) {
        let adjusted_range = range.start + self.hide_pre_len..range.end + self.hide_pre_len;
        self.raw.splice(adjusted_range, replace_with);
    }

    fn child(&mut self) -> Hide<'_> {
        Hide {
            raw: self.raw,
            hide_pre_len: self.hide_pre_len,
            hide_post_len: self.hide_post_len,
        }
    }

    /// Borrows into a child `Hide` with its visible part restricted to the given range.
    fn tighten(&mut self, visible_range: Range<usize>) -> Hide<'_> {
        debug_assert!(visible_range.start <= self.len());
        debug_assert!(visible_range.end <= self.len());

        let hide_pre_len = self.hide_pre_len + visible_range.start;
        let hide_post_len = self.hide_post_len + (self.len() - visible_range.end);
        Hide {
            raw: self.raw,
            hide_pre_len,
            hide_post_len,
        }
    }

    fn hidden_prefix(&self) -> &[u8] {
        &self.raw[..self.hide_pre_len]
    }

    fn hidden_suffix(&self) -> &[u8] {
        &self.raw[self.raw.len() - self.hide_post_len..]
    }
}

impl Deref for Hide<'_> {
    type Target = [u8];
    fn deref(&self) -> &Self::Target {
        &self.raw[self.hide_pre_len..self.raw.len() - self.hide_post_len]
    }
}

impl DerefMut for Hide<'_> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        let len = self.raw.len();
        &mut self.raw[self.hide_pre_len..len - self.hide_post_len]
    }
}

// Thought: if we loosen the invariant of Replaceable UTF-8 content to only need UTF-8 content in
//  the visible range, we would not need to make_contiguous in Insertable, which would avoid
//  moving around the tail after a non-final function call.
//  => invariants have been loosened, but to avoid make_contiguous our on_drop needs to be smarter
//  than right now, because it assumes self.curr == self.end().

// Thought: A transliteration run does not necessarily need cursors. In fact, cursor only exist
//  in the context of Transform Rules. We could have a wrapping CursoredReplaceable type
//  that is specifically for transform rules.

// Thought for above: Renames? Replaceable => Run, and CursoredRun/RunWithCursor.

/// Represents a transliteration run. It is aware of the range of the input that is allowed
/// to be transliterated, according to the filter.
///
/// `Replaceable`s are made to be stacked. This means that while a given `Replaceable` represents
/// a single run, it can be used to iterate over all sub-runs with a given filter of itself using
/// [`for_each_run`](Replaceable::for_each_run).
///
/// Typical usage of a `Replaceable` depends on the client:
/// - When transliterating transform rules, the `cursor`-related methods, as well as [`start_match`](Replaceable::start_match)
///   will be of interest.
/// - When transliterating with a black box, most likely [`replace_modifiable_with_str`](Replaceable::replace_modifiable_with_str)
///   is the only method that will be used.
///
/// # Safety
/// Note: `content` is a `Hide`. Whenever `content` is mentioned, only the *visible* part of it is
/// meant.
///
/// The invariants of this struct are as follows:
/// - (The visible part of) `content` must be valid UTF-8.
/// - `cursor` must be a valid UTF-8 index into the visible part of `content`.
/// - `run_range()` (as defined by `freeze_pre_len` and `freeze_post_len`), must always be a valid
///   UTF-8 range into `content`.
pub(crate) struct Replaceable<'a> {
    content: Hide<'a>,
    freeze_pre_len: usize,
    freeze_post_len: usize,
    cursor: usize,
}

impl<'a> Replaceable<'a> {
    pub(crate) fn new(buf: &'a mut TransliteratorBuffer) -> Self {
        // SAFETY: we have exclusive access to the buffer, so it must contain valid UTF-8
        unsafe { Replaceable::from_hide(Hide::new(&mut buf.0)) }
    }

    /// # Safety
    /// The caller must ensure the visible portion of `content` is valid UTF-8.
    unsafe fn from_hide(content: Hide<'a>) -> Self {
        debug_assert!(core::str::from_utf8(&content).is_ok());
        Self {
            content,
            // SAFETY: these uphold the invariants
            freeze_pre_len: 0,
            freeze_post_len: 0,
            cursor: 0,
        }
    }

    pub(crate) fn replace_modifiable_with_str(&mut self, s: &str) {
        // SAFETY: allowed_range() returns a valid UTF-8 range, and `s.bytes()` contains only valid UTF-8
        self.content.splice(self.allowed_range(), s.bytes());
    }

    /// Returns the full internal text as a `&str`.
    pub(crate) fn as_str(&self) -> &str {
        debug_assert!(core::str::from_utf8(&self.content).is_ok());
        // SAFETY: Replaceable's invariant states that content is always valid UTF-8
        unsafe { core::str::from_utf8_unchecked(&self.content) }
    }

    /// Returns the current modifiable text as a `&str`.
    pub(crate) fn as_str_modifiable(&self) -> &str {
        &self.as_str()[self.allowed_range()]
    }

    // Thought: rename to run_range()? also any associated mentions of this, the upper bound,
    //  "modifiable range" etc..
    /// Returns the range of bytes that are currently allowed to be modified.
    ///
    /// This is guaranteed to be a range compatible with the internal text.
    pub(crate) fn allowed_range(&self) -> Range<usize> {
        self.freeze_pre_len..self.allowed_upper_bound()
    }

    /// Returns the cursor.
    ///
    /// This is guaranteed to be a valid UTF-8 index into the internal text.
    pub(crate) fn cursor(&self) -> usize {
        // // eprintln!("cursor called with raw_cursor: {}, ignore_pre_len: {}", self.raw_cursor, self.ignore_pre_len);
        self.cursor
    }

    /// Advances the cursor by one char.
    pub(crate) fn step_cursor(&mut self) {
        let step_len = self.as_str()[self.cursor..]
            .chars()
            .next()
            .map(char::len_utf8)
            .unwrap_or(0);
        // // eprintln!("step_cursor: {}", step_len);
        // SAFETY: step_len is the UTF-8 length of the char after `self.cursor`
        self.cursor += step_len;
    }

    /// Sets the cursor. The cursor must remain in the modifiable window.
    ///
    /// # Safety
    /// The caller must ensure that `cursor` is a valid UTF-8 index into the internal text.
    unsafe fn set_cursor(&mut self, cursor: usize) {
        debug_assert!(cursor <= self.allowed_upper_bound());
        debug_assert!(cursor >= self.freeze_pre_len);
        self.cursor = cursor;
    }

    /// Returns true if the cursor is at the end of the modifiable range.
    pub(crate) fn is_finished(&self) -> bool {
        // the cursor should never be > the upper bound
        debug_assert!(self.cursor <= self.allowed_upper_bound());
        self.cursor >= self.allowed_upper_bound()
    }

    /// Returns a `Replaceable` with the same content as the current one.
    ///
    /// This is useful for repeated transliterations of the same modifiable range.
    pub(crate) fn child(&mut self) -> Replaceable<'_> {
        Replaceable {
            content: self.content.child(),
            freeze_pre_len: self.freeze_pre_len,
            freeze_post_len: self.freeze_post_len,
            cursor: self.cursor,
        }
    }

    // Thought: could replace the F generic with a InternalTransliteratorTrait generic, but this is fine?
    /// Applies `f` to each sub-run as defined by `filter` of the current `Replaceable`'s run.
    pub(crate) fn for_each_run<F>(&mut self, filter: &Filter, mut f: F)
    where
        F: FnMut(&mut Replaceable),
    {
        // sub-runs must be part of *self*'s run, so we can only start in our modifiable range.
        let mut start = self.freeze_pre_len;
        // SAFETY: start is always the result of a function returning valid UTF-8 indices
        while let Some(mut run) = unsafe { self.next_filtered_run(start, filter) } {
            f(&mut run);
            start = run.allowed_upper_bound();
        }
    }

    /// Initiate the matching process for a single rule, starting at the current cursor.
    pub(super) fn start_match(&mut self) -> RepMatcher<'a, '_, false> {
        let cursor = self.cursor;
        RepMatcher {
            rep: self,
            key_match_len: 0,
            forward_cursor: cursor,
            ante_match_len: 0,
            post_match_len: 0,
        }
    }

    /// Returns the next run (as a Replaceable with the corresponding frozen range)
    /// that occurs on or after `start`, if one exists.
    ///
    /// # Safety
    /// The caller must ensure that `start` is a valid UTF-8 index into the internal text.
    unsafe fn next_filtered_run(
        &mut self,
        start: usize,
        filter: &Filter,
    ) -> Option<Replaceable<'_>> {
        if start == self.allowed_upper_bound() {
            // we have reached the end, there are no more runs
            return None;
        }

        debug_assert!(
            start < self.allowed_upper_bound(),
            "start `{start}` must be within the content length `{}`",
            self.allowed_upper_bound()
        );
        debug_assert!(self.as_str().is_char_boundary(start));

        let run_start;
        let run_end;
        if filter == &Filter::all() {
            // special case for the noop filter

            run_start = start;
            run_end = self.allowed_upper_bound();
        } else {
            run_start = self.find_first_char_in_modifiable_range(start, |c| filter.contains(c))?;
            run_end = self
                .find_first_char_in_modifiable_range(run_start, |c| !filter.contains(c))
                .unwrap_or_else(|| self.allowed_upper_bound());
        }

        // eprintln!("computing filtered run for rep: {self:?}, start: {start}, run_start: {run_start}, run_end: {run_end}");

        let freeze_post_len = self.content.len() - run_end;

        Some(Replaceable {
            content: self.content.child(),
            // safety: these uphold the invariants
            freeze_pre_len: run_start,
            freeze_post_len,
            cursor: run_start,
        })
    }

    /// Returns the index of the first char in the modifiable text that satisfies `f`,
    /// starting at index `start`. The returned index is guaranteed to be a valid UTF-8 index
    /// into the internal text.
    ///
    /// `start` must be a valid UTF-8 index into into the internal text.
    fn find_first_char_in_modifiable_range<F>(&self, start: usize, f: F) -> Option<usize>
    where
        F: Fn(char) -> bool,
    {
        let tail = &self.as_str()[start..self.allowed_upper_bound()];
        let (idx, _) = tail.char_indices().find(|&(_, c)| f(c))?;
        Some(start + idx)
    }

    /// Returns the current (exclusive) upper bound of the modifiable range.
    ///
    /// This is guaranteed to be a valid UTF-8 index into the internal text.
    pub(crate) fn allowed_upper_bound(&self) -> usize {
        // // eprintln!("allowed_upper_bound called with len: {}, freeze_post_len: {}, ignore_pre_len: {}", self.content.len(), self.freeze_post_len, self.ignore_pre_len);
        self.content.len() - self.freeze_post_len
    }
}

impl Debug for Replaceable<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "{:?}", self.content.hidden_prefix())?;
        write!(f, "[[[")?;
        write!(f, "{}", &self.as_str()[..self.freeze_pre_len])?;
        write!(f, "{{{{{{")?;
        write!(f, "{}", &self.as_str()[self.freeze_pre_len..self.cursor()])?;
        write!(f, "|||")?;
        write!(
            f,
            "{}",
            &self.as_str()[self.cursor()..self.allowed_upper_bound()]
        )?;
        write!(f, "}}}}}}")?;
        write!(f, "{}", &self.as_str()[self.allowed_upper_bound()..])?;
        write!(f, "]]]")?;
        write!(f, "{:?}", self.content.hidden_suffix())?;

        Ok(())
    }
}

// Thought: Maybe this should be renamed as Matchable (also the trait), as we have "SpecialMatchers"
/// Supports safe conversion rule matching over a [`Replaceable`].
///
/// Conversion rule matching consists of three parts:
/// 1. Matching the ante. This is a right-aligned (reverse) match at the `Replaceable`'s cursor.
/// 2. Matching the key. This is a left-aligned (forward) match at the `Replaceable`'s cursor.
/// 3. Matching the post. This is a left-aligned (forward) match at the end of _the matched key_.
///    Note that this means we _cannot_ match the key after the post, as the post match is aligned
///    to the end of the key match. The `KEY_FINISHED` parameter enforces this at compile-time.
///
/// After a successful match, the replacement can be applied using the [`Insertable`] returned by
/// [`RepMatcher::finish_match`].
///
/// # Safety
/// The matched portions of the string (as defined by `rep.cursor`, `key_match_len`,
/// `ante_match_len` and `post_match_len`) are all guaranteed to be valid UTF-8 subslices of
/// the `Replaceable`'s internal text.
///
/// The `RepMatcher` does not modify the contained `Replaceable`.
#[derive(Debug)]
pub(super) struct RepMatcher<'a, 'b, const KEY_FINISHED: bool> {
    rep: &'b mut Replaceable<'a>,
    key_match_len: usize,  // relative to rep.cursor
    ante_match_len: usize, // relative to rep.cursor
    post_match_len: usize, // relative to rep.cursor + key_match_len
    forward_cursor: usize, // absolute
}

// we can only finish a KEY_FINISHED = true matcher
impl<'a, 'b> RepMatcher<'a, 'b, true> {
    pub(super) fn finish_match(self) -> Insertable<'a, 'b> {
        Insertable::from_matcher(self)
    }

    fn match_lens(&self) -> MatchLengths {
        MatchLengths {
            key: self.key_match_len,
            ante: self.ante_match_len,
            post: self.post_match_len,
        }
    }
}

// we can only finish matching the key once
impl<'a, 'b> RepMatcher<'a, 'b, false> {
    pub(super) fn finish_match(self) -> Insertable<'a, 'b> {
        Insertable::from_matcher(self.finish_key())
    }

    pub(super) fn finish_key(self) -> RepMatcher<'a, 'b, true> {
        RepMatcher {
            rep: self.rep,
            key_match_len: self.key_match_len,
            ante_match_len: self.ante_match_len,
            post_match_len: self.post_match_len,
            forward_cursor: self.forward_cursor,
        }
    }
}

impl<const KEY_FINISHED: bool> RepMatcher<'_, '_, KEY_FINISHED> {
    fn remaining(&self) -> usize {
        if KEY_FINISHED {
            self.rep.content.len() - self.forward_cursor
        } else {
            self.rep.allowed_upper_bound() - self.forward_cursor
        }
    }

    fn remaining_forward_slice(&self) -> &str {
        if KEY_FINISHED {
            &self.rep.as_str()[self.forward_cursor..]
        } else {
            &self.rep.as_str()[self.forward_cursor..self.rep.allowed_upper_bound()]
        }
    }

    /// Returns the index (which is a valid UTF-8 index) of the leftmost matched char in the ante context.
    #[inline]
    fn ante_cursor(&self) -> usize {
        self.rep.cursor - self.ante_match_len
    }

    fn remaining_ante_slice(&self) -> &str {
        &self.rep.as_str()[..self.ante_cursor()]
    }
}

impl<const KEY_FINISHED: bool> Utf8Matcher<Forward> for RepMatcher<'_, '_, KEY_FINISHED> {
    fn cursor(&self) -> usize {
        self.forward_cursor
    }

    fn str_range(&self, range: Range<usize>) -> Option<&str> {
        self.rep.as_str().get(range)
    }

    fn is_empty(&self) -> bool {
        self.remaining() == 0
    }

    fn match_str(&self, s: &str) -> bool {
        self.remaining_forward_slice().starts_with(s)
    }

    fn match_start_anchor(&self) -> bool {
        self.forward_cursor == 0
    }

    fn match_end_anchor(&self) -> bool {
        // no matter if we're matching key or post, we must be completely at the end of the string
        self.forward_cursor == self.rep.content.len()
    }

    fn consume(&mut self, len: usize) -> bool {
        if len <= self.remaining() {
            assert!(self.remaining_forward_slice().is_char_boundary(len));
            // SAFETY: `len` is guaranteed to be a valid UTF-8 length starting at `forward_cursor`.
            if KEY_FINISHED {
                self.post_match_len += len;
            } else {
                self.key_match_len += len;
            }
            self.forward_cursor += len;
            true
        } else {
            false
        }
    }

    fn next_char(&self) -> Option<char> {
        self.remaining_forward_slice().chars().next()
    }
}

// we can always reverse match, no matter if the key has finished matching or not
impl<const KEY_FINISHED: bool> Utf8Matcher<Reverse> for RepMatcher<'_, '_, KEY_FINISHED> {
    fn cursor(&self) -> usize {
        self.ante_cursor()
    }

    fn str_range(&self, range: Range<usize>) -> Option<&str> {
        self.rep.as_str().get(range)
    }

    fn is_empty(&self) -> bool {
        self.ante_cursor() == 0
    }

    fn match_str(&self, s: &str) -> bool {
        self.remaining_ante_slice().ends_with(s)
    }

    fn match_start_anchor(&self) -> bool {
        self.ante_cursor() == 0
    }

    fn match_end_anchor(&self) -> bool {
        self.ante_cursor() == self.rep.content.len()
    }

    fn consume(&mut self, len: usize) -> bool {
        if len <= self.ante_cursor() {
            assert!(self
                .remaining_ante_slice()
                .is_char_boundary(self.ante_cursor() - len));
            // SAFETY: `len` is guaranteed to be a valid UTF-8 length reverse-starting at `ante_cursor()`.
            self.ante_match_len += len;
            true
        } else {
            false
        }
    }

    fn next_char(&self) -> Option<char> {
        self.remaining_ante_slice().chars().next_back()
    }
}

mod sealed {
    pub(crate) trait Sealed {}
    impl Sealed for super::Forward {}
    impl Sealed for super::Reverse {}
}

/// A forward match.
///
/// For example, forward-matching `"aft"` on the input `"previous|after"`, with the cursor at the `'|'`,
/// would return a match, forward-matching `"fter"` would not.
pub(super) struct Forward;
/// A reverse match.
///
/// For example, reverse-matching `"ious"` on the input `"previous|after"`, with the cursor at the `'|'`,
/// would return a match, reverse-matching `"prev"` would not.
pub(super) struct Reverse;
/// The direction a match can be applied. Used in [`Utf8Matcher`].
///
/// See [`Forward`] and [`Reverse`] for implementors.
///
/// <div class="stab unstable">
/// 🚫 This trait is sealed; it cannot be implemented by user code. If an API requests an item that implements this
/// trait, please consider using a type from the implementors listed below.
/// </div>
pub(super) trait MatchDirection: sealed::Sealed {}
impl MatchDirection for Forward {}
impl MatchDirection for Reverse {}

/// Matching functionality on strings. Matching can be done in forward or reverse directions, see [`MatchDirection`].
///
/// The used indices in method parameters are all compatible with each other.
// Thought: I don't think this needs to be called *Utf8* matcher, maybe just Matcheable
pub(super) trait Utf8Matcher<D: MatchDirection>: Debug {
    fn cursor(&self) -> usize;

    fn str_range(&self, range: Range<usize>) -> Option<&str>;

    fn is_empty(&self) -> bool;
    fn match_str(&self, s: &str) -> bool;

    fn match_and_consume_str(&mut self, s: &str) -> bool {
        if self.match_str(s) {
            self.consume(s.len())
        } else {
            false
        }
    }

    fn match_and_consume_char(&mut self, c: char) -> bool {
        self.match_and_consume_str(c.encode_utf8(&mut [0; 4]))
    }

    fn match_start_anchor(&self) -> bool;
    fn match_end_anchor(&self) -> bool;
    fn consume(&mut self, len: usize) -> bool;
    fn next_char(&self) -> Option<char>;
}

/// The match length of the three match portions of a conversion rule.
#[derive(Debug, Clone, Copy)]
struct MatchLengths {
    ante: usize,
    key: usize,
    post: usize,
}

// TODO(#3957) about complexity: in the worst case, we need to move the full `end_len` tail for every
//  push. A good size_hint avoids this.
//  one fix could be to preemptively move the tail to the end of the buffer whenever the capacity
//  changes. Should give us the same benefits as an exponential allocation strategy.

// TODO(#3957): implementing this with a Rope/Cord data structure might be interesting. Decide if necessary

/// This provides (append-only) replacement APIs for a [`Replaceable`].
///
/// It can only be constructed with a complete match using a [`RepMatcher`].
///
/// A main goal of this type is to avoid unnecessary allocations when applying a rule replacement.
/// For this, it keeps track of the range of the `Replaceable`'s content that we are replacing
/// using `start` and `end_len`,
/// and lazily starts replacing this range from the left, in a grow-only fashion. That is,
/// there might be a suffix in this replace range that is invalid UTF-8.
///
/// A noteworthy feature of `Insertable`'s are getting back a *valid* sub-`Replaceable` for
/// full-blown transliteration of the replacement. This means that despite the grow-only
/// nature of this Insertable, we can do arbitrary replacements.
///
/// Furthermore, an `Insertable` takes care of updating its parent's cursor.
///
/// # Safety
/// While we hold onto the associated `Replaceable`, we may temporarily break its invariants.
/// As such, we cannot use any methods on `Replaceable` itself that rely on its invariants.
///
/// Specifically, we temporarily change the validity of `_rep.content`. During a replacement
/// with this `Insertable`, the following invariants hold:
/// - start is a valid UTF-8 index into `_rep.content` that signifies the start of the range we are replacing.
///   It may not be changed.
/// - `end_len` is a valid UTF-8 suffix length into `_rep.content` that signifies the suffix of
///   the `Replaceable`'s internal text that we are *not* replacing.
/// - `curr` is the growing cursor into the range we are replacing. It is relative to `_rep.content`,
///   i.e., `_rep.content[start..curr]` is the replaced text already. That range must be valid
///   UTF-8. We guarantee this by only exposing UTF-8 APIs (pushing `&str`s and `char`s, and
///   creating a sub-`Replaceable` that guarantees UTF-8 through its invariants).
/// - `_rep.content` has valid UTF-8 in the non-replaced portions as defined by `start` and
///   `end_len`.
///
/// In particular, these mean that the range of the `Replaceable`'s internal text that we have
/// *not yet replaced*, is allowed to be invalid UTF-8.
///
/// We fix the `Replaceable`'s invariants in the `Drop` impl using [`Insertable::cleanup`].
pub(crate) struct Insertable<'a, 'b> {
    // Replaceable's invariants may temporarily be broken while this Insertable is alive
    _rep: &'b mut Replaceable<'a>,
    start: usize,
    end_len: usize,
    curr: usize,
    match_lens: MatchLengths,
    cursor_offset: CursorOffset,
}

impl<'a, 'b> Insertable<'a, 'b> {
    fn from_matcher(matcher: RepMatcher<'a, 'b, true>) -> Insertable<'a, 'b> {
        let match_lens = matcher.match_lens();
        // SAFETY: RepMatcher does not modify the `Replaceable`.
        // This is the start of the range we want to replace.
        let start_idx = matcher.rep.cursor;
        // whatever is not matched *by the key* is unaffected by this Insertable
        // SAFETY: match_lens is guaranteed to have been created for
        // *this specific state of the Replaceable*, so these are valid lengths
        // according to RepMatcher's guarantees.
        let end_idx = start_idx + match_lens.key;
        let end_len = matcher.rep.content.len() - end_idx;

        Insertable {
            _rep: matcher.rep,
            start: start_idx,
            end_len,
            curr: start_idx,
            match_lens,
            cursor_offset: CursorOffset::Default,
        }
    }

    /// If we know the size of the replacement, we can pre-emptively grow the replacement range.
    /// This avoids many moves of the tail (defined by `end_len`) in case of repeated pushes on
    /// an empty replacement range.
    pub(crate) fn apply_size_hint(&mut self, size: usize) {
        let free_bytes = self.free_range().len();
        if free_bytes < size {
            self._rep.content.splice(
                self.end()..self.end(),
                core::iter::repeat_n(0, size - free_bytes),
            );
        }
    }

    /// Push a `char` on the replacement.
    pub(crate) fn push(&mut self, c: char) {
        let mut buf = [0; 4];
        let c_utf8 = c.encode_utf8(&mut buf);
        self.push_str(c_utf8);
        debug_assert!(self.curr <= self.end());
    }

    /// Push a `&str` on the replacement.
    pub(crate) fn push_str(&mut self, s: &str) {
        // SAFETY: s is valid UTF-8 by type
        unsafe { self.push_utf8(s.as_bytes()) };
        debug_assert!(self.curr <= self.end());
    }

    /// # Safety
    /// The caller must ensure that `code_units` is valid UTF-8.
    unsafe fn push_utf8(&mut self, code_units: &[u8]) {
        if self.free_range().len() >= code_units.len() {
            // SAFETY: The caller guarantees these are valid UTF-8
            self._rep.content[self.curr..self.curr + code_units.len()].copy_from_slice(code_units);
            self.curr += code_units.len();
            return;
        }
        // eprintln!("WARNING: free space not sufficient for Insertable::push_bytes");

        // SAFETY: The caller guarantees these are valid UTF-8
        self._rep
            .content
            .splice(self.free_range(), code_units.iter().copied());
        // SAFETY: The free range did not have enough space. The above splice replaces the completey
        // remaining free range with the replacement, so there are no more un-replaced bytes left
        // in the replacement range.
        self.curr = self.end();
    }

    pub(crate) fn curr_replacement_len(&self) -> usize {
        self.curr - self.start
    }

    pub(crate) fn curr_replacement(&self) -> &str {
        // SAFETY: the invariant states that this part of the content is valid UTF-8
        unsafe { core::str::from_utf8_unchecked(&self._rep.content[self.start..self.curr]) }
    }

    /// Will set the cursor to the current position of the replacement.
    pub(super) fn set_offset_to_here(&mut self) {
        self.cursor_offset = CursorOffset::Byte(self.curr_replacement_len());
    }

    /// Will set the cursor to `count` bytes after the end of the replacement, assuming
    /// the post context match is long enough.
    pub(super) fn set_offset_to_chars_off_end(&mut self, count: u16) {
        self.cursor_offset = CursorOffset::CharsOffEnd(count);
    }

    /// Will set the cursor to `count` bytes before the start of the replacement, assuming
    /// the ante context match is long enough.
    pub(super) fn set_offset_to_chars_off_start(&mut self, count: u16) {
        self.cursor_offset = CursorOffset::CharsOffStart(count);
    }

    /// Finishes the current replacement by applying the stored offset and calling
    /// `make_contiguous`. This is called automatically by the `Drop` impl.
    fn cleanup(&mut self) {
        self.make_contiguous();

        // SAFETY: make_contiguous ensures that `content` is contiguous, valid UTF-8 again, thus
        // we can call methods of Replaceable again.
        let rep = &self._rep;

        // SAFETY: this guaranteed to be the index of the first byte of the replacement
        let base_cursor = self.start;
        let replacement_len = self.curr_replacement_len();

        let cursor = match self.cursor_offset {
            CursorOffset::Default => {
                // SAFETY: replacement_len is a valid UTF-8 length after cursor.
                base_cursor + replacement_len
            }
            CursorOffset::Byte(offset) => {
                // SAFETY: CursorOffset::Byte is only constructed by passing a UTF-8 prefix-len of
                // the replacement, which is anchored at the cursor.
                base_cursor + offset
            }
            CursorOffset::CharsOffEnd(count) => {
                // they key has already been replaced, so the post match starts `replacement_len`
                // bytes after the cursor
                let post_start = base_cursor + replacement_len;
                // the replacement d oes not affect the post match length
                let post_end = post_start + self.match_lens.post;
                let matched_post = &rep.as_str()[post_start..post_end];
                // compute byte-length of `count` chars in post
                let post_offset_len = matched_post
                    .chars()
                    .take(count as usize)
                    .map(char::len_utf8)
                    .sum::<usize>();

                // SAFETY: this is a sum of valid UTF-8 lengths, so it is a valid UTF-8 index
                let computed_cursor = base_cursor + replacement_len + post_offset_len;

                // SAFETY: the returned range is guaranteed to be valid
                let max_cursor = rep.allowed_range().end;

                // we are not allowed to move the cursor into unmodifiable territory
                max_cursor.min(computed_cursor)
            }
            CursorOffset::CharsOffStart(count) => {
                let ante = &rep.as_str()[..base_cursor];
                // restricting to the matched substr ensures no overflow of the cursor past the
                // contexts, which is good
                let matched_ante = &ante[(ante.len() - self.match_lens.ante)..];
                // compute byte-length of `count` chars in ante (from the right)
                let ante_len = matched_ante
                    .chars()
                    .rev()
                    .take(count as usize)
                    .map(char::len_utf8)
                    .sum::<usize>();

                // not underflowing because ante_len is at most the cursor, because the cursor is
                // right after where the ante context ends.
                // SAFETY: ante_len is a valid UTF-8 substr length preceding the cursor, and the
                // cursor is also a valid index.
                let computed_cursor = base_cursor - ante_len;

                // SAFETY: the returned range is guaranteed to be valid
                let min_cursor = rep.allowed_range().start;

                // we are not allowed to move the cursor into unmodifiable territory
                min_cursor.max(computed_cursor)
            }
        };
        // SAFETY: all cases guarantee a valid UTF-8 index into the visible slice
        unsafe { self._rep.set_cursor(cursor) };
    }

    /// Gets rid of the unreplaced range remaining in this `Insertable`.
    ///
    /// A consequence of this is that `_rep.content` is now completely UTF-8 valid again.
    fn make_contiguous(&mut self) {
        // need to move the tail of the Vec to fill the remainder of the free range
        self._rep
            .content
            .splice(self.free_range(), core::iter::empty());
    }

    fn free_range(&self) -> Range<usize> {
        // eprintln!("free_range: curr: {}, end: {}", self.curr, self.end());
        debug_assert!(self.curr <= self.end());
        self.curr..self.end()
    }

    fn end(&self) -> usize {
        self._rep.content.len() - self.end_len
    }

    /// Use this if you want to get a sub-`Replaceable` for applying a full-blown transliteration to
    /// a range of `self`'s already replaced text.
    ///
    /// # Example
    /// ```ignore
    /// let mut ins = my_insertable();
    /// ins.push_str("hello ");
    /// let mut adpt = ins.start_replaceable_adapter();
    /// adpt.child_for_range().push_str("world");
    /// let mut rep = adpt.as_replaceable();
    /// // transliterates "world" and inserts it in the original `Insertable`.
    /// transliterate_with_rep(rep);
    /// ```
    pub(super) fn start_replaceable_adapter(
        &mut self,
    ) -> InsertableToReplaceableAdapter<'a, '_, impl FnMut(usize) + '_> {
        let range_start = self.curr;
        let child_insertable = Insertable {
            _rep: self._rep,
            start: self.curr,
            end_len: self.end_len,
            curr: self.curr,
            match_lens: self.match_lens,
            cursor_offset: CursorOffset::Default,
        };
        // only `curr` and `cursor_offset` may be modified by the child, so we need to get them back
        // once the child is dropped. however, replaceable_adapter is only used for function call
        // arguments, which may not contain cursors (and if they do, it's GIGO), so we don't need
        // to worry about the cursor_offset.
        let on_drop = |new_curr: usize| {
            // SAFETY: new_curr will be a later `curr` by `child_insertable`, and due to the grow-only
            // nature of curr and the invariants of `Insertable`, this new_curr must be valid for `self` too.
            self.curr = new_curr;
        };
        InsertableToReplaceableAdapter {
            child: ManuallyDrop::new(child_insertable),
            range_start,
            on_drop,
        }
    }
}

impl Drop for Insertable<'_, '_> {
    fn drop(&mut self) {
        self.cleanup();
    }
}

impl Debug for Insertable<'_, '_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> core::fmt::Result {
        write!(f, "{}|{}", self.curr_replacement(), self.free_range().len())
    }
}

/// See [`Insertable::start_replaceable_adapter`].
pub(super) struct InsertableToReplaceableAdapter<'a, 'b, F>
where
    F: FnMut(usize),
{
    // this is ManuallyDrop because we don't want the child's cleanup function to run.
    child: ManuallyDrop<Insertable<'a, 'b>>,
    range_start: usize,
    on_drop: F,
}

impl<F> InsertableToReplaceableAdapter<'_, '_, F>
where
    F: FnMut(usize),
{
    /// Returns a type that allows getting a `Replaceable` from it. The replaceable will
    /// transliterate everything since `self` was created with [`Insertable::start_replaceable_adapter`].
    pub(super) fn as_replaceable(&mut self) -> InsertableGuard<'_, impl FnMut(&[u8]) + '_> {
        // Thought: we don't need to make the Insertable contiguous because the visible length hides
        //  the invalid UTF-8 tail. However, we do not gain anything from that empty buffer at the
        //  moment, because the child Replaceable's Insertable will not know about it. can we
        //  somehow give it that information?
        //  Until that happens, calling make_contiguous has the benefit of not re-allocating the
        //  backing vec when there was still free space available in the parent Insertable.
        self.child.make_contiguous();
        let range_end = self.child.curr;
        let visible_range = self.range_start..range_end;

        let child = self.child.deref_mut();
        let hidden_len = child._rep.content.len() - visible_range.len();
        let content = &mut child._rep.content;
        let modifiable_content = content.tighten(visible_range);

        // SAFETY: visible_range is always the range from one valid content-based UTF-8 index of the
        // replacement to a subsequent valid UTF-8 content-index of the replacement.
        let rep = unsafe { Replaceable::from_hide(modifiable_content) };

        // `move` hack - don't move these into closure
        let child_curr = &mut child.curr;
        let child_end_len = &child.end_len;
        // will be called when the `Replaceable` is done.
        let on_drop = move |new_content: &[u8]| {
            // 1. child's content is contiguous, so we are inserting at the very end.
            // 2. we need hidden_len as well because `child.curr` is relative to `child.content`,
            //    not `new_content`, which is only the visible portion of the `rep`'s content.
            // We are updating the child's curr to point to the very end of the replaceable range.
            *child_curr = new_content.len() + hidden_len - child_end_len;
        };
        InsertableGuard::new(rep, on_drop)
    }
}

impl<F> Drop for InsertableToReplaceableAdapter<'_, '_, F>
where
    F: FnMut(usize),
{
    fn drop(&mut self) {
        (self.on_drop)(self.child.curr);
    }
}

impl<'a, 'b, F> Deref for InsertableToReplaceableAdapter<'a, 'b, F>
where
    F: FnMut(usize),
{
    type Target = Insertable<'a, 'b>;
    fn deref(&self) -> &Self::Target {
        self.child.deref()
    }
}

impl<F> DerefMut for InsertableToReplaceableAdapter<'_, '_, F>
where
    F: FnMut(usize),
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.child.deref_mut()
    }
}

pub(super) struct InsertableGuard<'a, F>
where
    F: FnMut(&[u8]),
{
    rep: Replaceable<'a>,
    on_drop: F,
}

impl<'a, F> InsertableGuard<'a, F>
where
    F: FnMut(&[u8]),
{
    fn new(rep: Replaceable<'a>, on_drop: F) -> Self {
        Self { rep, on_drop }
    }

    pub(crate) fn child(&mut self) -> Replaceable<'_> {
        self.rep.child()
    }
}

impl<F> Drop for InsertableGuard<'_, F>
where
    F: FnMut(&[u8]),
{
    fn drop(&mut self) {
        (self.on_drop)(&self.rep.content);
    }
}

/// Stores the kinds of cursor offsets that a replacement can produce.
#[derive(Debug, Clone, Copy, Default)]
enum CursorOffset {
    /// The default offset, which just puts the cursor at the end of the replacement.
    #[default]
    Default,
    /// A byte offset into the replacement string that is ready to use.
    Byte(usize),
    /// A `char`-based offset for after the replacement string.
    CharsOffEnd(u16),
    /// A `char`-based offset for before the replacement string.
    CharsOffStart(u16),
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic(expected = "valid UTF-8")]
    fn test_into_string_rejects_invalid_utf8() {
        // Simulate what would happen if a panic during transliteration
        // left the buffer with invalid UTF-8.
        let buffer = TransliteratorBuffer(vec![0xFF, 0xFE, 0xFD]);
        // With checked conversion: panics cleanly.
        // With unchecked conversion: silently produces an invalid String (UB).
        let _ = buffer.into_string();
    }
}
