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
    fmt::{self, Debug},
    ops::{Deref, DerefMut},
    thread::panicking,
};

use tracing::debug;

use super::{Applicability, Diagnostic, DiagnosticId, DiagnosticStyledString, Handler, Level};
use crate::syntax_pos::{MultiSpan, Span};

/// Used for emitting structured error messages and other diagnostic
/// information.
///
/// If there is some state in a downstream crate you would like to
/// access in the methods of `DiagnosticBuilder` here, consider
/// extending `HandlerFlags`, accessed via `self.handler.flags`.
#[must_use]
#[derive(Clone)]
pub struct DiagnosticBuilder<'a> {
    pub handler: &'a Handler,
    #[cfg(not(feature = "__plugin_mode"))]
    pub(crate) diagnostic: Box<Diagnostic>,
    #[cfg(feature = "__plugin_mode")]
    pub diagnostic: Box<Diagnostic>,
    allow_suggestions: bool,
}

/// In general, the `DiagnosticBuilder` uses deref to allow access to
/// the fields and methods of the embedded `diagnostic` in a
/// transparent way.  *However,* many of the methods are intended to
/// be used in a chained way, and hence ought to return `self`. In
/// that case, we can't just naively forward to the method on the
/// `diagnostic`, because the return type would be a `&Diagnostic`
/// instead of a `&DiagnosticBuilder<'a>`. This `forward!` macro makes
/// it easy to declare such methods on the builder.
#[allow(unused)]
macro_rules! forward {
    // Forward pattern for &self -> &Self
    (pub fn $n:ident(&self, $($name:ident: $ty:ty),* $(,)*) -> &Self) => {
        pub fn $n(&self, $($name: $ty),*) -> &Self {
            #[allow(deprecated)]
            self.diagnostic.$n($($name),*);
            self
        }
    };

    // Forward pattern for &mut self -> &mut Self
    (pub fn $n:ident(&mut self, $($name:ident: $ty:ty),* $(,)*) -> &mut Self) => {
        pub fn $n(&mut self, $($name: $ty),*) -> &mut Self {
            #[allow(deprecated)]
            self.diagnostic.$n($($name),*);
            self
        }
    };

    // Forward pattern for &mut self -> &mut Self, with S: Into<MultiSpan>
    // type parameter. No obvious way to make this more generic.
    (pub fn $n:ident<S: Into<MultiSpan>>(
                    &mut self,
                    $($name:ident: $ty:ty),*
                    $(,)*) -> &mut Self) => {
        pub fn $n<S: Into<MultiSpan>>(&mut self, $($name: $ty),*) -> &mut Self {
            #[allow(deprecated)]
            self.diagnostic.$n($($name),*);
            self
        }
    };
}

impl Deref for DiagnosticBuilder<'_> {
    type Target = Diagnostic;

    fn deref(&self) -> &Diagnostic {
        &self.diagnostic
    }
}

impl DerefMut for DiagnosticBuilder<'_> {
    fn deref_mut(&mut self) -> &mut Diagnostic {
        &mut self.diagnostic
    }
}

impl<'a> DiagnosticBuilder<'a> {
    forward!(pub fn note_expected_found(&mut self,
                                        label: &dyn fmt::Display,
                                        expected: DiagnosticStyledString,
                                        found: DiagnosticStyledString,
                                        ) -> &mut Self);

    forward!(pub fn note_expected_found_extra(&mut self,
                                              label: &dyn fmt::Display,
                                              expected: DiagnosticStyledString,
                                              found: DiagnosticStyledString,
                                              expected_extra: &dyn fmt::Display,
                                              found_extra: &dyn fmt::Display,
                                              ) -> &mut Self);

    forward!(pub fn note(&mut self, msg: &str) -> &mut Self);

    forward!(pub fn span_note<S: Into<MultiSpan>>(&mut self,
                                                  sp: S,
                                                  msg: &str,
                                                  ) -> &mut Self);

    forward!(pub fn warn(&mut self, msg: &str) -> &mut Self);

    forward!(pub fn span_warn<S: Into<MultiSpan>>(&mut self, sp: S, msg: &str) -> &mut Self);

    forward!(pub fn help(&mut self , msg: &str) -> &mut Self);

    forward!(pub fn span_help<S: Into<MultiSpan>>(&mut self,
                                                  sp: S,
                                                  msg: &str,
                                                  ) -> &mut Self);

    forward!(pub fn span_suggestion_short(
                                      &mut self,
                                      sp: Span,
                                      msg: &str,
                                      suggestion: String,
                                      ) -> &mut Self);

    forward!(pub fn multipart_suggestion(
        &mut self,
        msg: &str,
        suggestion: Vec<(Span, String)>,
    ) -> &mut Self);

    forward!(pub fn span_suggestion(&mut self,
                                    sp: Span,
                                    msg: &str,
                                    suggestion: String,
                                    ) -> &mut Self);

    forward!(pub fn span_suggestions(&mut self,
                                     sp: Span,
                                     msg: &str,
                                     suggestions: Vec<String>,
                                     ) -> &mut Self);

    forward!(pub fn set_span<S: Into<MultiSpan>>(&mut self, sp: S) -> &mut Self);

    forward!(pub fn code(&mut self, s: DiagnosticId) -> &mut Self);

    /// Emit the diagnostic.
    pub fn emit(&mut self) {
        if self.cancelled() {
            return;
        }

        self.handler.emit_db(self);
        self.cancel();
    }

    /// Buffers the diagnostic for later emission, unless handler
    /// has disabled such buffering.
    pub fn buffer(mut self, buffered_diagnostics: &mut Vec<Diagnostic>) {
        if self.handler.flags.dont_buffer_diagnostics || self.handler.flags.treat_err_as_bug {
            self.emit();
            return;
        }

        // We need to use `ptr::read` because `DiagnosticBuilder`
        // implements `Drop`.
        let diagnostic;
        unsafe {
            diagnostic = ::std::ptr::read(&self.diagnostic);
            ::std::mem::forget(self);
        };
        // Logging here is useful to help track down where in logs an error was
        // actually emitted.
        if cfg!(feature = "debug") {
            debug!("buffer: diagnostic={:?}", diagnostic);
        }
        buffered_diagnostics.push(*diagnostic);
    }

    /// Convenience function for internal use, clients should use one of the
    /// span_* methods instead.
    pub fn sub<S: Into<MultiSpan>>(
        &mut self,
        level: Level,
        message: &str,
        span: Option<S>,
    ) -> &mut Self {
        let span = span.map(|s| s.into()).unwrap_or_default();
        self.diagnostic.sub(level, message, span, None);
        self
    }

    /// Delay emission of this diagnostic as a bug.
    ///
    /// This can be useful in contexts where an error indicates a bug but
    /// typically this only happens when other compilation errors have already
    /// happened. In those cases this can be used to defer emission of this
    /// diagnostic as a bug in the compiler only if no other errors have been
    /// emitted.
    ///
    /// In the meantime, though, callsites are required to deal with the "bug"
    /// locally in whichever way makes the most sense.
    pub fn delay_as_bug(&mut self) {
        self.level = Level::Bug;
        self.handler.delay_as_bug(*self.diagnostic.clone());
        self.cancel();
    }

    /// Add a span/label to be included in the resulting snippet.
    /// This is pushed onto the `MultiSpan` that was created when the
    /// diagnostic was first built. If you don't call this function at
    /// all, and you just supplied a `Span` to create the diagnostic,
    /// then the snippet will just include that `Span`, which is
    /// called the primary span.
    pub fn span_label<T: Into<String>>(&mut self, span: Span, label: T) -> &mut Self {
        self.diagnostic.span_label(span, label);
        self
    }

    pub fn multipart_suggestion_with_applicability(
        &mut self,
        msg: &str,
        suggestion: Vec<(Span, String)>,
        applicability: Applicability,
    ) -> &mut Self {
        if !self.allow_suggestions {
            return self;
        }
        self.diagnostic
            .multipart_suggestion_with_applicability(msg, suggestion, applicability);
        self
    }

    pub fn span_suggestion_with_applicability(
        &mut self,
        sp: Span,
        msg: &str,
        suggestion: String,
        applicability: Applicability,
    ) -> &mut Self {
        if !self.allow_suggestions {
            return self;
        }
        self.diagnostic
            .span_suggestion_with_applicability(sp, msg, suggestion, applicability);
        self
    }

    pub fn span_suggestions_with_applicability(
        &mut self,
        sp: Span,
        msg: &str,
        suggestions: impl Iterator<Item = String>,
        applicability: Applicability,
    ) -> &mut Self {
        if !self.allow_suggestions {
            return self;
        }
        self.diagnostic
            .span_suggestions_with_applicability(sp, msg, suggestions, applicability);
        self
    }

    pub fn span_suggestion_short_with_applicability(
        &mut self,
        sp: Span,
        msg: &str,
        suggestion: String,
        applicability: Applicability,
    ) -> &mut Self {
        if !self.allow_suggestions {
            return self;
        }
        self.diagnostic.span_suggestion_short_with_applicability(
            sp,
            msg,
            suggestion,
            applicability,
        );
        self
    }

    pub fn allow_suggestions(&mut self, allow: bool) -> &mut Self {
        self.allow_suggestions = allow;
        self
    }

    /// Convenience function for internal use, clients should use one of the
    /// struct_* methods on Handler.
    pub fn new(handler: &'a Handler, level: Level, message: &str) -> DiagnosticBuilder<'a> {
        DiagnosticBuilder::new_with_code(handler, level, None, message)
    }

    /// Convenience function for internal use, clients should use one of the
    /// struct_* methods on Handler.
    pub fn new_with_code(
        handler: &'a Handler,
        level: Level,
        code: Option<DiagnosticId>,
        message: &str,
    ) -> DiagnosticBuilder<'a> {
        let diagnostic = Diagnostic::new_with_code(level, code, message);
        DiagnosticBuilder::new_diagnostic(handler, diagnostic)
    }

    /// Creates a new `DiagnosticBuilder` with an already constructed
    /// diagnostic.
    #[inline(always)] // box
    pub fn new_diagnostic(handler: &'a Handler, diagnostic: Diagnostic) -> DiagnosticBuilder<'a> {
        DiagnosticBuilder {
            handler,
            diagnostic: Box::new(diagnostic),
            allow_suggestions: true,
        }
    }

    pub fn take(&mut self) -> Diagnostic {
        std::mem::take(&mut *self.diagnostic)
    }
}

impl Debug for DiagnosticBuilder<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.diagnostic.fmt(f)
    }
}

/// Destructor bomb - a `DiagnosticBuilder` must be either emitted or canceled
/// or we emit a bug.
impl Drop for DiagnosticBuilder<'_> {
    fn drop(&mut self) {
        if !panicking() && !self.cancelled() {
            let mut db = DiagnosticBuilder::new(
                self.handler,
                Level::Bug,
                "Error constructed but not emitted",
            );
            db.emit();
            panic!();
        }
    }
}
