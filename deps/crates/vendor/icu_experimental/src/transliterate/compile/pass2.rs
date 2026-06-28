// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::*;
use crate::transliterate::provider as ds;
use alloc::borrow::ToOwned;
use alloc::collections::BTreeMap;
use alloc::string::ToString;
use core::fmt::{self, Display, Formatter};
use icu_collections::codepointinvlist::CodePointInversionList;
use icu_locale_core::Locale;
use zerovec::{vecs::Index32, VarZeroVec};

type Result<T> = core::result::Result<T, CompileError>;

macro_rules! impl_insert {
    ($fn_name:ident, $field:ident, $elt_type:ty, $($next_field:tt)*) => {
        fn $fn_name(&mut self, elt: $elt_type) -> char {
            // pass 1 is responsible for this
            debug_assert!(self.$field.current < self.$($next_field)*);
            // the whole PUP (15) consists of valid chars
            let standin = char::try_from(self.$field.current).unwrap();
            self.$field.vec.push(elt);
            self.$field.current += 1;
            standin
        }
    };
}

struct MutVarTableField<T> {
    vec: Vec<T>,
    base: u32,
    current: u32,
}

// a mutable version of the zero-copy `VarTable` that allows easy construction
struct MutVarTable {
    compounds: MutVarTableField<String>,
    quantifiers_opt: MutVarTableField<String>,
    quantifiers_kleene: MutVarTableField<String>,
    quantifiers_kleene_plus: MutVarTableField<String>,
    segments: MutVarTableField<ds::Segment<'static>>,
    unicode_sets: MutVarTableField<parse::UnicodeSet>,
    function_calls: MutVarTableField<ds::FunctionCall<'static>>,
    left_placeholder_base: u32,
    right_placeholder_base: u32,
    backref_base: u32,
    counts: pass1::SpecialConstructCounts,
}

impl MutVarTable {
    fn try_new_from_counts(counts: pass1::SpecialConstructCounts) -> Result<Self> {
        if counts.num_total() > ds::VarTable::NUM_DYNAMIC {
            return Err(CompileErrorKind::TooManySpecials.into());
        }

        let compounds_base = ds::VarTable::BASE as u32;
        let quantifiers_opt_base = compounds_base + counts.num_compounds as u32;
        let quantifiers_kleene_base = quantifiers_opt_base + counts.num_quantifiers_opt as u32;
        let quantifiers_kleene_plus_base =
            quantifiers_kleene_base + counts.num_quantifiers_kleene as u32;
        let segments_base =
            quantifiers_kleene_plus_base + counts.num_quantifiers_kleene_plus as u32;
        let unicode_sets_base = segments_base + counts.num_segments as u32;
        let function_calls_base = unicode_sets_base + counts.num_unicode_sets as u32;

        let left_placeholder_base = function_calls_base + counts.num_function_calls as u32;

        // if placeholders needed to encode 0, we would need to add 1 to this. they don't, so this
        // is fine.
        let right_placeholder_base = left_placeholder_base + counts.max_left_placeholders;
        // same here
        let backref_base = right_placeholder_base + counts.max_right_placeholders;

        Ok(Self {
            compounds: MutVarTableField {
                vec: Vec::with_capacity(counts.num_compounds),
                base: compounds_base,
                current: compounds_base,
            },
            quantifiers_opt: MutVarTableField {
                vec: Vec::with_capacity(counts.num_quantifiers_opt),
                base: quantifiers_opt_base,
                current: quantifiers_opt_base,
            },
            quantifiers_kleene: MutVarTableField {
                vec: Vec::with_capacity(counts.num_quantifiers_kleene),
                base: quantifiers_kleene_base,
                current: quantifiers_kleene_base,
            },
            quantifiers_kleene_plus: MutVarTableField {
                vec: Vec::with_capacity(counts.num_quantifiers_kleene_plus),
                base: quantifiers_kleene_plus_base,
                current: quantifiers_kleene_plus_base,
            },
            segments: MutVarTableField {
                vec: Vec::with_capacity(counts.num_segments),
                base: segments_base,
                current: segments_base,
            },
            unicode_sets: MutVarTableField {
                vec: Vec::with_capacity(counts.num_unicode_sets),
                base: unicode_sets_base,
                current: unicode_sets_base,
            },
            function_calls: MutVarTableField {
                vec: Vec::with_capacity(counts.num_function_calls),
                base: function_calls_base,
                current: function_calls_base,
            },
            counts,
            left_placeholder_base,
            right_placeholder_base,
            backref_base,
        })
    }

    impl_insert!(insert_compound, compounds, String, quantifiers_opt.base);
    impl_insert!(
        insert_quantifier_opt,
        quantifiers_opt,
        String,
        quantifiers_kleene.base
    );
    impl_insert!(
        insert_quantifier_kleene,
        quantifiers_kleene,
        String,
        quantifiers_kleene_plus.base
    );
    impl_insert!(
        insert_quantifier_kleene_plus,
        quantifiers_kleene_plus,
        String,
        segments.base
    );
    impl_insert!(
        insert_segment,
        segments,
        ds::Segment<'static>,
        unicode_sets.base
    );
    impl_insert!(
        insert_unicode_set,
        unicode_sets,
        parse::UnicodeSet,
        function_calls.base
    );
    impl_insert!(
        insert_function_call,
        function_calls,
        ds::FunctionCall<'static>,
        backref_base
    );

    fn standin_for_cursor(&self, left: u32, right: u32) -> char {
        match (left, right) {
            (0, 0) => ds::VarTable::RESERVED_PURE_CURSOR,
            (left, 0) => {
                debug_assert!(left <= self.counts.max_left_placeholders);
                #[expect(clippy::unwrap_used)] // constructor checks this via num_totals
                char::try_from(self.left_placeholder_base + left - 1).unwrap()
            }
            (0, right) => {
                debug_assert!(right <= self.counts.max_right_placeholders);
                #[expect(clippy::unwrap_used)] // constructor checks this via num_totals
                char::try_from(self.right_placeholder_base + right - 1).unwrap()
            }
            _ => {
                // validation catches (>0, >0) cases
                unreachable!()
            }
        }
    }

    fn standin_for_backref(&self, backref_num: u32) -> char {
        debug_assert!(backref_num > 0);
        // -1 because backrefs are 1-indexed
        let standin = self.backref_base + backref_num - 1;
        debug_assert!(standin <= ds::VarTable::MAX_DYNAMIC as u32);
        #[expect(clippy::unwrap_used)] // constructor checks this via num_totals
        char::try_from(standin).unwrap()
    }

    fn finalize(&self) -> ds::VarTable<'static> {
        // we computed the exact counts, so when we call finalize, we should be full
        debug_assert!(self.is_full());

        ds::VarTable {
            compounds: VarZeroVec::from(&self.compounds.vec),
            quantifiers_opt: VarZeroVec::from(&self.quantifiers_opt.vec),
            quantifiers_kleene: VarZeroVec::from(&self.quantifiers_kleene.vec),
            quantifiers_kleene_plus: VarZeroVec::from(&self.quantifiers_kleene_plus.vec),
            segments: VarZeroVec::from(&self.segments.vec),
            unicode_sets: VarZeroVec::from(&self.unicode_sets.vec),
            function_calls: VarZeroVec::from(&self.function_calls.vec),
            // these casts are safe, because the constructor checks that everything is in range,
            // and the range has a length of less than 2^16
            max_left_placeholder_count: self.counts.max_left_placeholders as u16,
            max_right_placeholder_count: self.counts.max_right_placeholders as u16,
        }
    }

    fn is_full(&self) -> bool {
        self.compounds.vec.len() == self.counts.num_compounds
            && self.quantifiers_opt.vec.len() == self.counts.num_quantifiers_opt
            && self.quantifiers_kleene.vec.len() == self.counts.num_quantifiers_kleene
            && self.quantifiers_kleene_plus.vec.len() == self.counts.num_quantifiers_kleene_plus
            && self.segments.vec.len() == self.counts.num_segments
            && self.unicode_sets.vec.len() == self.counts.num_unicode_sets
            && self.function_calls.vec.len() == self.counts.num_function_calls
    }
}

enum LiteralOrStandin<'a> {
    Literal(&'a str),
    Standin(char),
}

// gives us `to_string` and makes clippy happy
impl Display for LiteralOrStandin<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        match *self {
            LiteralOrStandin::Literal(s) => write!(f, "{s}"),
            LiteralOrStandin::Standin(c) => write!(f, "{c}"),
        }
    }
}

pub(super) struct Pass2<'a, 'p> {
    var_table: MutVarTable,
    var_definitions: &'a BTreeMap<String, &'p [parse::Element]>,
    // transliterators available at datagen time exist in this mapping from legacy ID to BCP47 ID
    id_mapping: &'a BTreeMap<String, Locale>,
    // the inverse of VarTable.compounds
    var_to_char: BTreeMap<String, char>,
    // the current segment index (per conversion rule)
    curr_segment: u16,
}

impl<'a, 'p> Pass2<'a, 'p> {
    pub(super) fn run(
        pass1: pass1::DirectedPass1Result<'p>,
        var_definitions: &'a BTreeMap<String, &'p [parse::Element]>,
        id_mapping: &'a BTreeMap<String, Locale>,
    ) -> Result<ds::RuleBasedTransliterator<'static>> {
        let mut pass2 = Pass2 {
            var_table: MutVarTable::try_new_from_counts(pass1.data.counts)?,
            var_definitions,
            id_mapping,
            var_to_char: BTreeMap::new(),
            curr_segment: 0,
        };
        let mut compiled_transform_groups: Vec<VarZeroVec<'static, ds::SimpleIdULE>> = Vec::new();
        let mut compiled_conversion_groups: Vec<VarZeroVec<'static, ds::RuleULE, Index32>> =
            Vec::new();

        for (transform_group, conversion_group) in pass1.groups {
            let compiled_transform_group: Vec<_> = transform_group
                .into_iter()
                .map(|id| pass2.compile_single_id(id.into_owned()))
                .collect();
            compiled_transform_groups.push(VarZeroVec::from(&compiled_transform_group));

            let compiled_conversion_group: Vec<_> = conversion_group
                .into_iter()
                .map(|rule| pass2.compile_conversion_rule(rule))
                .collect();
            compiled_conversion_groups.push(VarZeroVec::from(&compiled_conversion_group));
        }

        Ok(ds::RuleBasedTransliterator {
            visibility: true,
            filter: pass1
                .filter
                .unwrap_or(const { CodePointInversionList::all() }),
            id_group_list: VarZeroVec::from(&compiled_transform_groups),
            rule_group_list: VarZeroVec::from(&compiled_conversion_groups),
            variable_table: pass2.var_table.finalize(),
        })
    }

    fn compile_conversion_rule(
        &mut self,
        rule: rule_group_agg::UniConversionRule<'p>,
    ) -> ds::Rule<'static> {
        self.curr_segment = 0;
        let ante = self.compile_section(rule.ante, parse::ElementLocation::Source);
        let key = self.compile_section(rule.key, parse::ElementLocation::Source);
        let post = self.compile_section(rule.post, parse::ElementLocation::Source);
        let replacer = self.compile_section(rule.replacement, parse::ElementLocation::Target);
        ds::Rule {
            ante: ante.into(),
            key: key.into(),
            post: post.into(),
            replacer: replacer.into(),
        }
    }

    // returns the standin char
    fn compile_variable(&mut self, var: &str) -> char {
        if let Some(&c) = self.var_to_char.get(var) {
            return c;
        }
        // the first pass ensures that all variables are defined
        let definition = self.var_definitions[var];
        let compiled = self.compile_section(definition, parse::ElementLocation::VariableDefinition);
        let standin = self.var_table.insert_compound(compiled);
        self.var_to_char.insert(var.to_owned(), standin);
        standin
    }

    fn compile_section(
        &mut self,
        section: &'p [parse::Element],
        loc: parse::ElementLocation,
    ) -> String {
        let mut result = String::new();
        for elt in section {
            if elt.kind().skipped_in(loc) {
                continue;
            }
            match self.compile_element(elt, loc) {
                LiteralOrStandin::Literal(s) => result.push_str(s),
                LiteralOrStandin::Standin(c) => result.push(c),
            }
        }
        result
    }

    fn compile_element(
        &mut self,
        elt: &'p parse::Element,
        loc: parse::ElementLocation,
    ) -> LiteralOrStandin<'p> {
        debug_assert!(!elt.kind().skipped_in(loc));
        match elt {
            parse::Element::Literal(s) => LiteralOrStandin::Literal(s),
            parse::Element::VariableRef(v) => LiteralOrStandin::Standin(self.compile_variable(v)),
            parse::Element::Quantifier(kind, inner) => {
                let inner = self.compile_element(inner, loc).to_string();
                let standin = match kind {
                    parse::QuantifierKind::ZeroOrOne => self.var_table.insert_quantifier_opt(inner),
                    parse::QuantifierKind::ZeroOrMore => {
                        self.var_table.insert_quantifier_kleene(inner)
                    }
                    parse::QuantifierKind::OneOrMore => {
                        self.var_table.insert_quantifier_kleene_plus(inner)
                    }
                };
                LiteralOrStandin::Standin(standin)
            }
            &parse::Element::BackRef(num) => {
                LiteralOrStandin::Standin(self.var_table.standin_for_backref(num))
            }
            parse::Element::AnchorEnd => {
                LiteralOrStandin::Standin(ds::VarTable::RESERVED_ANCHOR_END)
            }
            parse::Element::AnchorStart => {
                LiteralOrStandin::Standin(ds::VarTable::RESERVED_ANCHOR_START)
            }
            parse::Element::UnicodeSet(set) => {
                let standin = self.var_table.insert_unicode_set(set.clone());
                LiteralOrStandin::Standin(standin)
            }
            parse::Element::Segment(inner) => {
                // important to store the number before the recursive call
                let idx = self.curr_segment;
                self.curr_segment += 1;
                let inner = self.compile_section(inner, loc);
                let standin = self.var_table.insert_segment(ds::Segment {
                    idx,
                    content: inner.into(),
                });
                LiteralOrStandin::Standin(standin)
            }
            parse::Element::FunctionCall(id, inner) => {
                let inner = self.compile_section(inner, loc);
                let id = self.compile_single_id(id.clone());
                let standin = self.var_table.insert_function_call(ds::FunctionCall {
                    translit: id,
                    arg: inner.into(),
                });
                LiteralOrStandin::Standin(standin)
            }
            &parse::Element::Cursor(pre, post) => {
                LiteralOrStandin::Standin(self.var_table.standin_for_cursor(pre, post))
            }
        }
    }

    fn compile_single_id(&self, id: parse::SingleId) -> ds::SimpleId<'static> {
        let unparsed = id.basic_id.to_string();

        let string = if matches!(
            unparsed.as_str(),
            "any-nfc"
                | "any-nfkc"
                | "any-nfd"
                | "any-nfkd"
                | "any-null"
                | "any-remove"
                | "any-lower"
                | "any-upper"
                | "any-hex/unicode"
                | "any-hex/rust"
                | "any-hex/xml"
                | "any-hex/perl"
                | "any-hex/plain"
        ) {
            unparsed
        } else if let Some(bcp47_id) = self.id_mapping.get(&unparsed) {
            bcp47_id.to_string()
        } else {
            icu_provider::log::warn!("Reference to unknown transliterator: {unparsed}");
            format!("x-{unparsed}")
        };
        ds::SimpleId {
            id: string.into(),
            filter: id.filter.unwrap_or(CodePointInversionList::all()),
        }
    }
}
