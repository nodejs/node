'use strict';

class TokenKind {
  static EOF = 'EOF';
  static EOL = 'EOL';
  static NUMERIC = 'Numeric';
  static LITERAL = 'Literal';
  static KEYWORD = 'Keyword';
  static WHITESPACE = 'Whitespace';
  static COMMENT = 'Comment';
  static DASH = 'Dash';
  static HASH = 'Hash';
  static ESCAPE = 'Escape';

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

  error(message, token, expected) {
    const COLOR_UNDERLINE = '\u001b[4m';
    const COLOR_WHITE = '\u001b[30;1m';
    const COLOR_RED = '\u001b[31m';
    const COLOR_RESET = '\u001b[0m';
    const COLOR_YELLOW_BG = '\u001b[43m';

    const { column, line } = token.location;
    const indent = column + ('' + line).length + 1;

    const sourceLines = this.input.split('\n');
    const sourceLine = sourceLines[line - 1];

    // highlight in red the token
    const sourceLineWithToken =
      sourceLine.slice(0, column - 1) +
      COLOR_UNDERLINE +
      COLOR_RED +
      sourceLine.slice(column - 1, column + (token.value.length - 1)) +
      COLOR_RESET +
      sourceLine.slice(column + (token.value.length - 1));

    console.error(
      `${COLOR_YELLOW_BG + COLOR_RED}TAP Syntax Error:${COLOR_RESET}`
    );

    sourceLines.forEach((sourceLine, index) => {
      const leadingZero = index < 9 ? '0' : '';
      if (index === line - 1) {
        console.error(
          `${COLOR_RED}${leadingZero}${index + 1}: ${sourceLine + COLOR_RESET}`
        );
      } else {
        console.error(
          `${COLOR_WHITE}${leadingZero}${
            index + 1
          }:${COLOR_RESET} ${sourceLine}`
        );
      }
    });

    console.error(`\n${COLOR_YELLOW_BG + COLOR_RED}Diagnostic:${COLOR_RESET}`);

    console.error(`${COLOR_WHITE}${line}:${COLOR_RESET} ${sourceLineWithToken}
${COLOR_RED}${' '.repeat(indent)}│${COLOR_RESET}
${COLOR_RED}${' '.repeat(indent)}└┤${message}, got "${
      token.value
    }" at line ${line}, column ${column} (start ${token.location.start}, end ${
      token.location.end + 1
    })${COLOR_RESET}
      `);
    process.exit(1);
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

  isComment = false;

  constructor(source) {
    this.source = new InputStream(source);
    this.index = 0;
    this.line = 1;
    this.column = 0;

    this.escapeStack = [];

    this.lastScannedToken = new Token({
      type: TokenKind.EOL,
      value: TokenKind.EOL,
      stream: this.source,
    });
  }

  *scan() {
    while (!this.source.eof()) {
      let token = this.scanToken();

      // remember the last scanned token (except for whitespace)
      if (token.type !== TokenKind.WHITESPACE) {
        this.lastScannedToken = token;
      }
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

  error(message, token, expected = '') {
    this.source.error(message, token, expected);
  }

  scanToken() {
    const char = this.next();

    if (this.isEOFSymbol(char)) {
      this.isComment = false;
      return this.scanEOF();
    } else if (this.isEOLSymbol(char)) {
      this.isComment = false;
      return this.scanEOL(char);
    } else if (this.isNumericSymbol(char)) {
      return this.scanNumeric(char);
    } else if (this.isDashSymbol(char)) {
      return this.scanDash(char);
    } else if (this.isHashSymbol(char)) {
      return this.scanHash(char);
    } else if (this.isEscapeSymbol(char)) {
      return this.scanEscapeSymbol(char);
    } else if (this.isWhitespaceSymbol(char)) {
      return this.scanWhitespace(char);
    } else if (this.isLiteralSymbol(char)) {
      return this.scanLiteral(char);
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

  scanEscapeSymbol(char) {
    // if the escape symbol has been escaped, then it is not an escape symbol
    // consume it as a literal.
    if (this.hasTheCurrentCharacterBeenEscaped(char)) {
      this.escapeStack.pop();
      return new Token({
        type: TokenKind.LITERAL,
        value: char,
        stream: this.source,
      });
    }

    // otherwise, consume the escape symbol as an escape symbol that should be ignored by the parser
    // we also need to push the escape symbol to the escape stack
    // and consume the next character as a literal (done in the next turn)
    this.escapeStack.push(char);
    return new Token({
      type: TokenKind.ESCAPE,
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

    // peek next 3 characters and check if it's a YAML start marker
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
    // if last token is whitespace or EOL, we consume it as a comment
    if (
      this.lastScannedToken.type === TokenKind.WHITESPACE ||
      this.lastScannedToken.type === TokenKind.EOL
    ) {
      this.isComment = true;
      return new Token({
        type: TokenKind.COMMENT,
        value: char,
        stream: this.source,
      });
    }

    const charHasBeenEscaped = this.hasTheCurrentCharacterBeenEscaped(char);
    if (this.isComment || charHasBeenEscaped) {

      if (charHasBeenEscaped) {
        this.escapeStack.pop();
      }

      return new Token({
        type: TokenKind.LITERAL,
        value: char,
        stream: this.source,
      });
    }

    // when a hash is found, we assume the rest of the line is a comment
    this.isComment = true;
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
      const token = this.scanTAPkeyword(word);
      if (token) {
        return token;
      }
    }

    return new Token({
      type: TokenKind.LITERAL,
      value: word,
      stream: this.source,
    });
  }

  scanTAPkeyword(word) {
    if (word === 'TAP' && this.lastScannedToken.type === TokenKind.EOL) {
      return new Token({
        type: TokenKind.TAP,
        value: word,
        stream: this.source,
      });
    }
    if (word === 'version' && this.lastScannedToken.type === TokenKind.TAP) {
      return new Token({
        type: TokenKind.TAP_VERSION,
        value: word,
        stream: this.source,
      });
    }
    if (word === '..' && this.lastScannedToken.type === TokenKind.NUMERIC) {
      return new Token({
        type: TokenKind.TAP_PLAN,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'not' && this.lastScannedToken.type === TokenKind.EOL) {
      return new Token({
        type: TokenKind.TAP_TEST_NOTOK,
        value: word,
        stream: this.source,
      });
    }

    if (
      word === 'ok' &&
      (this.lastScannedToken.type === TokenKind.TAP_TEST_NOTOK ||
        this.lastScannedToken.type === TokenKind.EOL)
    ) {
      return new Token({
        type: TokenKind.TAP_TEST_OK,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'Bail out!' && this.lastScannedToken.type === TokenKind.EOL) {
      return new Token({
        type: TokenKind.TAP_TEST_BAILOUT,
        value: word,
        stream: this.source,
      });
    }

    return null;
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
    // use the escapeStack to keep track of the escape characters
    return this.escapeStack.length > 0;
  }

  isNumericSymbol(char) {
    return char >= '0' && char <= '9';
  }

  isLiteralSymbol(char) {
    return (
      (char >= 'a' && char <= 'z') ||
      (char >= 'A' && char <= 'Z') ||
      this.isSpecialCharacterSymbol(char)
    );
  }

  isSpecialCharacterSymbol(char) {
    // we deliberately do not include # and \ in this list
    // these are used for comments/reasons exaplanations and escape characters
    // whitespace is not included because it is handled separately
    return '!"$%&\'()*+,-./:;<=>?@[]^_`{|}~'.indexOf(char) > -1;
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
