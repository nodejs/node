// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! This module has three main functions. First, it validates many aspects of transliterators.
//! Second, it compiles them into the zero-copy data struct defined in `transliterate`. Third,
//! it computes the dependencies of the transliterator.
//! It is responsible for both directions of a source file, but the rest of this documentation
//! assumes a single direction. The process is simply repeated for the other direction.
//!
//! # Terminology
//! * The "direction" of a rule: Whether a rule is _forward_ (left-to-right in the source) or
//!   _reverse_ (right-to-left in the source). At runtime, clients will apply a transliterator
//!   in one direction. The transliterator `a <> b` replaces `a` with `b` in the forward direction,
//!   and `b` with `a` in the reverse direction.
//! * The "side" of a rule: A rule has a _source_ and a _target_ side. The source is replaced
//!   with the target. If we're looking at a definition of a transliterator in the _forward_
//!   direction, the source is on the left and the target is on the right, and vice versa for
//!   the _reverse_ direction.
//! * "Special matchers" are non-literal items that can appear on the source side of a rule.
//!   This includes, e.g., `UnicodeSets` and quantifiers.
//! * "Special replacers" are non-literal items that can appear on the target side of a rule.
//!   This includes, e.g., function calls and back references.
//! * "Special constructs" are just any non-literal rule item.
//!
//! # Conversion rule encoding
//!
//! Conversion rules are encoded using `str`s, and private use code points are used to represent
//! the special constructs that can appear in a conversion rule (`UnicodeSets`, quantifiers, ...).
//! This works as follows:
//! * We use PUP (15), code points U+F0000 to U+FFFFD (inclusive)
//! * A private use code point simply encodes an integer, obtained by subtracting U+F0000 from it
//! * The integer is used as an index into `VarTable`
//! * As a `VarTable` has multiple `VarZeroVec`s (one for each special construct), an index
//!   overflows into the following `VZV`s:
//!    * An index of `vzv1.len() + vzv2.len() + 4` indexes the third `VZV` at index 4
//! * Thus, if the length of an earlier `VZV` changes, the index of an element in a later `VZV`
//!   will change, and its private use encoding will change
//! * Therefore we must know the number of elements of each `VZV` before we can start encoding
//!   conversion rules into `str`s.
//!
//! ## Special encodings
//!
//! ### Back references (`$1`, `$2`, ...)
//! Back references are encoded as the other special matchers with an index, except the index is
//! the value. Thus, `$1` should be encoded as if it was an index of 0 into some subsequent `VZV`,
//! `$2` as 1, etc. The index 0 decodes into 1 because there is no valid back reference of `$0`.
//!
//! ### Cursors (`|@@@ rest`, `rest @@@|`, `rest | rest`)
//! There are three kinds of cursors.
//! * `|@@@ rest`: The cursor is at the beginning of the replacement and has placeholders. We call
//!   this a cursor with _right_ placeholders, because they are on the right of the cursor.
//! * `rest @@@|`: The cursor is at the end of the replacement and has placeholders. We call this
//!   a cursor with _left_ placeholders, because they are on the left of the cursor.
//! * `rest | rest`: The cursor is in the middle of the replacement and has no placeholders. We call
//!   this a pure cursor.
//!
//! Pure cursors get a reserved code point that is inlined into the replacement encoding where the
//! cursor appears. Cursors with placeholders are encoded like back references, so two more
//! pseudo-VZV entries, one for left placeholders and one for right placeholders, are used to
//! encode the number of placeholders as indices. E.g., `|@@@` is the index 2 into the `left`
//! pseudo-VZV because there are 3 left placeholders.
//!
//!
//! ### Anchors (`^`, `$`)
//! These are encoded with a reserved private use code point per type (start `^` or end `$`).
//!
//! # Passes
//!
//! This module works by performing multiple passes over the rules.
//!
//! ## Pass 1
//! General validation of the rules and computation of the lengths of the `VZV`s in the `VarTable`.
//!
//! Only special constructs for the current direction contribute to the `VZV` lengths,
//! i.e., the rule `a <> [a-z] { b` will not increment the size of the
//! `VZV` for `UnicodeSets` if the current direction is `forward`, but it will if the current
//! direction is `reverse` (this is because contexts on the target side of a rule are ignored).
//!
//! Similarly, only recursive transliterators and variables actually used for this direction are
//! accounted for.
//!
//! ## Pass 2
//! Encoding of the zero-copy data struct.
//!
//! This works under the assumption that the first pass was successful, and thus generally
//! no more validation needs to be performed.
//!
//! To encode conversion rules into `str`s, we use the previously described encoded `VarTable`
//! indices. Because we know the lengths of each special construct list (in the form a `VZV`)
//! from the first pass, we can store the offsets for each special construct list (i.e., the sum of
//! the lengths of the previous lists) while encoding the conversion rules, and incrementing the
//! offset of a given special construct when we encode an element. The precomputed lengths mean we
//! never overflow into the indices of the following `VZV`.

// TODO(#3825): Datagen optimizations (variable inlining, set flattening, deduplicating VarTable)

/*
Encoding example:

    $b = bc+ ;
    $a = [a-z] $b ;
    $a > ;

b-data.counts: 1 compound (the definition itself), 1 quantifier plus (c+)
b-data.used_vars: -

a-data.counts: 1 compound (the definition itself), 1 unicodeset ([a-z])
a-data.used_vars: b

forward-data.counts: 0 (rules are inlined)
forward-data.used_vars: a

when collecting the counts (for forward) at the end, we sum over all counts of the transitive
dependencies of forward (using used_vars), and add the counts of forward itself.
we also compute the transitive closure of used variables.
this gives us the `Pass1Result`:
forward-data.counts: 2 compound, 1 quantifier plus, 1 unicodeset
forward-data.used_vars: a, b

this `Pass1Result` we give to Pass2, which will produce something like this:
(note that the integer-indexed maps shown here are only semantic, in actuality the indices are implicit,
as described in the zero-copy format, and the maps here are just arrays)

    VarTable {
        compounds: {
            0: "b<2>", // b's definition, bc+
            1: "<3><0>", // a's definition, [a-z] $b
        },
        quantifier_kleene_plus: {
            2: "c", // c+
        },
        unicode_sets: {
            3: <the set of a..z>, // [a-z]
        }
    }
    Rules: [
        {
            source: "<1>", // $a
            target: "",
        }
    ]
*/

use super::*;
use alloc::borrow::ToOwned;
use alloc::collections::{BTreeMap, BTreeSet};

type Result<T> = core::result::Result<T, CompileError>;

enum SingleDirection {
    Forward,
    Reverse,
}

/// The number of elements for each `VZV` in the `VarTable`.
#[derive(Debug, Copy, Clone, Default, PartialEq, Eq)]
pub(crate) struct SpecialConstructCounts {
    pub(crate) num_compounds: usize,
    pub(crate) num_quantifiers_opt: usize,
    pub(crate) num_quantifiers_kleene: usize,
    pub(crate) num_quantifiers_kleene_plus: usize,
    pub(crate) num_segments: usize,
    pub(crate) num_unicode_sets: usize,
    pub(crate) num_function_calls: usize,
    // a "left" placeholder is a placeholder on the left side of a cursor, e.g., `abc @@@ |`
    // this is means the cursor itself is on the very right of a replacement section
    pub(crate) max_left_placeholders: u32,
    // a "right" placeholder is a placeholder on the right side of a cursor, e.g., `| @@@ abc`
    // this is means the cursor itself is on the very left of a replacement section
    pub(crate) max_right_placeholders: u32,
    // backrefs have no data, they're simply an unsigned integer, so we only care about the maximum
    // number we need to encode
    pub(crate) max_backref_num: u32,
}

impl SpecialConstructCounts {
    pub(crate) fn num_total(&self) -> usize {
        self.num_compounds
            + self.num_quantifiers_opt
            + self.num_quantifiers_kleene
            + self.num_quantifiers_kleene_plus
            + self.num_segments
            + self.num_unicode_sets
            + self.num_function_calls
            + self.max_left_placeholders as usize
            + self.max_right_placeholders as usize
            + self.max_backref_num as usize
    }

    fn combine(&mut self, other: Self) {
        // destructuring to compile-time check the completeness of this function in case new fields
        // are added
        let Self {
            num_compounds,
            num_quantifiers_opt,
            num_quantifiers_kleene,
            num_quantifiers_kleene_plus,
            num_segments,
            num_unicode_sets,
            num_function_calls,
            max_left_placeholders,
            max_right_placeholders,
            max_backref_num,
        } = other;

        self.num_compounds += num_compounds;
        self.num_quantifiers_opt += num_quantifiers_opt;
        self.num_quantifiers_kleene += num_quantifiers_kleene;
        self.num_quantifiers_kleene_plus += num_quantifiers_kleene_plus;
        self.num_segments += num_segments;
        self.num_unicode_sets += num_unicode_sets;
        self.num_function_calls += num_function_calls;
        self.max_left_placeholders = self.max_left_placeholders.max(max_left_placeholders);
        self.max_right_placeholders = self.max_right_placeholders.max(max_right_placeholders);
        self.max_backref_num = self.max_backref_num.max(max_backref_num);
    }
}

// Data for a given direction or variable definition (the "key")
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub(crate) struct Pass1Data {
    pub(crate) counts: SpecialConstructCounts,
    // the variables used by the associated key
    pub(crate) used_variables: BTreeSet<String>,
}

#[derive(Debug, Clone)]
pub(crate) struct DirectedPass1Result<'p> {
    // data with dependencies resolved and counts summed
    pub(crate) data: Pass1Data,
    pub(crate) groups: rule_group_agg::RuleGroups<'p>,
    pub(crate) filter: Option<parse::FilterSet>,
}

#[derive(Debug, Clone)]
pub(crate) struct Pass1Result<'p> {
    pub(crate) forward_result: DirectedPass1Result<'p>,
    pub(crate) reverse_result: DirectedPass1Result<'p>,
    pub(crate) variable_definitions: BTreeMap<String, &'p [parse::Element]>,
}

/// Responsible for the first pass as described in the module-level documentation.
#[derive(Debug, Clone)]
pub(crate) struct Pass1<'p> {
    direction: Direction,
    // data for *direct* dependencies
    forward_data: Pass1Data,
    reverse_data: Pass1Data,
    variable_data: BTreeMap<String, Pass1Data>,
    forward_filter: Option<parse::FilterSet>,
    reverse_filter: Option<parse::FilterSet>,
    forward_rule_group_agg: rule_group_agg::ForwardRuleGroupAggregator<'p>,
    reverse_rule_group_agg: rule_group_agg::ReverseRuleGroupAggregator<'p>,
    variable_definitions: BTreeMap<String, &'p [parse::Element]>,
    // variables which contain constructs that are only allowed to appear on the source side
    // e.g., $a = c+; $set = [a-z]; ...
    target_disallowed_variables: BTreeSet<String>,
}

impl<'p> Pass1<'p> {
    // TODO(#3736): decide if validation should be direction dependent
    //  example: transliterator with direction "forward", and a rule `[a-z] < b ;` (invalid)
    //  - if validation is dependent, this rule is valid because it's not used in the forward direction
    //  - if validation is independent, this rule is invalid because the reverse direction is also checked
    pub(super) fn run(direction: Direction, rules: &'p [parse::Rule]) -> Result<Pass1Result<'p>> {
        let mut s = Self {
            direction,
            forward_data: Pass1Data::default(),
            reverse_data: Pass1Data::default(),
            variable_data: BTreeMap::new(),
            variable_definitions: BTreeMap::new(),
            target_disallowed_variables: BTreeSet::new(),
            forward_filter: None,
            reverse_filter: None,
            forward_rule_group_agg: rule_group_agg::ForwardRuleGroupAggregator::new(),
            reverse_rule_group_agg: rule_group_agg::ReverseRuleGroupAggregator::new(),
        };

        // first check global filter/global inverse filter.
        // after this check, they may not appear anywhere.
        let rules = s.validate_global_filters(rules);

        // iterate through remaining rules and perform checks according to interim specification

        for rule in rules {
            s.forward_rule_group_agg.push(rule);
            s.reverse_rule_group_agg.push(rule);

            match rule {
                parse::Rule::GlobalFilter(_) | parse::Rule::GlobalInverseFilter(_) => {
                    // the previous step ensures `rules` has no more global filters
                    return Err(CompileErrorKind::UnexpectedGlobalFilter.into());
                }
                parse::Rule::Transform(..) => {}
                parse::Rule::VariableDefinition(name, definition) => {
                    s.validate_variable_definition(name, definition)?;
                }
                parse::Rule::Conversion(hr1, dir, hr2) => {
                    s.validate_conversion(hr1, *dir, hr2)?;
                }
            }
        }

        Pass1ResultGenerator::generate(s)
    }

    fn validate_global_filters<'a>(&mut self, rules: &'a [parse::Rule]) -> &'a [parse::Rule] {
        let rules = match rules {
            [parse::Rule::GlobalFilter(filter), rest @ ..] => {
                self.forward_filter = Some(filter.clone());

                rest
            }
            _ => rules,
        };
        match rules {
            [rest @ .., parse::Rule::GlobalInverseFilter(filter)] => {
                self.reverse_filter = Some(filter.clone());

                rest
            }
            _ => rules,
        }
    }

    fn validate_variable_definition(
        &mut self,
        name: &String,
        definition: &'p [parse::Element],
    ) -> Result<()> {
        if self.variable_definitions.contains_key(name) {
            return Err(CompileErrorKind::DuplicateVariable.into());
        }
        self.variable_definitions.insert(name.clone(), definition);

        let mut data = Pass1Data::default();
        // the variable definition itself is counted here
        data.counts.num_compounds = 1;

        let mut validator = VariableDefinitionValidator::new(
            |s| self.variable_definitions.contains_key(s),
            &mut data,
            &self.target_disallowed_variables,
            definition,
        );
        validator.validate()?;
        if validator.used_target_disallowed_construct {
            self.target_disallowed_variables.insert(name.clone());
        }

        self.variable_data.insert(name.clone(), data);

        Ok(())
    }

    fn validate_conversion(
        &mut self,
        source: &parse::HalfRule,
        dir: Direction,
        target: &parse::HalfRule,
    ) -> Result<()> {
        // TODO(#3736): include source location/actual source text in these logs
        // logging for useless contexts
        #[cfg(feature = "datagen")]
        if dir == Direction::Forward && (!target.ante.is_empty() || !target.post.is_empty()) {
            log::warn!("forward conversion rule has ignored context on target side");
        }
        #[cfg(feature = "datagen")]
        if dir == Direction::Reverse && (!source.ante.is_empty() || !source.post.is_empty()) {
            log::warn!("reverse conversion rule has ignored context on target side");
        }

        if self.direction.permits(Direction::Forward) && dir.permits(Direction::Forward) {
            self.validate_conversion_one_direction(source, target, SingleDirection::Forward)?;
        }
        if self.direction.permits(Direction::Reverse) && dir.permits(Direction::Reverse) {
            self.validate_conversion_one_direction(target, source, SingleDirection::Reverse)?;
        }

        Ok(())
    }

    fn validate_conversion_one_direction(
        &mut self,
        source: &parse::HalfRule,
        target: &parse::HalfRule,
        dir: SingleDirection,
    ) -> Result<()> {
        let data = match dir {
            SingleDirection::Forward => &mut self.forward_data,
            SingleDirection::Reverse => &mut self.reverse_data,
        };
        let mut source_validator = SourceValidator::new(
            |s| self.variable_definitions.contains_key(s),
            data,
            &source.ante,
            &source.key,
            &source.post,
        );
        source_validator.validate()?;
        let num_source_segments = source_validator.num_segments;

        let mut target_validator = TargetValidator::new(
            |s| self.variable_definitions.contains_key(s),
            &mut self.target_disallowed_variables,
            data,
            &target.key,
            num_source_segments,
        );
        target_validator.validate()?;

        Ok(())
    }
}

struct SourceValidator<'a, 'p, F: Fn(&str) -> bool> {
    is_variable_defined: F,
    data: &'a mut Pass1Data,
    ante: &'p [parse::Element],
    key: &'p [parse::Element],
    post: &'p [parse::Element],
    // the number of segments this rule defines. consumed by TargetValidator.
    num_segments: u32,
}

/// Validates the source side of a rule.
///
/// Ensures that only special constructs that may appear on the source side of a rule are used.
/// Also validates certain other source-side-only constraints, such as anchors needing to be at the
/// beginning or end of the rule.
impl<'a, 'p, F: Fn(&str) -> bool> SourceValidator<'a, 'p, F> {
    fn new(
        is_variable_defined: F,
        data: &'a mut Pass1Data,
        ante: &'p [parse::Element],
        key: &'p [parse::Element],
        post: &'p [parse::Element],
    ) -> Self {
        Self {
            is_variable_defined,
            data,
            ante,
            key,
            post,
            num_segments: 0,
        }
    }

    fn validate(&mut self) -> Result<()> {
        // first validate position of ^ and $ anchors, if they exist
        // ^: if ante is non-empty, must be its first element, otherwise must be first element of key
        // $: if post is non-empty, must be its last element, otherwise must be last element of key

        let sections = [self.ante, self.key, self.post];
        // split off first element if it is a start anchor
        let sections = match sections {
            [[parse::Element::AnchorStart, ante @ ..], key, post] => [ante, key, post],
            [[], [parse::Element::AnchorStart, key @ ..], post] => [&[], key, post],
            _ => sections,
        };
        // split off last element if it is an end anchor
        let sections = match sections {
            [ante, key, [post @ .., parse::Element::AnchorEnd]] => [ante, key, post],
            [ante, [key @ .., parse::Element::AnchorEnd], []] => [ante, key, &[]],
            _ => sections,
        };

        // now neither start nor end anchors may appear anywhere in `order`

        sections
            .iter()
            .try_for_each(|s| self.validate_section(s, true))
    }

    fn validate_section(&mut self, section: &[parse::Element], top_level: bool) -> Result<()> {
        section
            .iter()
            .try_for_each(|element| self.validate_element(element, top_level))
    }

    fn validate_element(&mut self, element: &parse::Element, top_level: bool) -> Result<()> {
        match element {
            parse::Element::Literal(_) => {}
            parse::Element::VariableRef(name) => {
                if !(self.is_variable_defined)(name) {
                    return Err(CompileErrorKind::UnknownVariable.into());
                }
                self.data.used_variables.insert(name.clone());
            }
            parse::Element::Quantifier(kind, inner) => {
                self.validate_element(inner, false)?;
                match *kind {
                    parse::QuantifierKind::ZeroOrOne => self.data.counts.num_quantifiers_opt += 1,
                    parse::QuantifierKind::ZeroOrMore => {
                        self.data.counts.num_quantifiers_kleene += 1
                    }
                    parse::QuantifierKind::OneOrMore => {
                        self.data.counts.num_quantifiers_kleene_plus += 1
                    }
                }
            }
            parse::Element::Segment(inner) => {
                self.validate_section(inner, false)?;
                // increment the count for this specific rule
                self.num_segments += 1;
                // increment the count for this direction of the entire transliterator
                self.data.counts.num_segments += 1;
            }
            parse::Element::UnicodeSet(_) => {
                self.data.counts.num_unicode_sets += 1;
            }
            parse::Element::Cursor(_, _) => {
                // while cursors have no effect on the source side, they may appear nonetheless
                // TargetValidator validates these

                // however, cursors are only allowed at the top level
                if !top_level {
                    return Err(CompileErrorKind::InvalidCursor.into());
                }
            }
            parse::Element::AnchorStart => {
                // we check for these in `validate`
                return Err(CompileErrorKind::AnchorStartNotAtStart.into());
            }
            parse::Element::AnchorEnd => {
                // we check for these in `validate`
                return Err(CompileErrorKind::AnchorEndNotAtEnd.into());
            }
            elt => {
                return Err(
                    CompileErrorKind::UnexpectedElementInSource(elt.kind().debug_str()).into(),
                );
            }
        }
        Ok(())
    }
}

/// Validates the target side of a rule.
///
/// Ensures that only special constructs (including variables) that may appear on the target side
/// of a rule are used. Also validates other target-side-only constraints, such as
/// back references not being allowed to overflow and only one cursor being allowed.
struct TargetValidator<'a, 'p, F: Fn(&str) -> bool> {
    is_variable_defined: F,
    target_disallowed_variables: &'a mut BTreeSet<String>,
    data: &'a mut Pass1Data,
    replacer: &'p [parse::Element],
    // the number of segments defined on the corresponding source side. produced by SourceValidator
    num_segments: u32,
    // true if a cursor has already been encountered, i.e., any further cursors are disallowed
    encountered_cursor: bool,
}

impl<'a, 'p, F: Fn(&str) -> bool> TargetValidator<'a, 'p, F> {
    fn new(
        is_variable_defined: F,
        target_disallowed_variables: &'a mut BTreeSet<String>,
        data: &'a mut Pass1Data,
        replacer: &'p [parse::Element],
        num_segments: u32,
    ) -> Self {
        Self {
            is_variable_defined,
            target_disallowed_variables,
            data,
            replacer,
            num_segments,
            encountered_cursor: false,
        }
    }

    fn validate(&mut self) -> Result<()> {
        let section = self.replacer;
        // special case for a single cursor
        let section = match section {
            [parse::Element::Cursor(pre, post)] => {
                self.encounter_cursor()?;
                if *pre != 0 && *post != 0 {
                    // corrseponds to `@@@|@@@`, i.e., placeholders on both sides of the cursor
                    return Err(CompileErrorKind::InvalidCursor.into());
                }
                self.update_cursor_data(*pre, *post);
                return Ok(());
            }
            _ => section,
        };
        // strip |@@@ from beginning
        let section = match section {
            [parse::Element::Cursor(pre, post), rest @ ..] => {
                self.encounter_cursor()?;
                if *pre != 0 {
                    // corrseponds to `@@@|...`, i.e., placeholders in front of the cursor
                    return Err(CompileErrorKind::InvalidCursor.into());
                }
                self.update_cursor_data(*pre, *post);
                rest
            }
            _ => section,
        };
        // strip @@@| from end
        let section = match section {
            [rest @ .., parse::Element::Cursor(pre, post)] => {
                self.encounter_cursor()?;
                if *post != 0 {
                    // corrseponds to `...|@@@`, i.e., placeholders after the cursor
                    return Err(CompileErrorKind::InvalidCursor.into());
                }
                self.update_cursor_data(*pre, *post);
                rest
            }
            _ => section,
        };

        self.validate_section(section, true)
    }

    fn validate_section(&mut self, section: &[parse::Element], top_level: bool) -> Result<()> {
        section
            .iter()
            .try_for_each(|element| self.validate_element(element, top_level))
    }

    fn validate_element(&mut self, element: &parse::Element, top_level: bool) -> Result<()> {
        match element {
            parse::Element::Literal(_) => {}
            parse::Element::VariableRef(name) => {
                if !(self.is_variable_defined)(name) {
                    return Err(CompileErrorKind::UnknownVariable.into());
                }
                if self.target_disallowed_variables.contains(name) {
                    return Err(CompileErrorKind::SourceOnlyVariable.into());
                }
                self.data.used_variables.insert(name.clone());
            }
            parse::Element::BackRef(num) => {
                if *num > self.num_segments {
                    return Err(CompileErrorKind::BackReferenceOutOfRange.into());
                }
                self.data.counts.max_backref_num = self.data.counts.max_backref_num.max(*num);
            }
            parse::Element::FunctionCall(_, inner) => {
                self.validate_section(inner, false)?;
                self.data.counts.num_function_calls += 1;
            }
            &parse::Element::Cursor(pre, post) => {
                self.encounter_cursor()?;
                if !top_level || pre != 0 || post != 0 {
                    // pre and post must be 0 if the cursor does not appear at the very beginning or the very end
                    // we account for the beginning or the end in `validate`.
                    return Err(CompileErrorKind::InvalidCursor.into());
                }
                // no need to update cursor data: pre/post are 0
            }
            parse::Element::AnchorStart => {
                // while anchors have no effect on the target side, they may still appear
            }
            parse::Element::AnchorEnd => {
                // while anchors have no effect on the target side, they may still appear
            }
            elt => {
                return Err(
                    CompileErrorKind::UnexpectedElementInTarget(elt.kind().debug_str()).into(),
                );
            }
        }
        Ok(())
    }

    fn encounter_cursor(&mut self) -> Result<()> {
        if self.encountered_cursor {
            return Err(CompileErrorKind::DuplicateCursor.into());
        }
        self.encountered_cursor = true;
        Ok(())
    }

    /// Updates the max encountered cursor placeholders. Does no validation.
    fn update_cursor_data(&mut self, left: u32, right: u32) {
        self.data.counts.max_left_placeholders = self.data.counts.max_left_placeholders.max(left);
        self.data.counts.max_right_placeholders =
            self.data.counts.max_right_placeholders.max(right);
    }
}

/// Validates variable definitions.
///
/// This checks that only a limited subset of special constructs appear in a variable's definition.
/// For example, segments, back references, cursors, anchors, and function calls are not allowed.
///
/// It also propagates information about whether a variable may appear on the target side of a rule,
/// as variables are in general allowed on the target side, but only if they only contain
/// special constructs that are allowed to appear on the target side.
struct VariableDefinitionValidator<'a, 'p, F: Fn(&str) -> bool> {
    is_variable_defined: F,
    target_disallowed_variables: &'a BTreeSet<String>,
    data: &'a mut Pass1Data,
    definition: &'p [parse::Element],
    used_target_disallowed_construct: bool,
}

impl<'a, 'p, F: Fn(&str) -> bool> VariableDefinitionValidator<'a, 'p, F> {
    fn new(
        is_variable_defined: F,
        data: &'a mut Pass1Data,
        target_disallowed_variables: &'a BTreeSet<String>,
        definition: &'p [parse::Element],
    ) -> Self {
        Self {
            is_variable_defined,
            data,
            target_disallowed_variables,
            definition,
            used_target_disallowed_construct: false,
        }
    }

    fn validate(&mut self) -> Result<()> {
        self.validate_section(self.definition)
    }

    fn validate_section(&mut self, section: &[parse::Element]) -> Result<()> {
        section
            .iter()
            .try_for_each(|element| self.validate_element(element))
    }

    fn validate_element(&mut self, element: &parse::Element) -> Result<()> {
        match element {
            parse::Element::Literal(_) => {}
            parse::Element::VariableRef(name) => {
                if !(self.is_variable_defined)(name) {
                    return Err(CompileErrorKind::UnknownVariable.into());
                }
                if self.target_disallowed_variables.contains(name) {
                    self.used_target_disallowed_construct = true;
                }
                self.data.used_variables.insert(name.clone());
            }
            parse::Element::Quantifier(kind, inner) => {
                self.used_target_disallowed_construct = true;
                match *kind {
                    parse::QuantifierKind::ZeroOrOne => self.data.counts.num_quantifiers_opt += 1,
                    parse::QuantifierKind::ZeroOrMore => {
                        self.data.counts.num_quantifiers_kleene += 1
                    }
                    parse::QuantifierKind::OneOrMore => {
                        self.data.counts.num_quantifiers_kleene_plus += 1
                    }
                }
                self.validate_element(inner)?;
            }
            parse::Element::UnicodeSet(_) => {
                self.used_target_disallowed_construct = true;
                self.data.counts.num_unicode_sets += 1;
            }
            elt => {
                return Err(CompileErrorKind::UnexpectedElementInVariableDefinition(
                    elt.kind().debug_str(),
                )
                .into());
            }
        }
        Ok(())
    }
}

// TODO(#3736): Think about adding a fourth Validator here that is run for
//  all conversion rules in full (i.e., all contexts and the direction of the rule is part of the API)
//  that checks for edge cases that are difficult to validate otherwise:
//  - cursors (could move functionality from TargetValidator here too, but this is mostly intended for:
//    - any cursors on the source side for unidirectional rules
//    - any cursors in contexts)
//  - anchors (could move functionality from SourceValidator here too, but this is mostly intended for:
//    - anchors on the target side for unidirectional rules
//  - contexts on the target side for unidirectional rules (still need to discuss what exactly, could be disallowed
//    completely or just disallow target-only matchers (backrefs, function calls))
//  as part of this, it should also be decided whether these edge cases are full-blown errors or
//  merely logged warnings.

struct Pass1ResultGenerator {
    // for cycle-detection
    current_vars: BTreeSet<String>,
    transitive_var_dependencies: BTreeMap<String, BTreeSet<String>>,
}

impl Pass1ResultGenerator {
    fn generate(pass: Pass1) -> Result<Pass1Result> {
        let generator = Self {
            current_vars: BTreeSet::new(),
            transitive_var_dependencies: BTreeMap::new(),
        };
        generator.generate_result(pass)
    }

    fn generate_result(mut self, pass: Pass1) -> Result<Pass1Result> {
        // the result for a given direction is computed by first computing the transitive
        // used variables for each direction, then using that data summing over the
        // special construct counts, and at last filtering the variable definitions based on
        // the used variables in either direction.

        let forward_data =
            self.generate_result_one_direction(&pass.forward_data, &pass.variable_data)?;
        let reverse_data =
            self.generate_result_one_direction(&pass.reverse_data, &pass.variable_data)?;

        let variable_definitions = pass
            .variable_definitions
            .into_iter()
            .filter(|(var, _)| {
                forward_data.used_variables.contains(var)
                    || reverse_data.used_variables.contains(var)
            })
            .collect();

        let forward_rule_groups = pass.forward_rule_group_agg.finalize();
        let reverse_rule_groups = pass.reverse_rule_group_agg.finalize();

        Ok(Pass1Result {
            forward_result: DirectedPass1Result {
                data: forward_data,
                filter: pass.forward_filter,
                groups: forward_rule_groups,
            },
            reverse_result: DirectedPass1Result {
                data: reverse_data,
                filter: pass.reverse_filter,
                groups: reverse_rule_groups,
            },
            variable_definitions,
        })
    }

    fn generate_result_one_direction(
        &mut self,
        seed_data: &Pass1Data,
        var_data_map: &BTreeMap<String, Pass1Data>,
    ) -> Result<Pass1Data> {
        let seed_vars = &seed_data.used_variables;

        let mut used_variables = seed_vars.clone();
        for var in seed_vars {
            self.visit_var(var, var_data_map)?;
            #[expect(clippy::indexing_slicing)] // an non-error `visit_var` ensures this exists
            let deps = self.transitive_var_dependencies[var].clone();
            used_variables.extend(deps);
        }

        let mut combined_counts = seed_data.counts;

        for var in &used_variables {
            // we check for unknown variables during the first pass, so these should exist
            let var_data = var_data_map
                .get(var)
                .ok_or(CompileErrorKind::Internal("unexpected unknown variable"))?;
            combined_counts.combine(var_data.counts);
        }

        Ok(Pass1Data {
            used_variables,
            counts: combined_counts,
        })
    }

    fn visit_var(&mut self, name: &str, var_data_map: &BTreeMap<String, Pass1Data>) -> Result<()> {
        if self.transitive_var_dependencies.contains_key(name) {
            return Ok(());
        }
        if self.current_vars.contains(name) {
            // cyclic dependency - should not occur
            return Err(CompileErrorKind::Internal("unexpected cyclic variable").into());
        }
        self.current_vars.insert(name.to_owned());
        // we check for unknown variables during the first pass, so these should exist
        let var_data = var_data_map
            .get(name)
            .ok_or(CompileErrorKind::Internal("unexpected unknown variable"))?;
        let mut transitive_dependencies = var_data.used_variables.clone();
        var_data
            .used_variables
            .iter()
            .try_for_each(|var| -> Result<()> {
                self.visit_var(var, var_data_map)?;
                #[expect(clippy::indexing_slicing)] // an non-error `visit_var` ensures this exists
                let deps = self.transitive_var_dependencies[var].clone();
                transitive_dependencies.extend(deps);

                Ok(())
            })?;
        self.current_vars.remove(name);
        self.transitive_var_dependencies
            .insert(name.to_owned(), transitive_dependencies);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use core::ops::Deref;

    enum ExpectedOutcome {
        Pass,
        Fail,
    }
    use ExpectedOutcome::*;

    fn parse(s: &str) -> Vec<parse::Rule> {
        match parse::parse(s) {
            Ok(rules) => rules,
            Err(e) => panic!("unexpected error parsing rules {s:?}: {:?}", e.explain(s)),
        }
    }

    fn pass1data_from_parts(
        var_deps: &[&'static str],
        counts: SpecialConstructCounts,
    ) -> Pass1Data {
        let mut data = Pass1Data {
            counts,
            ..Default::default()
        };
        for &var in var_deps {
            data.used_variables.insert(var.into());
        }
        data
    }

    #[test]
    fn test_pass1_computed_data() {
        let source = r"
        :: [a-z] ;
        $used_both = [a-z] ; # only transitively used by reverse direction
        $used_rev = $used_both $used_both+ ;
        $unused = a+ b+ .? $used_both $used_rev ; # unused
        $unused2 = $unused ; # unused
        :: [:L:] Bidi-Dependency/One ;
        $used_fwd = [just a set] ;
        ($used_both [a-z]) > &[a-z] Forward-Dependency($1) ;
        $used_fwd > ;
        < $used_rev+? ;

        $literal1 = a ;
        $literal2 = b ;
        $literal1 <> $literal2 ;
        :: AnotherForwardDependency () ;
        :: ([set] Backward-Dependency) ;
        :: YetAnother-ForwardDependency (AnotherBackwardDependency) ;
        &Many(&Backwardz(&Deps($2))) < (a(bc)d)+ ;

        :: ([a-z]) ;
        ";

        let rules = parse(source);
        let result = Pass1::run(Direction::Both, &rules)
            .map_err(|e| e.explain(source))
            .expect("pass1 failed");
        {
            let vars_with_data: BTreeSet<_> = result
                .variable_definitions
                .keys()
                .map(Deref::deref)
                .collect();
            let expected_vars_with_data =
                BTreeSet::from(["used_both", "used_rev", "used_fwd", "literal1", "literal2"]);
            assert_eq!(expected_vars_with_data, vars_with_data);
        }
        {
            // check aggregated Pass1Result
            let fwd_counts = SpecialConstructCounts {
                num_compounds: 4,
                num_unicode_sets: 3,
                num_function_calls: 1,
                num_segments: 1,
                max_backref_num: 1,
                ..Default::default()
            };
            let fwd_data = pass1data_from_parts(
                &["used_both", "used_fwd", "literal1", "literal2"],
                fwd_counts,
            );

            let rev_counts = SpecialConstructCounts {
                num_compounds: 4,
                num_unicode_sets: 1,
                num_quantifiers_kleene_plus: 3,
                num_quantifiers_opt: 1,
                num_segments: 2,
                num_function_calls: 3,
                max_backref_num: 2,
                ..Default::default()
            };
            let rev_data = pass1data_from_parts(
                &["used_both", "used_rev", "literal1", "literal2"],
                rev_counts,
            );

            assert_eq!(fwd_data, result.forward_result.data);
            assert_eq!(rev_data, result.reverse_result.data);

            let actual_definition_keys: BTreeSet<_> = result
                .variable_definitions
                .keys()
                .map(Deref::deref)
                .collect();
            let expected_definition_keys =
                BTreeSet::from(["used_both", "used_fwd", "used_rev", "literal1", "literal2"]);
            assert_eq!(expected_definition_keys, actual_definition_keys);
        }
    }

    #[test]
    fn test_pass1_validate_conversion() {
        let sources = [
            // anchor start must be at the beginning
            (Pass, r"^ a > ;"),
            (Pass, r"^ a > ^ b;"),
            (Pass, r"^ a < ^ b;"),
            (Pass, r"^ a <> ^ b;"),
            (Pass, r"^ { a > ;"),
            (Pass, r"{ ^ a > ;"),
            (Fail, r"a { ^ a > ;"),
            // TODO(#3736): do we enforce this?
            // (Fail, r"{ ^ a > a ^ ;"),
            (Fail, r"a ^ a > ;"),
            (Fail, r"a ^ > ;"),
            (Fail, r"< a ^ ;"),
            (Fail, r"a } ^ > ;"),
            (Fail, r"a } ^ a > ;"),
            (Fail, r"(^) a > ;"),
            (Fail, r"^+ a > ;"),
            // anchor end must be at the end
            (Pass, r"a $ > ;"),
            (Pass, r"a $ > $;"),
            (Pass, r"a $ <> a$;"),
            (Pass, r"a } $ > ;"),
            (Pass, r"a $ } > ;"),
            (Fail, r"a $ } a > ;"),
            (Fail, r"< $ a ;"),
            (Fail, r"a $ a > ;"),
            (Fail, r"$ a > ;"),
            (Fail, r"$ { a > ;"),
            (Fail, r"a $ { a > ;"),
            (Fail, r"a ($) > ;"),
            (Fail, r"a $+ > ;"),
            // cursor checks
            (Pass, r"a | b <> c | d ;"),
            (Fail, r"a | b | <> | c | d ;"),
            (Fail, r"a > | c | d ;"),
            (Pass, r"a > | c d ;"),
            (Pass, r"a > | ;"),
            (Fail, r"a > || ;"),
            (Fail, r"a|? > ;"),
            (Fail, r"a(|) > ;"),
            (Fail, r"a > &Remove(|) ;"),
            (Pass, r"a > |@ ;"),
            (Pass, r"a > @| ;"),
            (Fail, r"a > @|@ ;"),
            (Fail, r"a > @|@| ;"),
            (Pass, r"a > xa @@@| ;"),
            (Pass, r"a > |@@ xa ;"),
            (Fail, r"a > x @| a ;"),
            (Fail, r"a > x |@ a ;"),
            (Fail, r"a > x @|@ a ;"),
            // UnicodeSets
            (Pass, r"[a-z] > a ;"),
            (Fail, r"[a-z] < a ;"),
            (Pass, r". > a ;"),
            (Fail, r". < a ;"),
            // segments
            (Fail, r"(a) <> $1 ;"),
            (Pass, r"(a) > $1 ;"),
            (Pass, r"(a()) > $1 $2;"),
            (Pass, r"(a()) > $2;"),
            (Fail, r"(a) > $2;"),
            (Pass, r"(a) } (abc) > $2;"),
            // variables
            (Fail, r"a > $a;"),
            // quantifiers
            (Pass, r"a+*? } b? > a;"),
            (Fail, r"a > a+;"),
            (Fail, r"a > a*;"),
            (Fail, r"a > a?;"),
            // function calls
            (Pass, r"a > &Remove();"),
            (Fail, r"a < &Remove();"),
            (Pass, r"a (.*)> &[a-z] Latin-Greek/BGN(abc &[a]Remove($1));"),
        ];

        for (expected_outcome, source) in sources {
            match (
                expected_outcome,
                Pass1::run(Direction::Both, &parse(source)),
            ) {
                (Fail, Ok(_)) => {
                    panic!("unexpected successful pass1 validation for rules {source:?}")
                }
                (Pass, Err(e)) => {
                    panic!(
                        "unexpected error in pass1 validation for rules {source:?}: {:?}",
                        e.explain(source)
                    )
                }
                _ => {}
            }
        }
    }

    #[test]
    fn test_pass1_validate_variable_definition() {
        let sources = [
            (Fail, r"$a = &Remove() ;"),
            (Fail, r"$a = (abc) ;"),
            (Fail, r"$a = | ;"),
            (Fail, r"$a = ^ ;"),
            (Fail, r"$a = $ ;"),
            (Fail, r"$a = $1 ;"),
            (Fail, r"$var = [a-z] ; a > $var ;"),
            (Fail, r"$var = a+ ; a > $var ;"),
            (Pass, r"$var = [a-z] ; $var > a ;"),
            (Pass, r"$var = a+ ; $var > a ;"),
            (Pass, r"$b = 'hello'; $var = a+*? [a-z] $b ;"),
        ];

        for (expected_outcome, source) in sources {
            match (
                expected_outcome,
                Pass1::run(Direction::Both, &parse(source)),
            ) {
                (Fail, Ok(_)) => {
                    panic!("unexpected successful pass1 validation for rules {source:?}")
                }
                (Pass, Err(e)) => {
                    panic!(
                        "unexpected error in pass1 validation for rules {source:?}: {:?}",
                        e.explain(source)
                    )
                }
                _ => {}
            }
        }
    }

    #[test]
    fn test_pass1_validate_global_filters() {
        let sources = [
            (Pass, r":: [a-z];"),
            (Pass, r":: ([a-z]);"),
            (Pass, r":: [a-z] ; :: ([a-z]);"),
            (Fail, r":: [a-z] ; :: [a-z] ;"),
            (Fail, r":: ([a-z]) ; :: ([a-z]) ;"),
            (Fail, r":: ([a-z]) ; :: [a-z] ;"),
            (Pass, r":: [a-z] ; :: Remove ; :: ([a-z]) ;"),
            (Fail, r":: Remove ; :: [a-z] ;"),
            (Fail, r":: ([a-z]) ; :: Remove ;"),
        ];

        for (expected_outcome, source) in sources {
            let rules = parse(source);
            let result = Pass1::run(Direction::Both, &rules);
            match (expected_outcome, result) {
                (Fail, Ok(_)) => {
                    panic!("unexpected successful pass1 validation for rules {source:?}")
                }
                (Pass, Err(e)) => {
                    panic!(
                        "unexpected error in pass1 validation for rules {source:?}: {:?}",
                        e.explain(source)
                    )
                }
                _ => {}
            }
        }
    }
}
