use std::{rc::Rc, sync::Arc};

use swc_common::{BytePos, SourceMap, SourceMapper, SourceMapperDyn, Span, Spanned};

use crate::list::ListFormat;

pub trait SpanExt: Spanned {
    #[inline]
    fn is_synthesized(&self) -> bool {
        false
    }

    fn starts_on_new_line(&self, format: ListFormat) -> bool {
        format.intersects(ListFormat::PreferNewLine)
    }

    /// Gets a custom text range to use when emitting comments.
    fn comment_range(&self) -> Span {
        //TODO
        self.span()
    }
}
impl<T: Spanned> SpanExt for T {}

pub trait SourceMapperExt {
    fn get_code_map(&self) -> &dyn SourceMapper;

    fn is_on_same_line(&self, _lo: BytePos, _hi: BytePos) -> bool {
        // let cm = self.get_code_map();

        // let lo = cm.lookup_char_pos(lo);
        // let hi = cm.lookup_char_pos(hi);

        // lo.line == hi.line && lo.file.name_hash == hi.file.name_hash
        false
    }

    fn should_write_separating_line_terminator(
        &self,
        prev: Option<Span>,
        next: Option<Span>,
        format: ListFormat,
    ) -> bool {
        if format.contains(ListFormat::MultiLine) {
            return true;
        }

        if format.contains(ListFormat::PreserveLines) {
            if let (Some(prev), Some(next)) = (prev, next) {
                if prev.is_synthesized() || next.is_synthesized() {
                    return prev.starts_on_new_line(format) || next.starts_on_new_line(format);
                }

                return !self.is_on_same_line(prev.hi(), next.lo());
            } else {
                return false;
            }
        }

        false
    }

    fn should_write_leading_line_terminator(
        &self,
        parent_node: Span,
        first_child: Option<Span>,
        format: ListFormat,
    ) -> bool {
        if format.contains(ListFormat::MultiLine) {
            return true;
        }

        if format.contains(ListFormat::PreserveLines) {
            if format.contains(ListFormat::PreferNewLine) {
                return true;
            }

            if first_child.is_none() {
                return !self.is_on_same_line(parent_node.lo(), parent_node.hi());
            }

            let first_child = first_child.unwrap();
            if parent_node.is_synthesized() || first_child.is_synthesized() {
                return first_child.starts_on_new_line(format);
            }

            !self.is_on_same_line(parent_node.lo(), first_child.lo())
        } else {
            false
        }
    }

    fn should_write_closing_line_terminator(
        &self,
        parent_node: Span,
        last_child: Option<Span>,
        format: ListFormat,
    ) -> bool {
        if format.contains(ListFormat::MultiLine) {
            return (format & ListFormat::NoTrailingNewLine) == ListFormat::None;
        }

        if format.contains(ListFormat::PreserveLines) {
            if format.contains(ListFormat::PreferNewLine) {
                return true;
            }

            if last_child.is_none() {
                return !self.is_on_same_line(parent_node.lo(), parent_node.hi());
            }

            let last_child = last_child.unwrap();

            if parent_node.is_synthesized() || last_child.is_synthesized() {
                last_child.starts_on_new_line(format)
            } else {
                !self.is_on_same_line(parent_node.hi(), last_child.hi())
            }
        } else {
            false
        }
    }
}

impl SourceMapperExt for SourceMap {
    fn get_code_map(&self) -> &dyn SourceMapper {
        self
    }
}

impl SourceMapperExt for dyn SourceMapper {
    fn get_code_map(&self) -> &dyn SourceMapper {
        self
    }
}

impl SourceMapperExt for Arc<SourceMapperDyn> {
    fn get_code_map(&self) -> &dyn SourceMapper {
        &**self
    }
}
impl SourceMapperExt for Rc<SourceMapperDyn> {
    fn get_code_map(&self) -> &dyn SourceMapper {
        &**self
    }
}

impl SourceMapperExt for Arc<SourceMap> {
    fn get_code_map(&self) -> &dyn SourceMapper {
        &**self
    }
}

impl SourceMapperExt for Rc<SourceMap> {
    fn get_code_map(&self) -> &dyn SourceMapper {
        &**self
    }
}
