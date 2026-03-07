// Copyright 2012-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{
    borrow::Cow,
    cell::RefCell,
    error, fmt,
    io::Write,
    panic,
    sync::atomic::{AtomicUsize, Ordering::SeqCst},
};

use rustc_hash::FxHashSet;
#[cfg(feature = "tty-emitter")]
use termcolor::{Color, ColorSpec};

use self::Level::*;
pub use self::{
    diagnostic::{Diagnostic, DiagnosticId, DiagnosticStyledString, Message, SubDiagnostic},
    diagnostic_builder::DiagnosticBuilder,
    emitter::{ColorConfig, Emitter, EmitterWriter},
    snippet::Style,
};
use crate::{
    rustc_data_structures::stable_hasher::StableHasher,
    sync::{Lock, LockCell, Lrc},
    syntax_pos::{BytePos, FileLinesResult, FileName, Loc, MultiSpan, Span},
    SpanSnippetError,
};

mod diagnostic;
mod diagnostic_builder;

pub mod emitter;
mod lock;
mod snippet;
mod styled_buffer;

#[derive(Copy, Clone, Debug, PartialEq, Eq, Hash)]
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
pub enum Applicability {
    MachineApplicable,
    HasPlaceholders,
    MaybeIncorrect,
    Unspecified,
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
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct CodeSuggestion {
    /// Each substitute can have multiple variants due to multiple
    /// applicable suggestions
    ///
    /// `foo.bar` might be replaced with `a.b` or `x.y` by replacing
    /// `foo` and `bar` on their own:
    ///
    /// ```rust,ignore
    /// vec![
    ///     Substitution {
    ///         parts: vec![(0..3, "a"), (4..7, "b")],
    ///     },
    ///     Substitution {
    ///         parts: vec![(0..3, "x"), (4..7, "y")],
    ///     },
    /// ]
    /// ```
    ///
    /// or by replacing the entire span:
    ///
    /// ```rust,ignore
    /// vec![
    ///     Substitution {
    ///         parts: vec![(0..7, "a.b")],
    ///     },
    ///     Substitution {
    ///         parts: vec![(0..7, "x.y")],
    ///     },
    /// ]
    /// ```
    pub substitutions: Vec<Substitution>,
    pub msg: String,
    pub show_code_when_inline: bool,
    /// Whether or not the suggestion is approximate
    ///
    /// Sometimes we may show suggestions with placeholders,
    /// which are useful for users but not useful for
    /// tools like rustfix
    pub applicability: Applicability,
}

#[derive(Clone, Debug, PartialEq, Eq, Hash)]
/// See the docs on `CodeSuggestion::substitutions`
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
pub struct Substitution {
    pub parts: Vec<SubstitutionPart>,
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
#[cfg_attr(feature = "rkyv-impl", repr(C))]
#[cfg_attr(
    feature = "encoding-impl",
    derive(::ast_node::Encode, ::ast_node::Decode)
)]
pub struct SubstitutionPart {
    pub span: Span,
    pub snippet: String,
}

pub type SourceMapperDyn = dyn SourceMapper;

pub trait SourceMapper: crate::sync::Send + crate::sync::Sync {
    fn lookup_char_pos(&self, pos: BytePos) -> Loc;
    fn span_to_lines(&self, sp: Span) -> FileLinesResult;
    fn span_to_string(&self, sp: Span) -> String;
    fn span_to_filename(&self, sp: Span) -> Lrc<FileName>;
    fn merge_spans(&self, sp_lhs: Span, sp_rhs: Span) -> Option<Span>;
    fn call_span_if_macro(&self, sp: Span) -> Span;
    fn doctest_offset_line(&self, line: usize) -> usize;
    fn span_to_snippet(&self, sp: Span) -> Result<String, Box<SpanSnippetError>>;
}

impl CodeSuggestion {
    /// Returns the assembled code suggestions and whether they should be shown
    /// with an underline.
    pub fn splice_lines(&self, cm: &SourceMapperDyn) -> Vec<(String, Vec<SubstitutionPart>)> {
        use crate::syntax_pos::{CharPos, SmallPos};

        fn push_trailing(
            buf: &mut String,
            line_opt: Option<&Cow<'_, str>>,
            lo: &Loc,
            hi_opt: Option<&Loc>,
        ) {
            let (lo, hi_opt) = (lo.col.to_usize(), hi_opt.map(|hi| hi.col.to_usize()));
            if let Some(line) = line_opt {
                if let Some(lo) = line.char_indices().map(|(i, _)| i).nth(lo) {
                    let hi_opt = hi_opt.and_then(|hi| line.char_indices().map(|(i, _)| i).nth(hi));
                    buf.push_str(match hi_opt {
                        Some(hi) => &line[lo..hi],
                        None => &line[lo..],
                    });
                }
                if hi_opt.is_none() {
                    buf.push('\n');
                }
            }
        }

        assert!(!self.substitutions.is_empty());

        self.substitutions
            .iter()
            .cloned()
            .map(|mut substitution| {
                // Assumption: all spans are in the same file, and all spans
                // are disjoint. Sort in ascending order.
                substitution.parts.sort_by_key(|part| part.span.lo());

                // Find the bounding span.
                let lo = substitution
                    .parts
                    .iter()
                    .map(|part| part.span.lo())
                    .min()
                    .unwrap();
                let hi = substitution
                    .parts
                    .iter()
                    .map(|part| part.span.hi())
                    .min()
                    .unwrap();
                let bounding_span = Span::new(lo, hi);
                let lines = cm.span_to_lines(bounding_span).unwrap();
                assert!(!lines.lines.is_empty());

                // To build up the result, we do this for each span:
                // - push the line segment trailing the previous span (at the beginning a
                //   "phantom" span pointing at the start of the line)
                // - push lines between the previous and current span (if any)
                // - if the previous and current span are not on the same line push the line
                //   segment leading up to the current span
                // - splice in the span substitution
                //
                // Finally push the trailing line segment of the last span
                let fm = &lines.file;
                let mut prev_hi = cm.lookup_char_pos(bounding_span.lo());
                prev_hi.col = CharPos::from_usize(0);

                let mut prev_line = fm.get_line(lines.lines[0].line_index);
                let mut buf = String::new();

                for part in &substitution.parts {
                    let cur_lo = cm.lookup_char_pos(part.span.lo());
                    if prev_hi.line == cur_lo.line {
                        push_trailing(&mut buf, prev_line.as_ref(), &prev_hi, Some(&cur_lo));
                    } else {
                        push_trailing(&mut buf, prev_line.as_ref(), &prev_hi, None);
                        // push lines between the previous and current span (if any)
                        for idx in prev_hi.line..(cur_lo.line - 1) {
                            if let Some(line) = fm.get_line(idx) {
                                buf.push_str(line.as_ref());
                                buf.push('\n');
                            }
                        }
                        if let Some(cur_line) = fm.get_line(cur_lo.line - 1) {
                            buf.push_str(&cur_line[..cur_lo.col.to_usize()]);
                        }
                    }
                    buf.push_str(&part.snippet);
                    prev_hi = cm.lookup_char_pos(part.span.hi());
                    prev_line = fm.get_line(prev_hi.line - 1);
                }
                // if the replacement already ends with a newline, don't print the next line
                if !buf.ends_with('\n') {
                    push_trailing(&mut buf, prev_line.as_ref(), &prev_hi, None);
                }
                // remove trailing newlines
                while buf.ends_with('\n') {
                    buf.pop();
                }
                (buf, substitution.parts)
            })
            .collect()
    }
}

/// Used as a return value to signify a fatal error occurred. (It is also
/// used as the argument to panic at the moment, but that will eventually
/// not be true.)
#[derive(Copy, Clone, Debug)]
#[must_use]
pub struct FatalError;

pub struct FatalErrorMarker;

// // Don't implement Send on FatalError. This makes it impossible to
// // panic!(FatalError). We don't want to invoke the panic handler and print a
// // backtrace for fatal errors.
// impl !Send for FatalError {}

impl FatalError {
    pub fn raise(self) -> ! {
        panic::resume_unwind(Box::new(FatalErrorMarker))
    }
}

impl fmt::Display for FatalError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "parser fatal error")
    }
}

impl error::Error for FatalError {
    fn description(&self) -> &str {
        "The parser has encountered a fatal error"
    }
}

/// Signifies that the compiler died with an explicit call to `.bug`
/// or `.span_bug` rather than a failed assertion, etc.
#[derive(Copy, Clone, Debug)]
pub struct ExplicitBug;

impl fmt::Display for ExplicitBug {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "parser internal bug")
    }
}

impl error::Error for ExplicitBug {
    fn description(&self) -> &str {
        "The parser has encountered an internal bug"
    }
}

/// A handler deals with errors; certain errors
/// (fatal, bug, unimpl) may cause immediate exit,
/// others log errors for later reporting.
///
/// # Example
///
/// `swc` provides a global-like variable ([HANDLER]) of type `Handler` that can
/// be used to report errors.
///
/// You can refer to [the lint rules](https://github.com/swc-project/swc/tree/main/crates/swc_ecma_lints/src/rules) for other example usages.
/// All lint rules have code for error reporting.
///
/// ## Error reporting in swc
///
/// ```rust,ignore
/// use swc_common::errors::HANDLER;
///
/// # fn main() {
///     HANDLER.with(|handler| {
///         // You can access the handler for the current file using HANDLER.with.
///
///         // We now report an error
///
///         // `struct_span_err` creates a builder for a diagnostic.
///         // The span passed to `struct_span_err` will used to point the problematic code.
///         //
///         // You may provide additional information, like a previous declaration of parameter.
///         handler
///             .struct_span_err(
///                 span,
///                 &format!("`{}` used as parameter more than once", atom),
///             )
///             .span_note(
///                 old_span,
///                 &format!("previous definition of `{}` here", atom),
///             )
///             .emit();
///     });
/// # }
/// ```
pub struct Handler {
    pub flags: HandlerFlags,

    err_count: AtomicUsize,
    emitter: Lock<Box<dyn Emitter>>,
    continue_after_error: LockCell<bool>,
    delayed_span_bugs: Lock<Vec<Diagnostic>>,

    // This set contains the `DiagnosticId` of all emitted diagnostics to avoid
    // emitting the same diagnostic with extended help (`--teach`) twice, which
    // would be unnecessary repetition.
    taught_diagnostics: Lock<FxHashSet<DiagnosticId>>,

    /// Used to suggest rustc --explain <error code>
    emitted_diagnostic_codes: Lock<FxHashSet<DiagnosticId>>,

    // This set contains a hash of every diagnostic that has been emitted by
    // this handler. These hashes is used to avoid emitting the same error
    // twice.
    emitted_diagnostics: Lock<FxHashSet<u128>>,
}

fn default_track_diagnostic(_: &Diagnostic) {}

thread_local!(pub static TRACK_DIAGNOSTICS: RefCell<Box<dyn Fn(&Diagnostic)>> =
                RefCell::new(Box::new(default_track_diagnostic)));

#[derive(Default)]
pub struct HandlerFlags {
    /// If false, warning-level lints are suppressed.
    /// (rustc: see `--allow warnings` and `--cap-lints`)
    pub can_emit_warnings: bool,
    /// If true, error-level diagnostics are upgraded to bug-level.
    /// (rustc: see `-Z treat-err-as-bug`)
    pub treat_err_as_bug: bool,
    /// If true, immediately emit diagnostics that would otherwise be buffered.
    /// (rustc: see `-Z dont-buffer-diagnostics` and `-Z treat-err-as-bug`)
    pub dont_buffer_diagnostics: bool,
    /// If true, immediately print bugs registered with `delay_span_bug`.
    /// (rustc: see `-Z report-delayed-bugs`)
    pub report_delayed_bugs: bool,
    /// show macro backtraces even for non-local macros.
    /// (rustc: see `-Z external-macro-backtrace`)
    pub external_macro_backtrace: bool,
}

impl Drop for Handler {
    fn drop(&mut self) {
        if self.err_count() == 0 {
            let mut bugs = self.delayed_span_bugs.borrow_mut();
            let has_bugs = !bugs.is_empty();
            for bug in bugs.drain(..) {
                DiagnosticBuilder::new_diagnostic(self, bug).emit();
            }
            if has_bugs {
                panic!("no errors encountered even though `delay_span_bug` issued");
            }
        }
    }
}

impl Handler {
    #[cfg(feature = "tty-emitter")]
    #[cfg_attr(docsrs, doc(cfg(feature = "tty-emitter")))]
    pub fn with_tty_emitter(
        color_config: ColorConfig,
        can_emit_warnings: bool,
        treat_err_as_bug: bool,
        cm: Option<Lrc<SourceMapperDyn>>,
    ) -> Handler {
        Handler::with_tty_emitter_and_flags(
            color_config,
            cm,
            HandlerFlags {
                can_emit_warnings,
                treat_err_as_bug,
                ..Default::default()
            },
        )
    }

    #[cfg(feature = "tty-emitter")]
    #[cfg_attr(docsrs, doc(cfg(feature = "tty-emitter")))]
    pub fn with_tty_emitter_and_flags(
        color_config: ColorConfig,
        cm: Option<Lrc<SourceMapperDyn>>,
        flags: HandlerFlags,
    ) -> Handler {
        let emitter = Box::new(EmitterWriter::stderr(color_config, cm, false, false));
        Handler::with_emitter_and_flags(emitter, flags)
    }

    /// Example implementation of [Emitter] is [EmitterWriter]
    pub fn with_emitter(
        can_emit_warnings: bool,
        treat_err_as_bug: bool,
        emitter: Box<dyn Emitter>,
    ) -> Handler {
        Handler::with_emitter_and_flags(
            emitter,
            HandlerFlags {
                can_emit_warnings,
                treat_err_as_bug,
                ..Default::default()
            },
        )
    }

    /// Calls [Self::with_emitter] with [EmitterWriter].
    pub fn with_emitter_writer(
        dst: Box<dyn Write + Send>,
        cm: Option<Lrc<SourceMapperDyn>>,
    ) -> Handler {
        Handler::with_emitter(
            true,
            false,
            Box::new(EmitterWriter::new(dst, cm, false, true)),
        )
    }

    pub fn with_emitter_and_flags(e: Box<dyn Emitter>, flags: HandlerFlags) -> Handler {
        Handler {
            flags,
            err_count: AtomicUsize::new(0),
            emitter: Lock::new(e),
            continue_after_error: LockCell::new(true),
            delayed_span_bugs: Lock::new(Vec::new()),
            taught_diagnostics: Default::default(),
            emitted_diagnostic_codes: Default::default(),
            emitted_diagnostics: Default::default(),
        }
    }

    pub fn set_continue_after_error(&self, continue_after_error: bool) {
        self.continue_after_error.set(continue_after_error);
    }

    /// Resets the diagnostic error count as well as the cached emitted
    /// diagnostics.
    ///
    /// NOTE: DO NOT call this function from rustc. It is only meant to be
    /// called from external tools that want to reuse a `Parser` cleaning
    /// the previously emitted diagnostics as well as the overall count of
    /// emitted error diagnostics.
    pub fn reset_err_count(&self) {
        // actually frees the underlying memory (which `clear` would not do)
        *self.emitted_diagnostics.borrow_mut() = Default::default();
        self.err_count.store(0, SeqCst);
    }

    pub fn struct_dummy(&self) -> DiagnosticBuilder<'_> {
        DiagnosticBuilder::new(self, Level::Cancelled, "")
    }

    pub fn struct_span_warn<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Warning, msg);
        result.set_span(sp);
        if !self.flags.can_emit_warnings {
            result.cancel();
        }
        result
    }

    pub fn struct_span_warn_with_code<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
        code: DiagnosticId,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Warning, msg);
        result.set_span(sp);
        result.code(code);
        if !self.flags.can_emit_warnings {
            result.cancel();
        }
        result
    }

    pub fn struct_warn<'a>(&'a self, msg: &str) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Warning, msg);
        if !self.flags.can_emit_warnings {
            result.cancel();
        }
        result
    }

    pub fn struct_span_err<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Error, msg);
        result.set_span(sp);
        result
    }

    pub fn struct_span_err_with_code<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
        code: DiagnosticId,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Error, msg);
        result.set_span(sp);
        result.code(code);
        result
    }

    // FIXME: This method should be removed (every error should have an associated
    // error code).
    pub fn struct_err<'a>(&'a self, msg: &str) -> DiagnosticBuilder<'a> {
        DiagnosticBuilder::new(self, Level::Error, msg)
    }

    pub fn struct_err_with_code<'a>(
        &'a self,
        msg: &str,
        code: DiagnosticId,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Error, msg);
        result.code(code);
        result
    }

    pub fn struct_span_fatal<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Fatal, msg);
        result.set_span(sp);
        result
    }

    pub fn struct_span_fatal_with_code<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
        code: DiagnosticId,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Fatal, msg);
        result.set_span(sp);
        result.code(code);
        result
    }

    pub fn struct_fatal<'a>(&'a self, msg: &str) -> DiagnosticBuilder<'a> {
        DiagnosticBuilder::new(self, Level::Fatal, msg)
    }

    pub fn cancel(&self, err: &mut DiagnosticBuilder<'_>) {
        err.cancel();
    }

    fn panic_if_treat_err_as_bug(&self) {
        if self.flags.treat_err_as_bug {
            panic!("encountered error with `-Z treat_err_as_bug");
        }
    }

    pub fn span_fatal<S: Into<MultiSpan>>(&self, sp: S, msg: &str) -> FatalError {
        self.emit(&sp.into(), msg, Fatal);
        FatalError
    }

    pub fn span_fatal_with_code<S: Into<MultiSpan>>(
        &self,
        sp: S,
        msg: &str,
        code: DiagnosticId,
    ) -> FatalError {
        self.emit_with_code(&sp.into(), msg, code, Fatal);
        FatalError
    }

    pub fn span_err<S: Into<MultiSpan>>(&self, sp: S, msg: &str) {
        self.emit(&sp.into(), msg, Error);
    }

    pub fn mut_span_err<'a, S: Into<MultiSpan>>(
        &'a self,
        sp: S,
        msg: &str,
    ) -> DiagnosticBuilder<'a> {
        let mut result = DiagnosticBuilder::new(self, Level::Error, msg);
        result.set_span(sp);
        result
    }

    pub fn span_err_with_code<S: Into<MultiSpan>>(&self, sp: S, msg: &str, code: DiagnosticId) {
        self.emit_with_code(&sp.into(), msg, code, Error);
    }

    pub fn span_warn<S: Into<MultiSpan>>(&self, sp: S, msg: &str) {
        self.emit(&sp.into(), msg, Warning);
    }

    pub fn span_warn_with_code<S: Into<MultiSpan>>(&self, sp: S, msg: &str, code: DiagnosticId) {
        self.emit_with_code(&sp.into(), msg, code, Warning);
    }

    pub fn span_bug<S: Into<MultiSpan>>(&self, sp: S, msg: &str) -> ! {
        self.emit(&sp.into(), msg, Bug);
        panic!("{}", ExplicitBug);
    }

    pub fn delay_span_bug<S: Into<MultiSpan>>(&self, sp: S, msg: &str) {
        if self.flags.treat_err_as_bug {
            // FIXME: don't abort here if report_delayed_bugs is off
            self.span_bug(sp, msg);
        }
        let mut diagnostic = Diagnostic::new(Level::Bug, msg);
        diagnostic.set_span(sp.into());
        self.delay_as_bug(diagnostic);
    }

    fn delay_as_bug(&self, diagnostic: Diagnostic) {
        if self.flags.report_delayed_bugs {
            DiagnosticBuilder::new_diagnostic(self, diagnostic.clone()).emit();
        }
        self.delayed_span_bugs.borrow_mut().push(diagnostic);
    }

    pub fn span_bug_no_panic<S: Into<MultiSpan>>(&self, sp: S, msg: &str) {
        self.emit(&sp.into(), msg, Bug);
    }

    pub fn span_note_without_error<S: Into<MultiSpan>>(&self, sp: S, msg: &str) {
        self.emit(&sp.into(), msg, Note);
    }

    pub fn span_note_diag<'a>(&'a self, sp: Span, msg: &str) -> DiagnosticBuilder<'a> {
        let mut db = DiagnosticBuilder::new(self, Note, msg);
        db.set_span(sp);
        db
    }

    pub fn span_unimpl<S: Into<MultiSpan>>(&self, sp: S, msg: &str) -> ! {
        self.span_bug(sp, &format!("unimplemented {msg}"));
    }

    pub fn failure(&self, msg: &str) {
        DiagnosticBuilder::new(self, FailureNote, msg).emit()
    }

    pub fn fatal(&self, msg: &str) -> FatalError {
        if self.flags.treat_err_as_bug {
            self.bug(msg);
        }
        DiagnosticBuilder::new(self, Fatal, msg).emit();
        FatalError
    }

    pub fn err(&self, msg: &str) {
        if self.flags.treat_err_as_bug {
            self.bug(msg);
        }
        let mut db = DiagnosticBuilder::new(self, Error, msg);
        db.emit();
    }

    pub fn err_with_code(&self, msg: &str, code: DiagnosticId) {
        if self.flags.treat_err_as_bug {
            self.bug(msg);
        }
        let mut db = DiagnosticBuilder::new_with_code(self, Error, Some(code), msg);
        db.emit();
    }

    pub fn warn(&self, msg: &str) {
        let mut db = DiagnosticBuilder::new(self, Warning, msg);
        db.emit();
    }

    pub fn note_without_error(&self, msg: &str) {
        let mut db = DiagnosticBuilder::new(self, Note, msg);
        db.emit();
    }

    pub fn bug(&self, msg: &str) -> ! {
        let mut db = DiagnosticBuilder::new(self, Bug, msg);
        db.emit();
        panic!("{}", ExplicitBug);
    }

    pub fn unimpl(&self, msg: &str) -> ! {
        self.bug(&format!("unimplemented {msg}"));
    }

    fn bump_err_count(&self) {
        self.panic_if_treat_err_as_bug();
        self.err_count.fetch_add(1, SeqCst);
    }

    pub fn err_count(&self) -> usize {
        self.err_count.load(SeqCst)
    }

    pub fn has_errors(&self) -> bool {
        self.err_count() > 0
    }

    pub fn print_error_count(&self) {
        let s = match self.err_count() {
            0 => return,
            1 => "aborting due to previous error".to_string(),
            _ => format!("aborting due to {} previous errors", self.err_count()),
        };

        let _ = self.fatal(&s);

        let can_show_explain = self.emitter.borrow().should_show_explain();
        let are_there_diagnostics = !self.emitted_diagnostic_codes.borrow().is_empty();
        if can_show_explain && are_there_diagnostics {
            let mut error_codes = self
                .emitted_diagnostic_codes
                .borrow()
                .iter()
                .filter_map(|x| match *x {
                    DiagnosticId::Error(ref s) => Some(s.clone()),
                    _ => None,
                })
                .collect::<Vec<_>>();
            if !error_codes.is_empty() {
                error_codes.sort();
                if error_codes.len() > 1 {
                    let limit = if error_codes.len() > 9 {
                        9
                    } else {
                        error_codes.len()
                    };
                    self.failure(&format!(
                        "Some errors occurred: {}{}",
                        error_codes[..limit].join(", "),
                        if error_codes.len() > 9 { "..." } else { "." }
                    ));
                    self.failure(&format!(
                        "For more information about an error, try `rustc --explain {}`.",
                        &error_codes[0]
                    ));
                } else {
                    self.failure(&format!(
                        "For more information about this error, try `rustc --explain {}`.",
                        &error_codes[0]
                    ));
                }
            }
        }
    }

    pub fn abort_if_errors(&self) {
        if self.err_count() == 0 {
            return;
        }
        FatalError.raise();
    }

    pub fn emit(&self, msp: &MultiSpan, msg: &str, lvl: Level) {
        if lvl == Warning && !self.flags.can_emit_warnings {
            return;
        }
        let mut db = DiagnosticBuilder::new(self, lvl, msg);
        db.set_span(msp.clone());
        db.emit();
        if !self.continue_after_error.get() {
            self.abort_if_errors();
        }
    }

    pub fn emit_with_code(&self, msp: &MultiSpan, msg: &str, code: DiagnosticId, lvl: Level) {
        if lvl == Warning && !self.flags.can_emit_warnings {
            return;
        }
        let mut db = DiagnosticBuilder::new_with_code(self, lvl, Some(code), msg);
        db.set_span(msp.clone());
        db.emit();
        if !self.continue_after_error.get() {
            self.abort_if_errors();
        }
    }

    /// `true` if we haven't taught a diagnostic with this code already.
    /// The caller must then teach the user about such a diagnostic.
    ///
    /// Used to suppress emitting the same error multiple times with extended
    /// explanation when calling `-Z teach`.
    pub fn must_teach(&self, code: &DiagnosticId) -> bool {
        self.taught_diagnostics.borrow_mut().insert(code.clone())
    }

    pub fn force_print_db(&self, mut db: DiagnosticBuilder<'_>) {
        self.emitter.borrow_mut().emit(&mut db);
        db.cancel();
    }

    fn emit_db(&self, db: &mut DiagnosticBuilder<'_>) {
        let diagnostic = &**db;

        TRACK_DIAGNOSTICS.with(|track_diagnostics| {
            track_diagnostics.borrow()(diagnostic);
        });

        if let Some(ref code) = diagnostic.code {
            self.emitted_diagnostic_codes
                .borrow_mut()
                .insert(code.clone());
        }

        let diagnostic_hash = {
            use std::hash::Hash;
            let mut hasher = StableHasher::new();
            diagnostic.hash(&mut hasher);
            hasher.finish()
        };

        // Only emit the diagnostic if we haven't already emitted an equivalent
        // one:
        if self
            .emitted_diagnostics
            .borrow_mut()
            .insert(diagnostic_hash)
        {
            self.emitter.borrow_mut().emit(db);
            if db.is_error() {
                self.bump_err_count();
            }
        }
    }

    pub fn take_diagnostics(&self) -> Vec<String> {
        self.emitter.borrow_mut().take_diagnostics()
    }
}

#[derive(Copy, PartialEq, Eq, Clone, Hash, Debug, Default)]
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
pub enum Level {
    Bug,
    Fatal,
    // An error which while not immediately fatal, should stop the compiler
    // progressing beyond the current phase.
    PhaseFatal,
    Error,
    Warning,
    Note,
    Help,
    #[default]
    Cancelled,
    FailureNote,
}

impl fmt::Display for Level {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.to_str().fmt(f)
    }
}

impl Level {
    #[cfg(feature = "tty-emitter")]
    fn color(self) -> ColorSpec {
        let mut spec = ColorSpec::new();
        match self {
            Bug | Fatal | PhaseFatal | Error => {
                spec.set_fg(Some(Color::Red)).set_intense(true);
            }
            Warning => {
                spec.set_fg(Some(Color::Yellow)).set_intense(cfg!(windows));
            }
            Note => {
                spec.set_fg(Some(Color::Green)).set_intense(true);
            }
            Help => {
                spec.set_fg(Some(Color::Cyan)).set_intense(true);
            }
            FailureNote => {}
            Cancelled => unreachable!(),
        }
        spec
    }

    pub fn to_str(self) -> &'static str {
        match self {
            Bug => "error: internal compiler error",
            Fatal | PhaseFatal | Error => "error",
            Warning => "warning",
            Note => "note",
            Help => "help",
            FailureNote => "",
            Cancelled => panic!("Shouldn't call on cancelled error"),
        }
    }

    pub fn is_failure_note(self) -> bool {
        matches!(self, FailureNote)
    }
}

better_scoped_tls::scoped_tls!(
    /// Used for error reporting in transform.
    ///
    /// This should be only used for errors from the api which does not returning errors.
    ///
    /// e.g.
    ///  - `parser` should not use this.
    ///  - `transforms` should use this to report error, as it does not return [Result].
    ///
    /// See [Handler] for actual usage examples.
    pub static HANDLER: Handler
);
