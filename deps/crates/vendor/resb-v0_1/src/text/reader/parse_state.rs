// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use std::{
    cell::RefCell,
    ops::{Deref, Range, RangeFrom, RangeTo},
    rc::Rc,
    str::{CharIndices, Chars},
};

use indexmap::IndexSet;
use nom::{Compare, FindSubstring, InputIter, InputLength, InputTake, Slice, UnspecializedInput};

use crate::bundle::Key;

/// The `ParseState` struct contains the information required to manage the
/// parse at any given position in the text.
///
/// # Caveats
///
/// This struct will be cloned or split repeatedly throughout the parse and so
/// should not contain excess state. Code making use of the parse state should
/// ensure that only a few exist at a time.
#[derive(Clone, Debug)]
pub(in crate::text::reader) struct ParseState<'a> {
    /// The current position in the input, represented as a slice beginning at
    /// the next character to be read and ending at the end of input.
    input: &'a str,

    /// The keys present in the resource bundle in the order we encounter them
    /// during the parse.
    ///
    /// This isn't important to the in-memory representation of resource bundles
    /// but is a part of the binary writing algorithm in the C++ parser and so
    /// we keep track of it in order to keep strict compatibility in output
    /// files.
    ///
    /// This cell should only be mutated internally when:
    /// - Adding a key while parsing a table resource.
    /// - Taking the list of keys when the parse is completed.
    keys_in_discovery_order: Rc<RefCell<IndexSet<Key<'a>>>>,
}

impl<'a> ParseState<'a> {
    /// Makes a new parse state with an empty set of keys.
    pub fn new(input: &'a str) -> Self {
        Self {
            input,
            keys_in_discovery_order: Default::default(),
        }
    }

    /// Notes that we have encountered a key in the resource bundle. If we have
    /// not encountered it before, it is added to the set in insertion order.
    ///
    /// Returns a reference to the key.
    pub fn encounter_key(&self, key: Key<'a>) -> bool {
        self.keys_in_discovery_order.borrow_mut().insert(key)
    }

    /// Gets a slice beginning at the current input position.
    pub fn input(&self) -> &'a str {
        self.input
    }

    /// Takes the set of keys in order of discovery and replaces it with an
    /// empty set.
    pub fn take_keys(&self) -> IndexSet<Key<'a>> {
        self.keys_in_discovery_order.take()
    }
}

/// # nom Input Adapters
///
/// These trait implementations provide access to the internal input string for
/// functions originating in [`nom`].
impl Compare<&str> for ParseState<'_> {
    fn compare(&self, t: &str) -> nom::CompareResult {
        self.input.compare(t)
    }

    fn compare_no_case(&self, t: &str) -> nom::CompareResult {
        self.input.compare_no_case(t)
    }
}

impl Compare<&[u8]> for ParseState<'_> {
    fn compare(&self, t: &[u8]) -> nom::CompareResult {
        self.input.compare(t)
    }

    fn compare_no_case(&self, t: &[u8]) -> nom::CompareResult {
        self.input.compare_no_case(t)
    }
}

impl Deref for ParseState<'_> {
    type Target = str;

    fn deref(&self) -> &Self::Target {
        self.input
    }
}

impl FindSubstring<&str> for ParseState<'_> {
    fn find_substring(&self, substr: &str) -> Option<usize> {
        self.input.find_substring(substr)
    }
}

impl<'a> InputIter for ParseState<'a> {
    type Item = char;
    type Iter = CharIndices<'a>;
    type IterElem = Chars<'a>;

    fn iter_indices(&self) -> Self::Iter {
        self.input.char_indices()
    }

    fn iter_elements(&self) -> Self::IterElem {
        self.input.chars()
    }

    fn position<P>(&self, predicate: P) -> Option<usize>
    where
        P: Fn(Self::Item) -> bool,
    {
        self.input.position(predicate)
    }

    fn slice_index(&self, count: usize) -> Result<usize, nom::Needed> {
        self.input.slice_index(count)
    }
}

impl InputLength for ParseState<'_> {
    fn input_len(&self) -> usize {
        self.input.input_len()
    }
}

impl InputTake for ParseState<'_> {
    fn take(&self, count: usize) -> Self {
        ParseState {
            input: self.input.take(count),
            keys_in_discovery_order: self.keys_in_discovery_order.clone(),
        }
    }

    fn take_split(&self, count: usize) -> (Self, Self) {
        let (suffix, prefix) = self.input.take_split(count);
        (
            ParseState {
                input: suffix,
                keys_in_discovery_order: self.keys_in_discovery_order.clone(),
            },
            ParseState {
                input: prefix,
                keys_in_discovery_order: self.keys_in_discovery_order.clone(),
            },
        )
    }
}

impl Slice<Range<usize>> for ParseState<'_> {
    fn slice(&self, range: Range<usize>) -> Self {
        Self {
            input: self.input.slice(range),
            keys_in_discovery_order: self.keys_in_discovery_order.clone(),
        }
    }
}

impl Slice<RangeFrom<usize>> for ParseState<'_> {
    fn slice(&self, range: RangeFrom<usize>) -> Self {
        Self {
            input: self.input.slice(range),
            keys_in_discovery_order: self.keys_in_discovery_order.clone(),
        }
    }
}

impl Slice<RangeTo<usize>> for ParseState<'_> {
    fn slice(&self, range: RangeTo<usize>) -> Self {
        Self {
            input: self.input.slice(range),
            keys_in_discovery_order: self.keys_in_discovery_order.clone(),
        }
    }
}

impl UnspecializedInput for ParseState<'_> {}
