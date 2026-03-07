// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{config::MAX_INLINE, SmartString, SmartStringMode};
use std::{
    cmp::Ordering,
    fmt::Debug,
    iter::FromIterator,
    ops::{Index, Range, RangeFrom, RangeFull, RangeInclusive, RangeTo, RangeToInclusive},
    panic::{catch_unwind, set_hook, take_hook, AssertUnwindSafe},
};

#[cfg(not(test))]
use arbitrary::Arbitrary;
#[cfg(test)]
use proptest::proptest;
#[cfg(test)]
use proptest_derive::Arbitrary;

pub fn assert_panic<A, F>(f: F)
where
    F: FnOnce() -> A,
{
    let old_hook = take_hook();
    set_hook(Box::new(|_| {}));
    let result = catch_unwind(AssertUnwindSafe(f));
    set_hook(old_hook);
    assert!(
        result.is_err(),
        "action that should have panicked didn't panic"
    );
}

#[derive(Arbitrary, Debug, Clone)]
pub enum Constructor {
    New,
    FromString(String),
    FromStringSlice(String),
    FromChars(Vec<char>),
}

impl Constructor {
    pub fn construct<Mode: SmartStringMode>(self) -> (String, SmartString<Mode>) {
        match self {
            Self::New => (String::new(), SmartString::new()),
            Self::FromString(string) => (string.clone(), SmartString::from(string)),
            Self::FromStringSlice(string) => (string.clone(), SmartString::from(string.as_str())),
            Self::FromChars(chars) => (
                String::from_iter(chars.clone()),
                SmartString::from_iter(chars),
            ),
        }
    }
}

#[derive(Arbitrary, Debug, Clone)]
pub enum TestBounds {
    Range(usize, usize),
    From(usize),
    To(usize),
    Full,
    Inclusive(usize, usize),
    ToInclusive(usize),
}

impl TestBounds {
    fn should_panic(&self, control: &str) -> bool {
        let len = control.len();
        match self {
            Self::Range(start, end)
                if start > end
                    || start > &len
                    || end > &len
                    || !control.is_char_boundary(*start)
                    || !control.is_char_boundary(*end) =>
            {
                true
            }
            Self::From(start) if start > &len || !control.is_char_boundary(*start) => true,
            Self::To(end) if end > &len || !control.is_char_boundary(*end) => true,
            Self::Inclusive(start, end)
                if *end == usize::max_value()
                    || *start > (end + 1)
                    || start > &len
                    || end > &len
                    || !control.is_char_boundary(*start)
                    || !control.is_char_boundary(*end + 1) =>
            {
                true
            }
            Self::ToInclusive(end) if end > &len || !control.is_char_boundary(*end + 1) => true,
            _ => false,
        }
    }

    fn assert_range<A, B>(&self, control: &A, subject: &B)
    where
        A: Index<Range<usize>>,
        B: Index<Range<usize>>,
        A: Index<RangeFrom<usize>>,
        B: Index<RangeFrom<usize>>,
        A: Index<RangeTo<usize>>,
        B: Index<RangeTo<usize>>,
        A: Index<RangeFull>,
        B: Index<RangeFull>,
        A: Index<RangeInclusive<usize>>,
        B: Index<RangeInclusive<usize>>,
        A: Index<RangeToInclusive<usize>>,
        B: Index<RangeToInclusive<usize>>,
        <A as Index<Range<usize>>>::Output: PartialEq<<B as Index<Range<usize>>>::Output> + Debug,
        <B as Index<Range<usize>>>::Output: Debug,
        <A as Index<RangeFrom<usize>>>::Output:
            PartialEq<<B as Index<RangeFrom<usize>>>::Output> + Debug,
        <B as Index<RangeFrom<usize>>>::Output: Debug,
        <A as Index<RangeTo<usize>>>::Output:
            PartialEq<<B as Index<RangeTo<usize>>>::Output> + Debug,
        <B as Index<RangeTo<usize>>>::Output: Debug,
        <A as Index<RangeFull>>::Output: PartialEq<<B as Index<RangeFull>>::Output> + Debug,
        <B as Index<RangeFull>>::Output: Debug,
        <A as Index<RangeInclusive<usize>>>::Output:
            PartialEq<<B as Index<RangeInclusive<usize>>>::Output> + Debug,
        <B as Index<RangeInclusive<usize>>>::Output: Debug,
        <A as Index<RangeToInclusive<usize>>>::Output:
            PartialEq<<B as Index<RangeToInclusive<usize>>>::Output> + Debug,
        <B as Index<RangeToInclusive<usize>>>::Output: Debug,
    {
        match self {
            Self::Range(start, end) => assert_eq!(control[*start..*end], subject[*start..*end]),
            Self::From(start) => assert_eq!(control[*start..], subject[*start..]),
            Self::To(end) => assert_eq!(control[..*end], subject[..*end]),
            Self::Full => assert_eq!(control[..], subject[..]),
            Self::Inclusive(start, end) => {
                assert_eq!(control[*start..=*end], subject[*start..=*end])
            }
            Self::ToInclusive(end) => assert_eq!(control[..=*end], subject[..=*end]),
        }
    }
}

#[derive(Arbitrary, Debug, Clone)]
pub enum Action {
    Slice(TestBounds),
    Push(char),
    PushStr(String),
    Truncate(usize),
    Pop,
    Remove(usize),
    Insert(usize, char),
    InsertStr(usize, String),
    SplitOff(usize),
    Clear,
    IntoString,
    Retain(String),
    Drain(TestBounds),
    ReplaceRange(TestBounds, String),
}

impl Action {
    pub fn perform<Mode: SmartStringMode>(
        self,
        control: &mut String,
        subject: &mut SmartString<Mode>,
    ) {
        match self {
            Self::Slice(range) => {
                if range.should_panic(control) {
                    assert_panic(|| range.assert_range(control, subject))
                } else {
                    range.assert_range(control, subject);
                }
            }
            Self::Push(ch) => {
                control.push(ch);
                subject.push(ch);
            }
            Self::PushStr(ref string) => {
                control.push_str(string);
                subject.push_str(string);
            }
            Self::Truncate(index) => {
                if index <= control.len() && !control.is_char_boundary(index) {
                    assert_panic(|| control.truncate(index));
                    assert_panic(|| subject.truncate(index));
                } else {
                    control.truncate(index);
                    subject.truncate(index);
                }
            }
            Self::Pop => {
                assert_eq!(control.pop(), subject.pop());
            }
            Self::Remove(index) => {
                if index >= control.len() || !control.is_char_boundary(index) {
                    assert_panic(|| control.remove(index));
                    assert_panic(|| subject.remove(index));
                } else {
                    assert_eq!(control.remove(index), subject.remove(index));
                }
            }
            Self::Insert(index, ch) => {
                if index > control.len() || !control.is_char_boundary(index) {
                    assert_panic(|| control.insert(index, ch));
                    assert_panic(|| subject.insert(index, ch));
                } else {
                    control.insert(index, ch);
                    subject.insert(index, ch);
                }
            }
            Self::InsertStr(index, ref string) => {
                if index > control.len() || !control.is_char_boundary(index) {
                    assert_panic(|| control.insert_str(index, string));
                    assert_panic(|| subject.insert_str(index, string));
                } else {
                    control.insert_str(index, string);
                    subject.insert_str(index, string);
                }
            }
            Self::SplitOff(index) => {
                if !control.is_char_boundary(index) {
                    assert_panic(|| control.split_off(index));
                    assert_panic(|| subject.split_off(index));
                } else {
                    assert_eq!(control.split_off(index), subject.split_off(index));
                }
            }
            Self::Clear => {
                control.clear();
                subject.clear();
            }
            Self::IntoString => {
                assert_eq!(control, &Into::<String>::into(subject.clone()));
            }
            Self::Retain(filter) => {
                let f = |ch| filter.contains(ch);
                control.retain(f);
                subject.retain(f);
            }
            Self::Drain(range) => {
                // FIXME: ignoring inclusive bounds at usize::max_value(), pending https://github.com/rust-lang/rust/issues/72237
                match range {
                    TestBounds::Inclusive(_, end) if end == usize::max_value() => return,
                    TestBounds::ToInclusive(end) if end == usize::max_value() => return,
                    _ => {}
                }
                if range.should_panic(control) {
                    assert_panic(|| match range {
                        TestBounds::Range(start, end) => {
                            (control.drain(start..end), subject.drain(start..end))
                        }
                        TestBounds::From(start) => (control.drain(start..), subject.drain(start..)),
                        TestBounds::To(end) => (control.drain(..end), subject.drain(..end)),
                        TestBounds::Full => (control.drain(..), subject.drain(..)),
                        TestBounds::Inclusive(start, end) => {
                            (control.drain(start..=end), subject.drain(start..=end))
                        }
                        TestBounds::ToInclusive(end) => {
                            (control.drain(..=end), subject.drain(..=end))
                        }
                    })
                } else {
                    let (control_iter, subject_iter) = match range {
                        TestBounds::Range(start, end) => {
                            (control.drain(start..end), subject.drain(start..end))
                        }
                        TestBounds::From(start) => (control.drain(start..), subject.drain(start..)),
                        TestBounds::To(end) => (control.drain(..end), subject.drain(..end)),
                        TestBounds::Full => (control.drain(..), subject.drain(..)),
                        TestBounds::Inclusive(start, end) => {
                            (control.drain(start..=end), subject.drain(start..=end))
                        }
                        TestBounds::ToInclusive(end) => {
                            (control.drain(..=end), subject.drain(..=end))
                        }
                    };
                    let control_result: String = control_iter.collect();
                    let subject_result: String = subject_iter.collect();
                    assert_eq!(control_result, subject_result);
                }
            }
            Self::ReplaceRange(range, string) => {
                // FIXME: ignoring inclusive bounds at usize::max_value(), pending https://github.com/rust-lang/rust/issues/72237
                match range {
                    TestBounds::Inclusive(_, end) if end == usize::max_value() => return,
                    TestBounds::ToInclusive(end) if end == usize::max_value() => return,
                    _ => {}
                }
                if range.should_panic(control) {
                    assert_panic(|| match range {
                        TestBounds::Range(start, end) => {
                            control.replace_range(start..end, &string);
                            subject.replace_range(start..end, &string);
                        }
                        TestBounds::From(start) => {
                            control.replace_range(start.., &string);
                            subject.replace_range(start.., &string);
                        }
                        TestBounds::To(end) => {
                            control.replace_range(..end, &string);
                            subject.replace_range(..end, &string);
                        }
                        TestBounds::Full => {
                            control.replace_range(.., &string);
                            subject.replace_range(.., &string);
                        }
                        TestBounds::Inclusive(start, end) => {
                            control.replace_range(start..=end, &string);
                            subject.replace_range(start..=end, &string);
                        }
                        TestBounds::ToInclusive(end) => {
                            control.replace_range(..=end, &string);
                            subject.replace_range(..=end, &string);
                        }
                    })
                } else {
                    match range {
                        TestBounds::Range(start, end) => {
                            control.replace_range(start..end, &string);
                            subject.replace_range(start..end, &string);
                        }
                        TestBounds::From(start) => {
                            control.replace_range(start.., &string);
                            subject.replace_range(start.., &string);
                        }
                        TestBounds::To(end) => {
                            control.replace_range(..end, &string);
                            subject.replace_range(..end, &string);
                        }
                        TestBounds::Full => {
                            control.replace_range(.., &string);
                            subject.replace_range(.., &string);
                        }
                        TestBounds::Inclusive(start, end) => {
                            control.replace_range(start..=end, &string);
                            subject.replace_range(start..=end, &string);
                        }
                        TestBounds::ToInclusive(end) => {
                            control.replace_range(..=end, &string);
                            subject.replace_range(..=end, &string);
                        }
                    }
                }
            }
        }
    }
}

fn assert_invariants<Mode: SmartStringMode>(control: &str, subject: &SmartString<Mode>) {
    assert_eq!(control.len(), subject.len());
    assert_eq!(control, subject.as_str());
    if Mode::DEALLOC {
        assert_eq!(
            subject.is_inline(),
            subject.len() <= MAX_INLINE,
            "len {} should be inline (MAX_INLINE = {}) but was boxed",
            subject.len(),
            MAX_INLINE
        );
    }
    assert_eq!(
        control.partial_cmp("ordering test"),
        subject.partial_cmp("ordering test")
    );
    let control_smart: SmartString<Mode> = control.into();
    assert_eq!(Ordering::Equal, subject.cmp(&control_smart));
}

pub fn test_everything<Mode: SmartStringMode>(constructor: Constructor, actions: Vec<Action>) {
    let (mut control, mut subject): (_, SmartString<Mode>) = constructor.construct();
    assert_invariants(&control, &subject);
    for action in actions {
        action.perform(&mut control, &mut subject);
        assert_invariants(&control, &subject);
    }
}

pub fn test_ordering<Mode: SmartStringMode>(left: String, right: String) {
    let smart_left = SmartString::<Mode>::from(&left);
    let smart_right = SmartString::<Mode>::from(&right);
    assert_eq!(left.cmp(&right), smart_left.cmp(&smart_right));
}

#[cfg(test)]
mod tests {
    use super::{Action::*, Constructor::*, TestBounds::*, *};

    use crate::{Compact, LazyCompact};

    proptest! {
        #[test]
        fn proptest_everything_compact(constructor: Constructor, actions: Vec<Action>) {
            test_everything::<Compact>(constructor, actions);
        }

        #[test]
        fn proptest_everything_lazycompact(constructor: Constructor, actions: Vec<Action>) {
            test_everything::<LazyCompact>(constructor, actions);
        }

        #[test]
        fn proptest_ordering_compact(left: String, right: String) {
            test_ordering::<Compact>(left,right)
        }

        #[test]
        fn proptest_ordering_lazycompact(left: String, right: String) {
            test_ordering::<LazyCompact>(left,right)
        }

        #[test]
        fn proptest_eq(left: String, right: String) {
            fn test_eq<Mode: SmartStringMode>(left: &str, right: &str) {
                let smart_left = SmartString::<Mode>::from(left);
                let smart_right = SmartString::<Mode>::from(right);
                assert_eq!(smart_left, left);
                assert_eq!(smart_left, *left);
                assert_eq!(smart_left, left.to_string());
                assert_eq!(smart_left == smart_right, left == right);
                assert_eq!(left, smart_left);
                assert_eq!(*left, smart_left);
                assert_eq!(left.to_string(), smart_left);
            }
            test_eq::<Compact>(&left, &right);
            test_eq::<LazyCompact>(&left, &right);
        }
    }

    #[test]
    fn must_panic_on_insert_outside_char_boundary() {
        test_everything::<Compact>(
            Constructor::FromString("a0 A୦a\u{2de0}0 🌀Aa".to_string()),
            vec![
                Action::Push(' '),
                Action::Push('¡'),
                Action::Pop,
                Action::Pop,
                Action::Push('¡'),
                Action::Pop,
                Action::Push('𐀀'),
                Action::Push('\u{e000}'),
                Action::Pop,
                Action::Insert(14, 'A'),
            ],
        );
    }

    #[test]
    fn must_panic_on_out_of_bounds_range() {
        test_everything::<Compact>(
            Constructor::New,
            vec![Action::Slice(TestBounds::Range(0, usize::MAX - 1))],
        );
    }

    #[test]
    fn must_not_promote_before_insert_succeeds() {
        test_everything::<Compact>(
            Constructor::FromString("ኲΣ A𑒀a ®Σ a0🠀  aA®A".to_string()),
            vec![Action::Insert(21, ' ')],
        );
    }

    #[test]
    fn must_panic_on_slice_outside_char_boundary() {
        test_everything::<Compact>(
            Constructor::New,
            vec![Action::Push('Ь'), Action::Slice(TestBounds::ToInclusive(0))],
        )
    }

    #[test]
    fn dont_panic_when_inserting_a_string_at_exactly_inline_capacity() {
        let string: String = (0..MAX_INLINE).map(|_| '\u{0}').collect();
        test_everything::<Compact>(Constructor::New, vec![Action::InsertStr(0, string)])
    }

    #[test]
    #[should_panic]
    fn drain_bounds_integer_overflow_must_panic() {
        let mut string = SmartString::<Compact>::from("מ");
        string.drain(..=usize::max_value());
    }

    #[test]
    fn shouldnt_panic_on_inclusive_range_end_one_less_than_start() {
        test_everything::<Compact>(
            Constructor::FromString("\'\'\'\'\'[[[[[[[[[[[-[[[[[[[[[[[[[[[[[[[[[[".to_string()),
            vec![Action::Slice(TestBounds::Inclusive(1, 0))],
        )
    }

    #[test]
    fn drain_over_inline_boundary() {
        test_everything::<Compact>(
            FromString((0..24).map(|_| 'x').collect()),
            vec![Drain(Range(0, 1))],
        )
    }

    #[test]
    fn drain_wrapped_shouldnt_drop_twice() {
        test_everything::<Compact>(
            FromString((0..25).map(|_| 'x').collect()),
            vec![Drain(Range(0, 1))],
        )
    }

    #[test]
    fn fail() {
        let value = "fo\u{0}\u{0}\u{0}\u{8}\u{0}\u{0}\u{0}\u{0}____bbbbb_____bbbbbbbbb";
        let mut control = String::from(value);
        let mut string = SmartString::<Compact>::from(value);
        control.drain(..=0);
        string.drain(..=0);
        let control_smart: SmartString<Compact> = control.into();
        assert_eq!(control_smart, string);
        assert_eq!(Ordering::Equal, string.cmp(&control_smart));
    }

    #[test]
    fn dont_panic_on_removing_last_index_from_an_inline_string() {
        let mut s =
            SmartString::<Compact>::from("\u{323}\u{323}\u{323}ω\u{323}\u{323}\u{323}㌣\u{e323}㤘");
        s.remove(20);
    }

    #[test]
    fn check_alignment() {
        use crate::boxed::BoxedString;
        use crate::inline::InlineString;
        use crate::marker_byte::Discriminant;

        let inline = InlineString::new();
        let inline_ptr: *const InlineString = &inline;
        let boxed_ptr: *const BoxedString = inline_ptr.cast();
        #[allow(unsafe_code)]
        let discriminant =
            Discriminant::from_bit(BoxedString::check_alignment(unsafe { &*boxed_ptr }));
        assert_eq!(Discriminant::Inline, discriminant);

        let boxed = BoxedString::from_str(32, "welp");
        let discriminant = Discriminant::from_bit(BoxedString::check_alignment(&boxed));
        assert_eq!(Discriminant::Boxed, discriminant);

        let mut s = SmartString::<Compact>::new();
        assert_eq!(Discriminant::Inline, s.discriminant());
        let big_str = "1234567890123456789012345678901234567890";
        assert!(big_str.len() > MAX_INLINE);
        s.push_str(big_str);
        assert_eq!(Discriminant::Boxed, s.discriminant());
        s.clear();
        assert_eq!(Discriminant::Inline, s.discriminant());
    }

    #[test]
    fn from_string() {
        let std_s =
            String::from("I am a teapot short and stout; here is my handle, here is my snout");
        let smart_s: SmartString<LazyCompact> = std_s.clone().into();
        assert_eq!(std_s, smart_s);
        let unsmart_s: String = smart_s.clone().into();
        assert_eq!(smart_s, unsmart_s);
        assert_eq!(std_s, unsmart_s);
        // This test exists just to provoke a Miri problem when dropping a string created by SmartString::into::<String>() (#28)
    }
}
