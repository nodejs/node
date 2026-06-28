// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module contains the logic for aggregating rules into groups.
//! Due to incompatible orders (see comment on `ReverseRuleGroupAggregator`), we need separate
//! aggregators for forward and reverse directions.
//!
//! These aggregators both accept source-order (!) `parse::Rule`s and aggregate them into the rule
//! groups that the data struct [`RuleBasedTransliterator`](icu::experimental::transliterate::provider::RuleBasedTransliterator)
//! semantically expects. A later step converts these into the actual zero-copy format.
//! They also transform the bidirectional `parse::Rule`s into a unidirectional version.

// note: the aggregators currently work in a one-`parse::Rule`-at-a-time fashion, but really, they
// could also receive the full `&[parse::Rule]` slice. with some careful index handling, some
// allocations could be avoided.

use super::*;
use alloc::borrow::Cow;
use alloc::collections::VecDeque;
use alloc::vec;

// parse::Rule::Conversion but unidirectional
#[derive(Debug, Clone)]
pub(crate) struct UniConversionRule<'p> {
    pub(crate) ante: &'p [parse::Element],
    pub(crate) key: &'p [parse::Element],
    pub(crate) post: &'p [parse::Element],
    pub(crate) replacement: &'p [parse::Element],
}

// transform + conversion rule groups for a single direction
pub(crate) type RuleGroups<'p> = Vec<(Vec<Cow<'p, parse::SingleId>>, Vec<UniConversionRule<'p>>)>;

// an intermediate enum for use in the aggregators
enum UniRule<'p> {
    Conversion(UniConversionRule<'p>),
    Transform(Cow<'p, parse::SingleId>),
}

#[derive(Debug, Clone)]
pub(crate) struct ForwardRuleGroupAggregator<'p> {
    current: ForwardRuleGroup<'p>,
    // the forward aggregator can use the final type directly, as source-order is equivalent to
    // forward-order.
    groups: RuleGroups<'p>,
    // the transform_group of a group pair appears first
    preceding_transform_group: Option<Vec<Cow<'p, parse::SingleId>>>,
}

impl<'p> ForwardRuleGroupAggregator<'p> {
    pub(crate) fn new() -> Self {
        Self {
            // this is a somewhat important first group.
            // we want &[(transform_group), (conversion_group)] in the end, and because we iterate
            // in source-order, the first element of that is a transform_group.
            current: ForwardRuleGroup::Transform(Vec::new()),
            groups: Vec::new(),
            preceding_transform_group: None,
        }
    }

    pub(crate) fn push(&mut self, rule: &'p parse::Rule) {
        match rule {
            parse::Rule::Conversion(source_half, dir, target_half) => {
                if !dir.permits(Direction::Forward) {
                    return;
                }

                let ante = &source_half.ante;
                let key = &source_half.key;
                let post = &source_half.post;
                let replacement = &target_half.key;

                let rule = UniConversionRule {
                    ante,
                    key,
                    post,
                    replacement,
                };

                let finished_group = self.current.push(UniRule::Conversion(rule));
                if let Some(finished_group) = finished_group {
                    self.push_rule_group(finished_group);
                }
            }
            parse::Rule::Transform(fwd, _) => {
                let finished_group = self.current.push(UniRule::Transform(Cow::Borrowed(fwd)));
                if let Some(finished_group) = finished_group {
                    self.push_rule_group(finished_group);
                }
            }
            parse::Rule::VariableDefinition(..) => {
                // variable definitions are handled in a previous step
            }
            parse::Rule::GlobalFilter(..) | parse::Rule::GlobalInverseFilter(..) => {
                // global filters are handled in a previous step
            }
        }
    }

    fn push_rule_group(&mut self, group: ForwardRuleGroup<'p>) {
        match group {
            ForwardRuleGroup::Transform(transform_group) => {
                // because ForwardRuleGroup returns a different kind of group every time,
                // the previous group must have been a conversion group which pushed the
                // finished group pair into self.groups.
                debug_assert!(self.preceding_transform_group.is_none());
                self.preceding_transform_group = Some(transform_group);
            }
            ForwardRuleGroup::Conversion(conversion_group) => {
                // unwrap is necessary if the first source-order rule group is a conversion group
                let associated_transform_group =
                    self.preceding_transform_group.take().unwrap_or_default();
                self.groups
                    .push((associated_transform_group, conversion_group));
            }
        }
    }

    pub(crate) fn finalize(mut self) -> RuleGroups<'p> {
        // push the current group
        // note: refactoring could get rid of clone
        self.push_rule_group(self.current.clone());
        // push any remaining group pairs
        if let Some(transform_group) = self.preceding_transform_group.take() {
            self.groups.push((transform_group, Vec::new()));
        }

        self.groups
    }
}

// Represents a non-empty rule group for the forward direction.
#[derive(Debug, Clone)]
enum ForwardRuleGroup<'p> {
    Conversion(Vec<UniConversionRule<'p>>),
    Transform(Vec<Cow<'p, parse::SingleId>>),
}

impl<'p> ForwardRuleGroup<'p> {
    fn new_conversion(rule: UniConversionRule<'p>) -> Self {
        Self::Conversion(vec![rule])
    }

    fn new_transform(rule: Cow<'p, parse::SingleId>) -> Self {
        Self::Transform(vec![rule])
    }

    // if the group is full return self, and push the rule into a new group
    fn push(&mut self, rule: UniRule<'p>) -> Option<Self> {
        // necessary reborrow for mem::replace
        match (&mut *self, rule) {
            (Self::Conversion(group), UniRule::Conversion(rule)) => {
                group.push(rule);
                None
            }
            (Self::Transform(group), UniRule::Transform(rule)) => {
                group.push(rule);
                None
            }
            (Self::Conversion(_), UniRule::Transform(new_rule)) => {
                Some(core::mem::replace(self, Self::new_transform(new_rule)))
            }
            (Self::Transform(_), UniRule::Conversion(new_rule)) => {
                Some(core::mem::replace(self, Self::new_conversion(new_rule)))
            }
        }
    }
}

// Rules will be pushed in source-order (i.e., forward order), which means we have to be careful
// in which order we aggregate them. Example: (T = transform rule, C = conversion rule)
// T1 T2 C1 C2 T3 C3 C4 T4 T5
// should be aggregated as
// (T5, T4), (C3, C4), (T3), (C1, C2), (T2, T1) (assuming all rules apply to the reverse direction)
// note in particular the discrepancy between the order of contiguous T's and contiguous C's:
// contiguous C's keep the source order, but contiguous T's are reversed. Also the overall order
// is reversed, of course.
//
// We do this by using VecDeque, push_front, and make_contiguous in the end.
#[derive(Debug, Clone)]
pub(crate) struct ReverseRuleGroupAggregator<'p> {
    current: ReverseRuleGroup<'p>,
    // VecDeque because we encounter groups in source-order, but we want to aggregate them in
    // reverse-order.
    groups: VecDeque<(Vec<Cow<'p, parse::SingleId>>, Vec<UniConversionRule<'p>>)>,
    // the conversion_group of a group pair appears first due to the reverse order
    preceding_conversion_group: Option<Vec<UniConversionRule<'p>>>,
}

impl<'p> ReverseRuleGroupAggregator<'p> {
    pub(crate) fn new() -> Self {
        Self {
            // this is a somewhat important first group.
            // we want &[(transform_group), (conversion_group)] in the end, and because we iterate
            // in opposite order, the last element of that slice is a conversion_group.
            current: ReverseRuleGroup::Conversion(Vec::new()),
            groups: VecDeque::new(),
            preceding_conversion_group: None,
        }
    }

    pub(crate) fn push(&mut self, rule: &'p parse::Rule) {
        match rule {
            parse::Rule::Conversion(target_half, dir, source_half) => {
                if !dir.permits(Direction::Reverse) {
                    return;
                }

                let ante = &source_half.ante;
                let key = &source_half.key;
                let post = &source_half.post;
                let replacement = &target_half.key;

                let rule = UniConversionRule {
                    ante,
                    key,
                    post,
                    replacement,
                };

                let finished_group = self.current.push(UniRule::Conversion(rule));
                if let Some(finished_group) = finished_group {
                    self.push_rule_group(finished_group);
                }
            }
            parse::Rule::Transform(fwd, rev) => {
                let rev = rev.clone().unwrap_or_else(|| fwd.clone().reverse());

                let finished_group = self.current.push(UniRule::Transform(Cow::Owned(rev)));
                if let Some(finished_group) = finished_group {
                    self.push_rule_group(finished_group);
                }
            }
            parse::Rule::VariableDefinition(..) => {
                // variable definitions are handled in a previous step
            }
            parse::Rule::GlobalFilter(..) | parse::Rule::GlobalInverseFilter(..) => {
                // global filters are handled in a previous step
            }
        }
    }

    fn push_rule_group(&mut self, group: ReverseRuleGroup<'p>) {
        match group {
            ReverseRuleGroup::Conversion(conv_group) => {
                // because ReverseRuleGroup returns a different kind of group every time,
                // the previous group must have been a transform group which pushed the
                // finished group pair into self.groups.
                debug_assert!(self.preceding_conversion_group.is_none());
                self.preceding_conversion_group = Some(conv_group);
            }
            ReverseRuleGroup::Transform(transform_group) => {
                // unwrap is necessary if the first source-order rule group is a transform group
                let associated_conv_group =
                    self.preceding_conversion_group.take().unwrap_or_default();
                let vec_transform_group = transform_group.into(); // non-allocating conversion
                self.groups
                    .push_front((vec_transform_group, associated_conv_group));
            }
        }
    }

    pub(crate) fn finalize(mut self) -> RuleGroups<'p> {
        // push the current group
        // note: refactoring could get rid of clone
        self.push_rule_group(self.current.clone());
        // push any remaining group pairs
        if let Some(conv_group) = self.preceding_conversion_group.take() {
            // a trailing conversion group in source order is the same as having a conversion
            // group as the first in-order group. we can just prepend an empty transform group.
            self.groups.push_front((Vec::new(), conv_group));
        }

        self.groups.into() // non-allocating conversion
    }
}

// Represents a non-empty rule group for the reverse direction.
#[derive(Debug, Clone)]
enum ReverseRuleGroup<'p> {
    // because contiguous C's are aggregated in source-order, we can just use a Vec
    Conversion(Vec<UniConversionRule<'p>>),
    // but contiguous T's are aggregated in reverse-order, so we need to use a VecDeque and push_front
    Transform(VecDeque<Cow<'p, parse::SingleId>>),
}

impl Default for ReverseRuleGroup<'_> {
    fn default() -> Self {
        Self::Conversion(Vec::new())
    }
}

impl<'p> ReverseRuleGroup<'p> {
    fn new_conversion(rule: UniConversionRule<'p>) -> Self {
        Self::Conversion(vec![rule])
    }

    fn new_transform(rule: Cow<'p, parse::SingleId>) -> Self {
        let mut group = VecDeque::new();
        group.push_front(rule);
        Self::Transform(group)
    }

    fn push(&mut self, rule: UniRule<'p>) -> Option<Self> {
        // necessary reborrow for mem::replace
        match (&mut *self, rule) {
            (Self::Conversion(group), UniRule::Conversion(rule)) => {
                group.push(rule);
                None
            }
            (Self::Transform(group), UniRule::Transform(rule)) => {
                // we receive rules via `push` in source-order, which is the opposite order we want,
                // so we push_front.
                group.push_front(rule);
                None
            }
            (Self::Conversion(_), UniRule::Transform(new_rule)) => {
                Some(core::mem::replace(self, Self::new_transform(new_rule)))
            }
            (Self::Transform(_), UniRule::Conversion(new_rule)) => {
                Some(core::mem::replace(self, Self::new_conversion(new_rule)))
            }
        }
    }
}
