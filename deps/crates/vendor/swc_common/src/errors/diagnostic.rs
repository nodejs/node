// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::fmt;

use super::{snippet::Style, Applicability, CodeSuggestion, Level, Substitution, SubstitutionPart};
use crate::syntax_pos::{MultiSpan, Span};

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
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
pub struct Message(pub String, pub Style);

#[must_use]
#[derive(Clone, Debug, PartialEq, Eq, Hash, Default)]
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
/// Represents a diagnostic message with its level, message, unique identifier,
/// span, children, and suggestions.
pub struct Diagnostic {
    /// The level of the diagnostic (e.g., error, warning, help)
    pub level: Level,
    /// The message(s) associated with the diagnostic
    pub message: Vec<Message>,
    /// A unique identifier for the diagnostic, which can be used to look up
    /// more information
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub code: Option<DiagnosticId>,
    /// The span of the source code where the diagnostic is located
    pub span: MultiSpan,
    /// Child diagnostics that are related to this diagnostic
    pub children: Vec<SubDiagnostic>,
    /// Suggestions for how to fix the issue identified by the diagnostic
    pub suggestions: Vec<CodeSuggestion>,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
#[cfg_attr(
    feature = "diagnostic-serde",
    derive(serde::Serialize, serde::Deserialize)
)]
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
pub enum DiagnosticId {
    Error(String),
    Lint(String),
}

/// For example a note attached to an error.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
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
pub struct SubDiagnostic {
    pub level: Level,
    pub message: Vec<Message>,
    pub span: MultiSpan,
    #[cfg_attr(
        feature = "encoding-impl",
        encoding(with = "cbor4ii::core::types::Maybe")
    )]
    pub render_span: Option<MultiSpan>,
}

#[derive(PartialEq, Eq, Default)]
pub struct DiagnosticStyledString(pub Vec<StringPart>);

impl DiagnosticStyledString {
    pub fn new() -> DiagnosticStyledString {
        Default::default()
    }

    pub fn push_normal<S: Into<String>>(&mut self, t: S) {
        self.0.push(StringPart::Normal(t.into()));
    }

    pub fn push_highlighted<S: Into<String>>(&mut self, t: S) {
        self.0.push(StringPart::Highlighted(t.into()));
    }

    pub fn normal<S: Into<String>>(t: S) -> DiagnosticStyledString {
        DiagnosticStyledString(vec![StringPart::Normal(t.into())])
    }

    pub fn highlighted<S: Into<String>>(t: S) -> DiagnosticStyledString {
        DiagnosticStyledString(vec![StringPart::Highlighted(t.into())])
    }

    pub fn content(&self) -> String {
        self.0.iter().map(|x| x.content()).collect::<String>()
    }
}

#[derive(PartialEq, Eq)]
pub enum StringPart {
    Normal(String),
    Highlighted(String),
}

impl StringPart {
    pub fn content(&self) -> &str {
        match self {
            &StringPart::Normal(ref s) | &StringPart::Highlighted(ref s) => s,
        }
    }
}

impl Diagnostic {
    pub fn new(level: Level, message: &str) -> Self {
        Diagnostic::new_with_code(level, None, message)
    }

    pub fn new_with_code(level: Level, code: Option<DiagnosticId>, message: &str) -> Self {
        Diagnostic {
            level,
            message: vec![Message(message.to_owned(), Style::NoStyle)],
            code,
            span: MultiSpan::new(),
            children: Vec::new(),
            suggestions: Vec::new(),
        }
    }

    pub fn is_error(&self) -> bool {
        match self.level {
            Level::Bug | Level::Fatal | Level::PhaseFatal | Level::Error | Level::FailureNote => {
                true
            }

            Level::Warning | Level::Note | Level::Help | Level::Cancelled => false,
        }
    }

    /// Cancel the diagnostic (a structured diagnostic must either be emitted or
    /// canceled or it will panic when dropped).
    pub fn cancel(&mut self) {
        self.level = Level::Cancelled;
    }

    pub fn cancelled(&self) -> bool {
        self.level == Level::Cancelled
    }

    /// Add a span/label to be included in the resulting snippet.
    /// This is pushed onto the `MultiSpan` that was created when the
    /// diagnostic was first built. If you don't call this function at
    /// all, and you just supplied a `Span` to create the diagnostic,
    /// then the snippet will just include that `Span`, which is
    /// called the primary span.
    pub fn span_label<T: Into<String>>(&mut self, span: Span, label: T) -> &mut Self {
        self.span.push_span_label(span, label.into());
        self
    }

    pub fn replace_span_with(&mut self, after: Span) -> &mut Self {
        let before = self.span.clone();
        self.set_span(after);
        for span_label in before.span_labels() {
            if let Some(label) = span_label.label {
                self.span_label(after, label);
            }
        }
        self
    }

    pub fn note_expected_found(
        &mut self,
        label: &dyn fmt::Display,
        expected: DiagnosticStyledString,
        found: DiagnosticStyledString,
    ) -> &mut Self {
        self.note_expected_found_extra(label, expected, found, &"", &"")
    }

    pub fn note_expected_found_extra(
        &mut self,
        label: &dyn fmt::Display,
        expected: DiagnosticStyledString,
        found: DiagnosticStyledString,
        expected_extra: &dyn fmt::Display,
        found_extra: &dyn fmt::Display,
    ) -> &mut Self {
        let mut msg: Vec<_> = vec![Message(format!("expected {label} `"), Style::NoStyle)];
        msg.extend(expected.0.iter().map(|x| match *x {
            StringPart::Normal(ref s) => Message(s.to_owned(), Style::NoStyle),
            StringPart::Highlighted(ref s) => Message(s.to_owned(), Style::Highlight),
        }));
        msg.push(Message(format!("`{expected_extra}\n"), Style::NoStyle));
        msg.push(Message(format!("   found {label} `"), Style::NoStyle));
        msg.extend(found.0.iter().map(|x| match *x {
            StringPart::Normal(ref s) => Message(s.to_owned(), Style::NoStyle),
            StringPart::Highlighted(ref s) => Message(s.to_owned(), Style::Highlight),
        }));
        msg.push(Message(format!("`{found_extra}"), Style::NoStyle));

        // For now, just attach these as notes
        self.highlighted_note(msg);
        self
    }

    pub fn note_trait_signature(&mut self, name: String, signature: String) -> &mut Self {
        self.highlighted_note(vec![
            Message(format!("`{name}` from trait: `"), Style::NoStyle),
            Message(signature, Style::Highlight),
            Message("`".to_string(), Style::NoStyle),
        ]);
        self
    }

    pub fn note(&mut self, msg: &str) -> &mut Self {
        self.sub(Level::Note, msg, MultiSpan::new(), None);
        self
    }

    pub fn highlighted_note(&mut self, msg: Vec<Message>) -> &mut Self {
        self.sub_with_highlights(Level::Note, msg, MultiSpan::new(), None);
        self
    }

    pub fn span_note<S: Into<MultiSpan>>(&mut self, sp: S, msg: &str) -> &mut Self {
        self.sub(Level::Note, msg, sp.into(), None);
        self
    }

    pub fn warn(&mut self, msg: &str) -> &mut Self {
        self.sub(Level::Warning, msg, MultiSpan::new(), None);
        self
    }

    pub fn span_warn<S: Into<MultiSpan>>(&mut self, sp: S, msg: &str) -> &mut Self {
        self.sub(Level::Warning, msg, sp.into(), None);
        self
    }

    pub fn help(&mut self, msg: &str) -> &mut Self {
        self.sub(Level::Help, msg, MultiSpan::new(), None);
        self
    }

    pub fn span_help<S: Into<MultiSpan>>(&mut self, sp: S, msg: &str) -> &mut Self {
        self.sub(Level::Help, msg, sp.into(), None);
        self
    }

    /// Prints out a message with a suggested edit of the code. If the
    /// suggestion is presented inline it will only show the text message
    /// and not the text.
    ///
    /// See `CodeSuggestion` for more information.
    #[deprecated(note = "Use `span_suggestion_short_with_applicability`")]
    pub fn span_suggestion_short(&mut self, sp: Span, msg: &str, suggestion: String) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: vec![Substitution {
                parts: vec![SubstitutionPart {
                    snippet: suggestion,
                    span: sp,
                }],
            }],
            msg: msg.to_owned(),
            show_code_when_inline: false,
            applicability: Applicability::Unspecified,
        });
        self
    }

    /// Prints out a message with a suggested edit of the code.
    ///
    /// In case of short messages and a simple suggestion,
    /// rustc displays it as a label like
    ///
    /// "try adding parentheses: `(tup.0).1`"
    ///
    /// The message
    ///
    /// * should not end in any punctuation (a `:` is added automatically)
    /// * should not be a question
    /// * should not contain any parts like "the following", "as shown"
    /// * may look like "to do xyz, use" or "to do xyz, use abc"
    /// * may contain a name of a function, variable or type, but not whole
    ///   expressions
    ///
    /// See `CodeSuggestion` for more information.
    #[deprecated(note = "Use `span_suggestion_with_applicability`")]
    pub fn span_suggestion(&mut self, sp: Span, msg: &str, suggestion: String) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: vec![Substitution {
                parts: vec![SubstitutionPart {
                    snippet: suggestion,
                    span: sp,
                }],
            }],
            msg: msg.to_owned(),
            show_code_when_inline: true,
            applicability: Applicability::Unspecified,
        });
        self
    }

    pub fn multipart_suggestion_with_applicability(
        &mut self,
        msg: &str,
        suggestion: Vec<(Span, String)>,
        applicability: Applicability,
    ) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: vec![Substitution {
                parts: suggestion
                    .into_iter()
                    .map(|(span, snippet)| SubstitutionPart { snippet, span })
                    .collect(),
            }],
            msg: msg.to_owned(),
            show_code_when_inline: true,
            applicability,
        });
        self
    }

    #[deprecated(note = "Use `multipart_suggestion_with_applicability`")]
    pub fn multipart_suggestion(
        &mut self,
        msg: &str,
        suggestion: Vec<(Span, String)>,
    ) -> &mut Self {
        self.multipart_suggestion_with_applicability(msg, suggestion, Applicability::Unspecified)
    }

    /// Prints out a message with multiple suggested edits of the code.
    #[deprecated(note = "Use `span_suggestions_with_applicability`")]
    pub fn span_suggestions(&mut self, sp: Span, msg: &str, suggestions: Vec<String>) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: suggestions
                .into_iter()
                .map(|snippet| Substitution {
                    parts: vec![SubstitutionPart { snippet, span: sp }],
                })
                .collect(),
            msg: msg.to_owned(),
            show_code_when_inline: true,
            applicability: Applicability::Unspecified,
        });
        self
    }

    /// This is a suggestion that may contain mistakes or fillers and should
    /// be read and understood by a human.
    pub fn span_suggestion_with_applicability(
        &mut self,
        sp: Span,
        msg: &str,
        suggestion: String,
        applicability: Applicability,
    ) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: vec![Substitution {
                parts: vec![SubstitutionPart {
                    snippet: suggestion,
                    span: sp,
                }],
            }],
            msg: msg.to_owned(),
            show_code_when_inline: true,
            applicability,
        });
        self
    }

    pub fn span_suggestions_with_applicability(
        &mut self,
        sp: Span,
        msg: &str,
        suggestions: impl Iterator<Item = String>,
        applicability: Applicability,
    ) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: suggestions
                .map(|snippet| Substitution {
                    parts: vec![SubstitutionPart { snippet, span: sp }],
                })
                .collect(),
            msg: msg.to_owned(),
            show_code_when_inline: true,
            applicability,
        });
        self
    }

    pub fn span_suggestion_short_with_applicability(
        &mut self,
        sp: Span,
        msg: &str,
        suggestion: String,
        applicability: Applicability,
    ) -> &mut Self {
        self.suggestions.push(CodeSuggestion {
            substitutions: vec![Substitution {
                parts: vec![SubstitutionPart {
                    snippet: suggestion,
                    span: sp,
                }],
            }],
            msg: msg.to_owned(),
            show_code_when_inline: false,
            applicability,
        });
        self
    }

    pub fn set_span<S: Into<MultiSpan>>(&mut self, sp: S) -> &mut Self {
        self.span = sp.into();
        self
    }

    pub fn code(&mut self, s: DiagnosticId) -> &mut Self {
        self.code = Some(s);
        self
    }

    pub fn get_code(&self) -> Option<DiagnosticId> {
        self.code.clone()
    }

    pub fn message(&self) -> String {
        self.message
            .iter()
            .map(|i| i.0.as_str())
            .collect::<String>()
    }

    pub fn styled_message(&self) -> &Vec<Message> {
        &self.message
    }

    /// Used by a lint. Copies over all details *but* the "main
    /// message".
    pub fn copy_details_not_message(&mut self, from: &Diagnostic) {
        self.span = from.span.clone();
        self.code.clone_from(&from.code);
        self.children.extend(from.children.iter().cloned())
    }

    /// Convenience function for internal use, clients should use one of the
    /// public methods above.
    pub fn sub(
        &mut self,
        level: Level,
        message: &str,
        span: MultiSpan,
        render_span: Option<MultiSpan>,
    ) {
        let sub = SubDiagnostic {
            level,
            message: vec![Message(message.to_owned(), Style::NoStyle)],
            span,
            render_span,
        };
        self.children.push(sub);
    }

    /// Convenience function for internal use, clients should use one of the
    /// public methods above.
    fn sub_with_highlights(
        &mut self,
        level: Level,
        message: Vec<Message>,
        span: MultiSpan,
        render_span: Option<MultiSpan>,
    ) {
        let sub = SubDiagnostic {
            level,
            message,
            span,
            render_span,
        };
        self.children.push(sub);
    }
}

impl SubDiagnostic {
    pub fn message(&self) -> String {
        self.message
            .iter()
            .map(|i| i.0.as_str())
            .collect::<String>()
    }

    pub fn styled_message(&self) -> &Vec<Message> {
        &self.message
    }
}
