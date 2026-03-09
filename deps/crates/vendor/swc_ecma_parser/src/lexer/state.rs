use std::mem::take;

use swc_atoms::{wtf8::CodePoint, Atom};
use swc_common::{BytePos, Span};
use swc_ecma_ast::EsVersion;

use super::{Context, Input, Lexer};
use crate::{
    byte_search,
    error::{Error, SyntaxError},
    input::Tokens,
    lexer::{
        char_ext::CharExt,
        comments_buffer::{BufferedCommentKind, CommentsBufferCheckpoint},
        search::SafeByteMatchTable,
        token::{Token, TokenAndSpan, TokenValue},
        LexResult,
    },
    safe_byte_match_table,
    syntax::SyntaxFlags,
};

bitflags::bitflags! {
    #[derive(Debug, Default, Clone, Copy)]
    pub struct TokenFlags: u8 {
        const UNICODE = 1 << 0;
    }
}

static JSX_CHILD_TABLE: SafeByteMatchTable =
    safe_byte_match_table!(|b| matches!(b, b'{' | b'}' | b'<' | b'>' | b'&'));

/// State of lexer.
///
/// Ported from babylon.
#[derive(Clone)]
pub struct State {
    /// if line break exists between previous token and new token?
    pub had_line_break: bool,
    pub next_regexp: Option<BytePos>,
    pub prev_hi: BytePos,

    pub(super) token_value: Option<TokenValue>,
    token_type: Option<Token>,
}

pub struct LexerCheckpoint {
    comments_buffer: CommentsBufferCheckpoint,
    state: State,
    ctx: Context,
    input_last_pos: BytePos,
}

impl crate::input::Tokens for Lexer<'_> {
    type Checkpoint = LexerCheckpoint;

    fn checkpoint_save(&self) -> LexerCheckpoint {
        LexerCheckpoint {
            state: self.state.clone(),
            ctx: self.ctx,
            input_last_pos: self.input.last_pos(),
            comments_buffer: self
                .comments_buffer
                .as_ref()
                .map(|cb| cb.checkpoint_save())
                .unwrap_or_default(),
        }
    }

    fn checkpoint_load(&mut self, checkpoint: LexerCheckpoint) {
        self.state = checkpoint.state;
        self.ctx = checkpoint.ctx;
        unsafe { self.input.reset_to(checkpoint.input_last_pos) };
        if let Some(comments_buffer) = self.comments_buffer.as_mut() {
            comments_buffer.checkpoint_load(checkpoint.comments_buffer);
        }
    }

    #[inline]
    fn read_string(&self, span: Span) -> &str {
        assert!(span.lo >= self.input.start_pos() && span.hi <= self.input.end_pos());
        unsafe { self.input_slice_str(span.lo, span.hi) }
    }

    #[inline]
    fn set_ctx(&mut self, ctx: Context) {
        self.ctx = ctx
    }

    #[inline]
    fn ctx(&self) -> Context {
        self.ctx
    }

    #[inline]
    fn ctx_mut(&mut self) -> &mut Context {
        &mut self.ctx
    }

    #[inline]
    fn syntax(&self) -> SyntaxFlags {
        self.syntax
    }

    #[inline]
    fn target(&self) -> EsVersion {
        self.target
    }

    #[inline]
    fn start_pos(&self) -> BytePos {
        self.start_pos
    }

    #[inline]
    fn set_expr_allowed(&mut self, _: bool) {}

    #[inline]
    fn set_next_regexp(&mut self, start: Option<BytePos>) {
        self.state.next_regexp = start;
    }

    fn add_error(&mut self, error: Error) {
        self.errors.push(error);
    }

    fn add_module_mode_error(&mut self, error: Error) {
        if self.ctx.contains(Context::Module) {
            self.add_error(error);
            return;
        }
        self.module_errors.push(error);
    }

    #[inline]
    fn take_errors(&mut self) -> Vec<Error> {
        let mut errors = take(&mut self.errors);
        if self.ctx.contains(Context::Module) && !self.module_errors.is_empty() {
            errors.append(&mut self.module_errors);
        }
        errors
    }

    #[inline]
    fn take_script_module_errors(&mut self) -> Vec<Error> {
        take(&mut self.module_errors)
    }

    #[inline]
    fn end_pos(&self) -> BytePos {
        self.input.end_pos()
    }

    #[inline]
    fn update_token_flags(&mut self, f: impl FnOnce(&mut TokenFlags)) {
        f(&mut self.token_flags)
    }

    #[inline]
    fn token_flags(&self) -> TokenFlags {
        self.token_flags
    }

    fn clone_token_value(&self) -> Option<TokenValue> {
        self.state.token_value.clone()
    }

    fn get_token_value(&self) -> Option<&TokenValue> {
        self.state.token_value.as_ref()
    }

    fn set_token_value(&mut self, token_value: Option<TokenValue>) {
        self.state.token_value = token_value;
    }

    fn take_token_value(&mut self) -> Option<TokenValue> {
        self.state.token_value.take()
    }

    fn first_token(&mut self) -> TokenAndSpan {
        let mut start = self.cur_pos();
        let token = match self.read_shebang() {
            Ok(Some(shebang)) => {
                self.state.set_token_value(TokenValue::Word(shebang));
                Ok(Token::Shebang)
            }
            // Fallback to other tokens
            Ok(None) => {
                self.state.had_line_break = true;
                self.skip_space();
                start = self.input.cur_pos();
                self.read_token()
            }
            Err(error) => Err(error),
        };

        let token = match token {
            Ok(token) => token,
            Err(error) => {
                self.state.set_token_value(TokenValue::Error(error));
                Token::Error
            }
        };
        self.finish_next_token(self.span(start), token)
    }

    fn next_token(&mut self) -> TokenAndSpan {
        let mut start = self.cur_pos();
        let token = match self.read_next_token(&mut start) {
            Ok(res) => res,
            Err(error) => {
                self.state.set_token_value(TokenValue::Error(error));
                Token::Error
            }
        };
        self.finish_next_token(self.span(start), token)
    }

    fn rescan_jsx_token(&mut self, reset: BytePos) -> TokenAndSpan {
        unsafe {
            self.input.reset_to(reset);
        }
        Tokens::scan_jsx_token(self)
    }

    fn rescan_jsx_open_el_terminal_token(&mut self, reset: BytePos) -> TokenAndSpan {
        unsafe {
            self.input.reset_to(reset);
        }
        Tokens::scan_jsx_open_el_terminal_token(self)
    }

    fn scan_jsx_token(&mut self) -> TokenAndSpan {
        let start = self.cur_pos();
        let res = match self.scan_jsx_token() {
            Ok(res) => Ok(res),
            Err(error) => {
                self.state.set_token_value(TokenValue::Error(error));
                Err(Token::Error)
            }
        };
        let token = match res {
            Ok(t) => t,
            Err(e) => e,
        };
        self.finish_next_token(self.span(start), token)
    }

    fn scan_jsx_open_el_terminal_token(&mut self) -> TokenAndSpan {
        self.skip_space();
        let start = self.input.cur_pos();
        let res = match self.scan_jsx_attrs_terminal_token() {
            Ok(res) => Ok(res),
            Err(error) => {
                self.state.set_token_value(TokenValue::Error(error));
                Err(Token::Error)
            }
        };
        let token = match res {
            Ok(t) => t,
            Err(e) => e,
        };
        self.finish_next_token(self.span(start), token)
    }

    fn scan_jsx_identifier(&mut self, start: BytePos) -> TokenAndSpan {
        let token = self.state.token_type.unwrap();
        debug_assert!(token.is_word());
        let mut v = String::with_capacity(16);
        while let Some(ch) = self.input().cur() {
            if ch == b'-' {
                v.push(ch as char);
                self.bump(1); // `-`
            } else {
                let old_pos = self.cur_pos();
                v.push_str(&self.scan_identifier_parts());
                if self.cur_pos() == old_pos {
                    break;
                }
            }
        }
        let v = if !v.is_empty() {
            let v = if token.is_known_ident() || token.is_keyword() {
                format!("{token}{v}")
            } else if let Some(TokenValue::Word(value)) = self.state.token_value.take() {
                format!("{value}{v}")
            } else {
                format!("{token}{v}")
            };
            self.atom(v)
        } else if token.is_known_ident() || token.is_keyword() {
            self.atom(token.to_string())
        } else if let Some(TokenValue::Word(value)) = self.state.token_value.take() {
            value
        } else {
            unreachable!(
                "`token_value` should be a word, but got: {:?}",
                self.state.token_value
            )
        };
        self.state.set_token_value(TokenValue::Word(v));
        TokenAndSpan {
            token: Token::JSXName,
            had_line_break: self.state.had_line_break,
            span: self.span(start),
        }
    }

    fn scan_jsx_attribute_value(&mut self) -> TokenAndSpan {
        let Some(cur) = self.cur() else {
            let start = self.cur_pos();
            return TokenAndSpan {
                token: Token::Eof,
                had_line_break: self.state.had_line_break,
                span: self.span(start),
            };
        };
        let start = self.cur_pos();

        match cur {
            b'\'' | b'"' => {
                let token = self.read_jsx_str(cur as char);
                let token = match token {
                    Ok(token) => token,
                    Err(e) => {
                        self.state.set_token_value(TokenValue::Error(e));
                        return TokenAndSpan {
                            token: Token::Error,
                            had_line_break: self.state.had_line_break,
                            span: self.span(start),
                        };
                    }
                };
                debug_assert!(self
                    .get_token_value()
                    .is_some_and(|t| matches!(t, TokenValue::Str { .. })));
                debug_assert!(token == Token::Str);
                TokenAndSpan {
                    token,
                    had_line_break: self.state.had_line_break,
                    span: self.span(start),
                }
            }
            _ => self.next_token(),
        }
    }

    fn rescan_template_token(
        &mut self,
        start: BytePos,
        start_with_back_tick: bool,
    ) -> TokenAndSpan {
        unsafe { self.input.reset_to(start) };
        let res = self.scan_template_token(start, start_with_back_tick);
        let token = match res.map_err(|e| {
            self.state.set_token_value(TokenValue::Error(e));
            Token::Error
        }) {
            Ok(t) => t,
            Err(e) => e,
        };
        let span = if start_with_back_tick {
            self.span(start)
        } else {
            // `+ BytePos(1)` is used to skip `{`
            self.span(start + BytePos(1))
        };
        self.finish_next_token(span, token)
    }
}

impl Lexer<'_> {
    fn read_next_token(&mut self, start: &mut BytePos) -> Result<Token, Error> {
        if let Some(next_regexp) = self.state.next_regexp {
            *start = next_regexp;
            return self.read_regexp(next_regexp);
        }

        self.state.had_line_break = false;
        self.skip_space();
        *start = self.input.cur_pos();

        self.read_token()
    }

    #[inline(always)]
    fn finish_next_token(&mut self, span: Span, token: Token) -> TokenAndSpan {
        if token == Token::Eof {
            self.consume_pending_comments();
        } else if let Some(comments) = self.comments_buffer.as_mut() {
            comments.pending_to_comment(BufferedCommentKind::Leading, span.lo);
        }

        self.state.set_token_type(token);
        self.state.prev_hi = self.last_pos();
        TokenAndSpan {
            token,
            had_line_break: self.state.had_line_break,
            span,
        }
    }

    fn scan_jsx_token(&mut self) -> Result<Token, Error> {
        debug_assert!(self.syntax.jsx());
        let start = self.input.cur_pos();
        match self.input.cur() {
            Some(b'<') => {
                self.bump(1);
                match self.input.cur() {
                    Some(b'/') => {
                        self.bump(1);
                        Ok(Token::LessSlash)
                    }
                    _ => Ok(Token::Lt),
                }
            }
            Some(b'{') => {
                self.bump(1);
                Ok(Token::LBrace)
            }
            Some(_) => {
                // Fast path: we assume there's no `&` in the jsx child
                byte_search! {
                    lexer: self,
                    table: JSX_CHILD_TABLE,
                    continue_if: (matched_byte, pos_offset) {
                        match matched_byte {
                            b'>' => {
                                let pos = start + BytePos(pos_offset as u32);
                                self.emit_error_span(
                                    Span::new_with_checked(pos, pos),
                                    SyntaxError::UnexpectedTokenWithSuggestions {
                                        candidate_list: vec!["`{'>'}`", "`&gt;`"],
                                    },
                                );
                                true
                            },
                            b'}' => {
                                let pos = start + BytePos(pos_offset as u32);
                                self.emit_error_span(
                                    Span::new_with_checked(pos, pos),
                                    SyntaxError::UnexpectedTokenWithSuggestions {
                                        candidate_list: vec!["`{'}'}`", "`&rbrace;`"],
                                    },
                                );
                                true
                            },
                            // Encountered `&`, go to the slow path
                            b'&' => return self.scan_jsx_token_with_jsx_entity(),
                            _ => false,
                        }
                    },
                    handle_eof: {
                        let s = unsafe {
                            // Safety: start and end are valid position because we got them from
                            // `self.input`
                            self.input_slice_str(start, self.cur_pos())
                        };

                        let value = self.atom(s);
                        self.state.set_token_value(TokenValue::JsxText(value));
                        return Ok(Token::JSXText);
                    },
                };

                let s = unsafe {
                    // Safety: start and end are valid position because we got them from
                    // `self.input`
                    self.input_slice_str(start, self.cur_pos())
                };

                let value = self.atom(s);
                self.state.set_token_value(TokenValue::JsxText(value));
                Ok(Token::JSXText)
            }
            None => Ok(Token::Eof),
        }
    }

    #[cold]
    /// Slow path: we encountered `&` in the jsx child, so we need to scan and
    /// resolve the jsx entity, which requires a dedicated string allocation.
    fn scan_jsx_token_with_jsx_entity(&mut self) -> LexResult<Token> {
        let mut value = String::new();
        let mut chunk_start = self.input.cur_pos();

        while let Some(ch) = self.input.cur_as_char() {
            match ch {
                '>' => {
                    let error_pos = self.input().cur_pos();
                    self.bump(1);
                    self.emit_error(
                        error_pos,
                        SyntaxError::UnexpectedTokenWithSuggestions {
                            candidate_list: vec!["`{'>'}`", "`&gt;`"],
                        },
                    );
                }
                '}' => {
                    let error_pos = self.input().cur_pos();
                    self.bump(1);
                    self.emit_error(
                        error_pos,
                        SyntaxError::UnexpectedTokenWithSuggestions {
                            candidate_list: vec!["`{'}'}`", "`&rbrace;`"],
                        },
                    );
                }
                '&' => {
                    // Push the string before the `&` to the result
                    let s = unsafe {
                        // Safety: We already checked for the range
                        self.input_slice_str(chunk_start, self.cur_pos())
                    };
                    value.push_str(s);

                    // Read the jsx entity and update the start of chunk
                    if let Ok(jsx_entity) = self.read_jsx_entity() {
                        value.push(jsx_entity.0);
                        chunk_start = self.input.cur_pos();
                    }
                }
                '<' | '{' => break,
                c => {
                    self.bump(c.len_utf8());
                }
            }
        }

        let s = unsafe {
            // Safety: start and end are valid position because we got them from
            // `self.input`
            self.input_slice_str(chunk_start, self.cur_pos())
        };

        value.push_str(s);
        let value = Atom::new(value);
        self.state.set_token_value(TokenValue::JsxText(value));
        Ok(Token::JSXText)
    }

    fn scan_jsx_attrs_terminal_token(&mut self) -> LexResult<Token> {
        if self.input.eat_byte(b'>') {
            Ok(Token::Gt)
        } else if self.input.eat_byte(b'/') {
            Ok(Token::Slash)
        } else {
            self.read_token()
        }
    }

    fn scan_identifier_parts(&mut self) -> String {
        let mut v = String::with_capacity(16);
        while let Some(ch) = self.input().cur() {
            // For ASCII, check if it's an identifier part quickly
            if ch <= 0x7f {
                if ch.is_ident_part() {
                    v.push(ch as char);
                    unsafe {
                        self.input_mut().bump_bytes(1);
                    }
                } else if ch == b'\\' {
                    self.bump(1); // bump '\'
                    if !self.is(b'u') {
                        self.emit_error(self.cur_pos(), SyntaxError::InvalidUnicodeEscape);
                        continue;
                    }
                    self.bump(1); // bump 'u'
                    let Ok(value) = self.read_unicode_escape() else {
                        self.emit_error(self.cur_pos(), SyntaxError::InvalidUnicodeEscape);
                        break;
                    };
                    if let Some(c) = CodePoint::from(value).to_char() {
                        v.push(c);
                    } else {
                        self.emit_error(self.cur_pos(), SyntaxError::InvalidUnicodeEscape);
                    }
                    self.token_flags |= TokenFlags::UNICODE;
                } else {
                    break;
                }
            } else {
                // Non-ASCII byte - need to get full UTF-8 character
                if let Some(c) = self.input().cur_as_char() {
                    if c.is_ident_part() {
                        v.push(c);
                        self.bump(c.len_utf8());
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        v
    }
}

/// Impl Iterator for Lexer for compatibility
impl Iterator for Lexer<'_> {
    type Item = TokenAndSpan;

    fn next(&mut self) -> Option<Self::Item> {
        let next_token = self.next_token();
        if next_token.token == Token::Eof {
            None
        } else {
            Some(next_token)
        }
    }
}

impl State {
    pub fn new(start_pos: BytePos) -> Self {
        State {
            had_line_break: false,
            next_regexp: None,
            prev_hi: start_pos,
            token_value: None,
            token_type: None,
        }
    }

    pub(crate) fn set_token_value(&mut self, token_value: TokenValue) {
        self.token_value = Some(token_value);
    }
}

impl State {
    #[inline(always)]
    pub fn set_token_type(&mut self, token_type: Token) {
        self.token_type = Some(token_type);
    }

    #[inline(always)]
    pub fn token_type(&self) -> Option<Token> {
        self.token_type
    }

    pub fn can_have_trailing_line_comment(&self) -> bool {
        let Some(t) = self.token_type() else {
            return true;
        };
        !t.is_bin_op()
    }

    pub fn can_have_trailing_comment(&self) -> bool {
        self.token_type().is_some_and(|t| {
            !t.is_keyword()
                && (t == Token::Semi
                    || t == Token::LBrace
                    || t.is_other_and_can_have_trailing_comment())
        })
    }
}
