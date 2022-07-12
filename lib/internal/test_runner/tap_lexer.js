'use strict';

const TokenKind = {
  EOF: 'EOF',
  EOL: 'EOL',
  NUMERIC: 'Numeric',
  LITERAL: 'Literal',
  KEYWORD: 'Keyword',
  WHITESPACE: 'Whitespace',
  COMMENT: 'Comment',
  DASH: 'Dash',
  PLUS: 'Plus',
  HASH: 'Hash',
  ESCAPE: 'Escape',

  // TAP tokens
  TAP: 'TAPKeyword',
  TAP_VERSION: 'VersionKeyword',
  TAP_PLAN: 'PlanKeyword',
  TAP_TEST_OK: 'TestOkKeyword',
  TAP_TEST_NOTOK: 'TestNotOkKeyword',
  TAP_YAML_START: 'YamlStartKeyword',
  TAP_YAML_END: 'YamlEndKeyword',
  TAP_YAML_BLOCK: 'YamlKeyword',
  TAP_PRAGMA: 'PragmaKeyword',
};

class Token {
  constructor({ kind, value, stream }) {
    this.kind = kind;
    this.value = value;
    this.location = {
      line: stream.line,
      column: Math.max(stream.column - ('' + value).length + 1, 1), // 1 based
      start: Math.max(stream.pos - ('' + value).length, 0), // zero based
      end: stream.pos - 1, // zero based
    };

    // EOF is a special case
    if (value === TokenKind.EOF) {
      const eofPosition = stream.input.length + 1; // we consider EOF to be outside the stream
      this.location.start = eofPosition;
      this.location.end = eofPosition;
      this.location.column = stream.column + 1; // 1 based
    }
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

  // this is an example error output:
  //
  // TAP Syntax Error:
  // 01: abc
  //
  // Diagnostic:
  // 01: abc
  //     ┬
  //     └┤Expected a valid token, got "abc" at line 1, column 1 (start 0, end 3)

  error(message, token) {
    const COLOR_UNDERLINE = '\u001b[4m';
    const COLOR_WHITE = '\u001b[30;1m';
    const COLOR_RED = '\u001b[31m';
    const COLOR_RESET = '\u001b[0m';
    const COLOR_YELLOW_BG = '\u001b[43m';

    const { column, line, start, end } = token.location;
    const indent = column + ('' + line).length + 1;

    const sourceLines = this.input.split('\n');
    const sourceLine = sourceLines[line - 1];

    const errorDetails = `, got "${token.value}" (${token.kind}) at line ${line}, column ${column} (start ${start}, end ${end})`;

    // highlight the errored token in red
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

    const indentString = ' '.repeat(indent) + (line < 9 ? ' ' : '');
    console.error(`${COLOR_WHITE}${
      (line < 9 ? '0' : '') + line
    }:${COLOR_RESET} ${sourceLineWithToken}
${COLOR_RED}${indentString}│${COLOR_RESET}
${COLOR_RED}${indentString}└┤${message}${errorDetails}${COLOR_RESET}
      `);

    throw new SyntaxError(message + errorDetails, {
      cause: token,
    });
  }
}

class TapLexer {
  static Keywords = new Set([
    'TAP',
    'version',
    'ok',
    'not',
    '...',
    '---',
    '..',
    'pragma',
    '-',
    '+',

    // NOTE: "Skip", "Todo" and "Bail out!" literals are deferred to the parser
  ]);

  #isComment = false;
  #source = null;
  #index = 0;
  #line = 1;
  #column = 0;
  #escapeStack = [];
  #lastScannedToken = null;

  constructor(source) {
    this.#source = new InputStream(source);
    this.#lastScannedToken = new Token({
      kind: TokenKind.EOL,
      value: TokenKind.EOL,
      stream: this.#source,
    });
  }

  *scan() {
    while (!this.#source.eof()) {
      let token = this.#scanToken();

      // remember the last scanned token (except for whitespace)
      if (token.kind !== TokenKind.WHITESPACE) {
        this.#lastScannedToken = token;
      }
      yield token;
    }

    yield this.#scanEOF();
  }

  next() {
    return this.#source.next();
  }

  eof() {
    return this.#source.eof();
  }

  error(message, token, expected = '') {
    this.#source.error(message, token, expected);
  }

  #scanToken() {
    const char = this.next();

    if (this.#isEOFSymbol(char)) {
      return this.#scanEOF();
    } else if (this.#isEOLSymbol(char)) {
      return this.#scanEOL(char);
    } else if (this.#isNumericSymbol(char)) {
      return this.#scanNumeric(char);
    } else if (this.#isDashSymbol(char)) {
      return this.#scanDash(char);
    } else if (this.#isPlusSymbol(char)) {
      return this.#scanPlus(char);
    } else if (this.#isHashSymbol(char)) {
      return this.#scanHash(char);
    } else if (this.#isEscapeSymbol(char)) {
      return this.#scanEscapeSymbol(char);
    } else if (this.#isWhitespaceSymbol(char)) {
      return this.#scanWhitespace(char);
    } else if (this.#isLiteralSymbol(char)) {
      return this.#scanLiteral(char);
    }

    throw new Error(
      `Unexpected character: ${char} at line ${this.#line}, column ${
        this.#column
      }`
    );
  }

  #scanEOL(char) {
    // in case of odd number of ESCAPE symbols, we need to clear the remaining
    // escape chars from the stack and start fresh for the next line
    this.#escapeStack = [];

    // we also need to reset the comment flag
    this.#isComment = false;

    return new Token({
      kind: TokenKind.EOL,
      value: char,
      stream: this.#source,
    });
  }

  #scanEOF() {
    this.#isComment = false;

    return new Token({
      kind: TokenKind.EOF,
      value: TokenKind.EOF,
      stream: this.#source,
    });
  }

  #scanEscapeSymbol(char) {
    // if the escape symbol has been escaped (by previous symbol),
    // or if the next symbol is a whitespace symbol,
    // then consume it as a literal.
    if (
      this.#hasTheCurrentCharacterBeenEscaped() ||
      this.#source.peek(1) === TokenKind.WHITESPACE
    ) {
      this.#escapeStack.pop();
      return new Token({
        kind: TokenKind.LITERAL,
        value: char,
        stream: this.#source,
      });
    }

    // otherwise, consume the escape symbol as an escape symbol that should be ignored by the parser
    // we also need to push the escape symbol to the escape stack
    // and consume the next character as a literal (done in the next turn)
    this.#escapeStack.push(char);
    return new Token({
      kind: TokenKind.ESCAPE,
      value: char,
      stream: this.#source,
    });
  }

  #scanWhitespace(char) {
    return new Token({
      kind: TokenKind.WHITESPACE,
      value: char,
      stream: this.#source,
    });
  }

  #scanDash(char) {
    // peek next 3 characters and check if it's a YAML start marker
    const marker = char + this.#source.peek() + this.#source.peek(1);

    if (this.#isYamlStartSymbol(marker)) {
      this.next(); // consume second -
      this.next(); // consume third -

      return new Token({
        kind: TokenKind.TAP_YAML_START,
        value: marker,
        stream: this.#source,
      });
    }

    return new Token({
      kind: TokenKind.DASH,
      value: char,
      stream: this.#source,
    });
  }

  #scanPlus(char) {
    return new Token({
      kind: TokenKind.PLUS,
      value: char,
      stream: this.#source,
    });
  }

  #scanHash(char) {
    const lastCharacter = this.#source.peek(-2);
    const nextToken = this.#source.peek();

    // if we encounter a hash symbol at the beginning of a line,
    // we consider it as a comment
    if (!lastCharacter || this.#isEOLSymbol(lastCharacter)) {
      this.#isComment = true;
      return new Token({
        kind: TokenKind.COMMENT,
        value: char,
        stream: this.#source,
      });
    }

    // the only valid case where a hash symbol is considered as a hash token
    // is when it's preceded by a whitespace symbol and followed by a non-hash symbol
    if (
      this.#isWhitespaceSymbol(lastCharacter) &&
      !this.#isHashSymbol(nextToken)
    ) {
      return new Token({
        kind: TokenKind.HASH,
        value: char,
        stream: this.#source,
      });
    }

    const charHasBeenEscaped = this.#hasTheCurrentCharacterBeenEscaped();
    if (this.#isComment || charHasBeenEscaped) {
      if (charHasBeenEscaped) {
        this.#escapeStack.pop();
      }

      return new Token({
        kind: TokenKind.LITERAL,
        value: char,
        stream: this.#source,
      });
    }

    // as a fallback, we consume the hash symbol as a literal
    return new Token({
      kind: TokenKind.LITERAL,
      value: char,
      stream: this.#source,
    });
  }

  #scanLiteral(char) {
    let word = char;
    while (!this.#source.eof()) {
      let char = this.#source.peek();
      if (this.#isLiteralSymbol(char)) {
        word += this.#source.next();
      } else {
        break;
      }
    }

    word = word.trim();

    if (TapLexer.Keywords.has(word)) {
      const token = this.#scanTAPkeyword(word);
      if (token) {
        return token;
      }
    }

    if (this.#isYamlEndSymbol(word)) {
      return new Token({
        kind: TokenKind.TAP_YAML_END,
        value: word,
        stream: this.#source,
      });
    }

    return new Token({
      kind: TokenKind.LITERAL,
      value: word,
      stream: this.#source,
    });
  }

  #scanTAPkeyword(word) {
    if (word === 'TAP' && this.#lastScannedToken.kind === TokenKind.EOL) {
      return new Token({
        kind: TokenKind.TAP,
        value: word,
        stream: this.#source,
      });
    }

    if (word === 'version' && this.#lastScannedToken.kind === TokenKind.TAP) {
      return new Token({
        kind: TokenKind.TAP_VERSION,
        value: word,
        stream: this.#source,
      });
    }

    if (word === '..' && this.#lastScannedToken.kind === TokenKind.NUMERIC) {
      return new Token({
        kind: TokenKind.TAP_PLAN,
        value: word,
        stream: this.#source,
      });
    }

    if (word === 'not' && this.#lastScannedToken.kind === TokenKind.EOL) {
      return new Token({
        kind: TokenKind.TAP_TEST_NOTOK,
        value: word,
        stream: this.#source,
      });
    }

    if (
      word === 'ok' &&
      (this.#lastScannedToken.kind === TokenKind.TAP_TEST_NOTOK ||
        this.#lastScannedToken.kind === TokenKind.EOL)
    ) {
      return new Token({
        kind: TokenKind.TAP_TEST_OK,
        value: word,
        stream: this.#source,
      });
    }

    if (word === 'pragma' && this.#lastScannedToken.kind === TokenKind.EOL) {
      return new Token({
        kind: TokenKind.TAP_PRAGMA,
        value: word,
        stream: this.#source,
      });
    }

    return null;
  }

  #scanNumeric(char) {
    let number = char;
    while (!this.#source.eof()) {
      let char = this.#source.peek();
      if (this.#isNumericSymbol(char)) {
        number += char;
        this.#source.next();
      } else {
        break;
      }
    }
    return new Token({
      kind: TokenKind.NUMERIC,
      value: number,
      stream: this.#source,
    });
  }

  #hasTheCurrentCharacterBeenEscaped() {
    // use the escapeStack to keep track of the escape characters
    return this.#escapeStack.length > 0;
  }

  #isNumericSymbol(char) {
    return char >= '0' && char <= '9';
  }

  #isLiteralSymbol(char) {
    return (
      (char >= 'a' && char <= 'z') ||
      (char >= 'A' && char <= 'Z') ||
      this.#isSpecialCharacterSymbol(char)
    );
  }

  #isSpecialCharacterSymbol(char) {
    // we deliberately do not include "# \ + -"" in this list
    // these are used for comments/reasons explanations, pragma and escape characters
    // whitespace is not included because it is handled separately
    return '!"$%&\'()*,./:;<=>?@[]^_`{|}~'.indexOf(char) > -1;
  }

  #isWhitespaceSymbol(char) {
    return char === ' ' || char === '\t';
  }

  #isEOFSymbol(char) {
    return char === undefined;
  }

  #isEOLSymbol(char) {
    return char === '\n' || char === '\r';
  }

  #isHashSymbol(char) {
    return char === '#';
  }

  #isDashSymbol(char) {
    return char === '-';
  }

  #isPlusSymbol(char) {
    return char === '+';
  }

  #isEscapeSymbol(char) {
    return char === '\\';
  }

  #isYamlStartSymbol(char) {
    return char === '---';
  }

  #isYamlEndSymbol(char) {
    return char === '...';
  }
}

module.exports = { TapLexer, TokenKind };
