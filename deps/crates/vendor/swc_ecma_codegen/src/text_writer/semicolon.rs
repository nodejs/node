use swc_common::{BytePos, Span, DUMMY_SP};

use super::{Result, WriteJs};

pub fn omit_trailing_semi<W: WriteJs>(w: W) -> impl WriteJs {
    OmitTrailingSemi {
        inner: w,
        pending_semi: None,
    }
}

#[derive(Debug, Clone)]
struct OmitTrailingSemi<W: WriteJs> {
    inner: W,
    pending_semi: Option<Span>,
}

macro_rules! with_semi {
    (
        $fn_name:ident
        (
            $(
                $arg_name:ident
                :
                $arg_ty:ty
            ),*
        )
    ) => {
        fn $fn_name(&mut self, $($arg_name: $arg_ty),* ) -> Result {
            self.commit_pending_semi()?;

            self.inner.$fn_name( $($arg_name),* )
        }
    };
}

impl<W: WriteJs> WriteJs for OmitTrailingSemi<W> {
    with_semi!(increase_indent());

    with_semi!(decrease_indent());

    with_semi!(write_space());

    with_semi!(write_comment(s: &str));

    with_semi!(write_keyword(span: Option<Span>, s: &'static str));

    with_semi!(write_operator(span: Option<Span>, s: &str));

    with_semi!(write_param(s: &str));

    with_semi!(write_property(s: &str));

    with_semi!(write_line());

    with_semi!(write_lit(span: Span, s: &str));

    with_semi!(write_str_lit(span: Span, s: &str));

    with_semi!(write_str(s: &str));

    with_semi!(write_symbol(span: Span, s: &str));

    fn write_semi(&mut self, span: Option<Span>) -> Result {
        self.pending_semi = Some(span.unwrap_or(DUMMY_SP));
        Ok(())
    }

    fn write_punct(
        &mut self,
        span: Option<Span>,
        s: &'static str,
        commit_pending_semi: bool,
    ) -> Result {
        if commit_pending_semi {
            self.commit_pending_semi()?;
        } else {
            self.pending_semi = None;
        }
        self.inner.write_punct(span, s, commit_pending_semi)
    }

    #[inline]
    fn care_about_srcmap(&self) -> bool {
        self.inner.care_about_srcmap()
    }

    #[inline]
    fn add_srcmap(&mut self, pos: BytePos) -> Result {
        self.inner.add_srcmap(pos)
    }

    fn commit_pending_semi(&mut self) -> Result {
        if let Some(span) = self.pending_semi {
            self.inner.write_semi(Some(span))?;
            self.pending_semi = None;
        }
        Ok(())
    }

    #[inline(always)]
    fn can_ignore_invalid_unicodes(&mut self) -> bool {
        self.inner.can_ignore_invalid_unicodes()
    }
}
