// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Features shared by parsing and pre-parsing scanners.

#include "src/parsing/scanner.h"

#include <stdint.h>

#include <cmath>

#include "src/ast/ast-value-factory.h"
#include "src/char-predicates-inl.h"
#include "src/conversions-inl.h"
#include "src/objects/bigint.h"
#include "src/parsing/duplicate-finder.h"  // For Scanner::FindSymbol
#include "src/unicode-cache-inl.h"

namespace v8 {
namespace internal {

class Scanner::ErrorState {
 public:
  ErrorState(MessageTemplate::Template* message_stack,
             Scanner::Location* location_stack)
      : message_stack_(message_stack),
        old_message_(*message_stack),
        location_stack_(location_stack),
        old_location_(*location_stack) {
    *message_stack_ = MessageTemplate::kNone;
    *location_stack_ = Location::invalid();
  }

  ~ErrorState() {
    *message_stack_ = old_message_;
    *location_stack_ = old_location_;
  }

  void MoveErrorTo(TokenDesc* dest) {
    if (*message_stack_ == MessageTemplate::kNone) {
      return;
    }
    if (dest->invalid_template_escape_message == MessageTemplate::kNone) {
      dest->invalid_template_escape_message = *message_stack_;
      dest->invalid_template_escape_location = *location_stack_;
    }
    *message_stack_ = MessageTemplate::kNone;
    *location_stack_ = Location::invalid();
  }

 private:
  MessageTemplate::Template* const message_stack_;
  MessageTemplate::Template const old_message_;
  Scanner::Location* const location_stack_;
  Scanner::Location const old_location_;
};

// ----------------------------------------------------------------------------
// Scanner::LiteralBuffer

Handle<String> Scanner::LiteralBuffer::Internalize(Isolate* isolate) const {
  if (is_one_byte()) {
    return isolate->factory()->InternalizeOneByteString(one_byte_literal());
  }
  return isolate->factory()->InternalizeTwoByteString(two_byte_literal());
}

int Scanner::LiteralBuffer::NewCapacity(int min_capacity) {
  int capacity = Max(min_capacity, backing_store_.length());
  int new_capacity = Min(capacity * kGrowthFactory, capacity + kMaxGrowth);
  return new_capacity;
}

void Scanner::LiteralBuffer::ExpandBuffer() {
  Vector<byte> new_store = Vector<byte>::New(NewCapacity(kInitialCapacity));
  MemCopy(new_store.start(), backing_store_.start(), position_);
  backing_store_.Dispose();
  backing_store_ = new_store;
}

void Scanner::LiteralBuffer::ConvertToTwoByte() {
  DCHECK(is_one_byte_);
  Vector<byte> new_store;
  int new_content_size = position_ * kUC16Size;
  if (new_content_size >= backing_store_.length()) {
    // Ensure room for all currently read code units as UC16 as well
    // as the code unit about to be stored.
    new_store = Vector<byte>::New(NewCapacity(new_content_size));
  } else {
    new_store = backing_store_;
  }
  uint8_t* src = backing_store_.start();
  uint16_t* dst = reinterpret_cast<uint16_t*>(new_store.start());
  for (int i = position_ - 1; i >= 0; i--) {
    dst[i] = src[i];
  }
  if (new_store.start() != backing_store_.start()) {
    backing_store_.Dispose();
    backing_store_ = new_store;
  }
  position_ = new_content_size;
  is_one_byte_ = false;
}

void Scanner::LiteralBuffer::AddCharSlow(uc32 code_unit) {
  if (position_ >= backing_store_.length()) ExpandBuffer();
  if (is_one_byte_) {
    if (code_unit <= static_cast<uc32>(unibrow::Latin1::kMaxChar)) {
      backing_store_[position_] = static_cast<byte>(code_unit);
      position_ += kOneByteSize;
      return;
    }
    ConvertToTwoByte();
  }
  if (code_unit <=
      static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) = code_unit;
    position_ += kUC16Size;
  } else {
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) =
        unibrow::Utf16::LeadSurrogate(code_unit);
    position_ += kUC16Size;
    if (position_ >= backing_store_.length()) ExpandBuffer();
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) =
        unibrow::Utf16::TrailSurrogate(code_unit);
    position_ += kUC16Size;
  }
}

// ----------------------------------------------------------------------------
// Scanner::BookmarkScope

const size_t Scanner::BookmarkScope::kBookmarkAtFirstPos =
    std::numeric_limits<size_t>::max() - 2;
const size_t Scanner::BookmarkScope::kNoBookmark =
    std::numeric_limits<size_t>::max() - 1;
const size_t Scanner::BookmarkScope::kBookmarkWasApplied =
    std::numeric_limits<size_t>::max();

void Scanner::BookmarkScope::Set() {
  DCHECK_EQ(bookmark_, kNoBookmark);
  DCHECK_EQ(scanner_->next_next_.token, Token::UNINITIALIZED);

  // The first token is a bit special, since current_ will still be
  // uninitialized. In this case, store kBookmarkAtFirstPos and special-case it
  // when
  // applying the bookmark.
  DCHECK_IMPLIES(
      scanner_->current_.token == Token::UNINITIALIZED,
      scanner_->current_.location.beg_pos == scanner_->next_.location.beg_pos);
  bookmark_ = (scanner_->current_.token == Token::UNINITIALIZED)
                  ? kBookmarkAtFirstPos
                  : scanner_->location().beg_pos;
}

void Scanner::BookmarkScope::Apply() {
  DCHECK(HasBeenSet());  // Caller hasn't called SetBookmark.
  if (bookmark_ == kBookmarkAtFirstPos) {
    scanner_->SeekNext(0);
  } else {
    scanner_->SeekNext(bookmark_);
    scanner_->Next();
    DCHECK_EQ(scanner_->location().beg_pos, static_cast<int>(bookmark_));
  }
  bookmark_ = kBookmarkWasApplied;
}

bool Scanner::BookmarkScope::HasBeenSet() {
  return bookmark_ != kNoBookmark && bookmark_ != kBookmarkWasApplied;
}

bool Scanner::BookmarkScope::HasBeenApplied() {
  return bookmark_ == kBookmarkWasApplied;
}

// ----------------------------------------------------------------------------
// Scanner

Scanner::Scanner(UnicodeCache* unicode_cache)
    : unicode_cache_(unicode_cache),
      octal_pos_(Location::invalid()),
      octal_message_(MessageTemplate::kNone),
      found_html_comment_(false),
      allow_harmony_bigint_(false) {}

void Scanner::Initialize(Utf16CharacterStream* source, bool is_module) {
  DCHECK_NOT_NULL(source);
  source_ = source;
  is_module_ = is_module;
  // Need to capture identifiers in order to recognize "get" and "set"
  // in object literals.
  Init();
  has_line_terminator_before_next_ = true;
  Scan();
}

template <bool capture_raw, bool unicode>
uc32 Scanner::ScanHexNumber(int expected_length) {
  DCHECK_LE(expected_length, 4);  // prevent overflow

  int begin = source_pos() - 2;
  uc32 x = 0;
  for (int i = 0; i < expected_length; i++) {
    int d = HexValue(c0_);
    if (d < 0) {
      ReportScannerError(Location(begin, begin + expected_length + 2),
                         unicode
                             ? MessageTemplate::kInvalidUnicodeEscapeSequence
                             : MessageTemplate::kInvalidHexEscapeSequence);
      return -1;
    }
    x = x * 16 + d;
    Advance<capture_raw>();
  }

  return x;
}

template <bool capture_raw>
uc32 Scanner::ScanUnlimitedLengthHexNumber(int max_value, int beg_pos) {
  uc32 x = 0;
  int d = HexValue(c0_);
  if (d < 0) return -1;

  while (d >= 0) {
    x = x * 16 + d;
    if (x > max_value) {
      ReportScannerError(Location(beg_pos, source_pos() + 1),
                         MessageTemplate::kUndefinedUnicodeCodePoint);
      return -1;
    }
    Advance<capture_raw>();
    d = HexValue(c0_);
  }

  return x;
}


// Ensure that tokens can be stored in a byte.
STATIC_ASSERT(Token::NUM_TOKENS <= 0x100);

// Table of one-character tokens, by character (0x00..0x7F only).
// clang-format off
static const byte one_char_tokens[] = {
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::LPAREN,       // 0x28
  Token::RPAREN,       // 0x29
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::COMMA,        // 0x2C
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::COLON,        // 0x3A
  Token::SEMICOLON,    // 0x3B
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::CONDITIONAL,  // 0x3F
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::LBRACK,     // 0x5B
  Token::ILLEGAL,
  Token::RBRACK,     // 0x5D
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::ILLEGAL,
  Token::LBRACE,       // 0x7B
  Token::ILLEGAL,
  Token::RBRACE,       // 0x7D
  Token::BIT_NOT,      // 0x7E
  Token::ILLEGAL
};
// clang-format on

Token::Value Scanner::Next() {
  if (next_.token == Token::EOS) {
    next_.location.beg_pos = current_.location.beg_pos;
    next_.location.end_pos = current_.location.end_pos;
  }
  current_ = next_;
  if (V8_UNLIKELY(next_next_.token != Token::UNINITIALIZED)) {
    next_ = next_next_;
    next_next_.token = Token::UNINITIALIZED;
    next_next_.contextual_token = Token::UNINITIALIZED;
    has_line_terminator_before_next_ = has_line_terminator_after_next_;
    return current_.token;
  }
  has_line_terminator_before_next_ = false;
  has_multiline_comment_before_next_ = false;
  if (static_cast<unsigned>(c0_) <= 0x7F) {
    Token::Value token = static_cast<Token::Value>(one_char_tokens[c0_]);
    if (token != Token::ILLEGAL) {
      int pos = source_pos();
      next_.token = token;
      next_.contextual_token = Token::UNINITIALIZED;
      next_.location.beg_pos = pos;
      next_.location.end_pos = pos + 1;
      next_.literal_chars = nullptr;
      next_.raw_literal_chars = nullptr;
      next_.invalid_template_escape_message = MessageTemplate::kNone;
      Advance();
      return current_.token;
    }
  }
  Scan();
  return current_.token;
}


Token::Value Scanner::PeekAhead() {
  DCHECK(next_.token != Token::DIV);
  DCHECK(next_.token != Token::ASSIGN_DIV);

  if (next_next_.token != Token::UNINITIALIZED) {
    return next_next_.token;
  }
  TokenDesc prev = current_;
  bool has_line_terminator_before_next =
      has_line_terminator_before_next_ || has_multiline_comment_before_next_;
  Next();
  has_line_terminator_after_next_ =
      has_line_terminator_before_next_ || has_multiline_comment_before_next_;
  has_line_terminator_before_next_ = has_line_terminator_before_next;
  Token::Value ret = next_.token;
  next_next_ = next_;
  next_ = current_;
  current_ = prev;
  return ret;
}


Token::Value Scanner::SkipWhiteSpace() {
  int start_position = source_pos();

  while (true) {
    while (true) {
      // Don't skip behind the end of input.
      if (c0_ == kEndOfInput) break;

      // Advance as long as character is a WhiteSpace or LineTerminator.
      // Remember if the latter is the case.
      if (unibrow::IsLineTerminator(c0_)) {
        has_line_terminator_before_next_ = true;
      } else if (!unicode_cache_->IsWhiteSpace(c0_)) {
        break;
      }
      Advance();
    }

    // If there is an HTML comment end '-->' at the beginning of a
    // line (with only whitespace in front of it), we treat the rest
    // of the line as a comment. This is in line with the way
    // SpiderMonkey handles it.
    if (c0_ != '-' || !has_line_terminator_before_next_) break;

    Advance();
    if (c0_ != '-') {
      PushBack('-');  // undo Advance()
      break;
    }

    Advance();
    if (c0_ != '>') {
      PushBack2('-', '-');  // undo 2x Advance();
      break;
    }

    // Treat the rest of the line as a comment.
    Token::Value token = SkipSingleHTMLComment();
    if (token == Token::ILLEGAL) {
      return token;
    }
  }

  // Return whether or not we skipped any characters.
  if (source_pos() == start_position) {
    return Token::ILLEGAL;
  }

  return Token::WHITESPACE;
}

Token::Value Scanner::SkipSingleHTMLComment() {
  if (is_module_) {
    ReportScannerError(source_pos(), MessageTemplate::kHtmlCommentInModule);
    return Token::ILLEGAL;
  }
  return SkipSingleLineComment();
}

Token::Value Scanner::SkipSingleLineComment() {
  Advance();

  // The line terminator at the end of the line is not considered
  // to be part of the single-line comment; it is recognized
  // separately by the lexical grammar and becomes part of the
  // stream of input elements for the syntactic grammar (see
  // ECMA-262, section 7.4).
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    Advance();
  }

  return Token::WHITESPACE;
}


Token::Value Scanner::SkipSourceURLComment() {
  TryToParseSourceURLComment();
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    Advance();
  }

  return Token::WHITESPACE;
}


void Scanner::TryToParseSourceURLComment() {
  // Magic comments are of the form: //[#@]\s<name>=\s*<value>\s*.* and this
  // function will just return if it cannot parse a magic comment.
  if (c0_ == kEndOfInput || !unicode_cache_->IsWhiteSpace(c0_)) return;
  Advance();
  LiteralBuffer name;
  while (c0_ != kEndOfInput &&
         !unicode_cache_->IsWhiteSpaceOrLineTerminator(c0_) && c0_ != '=') {
    name.AddChar(c0_);
    Advance();
  }
  if (!name.is_one_byte()) return;
  Vector<const uint8_t> name_literal = name.one_byte_literal();
  LiteralBuffer* value;
  if (name_literal == STATIC_CHAR_VECTOR("sourceURL")) {
    value = &source_url_;
  } else if (name_literal == STATIC_CHAR_VECTOR("sourceMappingURL")) {
    value = &source_mapping_url_;
  } else {
    return;
  }
  if (c0_ != '=')
    return;
  Advance();
  value->Reset();
  while (c0_ != kEndOfInput && unicode_cache_->IsWhiteSpace(c0_)) {
    Advance();
  }
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    // Disallowed characters.
    if (c0_ == '"' || c0_ == '\'') {
      value->Reset();
      return;
    }
    if (unicode_cache_->IsWhiteSpace(c0_)) {
      break;
    }
    value->AddChar(c0_);
    Advance();
  }
  // Allow whitespace at the end.
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    if (!unicode_cache_->IsWhiteSpace(c0_)) {
      value->Reset();
      break;
    }
    Advance();
  }
}


Token::Value Scanner::SkipMultiLineComment() {
  DCHECK_EQ(c0_, '*');
  Advance();

  while (c0_ != kEndOfInput) {
    uc32 ch = c0_;
    Advance();
    if (c0_ != kEndOfInput && unibrow::IsLineTerminator(ch)) {
      // Following ECMA-262, section 7.4, a comment containing
      // a newline will make the comment count as a line-terminator.
      has_multiline_comment_before_next_ = true;
    }
    // If we have reached the end of the multi-line comment, we
    // consume the '/' and insert a whitespace. This way all
    // multi-line comments are treated as whitespace.
    if (ch == '*' && c0_ == '/') {
      c0_ = ' ';
      return Token::WHITESPACE;
    }
  }

  // Unterminated multi-line comment.
  return Token::ILLEGAL;
}

Token::Value Scanner::ScanHtmlComment() {
  // Check for <!-- comments.
  DCHECK_EQ(c0_, '!');
  Advance();
  if (c0_ != '-') {
    PushBack('!');  // undo Advance()
    return Token::LT;
  }

  Advance();
  if (c0_ != '-') {
    PushBack2('-', '!');  // undo 2x Advance()
    return Token::LT;
  }

  found_html_comment_ = true;
  return SkipSingleHTMLComment();
}

void Scanner::Scan() {
  next_.literal_chars = nullptr;
  next_.raw_literal_chars = nullptr;
  next_.invalid_template_escape_message = MessageTemplate::kNone;
  Token::Value token;
  do {
    // Remember the position of the next token
    next_.location.beg_pos = source_pos();

    switch (c0_) {
      case ' ':
      case '\t':
        Advance();
        token = Token::WHITESPACE;
        break;

      case '\n':
        Advance();
        has_line_terminator_before_next_ = true;
        token = Token::WHITESPACE;
        break;

      case '"':
      case '\'':
        token = ScanString();
        break;

      case '<':
        // < <= << <<= <!--
        Advance();
        if (c0_ == '=') {
          token = Select(Token::LTE);
        } else if (c0_ == '<') {
          token = Select('=', Token::ASSIGN_SHL, Token::SHL);
        } else if (c0_ == '!') {
          token = ScanHtmlComment();
        } else {
          token = Token::LT;
        }
        break;

      case '>':
        // > >= >> >>= >>> >>>=
        Advance();
        if (c0_ == '=') {
          token = Select(Token::GTE);
        } else if (c0_ == '>') {
          // >> >>= >>> >>>=
          Advance();
          if (c0_ == '=') {
            token = Select(Token::ASSIGN_SAR);
          } else if (c0_ == '>') {
            token = Select('=', Token::ASSIGN_SHR, Token::SHR);
          } else {
            token = Token::SAR;
          }
        } else {
          token = Token::GT;
        }
        break;

      case '=':
        // = == === =>
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::EQ_STRICT, Token::EQ);
        } else if (c0_ == '>') {
          token = Select(Token::ARROW);
        } else {
          token = Token::ASSIGN;
        }
        break;

      case '!':
        // ! != !==
        Advance();
        if (c0_ == '=') {
          token = Select('=', Token::NE_STRICT, Token::NE);
        } else {
          token = Token::NOT;
        }
        break;

      case '+':
        // + ++ +=
        Advance();
        if (c0_ == '+') {
          token = Select(Token::INC);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_ADD);
        } else {
          token = Token::ADD;
        }
        break;

      case '-':
        // - -- --> -=
        Advance();
        if (c0_ == '-') {
          Advance();
          if (c0_ == '>' && HasAnyLineTerminatorBeforeNext()) {
            // For compatibility with SpiderMonkey, we skip lines that
            // start with an HTML comment end '-->'.
            token = SkipSingleHTMLComment();
          } else {
            token = Token::DEC;
          }
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_SUB);
        } else {
          token = Token::SUB;
        }
        break;

      case '*':
        // * *=
        Advance();
        if (c0_ == '*') {
          token = Select('=', Token::ASSIGN_EXP, Token::EXP);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_MUL);
        } else {
          token = Token::MUL;
        }
        break;

      case '%':
        // % %=
        token = Select('=', Token::ASSIGN_MOD, Token::MOD);
        break;

      case '/':
        // /  // /* /=
        Advance();
        if (c0_ == '/') {
          Advance();
          if (c0_ == '#' || c0_ == '@') {
            Advance();
            token = SkipSourceURLComment();
          } else {
            PushBack(c0_);
            token = SkipSingleLineComment();
          }
        } else if (c0_ == '*') {
          token = SkipMultiLineComment();
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_DIV);
        } else {
          token = Token::DIV;
        }
        break;

      case '&':
        // & && &=
        Advance();
        if (c0_ == '&') {
          token = Select(Token::AND);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_BIT_AND);
        } else {
          token = Token::BIT_AND;
        }
        break;

      case '|':
        // | || |=
        Advance();
        if (c0_ == '|') {
          token = Select(Token::OR);
        } else if (c0_ == '=') {
          token = Select(Token::ASSIGN_BIT_OR);
        } else {
          token = Token::BIT_OR;
        }
        break;

      case '^':
        // ^ ^=
        token = Select('=', Token::ASSIGN_BIT_XOR, Token::BIT_XOR);
        break;

      case '.':
        // . Number
        Advance();
        if (IsDecimalDigit(c0_)) {
          token = ScanNumber(true);
        } else {
          token = Token::PERIOD;
          if (c0_ == '.') {
            Advance();
            if (c0_ == '.') {
              Advance();
              token = Token::ELLIPSIS;
            } else {
              PushBack('.');
            }
          }
        }
        break;

      case ':':
        token = Select(Token::COLON);
        break;

      case ';':
        token = Select(Token::SEMICOLON);
        break;

      case ',':
        token = Select(Token::COMMA);
        break;

      case '(':
        token = Select(Token::LPAREN);
        break;

      case ')':
        token = Select(Token::RPAREN);
        break;

      case '[':
        token = Select(Token::LBRACK);
        break;

      case ']':
        token = Select(Token::RBRACK);
        break;

      case '{':
        token = Select(Token::LBRACE);
        break;

      case '}':
        token = Select(Token::RBRACE);
        break;

      case '?':
        token = Select(Token::CONDITIONAL);
        break;

      case '~':
        token = Select(Token::BIT_NOT);
        break;

      case '`':
        token = ScanTemplateStart();
        break;

      case '#':
        token = ScanPrivateName();
        break;

      default:
        if (c0_ == kEndOfInput) {
          token = Token::EOS;
        } else if (unicode_cache_->IsIdentifierStart(c0_)) {
          token = ScanIdentifierOrKeyword();
        } else if (IsDecimalDigit(c0_)) {
          token = ScanNumber(false);
        } else {
          token = SkipWhiteSpace();
          if (token == Token::ILLEGAL) {
            Advance();
          }
        }
        break;
    }

    // Continue scanning for tokens as long as we're just skipping
    // whitespace.
  } while (token == Token::WHITESPACE);

  next_.location.end_pos = source_pos();
  if (Token::IsContextualKeyword(token)) {
    next_.token = Token::IDENTIFIER;
    next_.contextual_token = token;
  } else {
    next_.token = token;
    next_.contextual_token = Token::UNINITIALIZED;
  }

#ifdef DEBUG
  SanityCheckTokenDesc(current_);
  SanityCheckTokenDesc(next_);
  SanityCheckTokenDesc(next_next_);
#endif
}

#ifdef DEBUG
void Scanner::SanityCheckTokenDesc(const TokenDesc& token) const {
  // Most tokens should not have literal_chars or even raw_literal chars.
  // The rules are:
  // - UNINITIALIZED: we don't care.
  // - TEMPLATE_*: need both literal + raw literal chars.
  // - IDENTIFIERS, STRINGS, etc.: need a literal, but no raw literal.
  // - all others: should have neither.
  // Furthermore, only TEMPLATE_* tokens can have a
  // invalid_template_escape_message.

  switch (token.token) {
    case Token::UNINITIALIZED:
      // token.literal_chars & other members might be garbage. That's ok.
      break;
    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      DCHECK_NOT_NULL(token.raw_literal_chars);
      DCHECK_NOT_NULL(token.literal_chars);
      break;
    case Token::ESCAPED_KEYWORD:
    case Token::ESCAPED_STRICT_RESERVED_WORD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
    case Token::IDENTIFIER:
    case Token::NUMBER:
    case Token::BIGINT:
    case Token::REGEXP_LITERAL:
    case Token::SMI:
    case Token::STRING:
    case Token::PRIVATE_NAME:
      DCHECK_NOT_NULL(token.literal_chars);
      DCHECK_NULL(token.raw_literal_chars);
      DCHECK_EQ(token.invalid_template_escape_message, MessageTemplate::kNone);
      break;
    default:
      DCHECK_NULL(token.literal_chars);
      DCHECK_NULL(token.raw_literal_chars);
      DCHECK_EQ(token.invalid_template_escape_message, MessageTemplate::kNone);
      break;
  }

  DCHECK_IMPLIES(token.token != Token::IDENTIFIER,
                 token.contextual_token == Token::UNINITIALIZED);
  DCHECK_IMPLIES(token.contextual_token != Token::UNINITIALIZED,
                 token.token == Token::IDENTIFIER &&
                     Token::IsContextualKeyword(token.contextual_token));
  DCHECK(!Token::IsContextualKeyword(token.token));
}
#endif  // DEBUG

void Scanner::SeekForward(int pos) {
  // After this call, we will have the token at the given position as
  // the "next" token. The "current" token will be invalid.
  if (pos == next_.location.beg_pos) return;
  int current_pos = source_pos();
  DCHECK_EQ(next_.location.end_pos, current_pos);
  // Positions inside the lookahead token aren't supported.
  DCHECK(pos >= current_pos);
  if (pos != current_pos) {
    source_->Seek(pos);
    Advance();
    // This function is only called to seek to the location
    // of the end of a function (at the "}" token). It doesn't matter
    // whether there was a line terminator in the part we skip.
    has_line_terminator_before_next_ = false;
    has_multiline_comment_before_next_ = false;
  }
  Scan();
}


template <bool capture_raw, bool in_template_literal>
bool Scanner::ScanEscape() {
  uc32 c = c0_;
  Advance<capture_raw>();

  // Skip escaped newlines.
  if (!in_template_literal && c0_ != kEndOfInput &&
      unibrow::IsLineTerminator(c)) {
    // Allow escaped CR+LF newlines in multiline string literals.
    if (IsCarriageReturn(c) && IsLineFeed(c0_)) Advance<capture_raw>();
    return true;
  }

  switch (c) {
    case '\'':  // fall through
    case '"' :  // fall through
    case '\\': break;
    case 'b' : c = '\b'; break;
    case 'f' : c = '\f'; break;
    case 'n' : c = '\n'; break;
    case 'r' : c = '\r'; break;
    case 't' : c = '\t'; break;
    case 'u' : {
      c = ScanUnicodeEscape<capture_raw>();
      if (c < 0) return false;
      break;
    }
    case 'v':
      c = '\v';
      break;
    case 'x': {
      c = ScanHexNumber<capture_raw>(2);
      if (c < 0) return false;
      break;
    }
    case '0':  // Fall through.
    case '1':  // fall through
    case '2':  // fall through
    case '3':  // fall through
    case '4':  // fall through
    case '5':  // fall through
    case '6':  // fall through
    case '7':
      c = ScanOctalEscape<capture_raw>(c, 2);
      break;
  }

  // Other escaped characters are interpreted as their non-escaped version.
  AddLiteralChar(c);
  return true;
}


template <bool capture_raw>
uc32 Scanner::ScanOctalEscape(uc32 c, int length) {
  uc32 x = c - '0';
  int i = 0;
  for (; i < length; i++) {
    int d = c0_ - '0';
    if (d < 0 || d > 7) break;
    int nx = x * 8 + d;
    if (nx >= 256) break;
    x = nx;
    Advance<capture_raw>();
  }
  // Anything except '\0' is an octal escape sequence, illegal in strict mode.
  // Remember the position of octal escape sequences so that an error
  // can be reported later (in strict mode).
  // We don't report the error immediately, because the octal escape can
  // occur before the "use strict" directive.
  if (c != '0' || i > 0 || c0_ == '8' || c0_ == '9') {
    octal_pos_ = Location(source_pos() - i - 1, source_pos() - 1);
    octal_message_ = MessageTemplate::kStrictOctalEscape;
  }
  return x;
}


Token::Value Scanner::ScanString() {
  uc32 quote = c0_;
  Advance<false, false>();  // consume quote

  LiteralScope literal(this);
  while (true) {
    if (c0_ > kMaxAscii) {
      HandleLeadSurrogate();
      break;
    }
    if (c0_ == kEndOfInput || c0_ == '\n' || c0_ == '\r') return Token::ILLEGAL;
    if (c0_ == quote) {
      literal.Complete();
      Advance<false, false>();
      return Token::STRING;
    }
    char c = static_cast<char>(c0_);
    if (c == '\\') break;
    Advance<false, false>();
    AddLiteralChar(c);
  }

  while (c0_ != quote && c0_ != kEndOfInput &&
         !unibrow::IsLineTerminator(c0_)) {
    uc32 c = c0_;
    Advance();
    if (c == '\\') {
      if (c0_ == kEndOfInput || !ScanEscape<false, false>()) {
        return Token::ILLEGAL;
      }
    } else {
      AddLiteralChar(c);
    }
  }
  if (c0_ != quote) return Token::ILLEGAL;
  literal.Complete();

  Advance();  // consume quote
  return Token::STRING;
}

Token::Value Scanner::ScanPrivateName() {
  if (!allow_harmony_private_fields()) {
    ReportScannerError(source_pos(),
                       MessageTemplate::kInvalidOrUnexpectedToken);
    return Token::ILLEGAL;
  }

  LiteralScope literal(this);
  DCHECK_EQ(c0_, '#');
  AddLiteralCharAdvance();
  if (c0_ == kEndOfInput || !unicode_cache_->IsIdentifierStart(c0_)) {
    PushBack(c0_);
    ReportScannerError(source_pos(),
                       MessageTemplate::kInvalidOrUnexpectedToken);
    return Token::ILLEGAL;
  }

  Token::Value token = ScanIdentifierOrKeywordInner(&literal);
  return token == Token::ILLEGAL ? Token::ILLEGAL : Token::PRIVATE_NAME;
}

Token::Value Scanner::ScanTemplateSpan() {
  // When scanning a TemplateSpan, we are looking for the following construct:
  // TEMPLATE_SPAN ::
  //     ` LiteralChars* ${
  //   | } LiteralChars* ${
  //
  // TEMPLATE_TAIL ::
  //     ` LiteralChars* `
  //   | } LiteralChar* `
  //
  // A TEMPLATE_SPAN should always be followed by an Expression, while a
  // TEMPLATE_TAIL terminates a TemplateLiteral and does not need to be
  // followed by an Expression.

  // These scoped helpers save and restore the original error state, so that we
  // can specially treat invalid escape sequences in templates (which are
  // handled by the parser).
  ErrorState scanner_error_state(&scanner_error_, &scanner_error_location_);
  ErrorState octal_error_state(&octal_message_, &octal_pos_);

  Token::Value result = Token::TEMPLATE_SPAN;
  LiteralScope literal(this);
  StartRawLiteral();
  const bool capture_raw = true;
  const bool in_template_literal = true;
  while (true) {
    uc32 c = c0_;
    Advance<capture_raw>();
    if (c == '`') {
      result = Token::TEMPLATE_TAIL;
      ReduceRawLiteralLength(1);
      break;
    } else if (c == '$' && c0_ == '{') {
      Advance<capture_raw>();  // Consume '{'
      ReduceRawLiteralLength(2);
      break;
    } else if (c == '\\') {
      if (c0_ != kEndOfInput && unibrow::IsLineTerminator(c0_)) {
        // The TV of LineContinuation :: \ LineTerminatorSequence is the empty
        // code unit sequence.
        uc32 lastChar = c0_;
        Advance<capture_raw>();
        if (lastChar == '\r') {
          ReduceRawLiteralLength(1);  // Remove \r
          if (c0_ == '\n') {
            Advance<capture_raw>();  // Adds \n
          } else {
            AddRawLiteralChar('\n');
          }
        }
      } else {
        bool success = ScanEscape<capture_raw, in_template_literal>();
        USE(success);
        DCHECK_EQ(!success, has_error());
        // For templates, invalid escape sequence checking is handled in the
        // parser.
        scanner_error_state.MoveErrorTo(&next_);
        octal_error_state.MoveErrorTo(&next_);
      }
    } else if (c < 0) {
      // Unterminated template literal
      PushBack(c);
      break;
    } else {
      // The TRV of LineTerminatorSequence :: <CR> is the CV 0x000A.
      // The TRV of LineTerminatorSequence :: <CR><LF> is the sequence
      // consisting of the CV 0x000A.
      if (c == '\r') {
        ReduceRawLiteralLength(1);  // Remove \r
        if (c0_ == '\n') {
          Advance<capture_raw>();  // Adds \n
        } else {
          AddRawLiteralChar('\n');
        }
        c = '\n';
      }
      AddLiteralChar(c);
    }
  }
  literal.Complete();
  next_.location.end_pos = source_pos();
  next_.token = result;
  next_.contextual_token = Token::UNINITIALIZED;

  return result;
}


Token::Value Scanner::ScanTemplateStart() {
  DCHECK_EQ(next_next_.token, Token::UNINITIALIZED);
  DCHECK_EQ(c0_, '`');
  next_.location.beg_pos = source_pos();
  Advance();  // Consume `
  return ScanTemplateSpan();
}

Handle<String> Scanner::SourceUrl(Isolate* isolate) const {
  Handle<String> tmp;
  if (source_url_.length() > 0) tmp = source_url_.Internalize(isolate);
  return tmp;
}

Handle<String> Scanner::SourceMappingUrl(Isolate* isolate) const {
  Handle<String> tmp;
  if (source_mapping_url_.length() > 0)
    tmp = source_mapping_url_.Internalize(isolate);
  return tmp;
}

void Scanner::ScanDecimalDigits() {
  while (IsDecimalDigit(c0_))
    AddLiteralCharAdvance();
}


Token::Value Scanner::ScanNumber(bool seen_period) {
  DCHECK(IsDecimalDigit(c0_));  // the first digit of the number or the fraction

  enum {
    DECIMAL,
    DECIMAL_WITH_LEADING_ZERO,
    HEX,
    OCTAL,
    IMPLICIT_OCTAL,
    BINARY
  } kind = DECIMAL;

  LiteralScope literal(this);
  bool at_start = !seen_period;
  int start_pos = source_pos();  // For reporting octal positions.
  if (seen_period) {
    // we have already seen a decimal point of the float
    AddLiteralChar('.');
    ScanDecimalDigits();  // we know we have at least one digit

  } else {
    // if the first character is '0' we must check for octals and hex
    if (c0_ == '0') {
      AddLiteralCharAdvance();

      // either 0, 0exxx, 0Exxx, 0.xxx, a hex number, a binary number or
      // an octal number.
      if (c0_ == 'x' || c0_ == 'X') {
        // hex number
        kind = HEX;
        AddLiteralCharAdvance();
        if (!IsHexDigit(c0_)) {
          // we must have at least one hex digit after 'x'/'X'
          return Token::ILLEGAL;
        }
        while (IsHexDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if (c0_ == 'o' || c0_ == 'O') {
        kind = OCTAL;
        AddLiteralCharAdvance();
        if (!IsOctalDigit(c0_)) {
          // we must have at least one octal digit after 'o'/'O'
          return Token::ILLEGAL;
        }
        while (IsOctalDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if (c0_ == 'b' || c0_ == 'B') {
        kind = BINARY;
        AddLiteralCharAdvance();
        if (!IsBinaryDigit(c0_)) {
          // we must have at least one binary digit after 'b'/'B'
          return Token::ILLEGAL;
        }
        while (IsBinaryDigit(c0_)) {
          AddLiteralCharAdvance();
        }
      } else if ('0' <= c0_ && c0_ <= '7') {
        // (possible) octal number
        kind = IMPLICIT_OCTAL;
        while (true) {
          if (c0_ == '8' || c0_ == '9') {
            at_start = false;
            kind = DECIMAL_WITH_LEADING_ZERO;
            break;
          }
          if (c0_  < '0' || '7'  < c0_) {
            // Octal literal finished.
            octal_pos_ = Location(start_pos, source_pos());
            octal_message_ = MessageTemplate::kStrictOctalLiteral;
            break;
          }
          AddLiteralCharAdvance();
        }
      } else if (c0_ == '8' || c0_ == '9') {
        kind = DECIMAL_WITH_LEADING_ZERO;
      }
    }

    // Parse decimal digits and allow trailing fractional part.
    if (kind == DECIMAL || kind == DECIMAL_WITH_LEADING_ZERO) {
      if (at_start) {
        uint64_t value = 0;
        while (IsDecimalDigit(c0_)) {
          value = 10 * value + (c0_ - '0');

          uc32 first_char = c0_;
          Advance<false, false>();
          AddLiteralChar(first_char);
        }

        if (next_.literal_chars->one_byte_literal().length() <= 10 &&
            value <= Smi::kMaxValue && c0_ != '.' &&
            (c0_ == kEndOfInput || !unicode_cache_->IsIdentifierStart(c0_))) {
          next_.smi_value_ = static_cast<uint32_t>(value);
          literal.Complete();
          HandleLeadSurrogate();

          if (kind == DECIMAL_WITH_LEADING_ZERO) {
            octal_pos_ = Location(start_pos, source_pos());
            octal_message_ = MessageTemplate::kStrictDecimalWithLeadingZero;
          }
          return Token::SMI;
        }
        HandleLeadSurrogate();
      }

      ScanDecimalDigits();  // optional
      if (c0_ == '.') {
        seen_period = true;
        AddLiteralCharAdvance();
        ScanDecimalDigits();  // optional
      }
    }
  }

  bool is_bigint = false;
  if (allow_harmony_bigint() && c0_ == 'n' && !seen_period &&
      (kind == DECIMAL || kind == HEX || kind == OCTAL || kind == BINARY)) {
    // Check that the literal is within our limits for BigInt length.
    // For simplicity, use 4 bits per character to calculate the maximum
    // allowed literal length.
    static const int kMaxBigIntCharacters = BigInt::kMaxLengthBits / 4;
    int length = source_pos() - start_pos - (kind != DECIMAL ? 2 : 0);
    if (length > kMaxBigIntCharacters) {
      ReportScannerError(Location(start_pos, source_pos()),
                         MessageTemplate::kBigIntTooBig);
      return Token::ILLEGAL;
    }

    is_bigint = true;
    Advance();
  } else if (c0_ == 'e' || c0_ == 'E') {
    // scan exponent, if any
    DCHECK(kind != HEX);  // 'e'/'E' must be scanned as part of the hex number
    if (!(kind == DECIMAL || kind == DECIMAL_WITH_LEADING_ZERO))
      return Token::ILLEGAL;
    // scan exponent
    AddLiteralCharAdvance();
    if (c0_ == '+' || c0_ == '-')
      AddLiteralCharAdvance();
    if (!IsDecimalDigit(c0_)) {
      // we must have at least one decimal digit after 'e'/'E'
      return Token::ILLEGAL;
    }
    ScanDecimalDigits();
  }

  // The source character immediately following a numeric literal must
  // not be an identifier start or a decimal digit; see ECMA-262
  // section 7.8.3, page 17 (note that we read only one decimal digit
  // if the value is 0).
  if (IsDecimalDigit(c0_) ||
      (c0_ != kEndOfInput && unicode_cache_->IsIdentifierStart(c0_)))
    return Token::ILLEGAL;

  literal.Complete();

  if (kind == DECIMAL_WITH_LEADING_ZERO) {
    octal_pos_ = Location(start_pos, source_pos());
    octal_message_ = MessageTemplate::kStrictDecimalWithLeadingZero;
  }

  return is_bigint ? Token::BIGINT : Token::NUMBER;
}


uc32 Scanner::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return -1;
  Advance();
  return ScanUnicodeEscape<false>();
}


template <bool capture_raw>
uc32 Scanner::ScanUnicodeEscape() {
  // Accept both \uxxxx and \u{xxxxxx}. In the latter case, the number of
  // hex digits between { } is arbitrary. \ and u have already been read.
  if (c0_ == '{') {
    int begin = source_pos() - 2;
    Advance<capture_raw>();
    uc32 cp = ScanUnlimitedLengthHexNumber<capture_raw>(0x10FFFF, begin);
    if (cp < 0 || c0_ != '}') {
      ReportScannerError(source_pos(),
                         MessageTemplate::kInvalidUnicodeEscapeSequence);
      return -1;
    }
    Advance<capture_raw>();
    return cp;
  }
  const bool unicode = true;
  return ScanHexNumber<capture_raw, unicode>(4);
}


// ----------------------------------------------------------------------------
// Keyword Matcher

#define KEYWORDS(KEYWORD_GROUP, KEYWORD)                    \
  KEYWORD_GROUP('a')                                        \
  KEYWORD("arguments", Token::ARGUMENTS)                    \
  KEYWORD("as", Token::AS)                                  \
  KEYWORD("async", Token::ASYNC)                            \
  KEYWORD("await", Token::AWAIT)                            \
  KEYWORD("anonymous", Token::ANONYMOUS)                    \
  KEYWORD_GROUP('b')                                        \
  KEYWORD("break", Token::BREAK)                            \
  KEYWORD_GROUP('c')                                        \
  KEYWORD("case", Token::CASE)                              \
  KEYWORD("catch", Token::CATCH)                            \
  KEYWORD("class", Token::CLASS)                            \
  KEYWORD("const", Token::CONST)                            \
  KEYWORD("constructor", Token::CONSTRUCTOR)                \
  KEYWORD("continue", Token::CONTINUE)                      \
  KEYWORD_GROUP('d')                                        \
  KEYWORD("debugger", Token::DEBUGGER)                      \
  KEYWORD("default", Token::DEFAULT)                        \
  KEYWORD("delete", Token::DELETE)                          \
  KEYWORD("do", Token::DO)                                  \
  KEYWORD_GROUP('e')                                        \
  KEYWORD("else", Token::ELSE)                              \
  KEYWORD("enum", Token::ENUM)                              \
  KEYWORD("eval", Token::EVAL)                              \
  KEYWORD("export", Token::EXPORT)                          \
  KEYWORD("extends", Token::EXTENDS)                        \
  KEYWORD_GROUP('f')                                        \
  KEYWORD("false", Token::FALSE_LITERAL)                    \
  KEYWORD("finally", Token::FINALLY)                        \
  KEYWORD("for", Token::FOR)                                \
  KEYWORD("from", Token::FROM)                              \
  KEYWORD("function", Token::FUNCTION)                      \
  KEYWORD_GROUP('g')                                        \
  KEYWORD("get", Token::GET)                                \
  KEYWORD_GROUP('i')                                        \
  KEYWORD("if", Token::IF)                                  \
  KEYWORD("implements", Token::FUTURE_STRICT_RESERVED_WORD) \
  KEYWORD("import", Token::IMPORT)                          \
  KEYWORD("in", Token::IN)                                  \
  KEYWORD("instanceof", Token::INSTANCEOF)                  \
  KEYWORD("interface", Token::FUTURE_STRICT_RESERVED_WORD)  \
  KEYWORD_GROUP('l')                                        \
  KEYWORD("let", Token::LET)                                \
  KEYWORD_GROUP('m')                                        \
  KEYWORD("meta", Token::META)                              \
  KEYWORD_GROUP('n')                                        \
  KEYWORD("name", Token::NAME)                              \
  KEYWORD("new", Token::NEW)                                \
  KEYWORD("null", Token::NULL_LITERAL)                      \
  KEYWORD_GROUP('o')                                        \
  KEYWORD("of", Token::OF)                                  \
  KEYWORD_GROUP('p')                                        \
  KEYWORD("package", Token::FUTURE_STRICT_RESERVED_WORD)    \
  KEYWORD("private", Token::FUTURE_STRICT_RESERVED_WORD)    \
  KEYWORD("protected", Token::FUTURE_STRICT_RESERVED_WORD)  \
  KEYWORD("prototype", Token::PROTOTYPE)                    \
  KEYWORD("public", Token::FUTURE_STRICT_RESERVED_WORD)     \
  KEYWORD_GROUP('r')                                        \
  KEYWORD("return", Token::RETURN)                          \
  KEYWORD_GROUP('s')                                        \
  KEYWORD("sent", Token::SENT)                              \
  KEYWORD("set", Token::SET)                                \
  KEYWORD("static", Token::STATIC)                          \
  KEYWORD("super", Token::SUPER)                            \
  KEYWORD("switch", Token::SWITCH)                          \
  KEYWORD_GROUP('t')                                        \
  KEYWORD("target", Token::TARGET)                          \
  KEYWORD("this", Token::THIS)                              \
  KEYWORD("throw", Token::THROW)                            \
  KEYWORD("true", Token::TRUE_LITERAL)                      \
  KEYWORD("try", Token::TRY)                                \
  KEYWORD("typeof", Token::TYPEOF)                          \
  KEYWORD_GROUP('u')                                        \
  KEYWORD("undefined", Token::UNDEFINED)                    \
  KEYWORD_GROUP('v')                                        \
  KEYWORD("var", Token::VAR)                                \
  KEYWORD("void", Token::VOID)                              \
  KEYWORD_GROUP('w')                                        \
  KEYWORD("while", Token::WHILE)                            \
  KEYWORD("with", Token::WITH)                              \
  KEYWORD_GROUP('y')                                        \
  KEYWORD("yield", Token::YIELD)                            \
  KEYWORD_GROUP('_')                                        \
  KEYWORD("__proto__", Token::PROTO_UNDERSCORED)

static Token::Value KeywordOrIdentifierToken(const uint8_t* input,
                                             int input_length) {
  DCHECK_GE(input_length, 1);
  const int kMinLength = 2;
  const int kMaxLength = 11;
  if (input_length < kMinLength || input_length > kMaxLength) {
    return Token::IDENTIFIER;
  }
  switch (input[0]) {
    default:
#define KEYWORD_GROUP_CASE(ch)                                \
      break;                                                  \
    case ch:
#define KEYWORD(keyword, token)                                           \
  {                                                                       \
    /* 'keyword' is a char array, so sizeof(keyword) is */                \
    /* strlen(keyword) plus 1 for the NUL char. */                        \
    const int keyword_length = sizeof(keyword) - 1;                       \
    STATIC_ASSERT(keyword_length >= kMinLength);                          \
    STATIC_ASSERT(keyword_length <= kMaxLength);                          \
    DCHECK_EQ(input[0], keyword[0]);                                      \
    DCHECK(token == Token::FUTURE_STRICT_RESERVED_WORD ||                 \
           0 == strncmp(keyword, Token::String(token), sizeof(keyword))); \
    if (input_length == keyword_length && input[1] == keyword[1] &&       \
        (keyword_length <= 2 || input[2] == keyword[2]) &&                \
        (keyword_length <= 3 || input[3] == keyword[3]) &&                \
        (keyword_length <= 4 || input[4] == keyword[4]) &&                \
        (keyword_length <= 5 || input[5] == keyword[5]) &&                \
        (keyword_length <= 6 || input[6] == keyword[6]) &&                \
        (keyword_length <= 7 || input[7] == keyword[7]) &&                \
        (keyword_length <= 8 || input[8] == keyword[8]) &&                \
        (keyword_length <= 9 || input[9] == keyword[9]) &&                \
        (keyword_length <= 10 || input[10] == keyword[10])) {             \
      return token;                                                       \
    }                                                                     \
  }
      KEYWORDS(KEYWORD_GROUP_CASE, KEYWORD)
  }
  return Token::IDENTIFIER;
}

Token::Value Scanner::ScanIdentifierOrKeyword() {
  LiteralScope literal(this);
  return ScanIdentifierOrKeywordInner(&literal);
}

Token::Value Scanner::ScanIdentifierOrKeywordInner(LiteralScope* literal) {
  DCHECK(unicode_cache_->IsIdentifierStart(c0_));
  if (IsInRange(c0_, 'a', 'z') || c0_ == '_') {
    do {
      char first_char = static_cast<char>(c0_);
      Advance<false, false>();
      AddLiteralChar(first_char);
    } while (IsInRange(c0_, 'a', 'z') || c0_ == '_');

    if (IsDecimalDigit(c0_) || IsInRange(c0_, 'A', 'Z') || c0_ == '_' ||
        c0_ == '$') {
      // Identifier starting with lowercase.
      char first_char = static_cast<char>(c0_);
      Advance<false, false>();
      AddLiteralChar(first_char);
      while (IsAsciiIdentifier(c0_)) {
        char first_char = static_cast<char>(c0_);
        Advance<false, false>();
        AddLiteralChar(first_char);
      }
      if (c0_ <= kMaxAscii && c0_ != '\\') {
        literal->Complete();
        return Token::IDENTIFIER;
      }
    } else if (c0_ <= kMaxAscii && c0_ != '\\') {
      // Only a-z+ or _: could be a keyword or identifier.
      Vector<const uint8_t> chars = next_.literal_chars->one_byte_literal();
      Token::Value token =
          KeywordOrIdentifierToken(chars.start(), chars.length());
      if (token == Token::IDENTIFIER ||
          token == Token::FUTURE_STRICT_RESERVED_WORD ||
          Token::IsContextualKeyword(token))
        literal->Complete();
      return token;
    }

    HandleLeadSurrogate();
  } else if (IsInRange(c0_, 'A', 'Z') || c0_ == '_' || c0_ == '$') {
    do {
      char first_char = static_cast<char>(c0_);
      Advance<false, false>();
      AddLiteralChar(first_char);
    } while (IsAsciiIdentifier(c0_));

    if (c0_ <= kMaxAscii && c0_ != '\\') {
      literal->Complete();
      return Token::IDENTIFIER;
    }

    HandleLeadSurrogate();
  } else if (c0_ == '\\') {
    // Scan identifier start character.
    uc32 c = ScanIdentifierUnicodeEscape();
    // Only allow legal identifier start characters.
    if (c < 0 ||
        c == '\\' ||  // No recursive escapes.
        !unicode_cache_->IsIdentifierStart(c)) {
      return Token::ILLEGAL;
    }
    AddLiteralChar(c);
    return ScanIdentifierSuffix(literal, true);
  } else {
    uc32 first_char = c0_;
    Advance();
    AddLiteralChar(first_char);
  }

  // Scan the rest of the identifier characters.
  while (c0_ != kEndOfInput && unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ != '\\') {
      uc32 next_char = c0_;
      Advance();
      AddLiteralChar(next_char);
      continue;
    }
    // Fallthrough if no longer able to complete keyword.
    return ScanIdentifierSuffix(literal, false);
  }

  if (next_.literal_chars->is_one_byte()) {
    Vector<const uint8_t> chars = next_.literal_chars->one_byte_literal();
    Token::Value token =
        KeywordOrIdentifierToken(chars.start(), chars.length());
    if (token == Token::IDENTIFIER ||
        token == Token::FUTURE_STRICT_RESERVED_WORD ||
        Token::IsContextualKeyword(token))
      literal->Complete();
    return token;
  }
  literal->Complete();
  return Token::IDENTIFIER;
}


Token::Value Scanner::ScanIdentifierSuffix(LiteralScope* literal,
                                           bool escaped) {
  // Scan the rest of the identifier characters.
  while (c0_ != kEndOfInput && unicode_cache_->IsIdentifierPart(c0_)) {
    if (c0_ == '\\') {
      uc32 c = ScanIdentifierUnicodeEscape();
      escaped = true;
      // Only allow legal identifier part characters.
      if (c < 0 ||
          c == '\\' ||
          !unicode_cache_->IsIdentifierPart(c)) {
        return Token::ILLEGAL;
      }
      AddLiteralChar(c);
    } else {
      AddLiteralChar(c0_);
      Advance();
    }
  }
  literal->Complete();

  if (escaped && next_.literal_chars->is_one_byte()) {
    Vector<const uint8_t> chars = next_.literal_chars->one_byte_literal();
    Token::Value token =
        KeywordOrIdentifierToken(chars.start(), chars.length());
    /* TODO(adamk): YIELD should be handled specially. */
    if (token == Token::IDENTIFIER || Token::IsContextualKeyword(token)) {
      return token;
    } else if (token == Token::FUTURE_STRICT_RESERVED_WORD ||
               token == Token::LET || token == Token::STATIC) {
      return Token::ESCAPED_STRICT_RESERVED_WORD;
    } else {
      return Token::ESCAPED_KEYWORD;
    }
  }
  return Token::IDENTIFIER;
}

bool Scanner::ScanRegExpPattern() {
  DCHECK(next_next_.token == Token::UNINITIALIZED);
  DCHECK(next_.token == Token::DIV || next_.token == Token::ASSIGN_DIV);

  // Scan: ('/' | '/=') RegularExpressionBody '/' RegularExpressionFlags
  bool in_character_class = false;
  bool seen_equal = (next_.token == Token::ASSIGN_DIV);

  // Previous token is either '/' or '/=', in the second case, the
  // pattern starts at =.
  next_.location.beg_pos = source_pos() - (seen_equal ? 2 : 1);
  next_.location.end_pos = source_pos() - (seen_equal ? 1 : 0);

  // Scan regular expression body: According to ECMA-262, 3rd, 7.8.5,
  // the scanner should pass uninterpreted bodies to the RegExp
  // constructor.
  LiteralScope literal(this);
  if (seen_equal) {
    AddLiteralChar('=');
  }

  while (c0_ != '/' || in_character_class) {
    if (c0_ == kEndOfInput || unibrow::IsLineTerminator(c0_)) {
      return false;
    }
    if (c0_ == '\\') {  // Escape sequence.
      AddLiteralCharAdvance();
      if (c0_ == kEndOfInput || unibrow::IsLineTerminator(c0_)) {
        return false;
      }
      AddLiteralCharAdvance();
      // If the escape allows more characters, i.e., \x??, \u????, or \c?,
      // only "safe" characters are allowed (letters, digits, underscore),
      // otherwise the escape isn't valid and the invalid character has
      // its normal meaning. I.e., we can just continue scanning without
      // worrying whether the following characters are part of the escape
      // or not, since any '/', '\\' or '[' is guaranteed to not be part
      // of the escape sequence.

      // TODO(896): At some point, parse RegExps more thoroughly to capture
      // octal esacpes in strict mode.
    } else {  // Unescaped character.
      if (c0_ == '[') in_character_class = true;
      if (c0_ == ']') in_character_class = false;
      AddLiteralCharAdvance();
    }
  }
  Advance();  // consume '/'

  literal.Complete();
  next_.token = Token::REGEXP_LITERAL;
  next_.contextual_token = Token::UNINITIALIZED;
  return true;
}


Maybe<RegExp::Flags> Scanner::ScanRegExpFlags() {
  DCHECK(next_.token == Token::REGEXP_LITERAL);

  // Scan regular expression flags.
  int flags = 0;
  while (c0_ != kEndOfInput && unicode_cache_->IsIdentifierPart(c0_)) {
    RegExp::Flags flag = RegExp::kNone;
    switch (c0_) {
      case 'g':
        flag = RegExp::kGlobal;
        break;
      case 'i':
        flag = RegExp::kIgnoreCase;
        break;
      case 'm':
        flag = RegExp::kMultiline;
        break;
      case 's':
        flag = RegExp::kDotAll;
        break;
      case 'u':
        flag = RegExp::kUnicode;
        break;
      case 'y':
        flag = RegExp::kSticky;
        break;
      default:
        return Nothing<RegExp::Flags>();
    }
    if (flags & flag) {
      return Nothing<RegExp::Flags>();
    }
    Advance();
    flags |= flag;
  }

  next_.location.end_pos = source_pos();
  return Just(RegExp::Flags(flags));
}

const AstRawString* Scanner::CurrentSymbol(
    AstValueFactory* ast_value_factory) const {
  if (is_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(literal_two_byte_string());
}

const AstRawString* Scanner::NextSymbol(
    AstValueFactory* ast_value_factory) const {
  if (is_next_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(next_literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(next_literal_two_byte_string());
}

const AstRawString* Scanner::CurrentRawSymbol(
    AstValueFactory* ast_value_factory) const {
  if (is_raw_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(raw_literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(raw_literal_two_byte_string());
}


double Scanner::DoubleValue() {
  DCHECK(is_literal_one_byte());
  return StringToDouble(
      unicode_cache_,
      literal_one_byte_string(),
      ALLOW_HEX | ALLOW_OCTAL | ALLOW_IMPLICIT_OCTAL | ALLOW_BINARY);
}

const char* Scanner::CurrentLiteralAsCString(Zone* zone) const {
  DCHECK(is_literal_one_byte());
  Vector<const uint8_t> vector = literal_one_byte_string();
  int length = vector.length();
  char* buffer = zone->NewArray<char>(length + 1);
  memcpy(buffer, vector.start(), length);
  buffer[length] = '\0';
  return buffer;
}

bool Scanner::IsDuplicateSymbol(DuplicateFinder* duplicate_finder,
                                AstValueFactory* ast_value_factory) const {
  DCHECK_NOT_NULL(duplicate_finder);
  DCHECK_NOT_NULL(ast_value_factory);
  const AstRawString* string = CurrentSymbol(ast_value_factory);
  return !duplicate_finder->known_symbols_.insert(string).second;
}

void Scanner::SeekNext(size_t position) {
  // Use with care: This cleanly resets most, but not all scanner state.
  // TODO(vogelheim): Fix this, or at least DCHECK the relevant conditions.

  // To re-scan from a given character position, we need to:
  // 1, Reset the current_, next_ and next_next_ tokens
  //    (next_ + next_next_ will be overwrittem by Next(),
  //     current_ will remain unchanged, so overwrite it fully.)
  current_ = {{0, 0},
              nullptr,
              nullptr,
              0,
              Token::UNINITIALIZED,
              MessageTemplate::kNone,
              {0, 0},
              Token::UNINITIALIZED};
  next_.token = Token::UNINITIALIZED;
  next_.contextual_token = Token::UNINITIALIZED;
  next_next_.token = Token::UNINITIALIZED;
  next_next_.contextual_token = Token::UNINITIALIZED;
  // 2, reset the source to the desired position,
  source_->Seek(position);
  // 3, re-scan, by scanning the look-ahead char + 1 token (next_).
  c0_ = source_->Advance();
  Next();
  DCHECK_EQ(next_.location.beg_pos, static_cast<int>(position));
}

}  // namespace internal
}  // namespace v8
