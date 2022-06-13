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
    throw new Error(
      `TAPSyntaxError: ${message}, got "${token.value}" at line ${this.line}, column ${this.column} (position ${this.pos})`
    );
  }
}

class TapLexer {
  static Tokens = {
    EOF: 'EOF',
    NUMERIC: 'Numeric',
    LITERAL: 'Literal',
    KEYWORD: 'Keyword',
    WHITESPACE: 'Whitespace',
    COMMENT: 'Comment',
    NEWLINE: 'Newline',

    // Special tokens
    TAP_VERSION: 'VersionLiteral',
    TAP_PLAN: 'PlanLiteral',
    TAP_TEST_OK: 'TestOkLiteral',
    TAP_TEST_NOTOK: 'TestNotOkLiteral',
    TAP_TEST_SKIP: 'TestSkipLiteral',
    TAP_TEST_TODO: 'TestTodoLiteral',
    TAP_TEST_BAILOUT: 'TestBailoutLiteral',
  };

  static Keywords = new Set([
    'ok',
    'not ok',
    '#',
    'TAP version',
    '...',
    '---',
    '..',
    'skip',
    'todo',
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

      if (!token || token.type === TapLexer.Tokens.EOF) {
        break;
      }

    }
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

  error(message) {
    this.source.error(message, this.currentToken);
  }

  scanToken() {
    const char = this.next();
    if (char === undefined) {
      return new Token({
        type: TapLexer.Tokens.EOF,
        stream: this.source,
      });
    }

    if (this.isNumericSymbol(char)) {
      return this.scanNumeric(char);
    } else if (this.isLiteralSymbol(char)) {
      return this.scanLiteral(char);
    } else if (this.isCommentSymbol(char)) {
      return this.scanComment(char);
    }

    // TODO(@manekinekko): current the description symbol is required, even if the specs say it's optional
    else if (this.isDescriptionSymbol(char)) {
      return this.scanDescription(char);
    }

    else if (this.isWhitespaceSymbol(char)) {
      return this.scanToken(char);
    } else if (this.isNewlineSymbol(char)) {
      return this.scanToken(char);
    }

    throw new Error(`Unexpected character: ${char} at line ${this.line}, column ${this.column}`);
  }

  scanNewline(char) {
    return new Token({
      type: TapLexer.Tokens.NEWLINE,
      value: char,
      stream: this.source,
    });
  }

  scanWhitespace(char) {
    return new Token({
      type: TapLexer.Tokens.WHITESPACE,
      value: char,
      stream: this.source,
    });
  }

  scanComment(char) {
    let comment = char;
    while (!this.source.eof()) {
      let char = this.next();
      if (this.isNewlineSymbol(char)) {
        break;
      }
      comment += char;
    }

    comment = comment.replace(/^# /, '');

    return new Token({
      type: TapLexer.Tokens.COMMENT,
      value: comment,
      stream: this.source,
    });
  }

  scanDescription(char) {
    let description = char;
    while (!this.source.eof()) {
      let char = this.source.peek();
      if (this.isNewlineSymbol(char) || this.isCommentSymbol(char)) {
        break;
      }
      char = this.next();
      description += char;
    }

    description = description.replace(/^- /, '');

    return new Token({
      type: TapLexer.Tokens.COMMENT,
      value: description.trim(),
      stream: this.source,
    });
  }

  scanLiteral(char) {
    let word = char;
    while (!this.source.eof()) {
      let char = this.source.peek();

      // TODO(@manekinekko): if the current token comes after a test number,
      // it should be tokenized as a test description!
      //
      // This is due to the fact that a test description may not be prefixed with a " - ".
      //
      // For example: ok 1 - description
      //                   ^ this is a description because of the "-"
      //
      // For example: ok 1 description
      //                   ^ this is a description even though the "-" is missing
      if (this.isLiteralSymbol(char) || this.isWhitespaceSymbol(char)) {
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
      type: TapLexer.Tokens.LITERAL,
      value: word,
      stream: this.source,
    });
  }

  scanTAPkeyword(word) {
    if (word === 'TAP version') {
      return new Token({
        type: TapLexer.Tokens.TAP_VERSION,
        value: word,
        stream: this.source,
      });
    }
    if (word === '..') {
      return new Token({
        type: TapLexer.Tokens.TAP_PLAN,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'ok') {
      return new Token({
        type: TapLexer.Tokens.TAP_TEST_OK,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'not ok') {
      return new Token({
        type: TapLexer.Tokens.TAP_TEST_NOTOK,
        value: word,
        stream: this.source,
      });
    }

    if (word === 'Bail out!') {
      return new Token({
        type: TapLexer.Tokens.TAP_TEST_BAILOUT,
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
      type: TapLexer.Tokens.NUMERIC,
      value: number,
      stream: this.source,
    });
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

  isWhitespaceSymbol(char) {
    return char === ' ' || char === '\t';
  }

  isNewlineSymbol(char) {
    return char === '\n' || char === '\r';
  }

  isCommentSymbol(char) {
    return char === '#';
  }

  isDescriptionSymbol(char) {
    return char === '-';
  }
}

module.exports = { TapLexer };
