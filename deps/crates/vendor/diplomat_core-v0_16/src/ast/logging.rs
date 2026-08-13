use std::sync::RwLock;

use crate::ast::{idents::Span, Ident, SpanLocation};

/// For overwriting by tests.
static WRITER: RwLock<&(dyn Fn() -> Box<dyn std::io::Write> + Send + Sync)> =
    RwLock::new(&(|| Box::new(std::io::stderr())));

pub(crate) struct ContextLocation {
    // Allow for pretty-print:
    #[allow(unused)]
    location: Span,
    // Allow for pretty-print:
    #[allow(unused)]
    label: String,
}

impl ContextLocation {
    pub fn new(location: Span, label: String) -> Self {
        Self { location, label }
    }
}

pub(crate) struct AstReport {
    title: String,
    primary_loc: Option<Span>,
    primary_label: String,
    // Used for pretty-printing, not ugly printing:
    #[allow(unused)]
    context_locations: Vec<ContextLocation>,
}

impl AstReport {
    pub fn new(
        title: String,
        primary_loc: Option<Span>,
        primary_label: String,
        context_locations: Vec<ContextLocation>,
    ) -> Self {
        Self {
            title,
            primary_loc,
            primary_label,
            context_locations,
        }
    }
}

pub(crate) fn create_simple_report(id: Ident, title: String, label: String) -> ! {
    create_report(AstReport {
        title,
        primary_loc: id.span(),
        primary_label: label,
        context_locations: vec![],
    })
}

/// Bytes range has not been stabilized in Rust macro.
/// We can't tell if we're in a proc macro context,
/// so we just check if the range doesn't make sense:
fn evaluate_bytes(sp: &super::idents::Span, src: &str) -> std::ops::Range<usize> {
    if sp.range.is_empty() && (sp.start.line != sp.end.line || sp.end.col - sp.start.col > 0) {
        match sp.span_location {
            SpanLocation::None => 0..0,
            _ => {
                // Need an accurate byte count that accounts for both:
                // CRLF and LF endings, so we just make sure to split on the end at `\n` (LF):
                let split = src.split_inclusive('\n');
                let mut start_byte = 0usize;
                let mut end_byte = src.len();
                let mut running_byte_total = 0usize;
                for (idx, st) in split.enumerate() {
                    if (idx + 1) == sp.start.line {
                        start_byte = running_byte_total + sp.start.col;
                    }
                    if (idx + 1) == sp.end.line {
                        end_byte = running_byte_total + sp.end.col;
                        break;
                    }
                    running_byte_total += st.len();
                }
                start_byte..end_byte
            }
        }
    } else {
        sp.range.clone()
    }
}

pub(crate) fn create_report(report: AstReport) -> ! {
    use std::io::Write;
    let mut out = WRITER.read().unwrap()();

    let span = report.primary_loc;
    let src = if let Some(sp) = &span {
        match &sp.span_location {
            SpanLocation::FilePath(f) => {
                let st = std::fs::read_to_string(f);
                if let Ok(s) = st {
                    s
                } else {
                    panic!("Could not read source file {f}: {:?}", st.unwrap_err());
                }
            }
            SpanLocation::LocalSource(src) => src.clone(),
            SpanLocation::None => "<unknown location>".into(),
        }
    } else {
        "<No associated span>".into()
    };

    let bytes_range = span.as_ref().map(|sp| evaluate_bytes(sp, &src));

    if let Some(b) = &bytes_range {
        // If we go past the length, then we've somehow got the wrong SpanLocation.
        if matches!(
            span.as_ref().map(|s| &s.span_location),
            Some(SpanLocation::FilePath(..)) | Some(SpanLocation::LocalSource(..))
        ) && b.end > src.len()
        {
            panic!(
                "Span source improperly calculated. Got range {} > {}. Original error: {}: {}",
                b.end,
                src.len(),
                report.title,
                report.primary_label
            );
        }
    }
    #[cfg(feature = "pretty-print")]
    {
        use annotate_snippets::{renderer::DecorStyle, Level, Renderer, Snippet};
        let report = if let Some(sp) = span {
            use annotate_snippets::{Annotation, AnnotationKind};

            let annotations = report.context_locations.iter().map(|l| {
                let bytes = evaluate_bytes(&l.location, &src);
                AnnotationKind::Context.span(bytes).label(&l.label)
            });

            &[Level::ERROR.primary_title(&report.title).element(
                Snippet::<Annotation>::source(&src)
                    .path(match sp.span_location {
                        SpanLocation::FilePath(f) => Some(f),
                        _ => None,
                    })
                    .annotation(
                        AnnotationKind::Primary
                            .span(bytes_range.unwrap())
                            .label(report.primary_label),
                    )
                    .annotations(annotations),
            )]
        } else {
            &[Level::ERROR
                .primary_title(&report.title)
                .element(Level::ERROR.message(report.primary_label))]
        };
        let renderer = Renderer::styled().decor_style(DecorStyle::Unicode);
        writeln!(out, "{}", renderer.render(report)).expect("Could not write report.");
    }
    #[cfg(not(feature = "pretty-print"))]
    {
        let (location, excerpt_pre, excerpt, excerpt_post) = if let Some(sp) = span {
            let range = bytes_range.unwrap();
            let start = sp.start.line;
            // Columns are 0-indexed, but most editors use 1-indexing:
            let col = sp.start.col + 1;
            let span_location = match &sp.span_location {
                SpanLocation::FilePath(f) => f.clone(),
                SpanLocation::LocalSource(..) => "<Inline source>".into(),
                SpanLocation::None => "<Unknown location>".into(),
            };
            let range_pre = range.start.saturating_sub(5);
            let range_post = std::cmp::min(range.end.saturating_add(5), src.len());
            match sp.span_location {
                SpanLocation::None => (span_location, "", "<Excerpt not available>", ""),
                _ => (
                    format!("{span_location}:{start}:{col}"),
                    &src[range_pre..range.start],
                    &src[range.start..range.end],
                    &src[range.end..range_post],
                ),
            }
        } else {
            (
                "<No associated span>".into(),
                "",
                "<Excerpt not available>",
                "",
            )
        };
        write!(out, "Diplomat error: ").expect("Could not write to report.");
        writeln!(out, "{}", report.title).expect("Could not write to report.");
        writeln!(out, "In {location}:").expect("Could not write to report.");

        let excerpt_pre_trimmed = excerpt_pre.trim_start();

        if !excerpt_pre.is_empty() {
            write!(out, "...{}", excerpt_pre_trimmed).expect("Could not write to report.");
        }

        write!(out, "{excerpt}").expect("Could not write to report.");

        if !excerpt_post.is_empty() {
            writeln!(out, "{}...", excerpt_post.trim_end()).expect("Could not write to report.");
        }

        // Add visible separation between the label and the excerpt.
        if !excerpt.is_empty() {
            // This works well for most one-line excerpts.
            // The pretty-printer tends to handle whitespacing better, however.
            writeln!(
                out,
                "{}{}",
                " ".repeat(3 + excerpt_pre_trimmed.len()),
                "^".repeat(excerpt.len())
            )
            .expect("Could not write to report.");
        }

        write!(out, "{}", report.primary_label).expect("Could not write to report.");
    }
    out.flush().expect("Could not write to output.");
    // Rust-analyzer will not show error messages unless we panic,
    // This just tells rust-analyzer users to check stderr:
    panic!("Diplomat error: {} (check stderr for more)", report.title);
}

#[cfg(all(test, not(feature = "pretty-print")))]
mod tests {
    use std::fmt::Write;

    use crate::ast::ModuleIncludeInfo;

    #[derive(Clone, Debug)]
    struct StderrWrapper {
        buf: String,
    }
    impl std::io::Write for StderrWrapper {
        fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
            let st = str::from_utf8(buf).expect("Could not read utf8");
            self.buf.write_str(st).expect("Could not write str");
            Ok(buf.len())
        }

        fn flush(&mut self) -> std::io::Result<()> {
            insta::assert_snapshot!(self.buf);
            Ok(())
        }
    }

    fn reader_fn() -> Box<dyn std::io::Write> {
        Box::new(StderrWrapper { buf: String::new() })
    }

    fn parse_file_hook_errors(file_loc: &str, suffix: &str) {
        let folder_pth_name = format!("src/ast/snapshots/span_testing");
        let folder_pth = std::path::Path::new(&folder_pth_name);
        let file_path = folder_pth.join(file_loc);

        let mut settings = insta::Settings::clone_current();
        settings.set_snapshot_suffix(format!("{file_loc}_{suffix}"));
        settings.set_snapshot_path("snapshots/span_testing");
        let _drop = settings.bind_to_scope();

        let st = std::fs::read_to_string(&file_path).expect("Could not read file.");
        let p = syn::parse_str::<syn::ItemMod>(&st).expect("Could not parse syn mod");
        crate::ast::Module::from_syn(
            &p,
            true,
            Some(ModuleIncludeInfo {
                base_path: &folder_pth,
                cache: None,
            }),
            &crate::ast::SpanLocation::FilePath(format!("{}/{file_loc}", folder_pth_name)),
        );
    }

    const FILES_TO_TEST: &[&str] = &[
        "duplicate_attrs.rs",
        "enum_field_variant.rs",
        "attr_on_non_pub.rs",
        "nonstd_result.rs",
        "self_in_free_func.rs",
        "lifetime_on_trait.rs",
        "generic_types.rs",
        "where_pred.rs",
        "lifetime_redefine.rs",
        "undefined_macro.rs",
        "macro_parse_error.rs",
        "macro_expansion_error.rs",
        "self_type.rs",
        "param_invalid_type.rs",
        "undefined_type.rs",
        "non_opaque_tuple.rs",
        "generic_type.rs",
        "free_func_generics.rs",
        "trait_bound_generics.rs",
        "mut_string.rs",
        "slice_of_slice.rs",
        "owned_slice.rs",
        "box_type_arg.rs",
        "missing_angle_brackets.rs",
        "option_type_args.rs",
        "option_without_angle_brackets.rs",
        "invalid_slice.rs",
        "tuples.rs",
        "multi_traits.rs",
        "result_type_args.rs",
        "result_angle_brackets.rs",
        "lifetime_callback_param.rs",
        "unsupported_fn_type.rs",
        "ns_lifetime.rs",
        "unsupported_bound.rs",
        "unsupported_type.rs",
        "slice_no_type.rs",
        "malformed_attr.rs",
        "malformed_cfg.rs",
        "malformed_demo.rs",
        "malformed_include.rs",
        "malformed_rust_link.rs",
        "error_in_included.rs",
    ];

    fn test_file_list(suffix: &'static str) {
        {
            let mut inner = super::WRITER.try_write().unwrap();
            *inner = &reader_fn;
        }
        let mut results = vec![];
        for f in FILES_TO_TEST {
            let t = std::thread::spawn(|| {
                parse_file_hook_errors(f, suffix);
            });
            results.push((f, t.join()));
        }
        for (f, res) in results {
            match res {
                Ok(_) => {}
                Err(p) => {
                    if let Some(st) = p.downcast_ref::<String>() {
                        if !st.contains("Diplomat error") {
                            panic!("{f}: {st}");
                        }
                    } else {
                        panic!("{f}: Could not convert error to string.");
                    }
                }
            }
        }
    }

    #[cfg(not(feature = "pretty-print"))]
    #[test]
    fn test_errors_ugly() {
        test_file_list("ugly");
    }
}
