'use strict';

const console = require('console');

class TokenKind {
  static EOF = 'EOF';
  static EOL = 'EOL';
  static NUMERIC = 'Numeric';
  static LITERAL = 'Literal';
  static KEYWORD = 'Keyword';
  static WHITESPACE = 'Whitespace';
  static COMMENT = 'Comment';
  static NEWLINE = 'Newline';
  static DASH = 'Dash';
  static HASH = 'Hash';
  static BACK_SLASH = 'BackSlash';

  // TAP tokens
  static TAP = 'TAPLiteral';
  static TAP_VERSION = 'VersionLiteral';
  static TAP_PLAN = 'PlanLiteral';
  static TAP_TEST_OK = 'TestOkLiteral';
  static TAP_TEST_NOTOK = 'TestNotOkLiteral';
  static TAP_TEST_BAILOUT = 'TestBailoutLiteral';
  static TAP_YAML = 'YamlLiteral';
}

class Token {
  constructor({ type, value, stream }) {
    this.type = type;
    this.value = value;
    this.location = {
      line: stream.line,
      column: Math.max(stream.column - ('' + value).length + 1, 0),
      start: stream.pos - ('' + value).length - 1, // zero based
      end: stream.pos - 2, // zero based
    };
  }
}

class InputStream {
  constructor(input) {
    this.input = input;
    this.pos = 0;
    this.column = 0;
    this.line = 1;
  }

  eof() {
    return this.peek() === undefined;
  }

  peek(offset = 0) {
    return this.input[this.pos + offset];
  }

  next() {
    const char = this.peek();
    if (char === undefined) {
      return undefined;
    }

    this.pos++;
    this.column++;
    if (char === '\n') {
      this.line++;
      this.column = 0;
    }

    return char;
  }

  error(message, token) {
    const RED_COLOR = '\x1b[31m';
    const RESET_COLOR = '\x1b[0m';
    const sourceLine = this.input.split('\n')[token.location.line - 1];

    // highlight in red the token
    const sourceLineWithToken =
      sourceLine.slice(0, token.location.column - 1) +
      RED_COLOR +
      sourceLine.slice(
        token.location.column - 1,
        token.location.column + (token.value.length - 1)
      ) +
      RESET_COLOR +
      sourceLine.slice(token.location.column + (token.value.length - 1));

    throw new Error(
      `TAP: ${message}, got "${token.value}" at line ${
        token.location.line
      }, column ${token.location.column} (start ${token.location.start}, end ${
        token.location.end + 1
      })

      ${sourceLineWithToken}
      ${RED_COLOR}${'─'.repeat(token.location.column - 1)}^${RESET_COLOR}

      `
    );
  }
}

class TapLexer {
  static Keywords = new Set([
    'TAP',
    'version',
    'ok',
    'not',
    '#',
    '...',
    '---',
    '..',
    'Bail out!',
  ]);

  constructor(source) {
    this.source = new InputStream(source);
    this.index = 0;
    this.line = 1;
    this.column = 0;
  }

  *scan() {
    while (!this.source.eof()) {
      let token = this.scanToken();
      this.currentToken = token;
      yield token;
    }

    yield this.scanEOF();
  }

  *scanAll() {
    for (let token of this.scan()) {
      yield token;
    }
  }

  next() {
    return this.source.next();
  }

  eof() {
    return this.source.eof();
  }

  error(message, token) {
    this.source.error(message, token);
  }

  scanToken() {
    const char = this.next();

    if (this.isEOFSymbol(char)) {
      return this.scanEOF();
    } else if (this.isEOLSymbol(char)) {
      return this.scanEOL(char);
    } else if (this.isNumericSymbol(char)) {
      return this.scanNumeric(char);
    } else if (this.isLiteralSymbol(char)) {
      return this.scanLiteral(char);
    } else if (this.isDashSymbol(char)) {
      return this.scanDash(char);
    } else if (this.isHashSymbol(char)) {
      return this.scanHash(char);
    } else if (this.isWhitespaceSymbol(char)) {
      return this.scanToken(char);
    }

    // scan special characters at the end of the line
    else if (this.isSpecialCharacterSymbol(char)) {
      return this.scanSpecialCharacter(char);
    }

    throw new Error(
      `Unexpected character: ${char} at line ${this.line}, column ${this.column}`
    );
  }

  scanEOL(char) {
    return new Token({
      type: TokenKind.EOL,
      value: char,
      stream: this.source,
    });
  }

  scanEOF() {
    return new Token({
      type: TokenKind.EOF,
      value: TokenKind.EOF,
      stream: this.source,
    });
  }

  scanSpecialCharacter(char) {
    // handle special case of double backslash
    // we just consume it as a literal
    if (char === '\\') {
      return new Token({
        type: TokenKind.LITERAL,
        value: char,
        stream: this.source,
      });
    }

    // if we have a special character
    // we consume only and only if it's been escaped
    // for example:
    //    \# is a literal, but # is a special token
    if (this.hasTheCurrentCharacterBeenEscaped(char)) {
      return this.scanLiteral(char);
    }

    // consume all other special characters as a literals
    return new Token({
      type: TokenKind.LITERAL,
      value: char,
      stream: this.source,
    });
  }

  scanWhitespace(char) {
    return new Token({
      type: TokenKind.WHITESPACE,
      value: char,
      stream: this.source,
    });
  }

  scanDash(char) {
    if (this.hasTheCurrentCharacterBeenEscaped(char)) {
      return new Token({
        type: TokenKind.LITERAL,
        value: char,
        stream: this.source,
      });
    }

    // peek next 3 caracters and check if it's a YAML start marker
    const marker = char + this.source.peek(2) + this.source.peek(3);
    if (this.isYamlStartSymbol(marker)) {
      return this.scanYaml(char);
    }

    return new Token({
      type: TokenKind.DASH,
      value: char,
      stream: this.source,
    });
  }

  scanHash(char) {
    if (this.hasTheCurrentCharacterBeenEscaped(char)) {
      return new Token({
        type: TokenKind.LITERAL,
        value: char,
        stream: this.source,
      });
    }

    return new Token({
      type: TokenKind.HASH,
      value: char,
      stream: this.source,
    });
  }

  scanYaml(char) {
    // consume the YAML content until the end of the YAML end marker
    let yaml = '';
    while (1) {
      const marker = char + this.source.peek(2) + this.source.peek(3);
      if (this.isYamlEndSymbol(marker)) {
        break;
      }

      yaml += this.source.next();

      if (this.source.eof()) {
        this.error(`Missing YAML end marker`);
      }
    }

    return new Token({
      type: TokenKind.TAP_YAML,
      value: yaml,
      stream: this.source,
    });
  }

  scanComment(char) {
    let comment = char;
    while (!this.source.eof()) {
      let char = this.next();
      if (this.isEOLSymbol(char)) {
        break;
      }
      comment += char;
    }

    comment = comment.replace(/^# /, '');

    return new Token({
      type: TokenKind.COMMENT,
      value: comment,
      stream: this.source,
    });
  }

  scanDescription(char) {
    let description = char;
    while (!this.source.eof()) {
      let char = this.source.peek();
      if (this.isEOLSymbol(char) || this.isCommentSymbol(char)) {
        break;
      }
      char = this.next();
      description += char;
    }

    description = description.replace(/^- /, '');

    return new Token({
      type: TokenKind.COMMENT,
      value: description.trim(),
      stream: this.source,
    });
  }

  scanLiteral(char) {
    let word = char;
    while (!this.source.eof()) {
      let char = this.source.peek();
      if (this.isLiteralSymbol(char)) {
        word += this.source.next();
      } else {
        break;
      }
    }

    word = word.trim();

    if (TapLexer.Keywords.has(word)) {
      return this.scanTAPkeyword(word);
    }

    return new Token({
      type: TokenKind.LITERAL,
      value: word,
      stream: this.source,
    });
  }

  scanTAPkeyword(word) {
    if (word === 'TAP') {
      return new Token({
        type: TokenKind.TAP,
        value: word,
        stream: this.source,
      });
    }
    if (word === 'version') {
      return new Token({
        type: TokenKind.TAP_VERSION,
        value: word,
        stream: this.source,
      });
    }
    if (word === '..') {
      return new Token({
        type: TokenKind.TAP_PLAN,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'not') {
      return new Token({
        type: TokenKind.TAP_TEST_NOTOK,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'ok') {
      return new Token({
        type: TokenKind.TAP_TEST_OK,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'Bail out!') {
      return new Token({
        type: TokenKind.TAP_TEST_BAILOUT,
        value: word,
        stream: this.source,
      });
    }
  }

  scanNumeric(char) {
    let number = char;
    while (!this.source.eof()) {
      let char = this.source.peek();
      if (this.isNumericSymbol(char)) {
        number += char;
        this.source.next();
      } else {
        break;
      }
    }
    return new Token({
      type: TokenKind.NUMERIC,
      value: number,
      stream: this.source,
    });
  }

  hasTheCurrentCharacterBeenEscaped(char) {
    // check if previous character is an escape character
    // token at -1 is the current character that we just consumed
    // token at -2 is the previous character, the one we want to check
    return this.isEscapeSymbol(this.source.peek(-2));
  }

  isNumericSymbol(char) {
    return char >= '0' && char <= '9';
  }

  isLiteralSymbol(char) {
    return (
      (char >= 'a' && char <= 'z') ||
      (char >= 'A' && char <= 'Z') ||
      char === '.'
    );
  }

  isSpecialCharacterSymbol(char) {
    return ' !"#$%&\'()*+,-./:;<=>?@[]^_`{|}~\\'.indexOf(char) > -1;
  }

  isWhitespaceSymbol(char) {
    return char === ' ' || char === '\t';
  }

  isEOFSymbol(char) {
    return char === undefined;
  }

  isEOLSymbol(char) {
    return char === '\n' || char === '\r';
  }

  isHashSymbol(char) {
    return char === '#';
  }

  isDashSymbol(char) {
    return char === '-';
  }

  isEscapeSymbol(char) {
    return char === '\\';
  }

  isYamlStartSymbol(char) {
    return char === '---';
  }

  isYamlEndSymbol(char) {
    return char === '...';
  }
}

module.exports = { TapLexer, TokenKind };
