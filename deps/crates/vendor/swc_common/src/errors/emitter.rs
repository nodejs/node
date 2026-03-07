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
    cmp::{min, Reverse},
    collections::HashMap,
    io::{self, prelude::*},
};

#[cfg(feature = "tty-emitter")]
use termcolor::{Buffer, BufferWriter, Color, ColorChoice, ColorSpec, StandardStream, WriteColor};

use self::Destination::*;
use super::{
    diagnostic::Message,
    snippet::{Annotation, AnnotationType, Line, MultilineAnnotation, Style, StyledString},
    styled_buffer::StyledBuffer,
    CodeSuggestion, DiagnosticBuilder, DiagnosticId, Level, SourceMapperDyn, SubDiagnostic,
};
use crate::{
    sync::Lrc,
    syntax_pos::{MultiSpan, SourceFile, Span},
};

const ANONYMIZED_LINE_NUM: &str = "LL";

/// Emitter trait for emitting errors.
pub trait Emitter: crate::sync::Send {
    /// Emit a structured diagnostic.
    fn emit(&mut self, db: &mut DiagnosticBuilder<'_>);

    /// Check if should show explanations about "rustc --explain"
    fn should_show_explain(&self) -> bool {
        true
    }

    fn take_diagnostics(&mut self) -> Vec<String> {
        vec![]
    }
}

impl Emitter for EmitterWriter {
    fn emit(&mut self, db: &mut DiagnosticBuilder<'_>) {
        let mut primary_span = db.span.clone();
        let mut children = db.children.clone();
        let mut suggestions: &[_] = &[];

        if let Some((sugg, rest)) = db.suggestions.split_first() {
            if rest.is_empty() &&
               // don't display multi-suggestions as labels
               sugg.substitutions.len() == 1 &&
               // don't display multipart suggestions as labels
               sugg.substitutions[0].parts.len() == 1 &&
               // don't display long messages as labels
               sugg.msg.split_whitespace().count() < 10 &&
               // don't display multiline suggestions as labels
               !sugg.substitutions[0].parts[0].snippet.contains('\n')
            {
                let substitution = &sugg.substitutions[0].parts[0].snippet.trim();
                let msg = if substitution.is_empty() || !sugg.show_code_when_inline {
                    // This substitution is only removal or we explicitly don't want to show the
                    // code inline, don't show it
                    format!("help: {}", sugg.msg)
                } else {
                    format!("help: {}: `{}`", sugg.msg, substitution)
                };
                primary_span.push_span_label(sugg.substitutions[0].parts[0].span, msg);
            } else {
                // if there are multiple suggestions, print them all in full
                // to be consistent. We could try to figure out if we can
                // make one (or the first one) inline, but that would give
                // undue importance to a semi-random suggestion
                suggestions = &db.suggestions;
            }
        }

        self.fix_multispans_in_std_macros(
            &mut primary_span,
            &mut children,
            db.handler.flags.external_macro_backtrace,
        );

        self.emit_messages_default(
            db.level,
            db.styled_message(),
            &db.code,
            &primary_span,
            &children,
            suggestions,
        );
    }

    fn should_show_explain(&self) -> bool {
        !self.short_message
    }
}

/// maximum number of lines we will print for each error; arbitrary.
pub const MAX_HIGHLIGHT_LINES: usize = 6;
/// maximum number of suggestions to be shown
///
/// Arbitrary, but taken from trait import suggestion limit
pub const MAX_SUGGESTIONS: usize = 4;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ColorConfig {
    Auto,
    Always,
    Never,
}

impl ColorConfig {
    #[cfg(feature = "tty-emitter")]
    fn to_color_choice(self) -> ColorChoice {
        use std::io::IsTerminal;

        let stderr = io::stderr();

        match self {
            ColorConfig::Always => {
                if stderr.is_terminal() {
                    ColorChoice::Always
                } else {
                    ColorChoice::AlwaysAnsi
                }
            }
            ColorConfig::Never => ColorChoice::Never,
            ColorConfig::Auto if stderr.is_terminal() => ColorChoice::Auto,
            ColorConfig::Auto => ColorChoice::Never,
        }
    }
}

/// Implementation of [Emitter] which pretty-prints the errors.
pub struct EmitterWriter {
    dst: Destination,
    sm: Option<Lrc<SourceMapperDyn>>,
    short_message: bool,
    teach: bool,
    ui_testing: bool,

    skip_filename: bool,
}

struct FileWithAnnotatedLines {
    file: Lrc<SourceFile>,
    lines: Vec<Line>,
    multiline_depth: usize,
}

#[cfg(feature = "tty-emitter")]
#[cfg_attr(docsrs, doc(cfg(feature = "tty-emitter")))]
impl EmitterWriter {
    pub fn stderr(
        color_config: ColorConfig,
        source_map: Option<Lrc<SourceMapperDyn>>,
        short_message: bool,
        teach: bool,
    ) -> EmitterWriter {
        let dst = Destination::from_stderr(color_config);
        EmitterWriter {
            dst,
            sm: source_map,
            short_message,
            teach,
            ui_testing: false,
            skip_filename: false,
        }
    }
}

impl EmitterWriter {
    pub fn new(
        dst: Box<dyn Write + Send>,
        source_map: Option<Lrc<SourceMapperDyn>>,
        short_message: bool,
        teach: bool,
    ) -> EmitterWriter {
        EmitterWriter {
            dst: Raw(dst),
            sm: source_map,
            short_message,
            teach,
            ui_testing: false,
            skip_filename: false,
        }
    }

    pub fn ui_testing(mut self, ui_testing: bool) -> Self {
        self.ui_testing = ui_testing;
        self
    }

    pub fn skip_filename(mut self, skip_filename: bool) -> Self {
        self.skip_filename = skip_filename;
        self
    }

    fn maybe_anonymized(&self, line_num: usize) -> String {
        if self.ui_testing {
            ANONYMIZED_LINE_NUM.to_string()
        } else {
            line_num.to_string()
        }
    }

    fn preprocess_annotations(&mut self, msp: &MultiSpan) -> Vec<FileWithAnnotatedLines> {
        fn add_annotation_to_file(
            file_vec: &mut Vec<FileWithAnnotatedLines>,
            file: Lrc<SourceFile>,
            line_index: usize,
            ann: Annotation,
        ) {
            for slot in file_vec.iter_mut() {
                // Look through each of our files for the one we're adding to
                if slot.file.name == file.name {
                    // See if we already have a line for it
                    for line_slot in &mut slot.lines {
                        if line_slot.line_index == line_index {
                            line_slot.annotations.push(ann);
                            return;
                        }
                    }
                    // We don't have a line yet, create one
                    slot.lines.push(Line {
                        line_index,
                        annotations: vec![ann],
                    });
                    slot.lines.sort();
                    return;
                }
            }
            // This is the first time we're seeing the file
            file_vec.push(FileWithAnnotatedLines {
                file,
                lines: vec![Line {
                    line_index,
                    annotations: vec![ann],
                }],
                multiline_depth: 0,
            });
        }

        let mut output = Vec::new();
        let mut multiline_annotations = Vec::new();

        if let Some(ref sm) = self.sm {
            for span_label in msp.span_labels() {
                if span_label.span.is_dummy() {
                    continue;
                }

                let lo = sm.lookup_char_pos(span_label.span.lo());
                let mut hi = sm.lookup_char_pos(span_label.span.hi());

                // Watch out for "empty spans". If we get a span like 6..6, we
                // want to just display a `^` at 6, so convert that to
                // 6..7. This is degenerate input, but it's best to degrade
                // gracefully -- and the parser likes to supply a span like
                // that for EOF, in particular.
                if lo.col_display == hi.col_display && lo.line == hi.line {
                    hi.col_display += 1;
                }

                let ann_type = if lo.line != hi.line {
                    let ml = MultilineAnnotation {
                        depth: 1,
                        line_start: lo.line,
                        line_end: hi.line,
                        start_col: lo.col_display,
                        end_col: hi.col_display,
                        is_primary: span_label.is_primary,
                        label: span_label.label.clone(),
                    };
                    multiline_annotations.push((lo.file.clone(), ml.clone()));
                    AnnotationType::Multiline(ml)
                } else {
                    AnnotationType::Singleline
                };
                let ann = Annotation {
                    start_col: lo.col_display,
                    end_col: hi.col_display,
                    is_primary: span_label.is_primary,
                    label: span_label.label.clone(),
                    annotation_type: ann_type,
                };

                if !ann.is_multiline() {
                    add_annotation_to_file(&mut output, lo.file, lo.line, ann);
                }
            }
        }

        // Find overlapping multiline annotations, put them at different depths
        multiline_annotations.sort_by_key(|(_, ml)| (ml.line_start, ml.line_end));
        for item in multiline_annotations.clone() {
            let ann = item.1;
            for item in multiline_annotations.iter_mut() {
                let a = &mut item.1;
                // Move all other multiline annotations overlapping with this one
                // one level to the right.
                if &ann != a
                    && num_overlap(ann.line_start, ann.line_end, a.line_start, a.line_end, true)
                {
                    a.increase_depth();
                } else {
                    break;
                }
            }
        }

        let mut max_depth = 0; // max overlapping multiline spans
        for (file, ann) in multiline_annotations {
            if ann.depth > max_depth {
                max_depth = ann.depth;
            }
            add_annotation_to_file(&mut output, file.clone(), ann.line_start, ann.as_start());
            let middle = min(ann.line_start + 4, ann.line_end);
            for line in ann.line_start + 1..middle {
                add_annotation_to_file(&mut output, file.clone(), line, ann.as_line());
            }
            if middle < ann.line_end - 1 {
                for line in ann.line_end - 1..ann.line_end {
                    add_annotation_to_file(&mut output, file.clone(), line, ann.as_line());
                }
            }
            add_annotation_to_file(&mut output, file, ann.line_end, ann.as_end());
        }
        for file_vec in output.iter_mut() {
            file_vec.multiline_depth = max_depth;
        }
        output
    }

    #[allow(clippy::cognitive_complexity)]
    fn render_source_line(
        &self,
        buffer: &mut StyledBuffer,
        file: Lrc<SourceFile>,
        line: &Line,
        width_offset: usize,
        code_offset: usize,
    ) -> Vec<(usize, Style)> {
        if line.line_index == 0 {
            return Vec::new();
        }

        let source_string = match file.get_line(line.line_index - 1) {
            Some(s) => s,
            None => return Vec::new(),
        };

        let line_offset = buffer.num_lines();

        // First create the source line we will highlight.
        buffer.puts(line_offset, code_offset, &source_string, Style::Quotation);
        buffer.puts(
            line_offset,
            0,
            &self.maybe_anonymized(line.line_index),
            Style::LineNumber,
        );

        draw_col_separator(buffer, line_offset, width_offset - 2);

        // Special case when there's only one annotation involved, it is the start of a
        // multiline span and there's no text at the beginning of the code line.
        // Instead of doing the whole graph:
        //
        // 2 |   fn foo() {
        //   |  _^
        // 3 | |
        // 4 | | }
        //   | |_^ test
        //
        // we simplify the output to:
        //
        // 2 | / fn foo() {
        // 3 | |
        // 4 | | }
        //   | |_^ test
        if line.annotations.len() == 1 {
            if let Some(ann) = line.annotations.first() {
                if let AnnotationType::MultilineStart(depth) = ann.annotation_type {
                    if source_string
                        .chars()
                        .take(ann.start_col)
                        .all(|c| c.is_whitespace())
                    {
                        let style = if ann.is_primary {
                            Style::UnderlinePrimary
                        } else {
                            Style::UnderlineSecondary
                        };
                        buffer.putc(line_offset, width_offset + depth - 1, '/', style);
                        return vec![(depth, style)];
                    }
                }
            }
        }

        // We want to display like this:
        //
        //      vec.push(vec.pop().unwrap());
        //      ---      ^^^               - previous borrow ends here
        //      |        |
        //      |        error occurs here
        //      previous borrow of `vec` occurs here
        //
        // But there are some weird edge cases to be aware of:
        //
        //      vec.push(vec.pop().unwrap());
        //      --------                    - previous borrow ends here
        //      ||
        //      |this makes no sense
        //      previous borrow of `vec` occurs here
        //
        // For this reason, we group the lines into "highlight lines"
        // and "annotations lines", where the highlight lines have the `^`.

        // Sort the annotations by (start, end col)
        // The labels are reversed, sort and then reversed again.
        // Consider a list of annotations (A1, A2, C1, C2, B1, B2) where
        // the letter signifies the span. Here we are only sorting by the
        // span and hence, the order of the elements with the same span will
        // not change. On reversing the ordering (|a, b| but b.cmp(a)), you get
        // (C1, C2, B1, B2, A1, A2). All the elements with the same span are
        // still ordered first to last, but all the elements with different
        // spans are ordered by their spans in last to first order. Last to
        // first order is important, because the jiggly lines and | are on
        // the left, so the rightmost span needs to be rendered first,
        // otherwise the lines would end up needing to go over a message.

        let mut annotations = line.annotations.clone();
        annotations.sort_by_key(|a| Reverse(a.start_col));

        // First, figure out where each label will be positioned.
        //
        // In the case where you have the following annotations:
        //
        //      vec.push(vec.pop().unwrap());
        //      --------                    - previous borrow ends here [C]
        //      ||
        //      |this makes no sense [B]
        //      previous borrow of `vec` occurs here [A]
        //
        // `annotations_position` will hold [(2, A), (1, B), (0, C)].
        //
        // We try, when possible, to stick the rightmost annotation at the end
        // of the highlight line:
        //
        //      vec.push(vec.pop().unwrap());
        //      ---      ---               - previous borrow ends here
        //
        // But sometimes that's not possible because one of the other
        // annotations overlaps it. For example, from the test
        // `span_overlap_label`, we have the following annotations
        // (written on distinct lines for clarity):
        //
        //      fn foo(x: u32) {
        //      --------------
        //             -
        //
        // In this case, we can't stick the rightmost-most label on
        // the highlight line, or we would get:
        //
        //      fn foo(x: u32) {
        //      -------- x_span
        //      |
        //      fn_span
        //
        // which is totally weird. Instead we want:
        //
        //      fn foo(x: u32) {
        //      --------------
        //      |      |
        //      |      x_span
        //      fn_span
        //
        // which is...less weird, at least. In fact, in general, if
        // the rightmost span overlaps with any other span, we should
        // use the "hang below" version, so we can at least make it
        // clear where the span *starts*. There's an exception for this
        // logic, when the labels do not have a message:
        //
        //      fn foo(x: u32) {
        //      --------------
        //             |
        //             x_span
        //
        // instead of:
        //
        //      fn foo(x: u32) {
        //      --------------
        //      |      |
        //      |      x_span
        //      <EMPTY LINE>
        //
        let mut annotations_position = Vec::new();
        let mut line_len = 0;
        let mut p = 0;
        for (i, annotation) in annotations.iter().enumerate() {
            for (j, next) in annotations.iter().enumerate() {
                if overlaps(next, annotation, 0)  // This label overlaps with another one and both
                    && annotation.has_label()     // take space (they have text and are not
                    && j > i                      // multiline lines).
                    && p == 0
                // We're currently on the first line, move the label one line down
                {
                    // This annotation needs a new line in the output.
                    p += 1;
                    break;
                }
            }
            annotations_position.push((p, annotation));
            for (j, next) in annotations.iter().enumerate() {
                if j > i {
                    let l = if let Some(ref label) = next.label {
                        label.len() + 2
                    } else {
                        0
                    };
                    if (overlaps(next, annotation, l) // Do not allow two labels to be in the same
                                                     // line if they overlap including padding, to
                                                     // avoid situations like:
                                                     //
                                                     //      fn foo(x: u32) {
                                                     //      -------^------
                                                     //      |      |
                                                     //      fn_spanx_span
                                                     //
                        && annotation.has_label()    // Both labels must have some text, otherwise
                        && next.has_label())         // they are not overlapping.
                                                     // Do not add a new line if this annotation
                                                     // or the next are vertical line placeholders.
                        || (annotation.takes_space() // If either this or the next annotation is
                            && next.has_label())     // multiline start/end, move it to a new line
                        || (annotation.has_label()   // so as not to overlap the horizontal lines.
                            && next.takes_space())
                        || (annotation.takes_space() && next.takes_space())
                        || (overlaps(next, annotation, l)
                            && next.end_col <= annotation.end_col
                            && next.has_label()
                            && p == 0)
                    // Avoid #42595.
                    {
                        // This annotation needs a new line in the output.
                        p += 1;
                        break;
                    }
                }
            }
            if line_len < p {
                line_len = p;
            }
        }

        if line_len != 0 {
            line_len += 1;
        }

        // If there are no annotations or the only annotations on this line are
        // MultilineLine, then there's only code being shown, stop processing.
        if line.annotations.iter().all(|a| a.is_line()) {
            return Vec::new();
        }

        // Write the column separator.
        //
        // After this we will have:
        //
        // 2 |   fn foo() {
        //   |
        //   |
        //   |
        // 3 |
        // 4 |   }
        //   |
        for pos in 0..=line_len {
            draw_col_separator(buffer, line_offset + pos + 1, width_offset - 2);
            buffer.putc(
                line_offset + pos + 1,
                width_offset - 2,
                '|',
                Style::LineNumber,
            );
        }

        // Write the horizontal lines for multiline annotations
        // (only the first and last lines need this).
        //
        // After this we will have:
        //
        // 2 |   fn foo() {
        //   |  __________
        //   |
        //   |
        // 3 |
        // 4 |   }
        //   |  _
        for &(pos, annotation) in &annotations_position {
            let style = if annotation.is_primary {
                Style::UnderlinePrimary
            } else {
                Style::UnderlineSecondary
            };
            let pos = pos + 1;
            match annotation.annotation_type {
                AnnotationType::MultilineStart(depth) | AnnotationType::MultilineEnd(depth) => {
                    draw_range(
                        buffer,
                        '_',
                        line_offset + pos,
                        width_offset + depth,
                        code_offset + annotation.start_col,
                        style,
                    );
                }
                _ if self.teach => {
                    buffer.set_style_range(
                        line_offset,
                        code_offset + annotation.start_col,
                        code_offset + annotation.end_col,
                        style,
                        annotation.is_primary,
                    );
                }
                _ => {}
            }
        }

        // Write the vertical lines for labels that are on a different line as the
        // underline.
        //
        // After this we will have:
        //
        // 2 |   fn foo() {
        //   |  __________
        //   | |    |
        //   | |
        // 3 |
        // 4 | | }
        //   | |_
        for &(pos, annotation) in &annotations_position {
            let style = if annotation.is_primary {
                Style::UnderlinePrimary
            } else {
                Style::UnderlineSecondary
            };
            let pos = pos + 1;

            if pos > 1 && (annotation.has_label() || annotation.takes_space()) {
                for p in line_offset + 1..=line_offset + pos {
                    buffer.putc(p, code_offset + annotation.start_col, '|', style);
                }
            }
            match annotation.annotation_type {
                AnnotationType::MultilineStart(depth) => {
                    for p in line_offset + pos + 1..line_offset + line_len + 2 {
                        buffer.putc(p, width_offset + depth - 1, '|', style);
                    }
                }
                AnnotationType::MultilineEnd(depth) => {
                    for p in line_offset..=line_offset + pos {
                        buffer.putc(p, width_offset + depth - 1, '|', style);
                    }
                }
                _ => (),
            }
        }

        // Write the labels on the annotations that actually have a label.
        //
        // After this we will have:
        //
        // 2 |   fn foo() {
        //   |  __________
        //   |      |
        //   |      something about `foo`
        // 3 |
        // 4 |   }
        //   |  _  test
        for &(pos, annotation) in &annotations_position {
            let style = if annotation.is_primary {
                Style::LabelPrimary
            } else {
                Style::LabelSecondary
            };
            let (pos, col) = if pos == 0 {
                (pos + 1, annotation.end_col + 1)
            } else {
                (pos + 2, annotation.start_col)
            };
            if let Some(ref label) = annotation.label {
                buffer.puts(line_offset + pos, code_offset + col, label, style);
            }
        }

        // Sort from biggest span to smallest span so that smaller spans are
        // represented in the output:
        //
        // x | fn foo()
        //   | ^^^---^^
        //   | |  |
        //   | |  something about `foo`
        //   | something about `fn foo()`
        annotations_position.sort_by(|a, b| {
            // Decreasing order
            a.1.len().cmp(&b.1.len()).reverse()
        });

        // Write the underlines.
        //
        // After this we will have:
        //
        // 2 |   fn foo() {
        //   |  ____-_____^
        //   |      |
        //   |      something about `foo`
        // 3 |
        // 4 |   }
        //   |  _^  test
        for &(_, annotation) in &annotations_position {
            let (underline, style) = if annotation.is_primary {
                ('^', Style::UnderlinePrimary)
            } else {
                ('-', Style::UnderlineSecondary)
            };
            for p in annotation.start_col..annotation.end_col {
                buffer.putc(line_offset + 1, code_offset + p, underline, style);
            }
        }
        annotations_position
            .iter()
            .filter_map(|&(_, annotation)| match annotation.annotation_type {
                AnnotationType::MultilineStart(p) | AnnotationType::MultilineEnd(p) => {
                    let style = if annotation.is_primary {
                        Style::LabelPrimary
                    } else {
                        Style::LabelSecondary
                    };
                    Some((p, style))
                }
                _ => None,
            })
            .collect::<Vec<_>>()
    }

    fn get_multispan_max_line_num(&mut self, msp: &MultiSpan) -> usize {
        let mut max = 0;
        if let Some(ref sm) = self.sm {
            for primary_span in msp.primary_spans() {
                if !primary_span.is_dummy() {
                    let hi = sm.lookup_char_pos(primary_span.hi());
                    if hi.line > max {
                        max = hi.line;
                    }
                }
            }
            if !self.short_message {
                for span_label in msp.span_labels() {
                    if !span_label.span.is_dummy() {
                        let hi = sm.lookup_char_pos(span_label.span.hi());
                        if hi.line > max {
                            max = hi.line;
                        }
                    }
                }
            }
        }
        max
    }

    fn get_max_line_num(&mut self, span: &MultiSpan, children: &[SubDiagnostic]) -> usize {
        let primary = self.get_multispan_max_line_num(span);
        children
            .iter()
            .map(|sub| self.get_multispan_max_line_num(&sub.span))
            .max()
            .unwrap_or(0)
            .max(primary)
    }

    // This "fixes" MultiSpans that contain Spans that are pointing to locations
    // inside of <*macros>. Since these locations are often difficult to read,
    // we move these Spans from <*macros> to their corresponding use site.
    fn fix_multispan_in_std_macros(
        &mut self,
        span: &mut MultiSpan,
        always_backtrace: bool,
    ) -> bool {
        let mut spans_updated = false;

        if let Some(ref sm) = self.sm {
            let mut before_after: Vec<(Span, Span)> = Vec::new();
            let new_labels: Vec<(Span, String)> = Vec::new();

            // First, find all the spans in <*macros> and point instead at their use site
            for sp in span.primary_spans() {
                if sp.is_dummy() {
                    continue;
                }
                let call_sp = sm.call_span_if_macro(*sp);
                if call_sp != *sp && !always_backtrace {
                    before_after.push((*sp, call_sp));
                }
            }
            for (label_span, label_text) in new_labels {
                span.push_span_label(label_span, label_text);
            }
            for sp_label in span.span_labels() {
                if sp_label.span.is_dummy() {
                    continue;
                }
            }
            // After we have them, make sure we replace these 'bad' def sites with their use
            // sites
            for (before, after) in before_after {
                span.replace(before, after);
                spans_updated = true;
            }
        }

        spans_updated
    }

    // This does a small "fix" for multispans by looking to see if it can find any
    // that point directly at <*macros>. Since these are often difficult to
    // read, this will change the span to point at the use site.
    fn fix_multispans_in_std_macros(
        &mut self,
        span: &mut MultiSpan,
        children: &mut Vec<SubDiagnostic>,
        backtrace: bool,
    ) {
        let mut spans_updated = self.fix_multispan_in_std_macros(span, backtrace);
        for child in children.iter_mut() {
            spans_updated |= self.fix_multispan_in_std_macros(&mut child.span, backtrace);
        }
        if spans_updated {
            children.push(SubDiagnostic {
                level: Level::Note,
                message: vec![Message(
                    "this error originates in a macro outside of the current crate (in Nightly \
                     builds, run with -Z external-macro-backtrace for more info)"
                        .to_string(),
                    Style::NoStyle,
                )],
                span: MultiSpan::new(),
                render_span: None,
            });
        }
    }

    /// Add a left margin to every line but the first, given a padding length
    /// and the label being displayed, keeping the provided highlighting.
    fn msg_to_buffer(
        &self,
        buffer: &mut StyledBuffer,
        msg: &[Message],
        padding: usize,
        label: &str,
        override_style: Option<Style>,
    ) {
        // The extra 5 ` ` is padding that's always needed to align to the `note: `:
        //
        //   error: message
        //     --> file.rs:13:20
        //      |
        //   13 |     <CODE>
        //      |      ^^^^
        //      |
        //      = note: multiline
        //              message
        //   ++^^^----xx
        //    |  |   | |
        //    |  |   | magic `2`
        //    |  |   length of label
        //    |  magic `3`
        //    `max_line_num_len`
        let padding = " ".repeat(padding + label.len() + 5);

        /// Returns `override` if it is present and `style` is `NoStyle` or
        /// `style` otherwise
        fn style_or_override(style: Style, override_: Option<Style>) -> Style {
            match (style, override_) {
                (Style::NoStyle, Some(override_)) => override_,
                _ => style,
            }
        }

        let mut line_number = 0;

        // Provided the following diagnostic message:
        //
        //     let msg = vec![
        //       ("
        //       ("highlighted multiline\nstring to\nsee how it ", Style::NoStyle),
        //       ("looks", Style::Highlight),
        //       ("with\nvery ", Style::NoStyle),
        //       ("weird", Style::Highlight),
        //       (" formats\n", Style::NoStyle),
        //       ("see?", Style::Highlight),
        //     ];
        //
        // the expected output on a note is (* surround the highlighted text)
        //
        //        = note: highlighted multiline
        //                string to
        //                see how it *looks* with
        //                very *weird* formats
        //                see?
        for Message(text, ref style) in msg.iter() {
            let lines = text.split('\n').collect::<Vec<_>>();
            if lines.len() > 1 {
                for (i, line) in lines.iter().enumerate() {
                    if i != 0 {
                        line_number += 1;
                        buffer.append(line_number, &padding, Style::NoStyle);
                    }
                    buffer.append(line_number, line, style_or_override(*style, override_style));
                }
            } else {
                buffer.append(line_number, text, style_or_override(*style, override_style));
            }
        }
    }

    #[allow(clippy::cognitive_complexity, clippy::comparison_chain)]
    fn emit_message_default(
        &mut self,
        msp: &MultiSpan,
        msg: &[Message],
        code: &Option<DiagnosticId>,
        level: Level,
        max_line_num_len: usize,
        is_secondary: bool,
    ) -> io::Result<()> {
        let mut buffer = StyledBuffer::new();
        let header_style = if is_secondary {
            Style::HeaderMsg
        } else {
            Style::MainHeaderMsg
        };

        if msp.primary_spans().is_empty()
            && msp.span_labels().is_empty()
            && is_secondary
            && !self.short_message
        {
            // This is a secondary message with no span info
            for _ in 0..max_line_num_len {
                buffer.prepend(0, " ", Style::NoStyle);
            }
            draw_note_separator(&mut buffer, 0, max_line_num_len + 1);
            let level_str = level.to_string();
            if !level_str.is_empty() {
                buffer.append(0, &level_str, Style::MainHeaderMsg);
                buffer.append(0, ": ", Style::NoStyle);
            }
            self.msg_to_buffer(&mut buffer, msg, max_line_num_len, "note", None);
        } else {
            let level_str = level.to_string();
            if !level_str.is_empty() {
                buffer.append(0, &level_str, Style::Level(level));
            }
            // only render error codes, not lint codes
            if let Some(DiagnosticId::Error(ref code)) = *code {
                buffer.append(0, "[", Style::Level(level));
                buffer.append(0, code, Style::Level(level));
                buffer.append(0, "]", Style::Level(level));
            }
            if !level_str.is_empty() {
                buffer.append(0, ": ", header_style);
            }
            for Message(text, _) in msg.iter() {
                buffer.append(0, text, header_style);
            }
        }

        // Preprocess all the annotations so that they are grouped by file and by line
        // number This helps us quickly iterate over the whole message
        // (including secondary file spans)
        let mut annotated_files = self.preprocess_annotations(msp);

        // Make sure our primary file comes first
        let (primary_lo, sm) = if let (Some(sm), Some(primary_span)) =
            (self.sm.as_ref(), msp.primary_span().as_ref())
        {
            if !primary_span.is_dummy() {
                (sm.lookup_char_pos(primary_span.lo()), sm)
            } else {
                emit_to_destination(&buffer.render(), level, &mut self.dst, self.short_message)?;
                return Ok(());
            }
        } else {
            // If we don't have span information, emit and exit
            emit_to_destination(&buffer.render(), level, &mut self.dst, self.short_message)?;
            return Ok(());
        };
        if let Ok(pos) =
            annotated_files.binary_search_by(|x| x.file.name.cmp(&primary_lo.file.name))
        {
            annotated_files.swap(0, pos);
        }

        // Print out the annotate source lines that correspond with the error
        for annotated_file in annotated_files {
            // print out the span location and spacer before we print the annotated source
            // to do this, we need to know if this span will be primary
            let is_primary = primary_lo.file.name == annotated_file.file.name;
            if is_primary {
                let loc = primary_lo.clone();
                if !self.short_message {
                    // remember where we are in the output buffer for easy reference
                    let buffer_msg_line_offset = buffer.num_lines();

                    if !self.skip_filename {
                        buffer.prepend(buffer_msg_line_offset, "--> ", Style::LineNumber);

                        buffer.append(
                            buffer_msg_line_offset,
                            &format!(
                                "{}:{}:{}",
                                loc.file.name,
                                sm.doctest_offset_line(loc.line),
                                loc.col.0 + 1
                            ),
                            Style::LineAndColumn,
                        );
                    }

                    for _ in 0..max_line_num_len {
                        buffer.prepend(buffer_msg_line_offset, " ", Style::NoStyle);
                    }
                } else {
                    buffer.prepend(
                        0,
                        &format!(
                            "{}:{}:{}: ",
                            loc.file.name,
                            sm.doctest_offset_line(loc.line),
                            loc.col.0 + 1
                        ),
                        Style::LineAndColumn,
                    );
                }
            } else if !self.short_message {
                // remember where we are in the output buffer for easy reference
                let buffer_msg_line_offset = buffer.num_lines();

                // Add spacing line
                draw_col_separator(&mut buffer, buffer_msg_line_offset, max_line_num_len + 1);

                // Then, the secondary file indicator
                buffer.prepend(buffer_msg_line_offset + 1, "::: ", Style::LineNumber);
                let loc = if let Some(first_line) = annotated_file.lines.first() {
                    let col = if let Some(first_annotation) = first_line.annotations.first() {
                        format!(":{}", first_annotation.start_col + 1)
                    } else {
                        String::new()
                    };
                    format!(
                        "{}:{}{}",
                        annotated_file.file.name,
                        sm.doctest_offset_line(first_line.line_index),
                        col
                    )
                } else {
                    annotated_file.file.name.to_string()
                };
                buffer.append(buffer_msg_line_offset + 1, &loc, Style::LineAndColumn);
                for _ in 0..max_line_num_len {
                    buffer.prepend(buffer_msg_line_offset + 1, " ", Style::NoStyle);
                }
            }

            if !self.short_message {
                // Put in the spacer between the location and annotated source
                let buffer_msg_line_offset = buffer.num_lines();
                draw_col_separator_no_space(
                    &mut buffer,
                    buffer_msg_line_offset,
                    max_line_num_len + 1,
                );

                // Contains the vertical lines' positions for active multiline annotations
                let mut multilines = HashMap::<_, _>::default();

                // Next, output the annotate source for this file
                for line_idx in 0..annotated_file.lines.len() {
                    let previous_buffer_line = buffer.num_lines();

                    let width_offset = 3 + max_line_num_len;
                    let code_offset = if annotated_file.multiline_depth == 0 {
                        width_offset
                    } else {
                        width_offset + annotated_file.multiline_depth + 1
                    };

                    let depths = self.render_source_line(
                        &mut buffer,
                        annotated_file.file.clone(),
                        &annotated_file.lines[line_idx],
                        width_offset,
                        code_offset,
                    );

                    let mut to_add = HashMap::<_, _>::default();

                    for (depth, style) in depths {
                        if multilines.contains_key(&depth) {
                            multilines.remove(&depth);
                        } else {
                            to_add.insert(depth, style);
                        }
                    }

                    // Set the multiline annotation vertical lines to the left of
                    // the code in this line.
                    for (depth, style) in &multilines {
                        for line in previous_buffer_line..buffer.num_lines() {
                            draw_multiline_line(&mut buffer, line, width_offset, *depth, *style);
                        }
                    }
                    // check to see if we need to print out or elide lines that come between
                    // this annotated line and the next one.
                    if line_idx < (annotated_file.lines.len() - 1) {
                        let line_idx_delta = annotated_file.lines[line_idx + 1].line_index
                            - annotated_file.lines[line_idx].line_index;
                        if line_idx_delta > 2 {
                            let last_buffer_line_num = buffer.num_lines();
                            buffer.puts(last_buffer_line_num, 0, "...", Style::LineNumber);

                            // Set the multiline annotation vertical lines on `...` bridging line.
                            for (depth, style) in &multilines {
                                draw_multiline_line(
                                    &mut buffer,
                                    last_buffer_line_num,
                                    width_offset,
                                    *depth,
                                    *style,
                                );
                            }
                        } else if line_idx_delta == 2 {
                            let unannotated_line = annotated_file
                                .file
                                .get_line(annotated_file.lines[line_idx].line_index)
                                .unwrap_or_else(|| Cow::from(""));

                            let last_buffer_line_num = buffer.num_lines();

                            buffer.puts(
                                last_buffer_line_num,
                                0,
                                &self.maybe_anonymized(
                                    annotated_file.lines[line_idx + 1].line_index - 1,
                                ),
                                Style::LineNumber,
                            );
                            draw_col_separator(
                                &mut buffer,
                                last_buffer_line_num,
                                1 + max_line_num_len,
                            );
                            buffer.puts(
                                last_buffer_line_num,
                                code_offset,
                                &unannotated_line,
                                Style::Quotation,
                            );

                            for (depth, style) in &multilines {
                                draw_multiline_line(
                                    &mut buffer,
                                    last_buffer_line_num,
                                    width_offset,
                                    *depth,
                                    *style,
                                );
                            }
                        }
                    }

                    multilines.extend(&to_add);
                }
            }
        }

        // final step: take our styled buffer, render it, then output it
        emit_to_destination(&buffer.render(), level, &mut self.dst, self.short_message)?;

        Ok(())
    }

    fn emit_suggestion_default(
        &mut self,
        suggestion: &CodeSuggestion,
        level: Level,
        max_line_num_len: usize,
    ) -> io::Result<()> {
        if let Some(ref sm) = self.sm {
            let mut buffer = StyledBuffer::new();

            // Render the suggestion message
            let level_str = level.to_string();
            if !level_str.is_empty() {
                buffer.append(0, &level_str, Style::Level(level));
                buffer.append(0, ": ", Style::HeaderMsg);
            }
            self.msg_to_buffer(
                &mut buffer,
                &[Message(suggestion.msg.to_owned(), Style::NoStyle)],
                max_line_num_len,
                "suggestion",
                Some(Style::HeaderMsg),
            );

            // Render the replacements for each suggestion
            let suggestions = suggestion.splice_lines(&**sm);

            let mut row_num = 2;
            for (complete, parts) in suggestions.iter().take(MAX_SUGGESTIONS) {
                // Only show underline if the suggestion spans a single line and doesn't cover
                // the entirety of the code output. If you have multiple
                // replacements in the same line of code, show the underline.
                let show_underline = !(parts.len() == 1
                    && parts[0].snippet.trim() == complete.trim())
                    && complete.lines().count() == 1;

                let lines = sm.span_to_lines(parts[0].span).unwrap();

                assert!(!lines.lines.is_empty());

                let line_start = sm.lookup_char_pos(parts[0].span.lo()).line;
                draw_col_separator_no_space(&mut buffer, 1, max_line_num_len + 1);
                let mut lines = complete.lines();
                for (line_pos, line) in lines.by_ref().take(MAX_HIGHLIGHT_LINES).enumerate() {
                    // Print the span column to avoid confusion
                    buffer.puts(
                        row_num,
                        0,
                        &self.maybe_anonymized(line_start + line_pos),
                        Style::LineNumber,
                    );
                    // print the suggestion
                    draw_col_separator(&mut buffer, row_num, max_line_num_len + 1);
                    buffer.append(row_num, line, Style::NoStyle);
                    row_num += 1;
                }

                // This offset and the ones below need to be signed to account for replacement
                // code that is shorter than the original code.
                let mut offset: isize = 0;
                // Only show an underline in the suggestions if the suggestion is not the
                // entirety of the code being shown and the displayed code is not multiline.
                if show_underline {
                    draw_col_separator(&mut buffer, row_num, max_line_num_len + 1);
                    for part in parts {
                        let span_start_pos = sm.lookup_char_pos(part.span.lo()).col_display;
                        let span_end_pos = sm.lookup_char_pos(part.span.hi()).col_display;

                        // Do not underline the leading...
                        let start = part
                            .snippet
                            .len()
                            .saturating_sub(part.snippet.trim_start().len());
                        // ...or trailing spaces. Account for substitutions containing unicode
                        // characters.
                        let sub_len = part.snippet.trim().chars().fold(0, |acc, ch| {
                            acc + unicode_width::UnicodeWidthChar::width(ch).unwrap_or(0)
                        });

                        let underline_start = (span_start_pos + start) as isize + offset;
                        let underline_end = (span_start_pos + start + sub_len) as isize + offset;
                        for p in underline_start..underline_end {
                            buffer.putc(
                                row_num,
                                max_line_num_len + 3 + p as usize,
                                '^',
                                Style::UnderlinePrimary,
                            );
                        }
                        // underline removals too
                        if underline_start == underline_end {
                            for p in underline_start - 1..=underline_start {
                                buffer.putc(
                                    row_num,
                                    max_line_num_len + 3 + p as usize,
                                    '-',
                                    Style::UnderlineSecondary,
                                );
                            }
                        }

                        // length of the code after substitution
                        let full_sub_len = part.snippet.chars().fold(0, |acc, ch| {
                            acc + unicode_width::UnicodeWidthChar::width(ch).unwrap_or(0) as isize
                        });

                        // length of the code to be substituted
                        let snippet_len = span_end_pos as isize - span_start_pos as isize;
                        // For multiple substitutions, use the position *after* the previous
                        // substitutions have happened.
                        offset += full_sub_len - snippet_len;
                    }
                    row_num += 1;
                }

                // if we elided some lines, add an ellipsis
                if lines.next().is_some() {
                    buffer.puts(row_num, max_line_num_len - 1, "...", Style::LineNumber);
                } else if !show_underline {
                    draw_col_separator_no_space(&mut buffer, row_num, max_line_num_len + 1);
                    row_num += 1;
                }
            }
            if suggestions.len() > MAX_SUGGESTIONS {
                let msg = format!(
                    "and {} other candidates",
                    suggestions.len() - MAX_SUGGESTIONS
                );
                buffer.puts(row_num, 0, &msg, Style::NoStyle);
            }
            emit_to_destination(&buffer.render(), level, &mut self.dst, self.short_message)?;
        }
        Ok(())
    }

    fn emit_messages_default(
        &mut self,
        level: Level,
        message: &[Message],
        code: &Option<DiagnosticId>,
        span: &MultiSpan,
        children: &[SubDiagnostic],
        suggestions: &[CodeSuggestion],
    ) {
        let max_line_num_len = if self.ui_testing {
            ANONYMIZED_LINE_NUM.len()
        } else {
            self.get_max_line_num(span, children).to_string().len()
        };

        match self.emit_message_default(span, message, code, level, max_line_num_len, false) {
            Ok(()) => {
                if !children.is_empty() {
                    let mut buffer = StyledBuffer::new();
                    if !self.short_message {
                        draw_col_separator_no_space(&mut buffer, 0, max_line_num_len + 1);
                    }
                    match emit_to_destination(
                        &buffer.render(),
                        level,
                        &mut self.dst,
                        self.short_message,
                    ) {
                        Ok(()) => (),
                        Err(e) => panic!("failed to emit error: {e}"),
                    }
                }
                if !self.short_message {
                    for child in children {
                        let span = child.render_span.as_ref().unwrap_or(&child.span);
                        if let Err(e) = self.emit_message_default(
                            span,
                            child.styled_message(),
                            &None,
                            child.level,
                            max_line_num_len,
                            true,
                        ) {
                            panic!("failed to emit error: {e}")
                        }
                    }
                    for sugg in suggestions {
                        if let Err(e) =
                            self.emit_suggestion_default(sugg, Level::Help, max_line_num_len)
                        {
                            panic!("failed to emit error: {e}")
                        }
                    }
                }
            }
            Err(e) => panic!("failed to emit error: {e}"),
        }

        let mut dst = self.dst.writable();
        if let Err(e) = writeln!(dst).and_then(|_| dst.flush()) {
            panic!("failed to emit error: {e}")
        }
    }
}

fn draw_col_separator(buffer: &mut StyledBuffer, line: usize, col: usize) {
    buffer.puts(line, col, "| ", Style::LineNumber);
}

fn draw_col_separator_no_space(buffer: &mut StyledBuffer, line: usize, col: usize) {
    draw_col_separator_no_space_with_style(buffer, line, col, Style::LineNumber);
}

fn draw_col_separator_no_space_with_style(
    buffer: &mut StyledBuffer,
    line: usize,
    col: usize,
    style: Style,
) {
    buffer.putc(line, col, '|', style);
}

fn draw_range(
    buffer: &mut StyledBuffer,
    symbol: char,
    line: usize,
    col_from: usize,
    col_to: usize,
    style: Style,
) {
    for col in col_from..col_to {
        buffer.putc(line, col, symbol, style);
    }
}

fn draw_note_separator(buffer: &mut StyledBuffer, line: usize, col: usize) {
    buffer.puts(line, col, "= ", Style::LineNumber);
}

fn draw_multiline_line(
    buffer: &mut StyledBuffer,
    line: usize,
    offset: usize,
    depth: usize,
    style: Style,
) {
    buffer.putc(line, offset + depth - 1, '|', style);
}

fn num_overlap(
    a_start: usize,
    a_end: usize,
    b_start: usize,
    b_end: usize,
    inclusive: bool,
) -> bool {
    let extra = usize::from(inclusive);
    (b_start..b_end + extra).contains(&a_start) || (a_start..a_end + extra).contains(&b_start)
}

fn overlaps(a1: &Annotation, a2: &Annotation, padding: usize) -> bool {
    num_overlap(
        a1.start_col,
        a1.end_col + padding,
        a2.start_col,
        a2.end_col,
        false,
    )
}

fn emit_to_destination(
    rendered_buffer: &[Vec<StyledString>],
    lvl: Level,
    dst: &mut Destination,
    short_message: bool,
) -> io::Result<()> {
    use super::lock;

    let mut dst = dst.writable();

    // In order to prevent error message interleaving, where multiple error lines
    // get intermixed when multiple compiler processes error simultaneously, we
    // emit errors with additional steps.
    //
    // On Unix systems, we write into a buffered terminal rather than directly to a
    // terminal. When the .flush() is called we take the buffer created from the
    // buffered writes and write it at one shot.  Because the Unix systems use
    // ANSI for the colors, which is a text-based styling scheme, this buffered
    // approach works and maintains the styling.
    //
    // On Windows, styling happens through calls to a terminal API. This prevents us
    // from using the same buffering approach.  Instead, we use a global Windows
    // mutex, which we acquire long enough to output the full error message,
    // then we release.
    let _buffer_lock = lock::acquire_global_lock("rustc_errors");
    for (pos, line) in rendered_buffer.iter().enumerate() {
        for part in line {
            #[cfg(feature = "tty-emitter")]
            dst.apply_style(lvl, part.style)?;
            write!(dst, "{}", part.text)?;
            dst.reset()?;
        }
        if !short_message && (!lvl.is_failure_note() || pos != rendered_buffer.len() - 1) {
            writeln!(dst)?;
        }
    }
    dst.flush()?;
    Ok(())
}

pub enum Destination {
    #[cfg(feature = "tty-emitter")]
    Terminal(StandardStream),
    #[cfg(feature = "tty-emitter")]
    Buffered(BufferWriter),
    Raw(Box<dyn Write + Send>),
}

pub enum WritableDst<'a> {
    #[cfg(feature = "tty-emitter")]
    Terminal(&'a mut StandardStream),
    #[cfg(feature = "tty-emitter")]
    Buffered(&'a mut BufferWriter, Buffer),
    Raw(&'a mut (dyn Write + Send)),
}

impl Destination {
    #[cfg(feature = "tty-emitter")]
    fn from_stderr(color: ColorConfig) -> Destination {
        let choice = color.to_color_choice();
        // On Windows we'll be performing global synchronization on the entire
        // system for emitting rustc errors, so there's no need to buffer
        // anything.
        //
        // On non-Windows we rely on the atomicity of `write` to ensure errors
        // don't get all jumbled up.
        if cfg!(windows) {
            Terminal(StandardStream::stderr(choice))
        } else {
            Buffered(BufferWriter::stderr(choice))
        }
    }

    fn writable(&mut self) -> WritableDst<'_> {
        match *self {
            #[cfg(feature = "tty-emitter")]
            Destination::Terminal(ref mut t) => WritableDst::Terminal(t),
            #[cfg(feature = "tty-emitter")]
            Destination::Buffered(ref mut t) => {
                let buf = t.buffer();
                WritableDst::Buffered(t, buf)
            }
            Destination::Raw(ref mut t) => WritableDst::Raw(t),
        }
    }
}

impl WritableDst<'_> {
    #[cfg(feature = "tty-emitter")]
    fn apply_style(&mut self, lvl: Level, style: Style) -> io::Result<()> {
        let mut spec = ColorSpec::new();
        match style {
            Style::LineAndColumn => {}
            Style::LineNumber => {
                spec.set_bold(true);
                spec.set_intense(true);
                if cfg!(windows) {
                    spec.set_fg(Some(Color::Cyan));
                } else {
                    spec.set_fg(Some(Color::Blue));
                }
            }
            Style::Quotation => {}
            Style::OldSchoolNoteText | Style::MainHeaderMsg => {
                spec.set_bold(true);
                if cfg!(windows) {
                    spec.set_intense(true).set_fg(Some(Color::White));
                }
            }
            Style::UnderlinePrimary | Style::LabelPrimary => {
                spec = lvl.color();
                spec.set_bold(true);
            }
            Style::UnderlineSecondary | Style::LabelSecondary => {
                spec.set_bold(true).set_intense(true);
                if cfg!(windows) {
                    spec.set_fg(Some(Color::Cyan));
                } else {
                    spec.set_fg(Some(Color::Blue));
                }
            }
            Style::HeaderMsg | Style::NoStyle => {}
            Style::Level(lvl) => {
                spec = lvl.color();
                spec.set_bold(true);
            }
            Style::Highlight => {
                spec.set_bold(true);
            }
        }
        self.set_color(&spec)
    }

    #[cfg(feature = "tty-emitter")]
    fn set_color(&mut self, color: &ColorSpec) -> io::Result<()> {
        match *self {
            #[cfg(feature = "tty-emitter")]
            WritableDst::Terminal(ref mut t) => t.set_color(color),
            #[cfg(feature = "tty-emitter")]
            WritableDst::Buffered(_, ref mut t) => t.set_color(color),
            WritableDst::Raw(_) => Ok(()),
        }
    }

    fn reset(&mut self) -> io::Result<()> {
        match *self {
            #[cfg(feature = "tty-emitter")]
            WritableDst::Terminal(ref mut t) => t.reset(),
            #[cfg(feature = "tty-emitter")]
            WritableDst::Buffered(_, ref mut t) => t.reset(),
            WritableDst::Raw(_) => Ok(()),
        }
    }
}

impl Write for WritableDst<'_> {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        match *self {
            #[cfg(feature = "tty-emitter")]
            WritableDst::Terminal(ref mut t) => t.write(bytes),
            #[cfg(feature = "tty-emitter")]
            WritableDst::Buffered(_, ref mut buf) => buf.write(bytes),
            WritableDst::Raw(ref mut w) => w.write(bytes),
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        match *self {
            #[cfg(feature = "tty-emitter")]
            WritableDst::Terminal(ref mut t) => t.flush(),
            #[cfg(feature = "tty-emitter")]
            WritableDst::Buffered(_, ref mut buf) => buf.flush(),
            WritableDst::Raw(ref mut w) => w.flush(),
        }
    }
}

impl Drop for WritableDst<'_> {
    fn drop(&mut self) {
        #[cfg(feature = "tty-emitter")]
        if let WritableDst::Buffered(ref mut dst, ref mut buf) = self {
            drop(dst.print(buf));
        }
    }
}
