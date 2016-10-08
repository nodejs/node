"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Token = undefined;

var _createClass2 = require("babel-runtime/helpers/createClass");

var _createClass3 = _interopRequireDefault(_createClass2);

var _classCallCheck2 = require("babel-runtime/helpers/classCallCheck");

var _classCallCheck3 = _interopRequireDefault(_classCallCheck2);

var _identifier = require("../util/identifier");

var _types = require("./types");

var _context = require("./context");

var _location = require("../util/location");

var _whitespace = require("../util/whitespace");

var _state = require("./state");

var _state2 = _interopRequireDefault(_state);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

// Object type used to represent tokens. Note that normally, tokens
// simply exist as properties on the parser object. This is only
// used for the onToken callback and the external tokenizer.

var Token = exports.Token = function Token(state) {
  (0, _classCallCheck3.default)(this, Token);

  this.type = state.type;
  this.value = state.value;
  this.start = state.start;
  this.end = state.end;
  this.loc = new _location.SourceLocation(state.startLoc, state.endLoc);
};

// ## Tokenizer

/* eslint max-len: 0 */
/* eslint indent: 0 */

function codePointToString(code) {
  // UTF-16 Decoding
  if (code <= 0xFFFF) {
    return String.fromCharCode(code);
  } else {
    return String.fromCharCode((code - 0x10000 >> 10) + 0xD800, (code - 0x10000 & 1023) + 0xDC00);
  }
}

var Tokenizer = function () {
  function Tokenizer(options, input) {
    (0, _classCallCheck3.default)(this, Tokenizer);

    this.state = new _state2.default();
    this.state.init(options, input);
  }

  // Move to the next token

  (0, _createClass3.default)(Tokenizer, [{
    key: "next",
    value: function next() {
      if (!this.isLookahead) {
        this.state.tokens.push(new Token(this.state));
      }

      this.state.lastTokEnd = this.state.end;
      this.state.lastTokStart = this.state.start;
      this.state.lastTokEndLoc = this.state.endLoc;
      this.state.lastTokStartLoc = this.state.startLoc;
      this.nextToken();
    }

    // TODO

  }, {
    key: "eat",
    value: function eat(type) {
      if (this.match(type)) {
        this.next();
        return true;
      } else {
        return false;
      }
    }

    // TODO

  }, {
    key: "match",
    value: function match(type) {
      return this.state.type === type;
    }

    // TODO

  }, {
    key: "isKeyword",
    value: function isKeyword(word) {
      return (0, _identifier.isKeyword)(word);
    }

    // TODO

  }, {
    key: "lookahead",
    value: function lookahead() {
      var old = this.state;
      this.state = old.clone(true);

      this.isLookahead = true;
      this.next();
      this.isLookahead = false;

      var curr = this.state.clone(true);
      this.state = old;
      return curr;
    }

    // Toggle strict mode. Re-reads the next number or string to please
    // pedantic tests (`"use strict"; 010;` should fail).

  }, {
    key: "setStrict",
    value: function setStrict(strict) {
      this.state.strict = strict;
      if (!this.match(_types.types.num) && !this.match(_types.types.string)) return;
      this.state.pos = this.state.start;
      while (this.state.pos < this.state.lineStart) {
        this.state.lineStart = this.input.lastIndexOf("\n", this.state.lineStart - 2) + 1;
        --this.state.curLine;
      }
      this.nextToken();
    }
  }, {
    key: "curContext",
    value: function curContext() {
      return this.state.context[this.state.context.length - 1];
    }

    // Read a single token, updating the parser object's token-related
    // properties.

  }, {
    key: "nextToken",
    value: function nextToken() {
      var curContext = this.curContext();
      if (!curContext || !curContext.preserveSpace) this.skipSpace();

      this.state.containsOctal = false;
      this.state.octalPosition = null;
      this.state.start = this.state.pos;
      this.state.startLoc = this.state.curPosition();
      if (this.state.pos >= this.input.length) return this.finishToken(_types.types.eof);

      if (curContext.override) {
        return curContext.override(this);
      } else {
        return this.readToken(this.fullCharCodeAtPos());
      }
    }
  }, {
    key: "readToken",
    value: function readToken(code) {
      // Identifier or keyword. '\uXXXX' sequences are allowed in
      // identifiers, so '\' also dispatches to that.
      if ((0, _identifier.isIdentifierStart)(code) || code === 92 /* '\' */) {
          return this.readWord();
        } else {
        return this.getTokenFromCode(code);
      }
    }
  }, {
    key: "fullCharCodeAtPos",
    value: function fullCharCodeAtPos() {
      var code = this.input.charCodeAt(this.state.pos);
      if (code <= 0xd7ff || code >= 0xe000) return code;

      var next = this.input.charCodeAt(this.state.pos + 1);
      return (code << 10) + next - 0x35fdc00;
    }
  }, {
    key: "pushComment",
    value: function pushComment(block, text, start, end, startLoc, endLoc) {
      var comment = {
        type: block ? "CommentBlock" : "CommentLine",
        value: text,
        start: start,
        end: end,
        loc: new _location.SourceLocation(startLoc, endLoc)
      };

      if (!this.isLookahead) {
        this.state.tokens.push(comment);
        this.state.comments.push(comment);
      }

      this.addComment(comment);
    }
  }, {
    key: "skipBlockComment",
    value: function skipBlockComment() {
      var startLoc = this.state.curPosition();
      var start = this.state.pos,
          end = this.input.indexOf("*/", this.state.pos += 2);
      if (end === -1) this.raise(this.state.pos - 2, "Unterminated comment");

      this.state.pos = end + 2;
      _whitespace.lineBreakG.lastIndex = start;
      var match = void 0;
      while ((match = _whitespace.lineBreakG.exec(this.input)) && match.index < this.state.pos) {
        ++this.state.curLine;
        this.state.lineStart = match.index + match[0].length;
      }

      this.pushComment(true, this.input.slice(start + 2, end), start, this.state.pos, startLoc, this.state.curPosition());
    }
  }, {
    key: "skipLineComment",
    value: function skipLineComment(startSkip) {
      var start = this.state.pos;
      var startLoc = this.state.curPosition();
      var ch = this.input.charCodeAt(this.state.pos += startSkip);
      while (this.state.pos < this.input.length && ch !== 10 && ch !== 13 && ch !== 8232 && ch !== 8233) {
        ++this.state.pos;
        ch = this.input.charCodeAt(this.state.pos);
      }

      this.pushComment(false, this.input.slice(start + startSkip, this.state.pos), start, this.state.pos, startLoc, this.state.curPosition());
    }

    // Called at the start of the parse and after every token. Skips
    // whitespace and comments, and.

  }, {
    key: "skipSpace",
    value: function skipSpace() {
      loop: while (this.state.pos < this.input.length) {
        var ch = this.input.charCodeAt(this.state.pos);
        switch (ch) {
          case 32:case 160:
            // ' '
            ++this.state.pos;
            break;

          case 13:
            if (this.input.charCodeAt(this.state.pos + 1) === 10) {
              ++this.state.pos;
            }

          case 10:case 8232:case 8233:
            ++this.state.pos;
            ++this.state.curLine;
            this.state.lineStart = this.state.pos;
            break;

          case 47:
            // '/'
            switch (this.input.charCodeAt(this.state.pos + 1)) {
              case 42:
                // '*'
                this.skipBlockComment();
                break;

              case 47:
                this.skipLineComment(2);
                break;

              default:
                break loop;
            }
            break;

          default:
            if (ch > 8 && ch < 14 || ch >= 5760 && _whitespace.nonASCIIwhitespace.test(String.fromCharCode(ch))) {
              ++this.state.pos;
            } else {
              break loop;
            }
        }
      }
    }

    // Called at the end of every token. Sets `end`, `val`, and
    // maintains `context` and `exprAllowed`, and skips the space after
    // the token, so that the next one's `start` will point at the
    // right position.

  }, {
    key: "finishToken",
    value: function finishToken(type, val) {
      this.state.end = this.state.pos;
      this.state.endLoc = this.state.curPosition();
      var prevType = this.state.type;
      this.state.type = type;
      this.state.value = val;

      this.updateContext(prevType);
    }

    // ### Token reading

    // This is the function that is called to fetch the next token. It
    // is somewhat obscure, because it works in character codes rather
    // than characters, and because operator parsing has been inlined
    // into it.
    //
    // All in the name of speed.
    //

  }, {
    key: "readToken_dot",
    value: function readToken_dot() {
      var next = this.input.charCodeAt(this.state.pos + 1);
      if (next >= 48 && next <= 57) {
        return this.readNumber(true);
      }

      var next2 = this.input.charCodeAt(this.state.pos + 2);
      if (next === 46 && next2 === 46) {
        // 46 = dot '.'
        this.state.pos += 3;
        return this.finishToken(_types.types.ellipsis);
      } else {
        ++this.state.pos;
        return this.finishToken(_types.types.dot);
      }
    }
  }, {
    key: "readToken_slash",
    value: function readToken_slash() {
      // '/'
      if (this.state.exprAllowed) {
        ++this.state.pos;
        return this.readRegexp();
      }

      var next = this.input.charCodeAt(this.state.pos + 1);
      if (next === 61) {
        return this.finishOp(_types.types.assign, 2);
      } else {
        return this.finishOp(_types.types.slash, 1);
      }
    }
  }, {
    key: "readToken_mult_modulo",
    value: function readToken_mult_modulo(code) {
      // '%*'
      var type = code === 42 ? _types.types.star : _types.types.modulo;
      var width = 1;
      var next = this.input.charCodeAt(this.state.pos + 1);

      if (next === 42 && this.hasPlugin("exponentiationOperator")) {
        // '*'
        width++;
        next = this.input.charCodeAt(this.state.pos + 2);
        type = _types.types.exponent;
      }

      if (next === 61) {
        width++;
        type = _types.types.assign;
      }

      return this.finishOp(type, width);
    }
  }, {
    key: "readToken_pipe_amp",
    value: function readToken_pipe_amp(code) {
      // '|&'
      var next = this.input.charCodeAt(this.state.pos + 1);
      if (next === code) return this.finishOp(code === 124 ? _types.types.logicalOR : _types.types.logicalAND, 2);
      if (next === 61) return this.finishOp(_types.types.assign, 2);
      return this.finishOp(code === 124 ? _types.types.bitwiseOR : _types.types.bitwiseAND, 1);
    }
  }, {
    key: "readToken_caret",
    value: function readToken_caret() {
      // '^'
      var next = this.input.charCodeAt(this.state.pos + 1);
      if (next === 61) {
        return this.finishOp(_types.types.assign, 2);
      } else {
        return this.finishOp(_types.types.bitwiseXOR, 1);
      }
    }
  }, {
    key: "readToken_plus_min",
    value: function readToken_plus_min(code) {
      // '+-'
      var next = this.input.charCodeAt(this.state.pos + 1);

      if (next === code) {
        if (next === 45 && this.input.charCodeAt(this.state.pos + 2) === 62 && _whitespace.lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.pos))) {
          // A `-->` line comment
          this.skipLineComment(3);
          this.skipSpace();
          return this.nextToken();
        }
        return this.finishOp(_types.types.incDec, 2);
      }

      if (next === 61) {
        return this.finishOp(_types.types.assign, 2);
      } else {
        return this.finishOp(_types.types.plusMin, 1);
      }
    }
  }, {
    key: "readToken_lt_gt",
    value: function readToken_lt_gt(code) {
      // '<>'
      var next = this.input.charCodeAt(this.state.pos + 1);
      var size = 1;

      if (next === code) {
        size = code === 62 && this.input.charCodeAt(this.state.pos + 2) === 62 ? 3 : 2;
        if (this.input.charCodeAt(this.state.pos + size) === 61) return this.finishOp(_types.types.assign, size + 1);
        return this.finishOp(_types.types.bitShift, size);
      }

      if (next === 33 && code === 60 && this.input.charCodeAt(this.state.pos + 2) === 45 && this.input.charCodeAt(this.state.pos + 3) === 45) {
        if (this.inModule) this.unexpected();
        // `<!--`, an XML-style comment that should be interpreted as a line comment
        this.skipLineComment(4);
        this.skipSpace();
        return this.nextToken();
      }

      if (next === 61) {
        // <= | >=
        size = 2;
      }

      return this.finishOp(_types.types.relational, size);
    }
  }, {
    key: "readToken_eq_excl",
    value: function readToken_eq_excl(code) {
      // '=!'
      var next = this.input.charCodeAt(this.state.pos + 1);
      if (next === 61) return this.finishOp(_types.types.equality, this.input.charCodeAt(this.state.pos + 2) === 61 ? 3 : 2);
      if (code === 61 && next === 62) {
        // '=>'
        this.state.pos += 2;
        return this.finishToken(_types.types.arrow);
      }
      return this.finishOp(code === 61 ? _types.types.eq : _types.types.prefix, 1);
    }
  }, {
    key: "getTokenFromCode",
    value: function getTokenFromCode(code) {
      switch (code) {
        // The interpretation of a dot depends on whether it is followed
        // by a digit or another two dots.
        case 46:
          // '.'
          return this.readToken_dot();

        // Punctuation tokens.
        case 40:
          ++this.state.pos;return this.finishToken(_types.types.parenL);
        case 41:
          ++this.state.pos;return this.finishToken(_types.types.parenR);
        case 59:
          ++this.state.pos;return this.finishToken(_types.types.semi);
        case 44:
          ++this.state.pos;return this.finishToken(_types.types.comma);
        case 91:
          ++this.state.pos;return this.finishToken(_types.types.bracketL);
        case 93:
          ++this.state.pos;return this.finishToken(_types.types.bracketR);
        case 123:
          ++this.state.pos;return this.finishToken(_types.types.braceL);
        case 125:
          ++this.state.pos;return this.finishToken(_types.types.braceR);

        case 58:
          if (this.hasPlugin("functionBind") && this.input.charCodeAt(this.state.pos + 1) === 58) {
            return this.finishOp(_types.types.doubleColon, 2);
          } else {
            ++this.state.pos;
            return this.finishToken(_types.types.colon);
          }

        case 63:
          ++this.state.pos;return this.finishToken(_types.types.question);
        case 64:
          ++this.state.pos;return this.finishToken(_types.types.at);

        case 96:
          // '`'
          ++this.state.pos;
          return this.finishToken(_types.types.backQuote);

        case 48:
          // '0'
          var next = this.input.charCodeAt(this.state.pos + 1);
          if (next === 120 || next === 88) return this.readRadixNumber(16); // '0x', '0X' - hex number
          if (next === 111 || next === 79) return this.readRadixNumber(8); // '0o', '0O' - octal number
          if (next === 98 || next === 66) return this.readRadixNumber(2); // '0b', '0B' - binary number
        // Anything else beginning with a digit is an integer, octal
        // number, or float.
        case 49:case 50:case 51:case 52:case 53:case 54:case 55:case 56:case 57:
          // 1-9
          return this.readNumber(false);

        // Quotes produce strings.
        case 34:case 39:
          // '"', "'"
          return this.readString(code);

        // Operators are parsed inline in tiny state machines. '=' (61) is
        // often referred to. `finishOp` simply skips the amount of
        // characters it is given as second argument, and returns a token
        // of the type given by its first argument.

        case 47:
          // '/'
          return this.readToken_slash();

        case 37:case 42:
          // '%*'
          return this.readToken_mult_modulo(code);

        case 124:case 38:
          // '|&'
          return this.readToken_pipe_amp(code);

        case 94:
          // '^'
          return this.readToken_caret();

        case 43:case 45:
          // '+-'
          return this.readToken_plus_min(code);

        case 60:case 62:
          // '<>'
          return this.readToken_lt_gt(code);

        case 61:case 33:
          // '=!'
          return this.readToken_eq_excl(code);

        case 126:
          // '~'
          return this.finishOp(_types.types.prefix, 1);
      }

      this.raise(this.state.pos, "Unexpected character '" + codePointToString(code) + "'");
    }
  }, {
    key: "finishOp",
    value: function finishOp(type, size) {
      var str = this.input.slice(this.state.pos, this.state.pos + size);
      this.state.pos += size;
      return this.finishToken(type, str);
    }
  }, {
    key: "readRegexp",
    value: function readRegexp() {
      var escaped = void 0,
          inClass = void 0,
          start = this.state.pos;
      for (;;) {
        if (this.state.pos >= this.input.length) this.raise(start, "Unterminated regular expression");
        var ch = this.input.charAt(this.state.pos);
        if (_whitespace.lineBreak.test(ch)) {
          this.raise(start, "Unterminated regular expression");
        }
        if (escaped) {
          escaped = false;
        } else {
          if (ch === "[") {
            inClass = true;
          } else if (ch === "]" && inClass) {
            inClass = false;
          } else if (ch === "/" && !inClass) {
            break;
          }
          escaped = ch === "\\";
        }
        ++this.state.pos;
      }
      var content = this.input.slice(start, this.state.pos);
      ++this.state.pos;
      // Need to use `readWord1` because '\uXXXX' sequences are allowed
      // here (don't ask).
      var mods = this.readWord1();
      if (mods) {
        var validFlags = /^[gmsiyu]*$/;
        if (!validFlags.test(mods)) this.raise(start, "Invalid regular expression flag");
      }
      return this.finishToken(_types.types.regexp, {
        pattern: content,
        flags: mods
      });
    }

    // Read an integer in the given radix. Return null if zero digits
    // were read, the integer value otherwise. When `len` is given, this
    // will return `null` unless the integer has exactly `len` digits.

  }, {
    key: "readInt",
    value: function readInt(radix, len) {
      var start = this.state.pos,
          total = 0;
      for (var i = 0, e = len == null ? Infinity : len; i < e; ++i) {
        var code = this.input.charCodeAt(this.state.pos),
            val = void 0;
        if (code >= 97) {
          val = code - 97 + 10; // a
        } else if (code >= 65) {
            val = code - 65 + 10; // A
          } else if (code >= 48 && code <= 57) {
              val = code - 48; // 0-9
            } else {
                val = Infinity;
              }
        if (val >= radix) break;
        ++this.state.pos;
        total = total * radix + val;
      }
      if (this.state.pos === start || len != null && this.state.pos - start !== len) return null;

      return total;
    }
  }, {
    key: "readRadixNumber",
    value: function readRadixNumber(radix) {
      this.state.pos += 2; // 0x
      var val = this.readInt(radix);
      if (val == null) this.raise(this.state.start + 2, "Expected number in radix " + radix);
      if ((0, _identifier.isIdentifierStart)(this.fullCharCodeAtPos())) this.raise(this.state.pos, "Identifier directly after number");
      return this.finishToken(_types.types.num, val);
    }

    // Read an integer, octal integer, or floating-point number.

  }, {
    key: "readNumber",
    value: function readNumber(startsWithDot) {
      var start = this.state.pos,
          isFloat = false,
          octal = this.input.charCodeAt(this.state.pos) === 48;
      if (!startsWithDot && this.readInt(10) === null) this.raise(start, "Invalid number");
      var next = this.input.charCodeAt(this.state.pos);
      if (next === 46) {
        // '.'
        ++this.state.pos;
        this.readInt(10);
        isFloat = true;
        next = this.input.charCodeAt(this.state.pos);
      }
      if (next === 69 || next === 101) {
        // 'eE'
        next = this.input.charCodeAt(++this.state.pos);
        if (next === 43 || next === 45) ++this.state.pos; // '+-'
        if (this.readInt(10) === null) this.raise(start, "Invalid number");
        isFloat = true;
      }
      if ((0, _identifier.isIdentifierStart)(this.fullCharCodeAtPos())) this.raise(this.state.pos, "Identifier directly after number");

      var str = this.input.slice(start, this.state.pos),
          val = void 0;
      if (isFloat) {
        val = parseFloat(str);
      } else if (!octal || str.length === 1) {
        val = parseInt(str, 10);
      } else if (/[89]/.test(str) || this.state.strict) {
        this.raise(start, "Invalid number");
      } else {
        val = parseInt(str, 8);
      }
      return this.finishToken(_types.types.num, val);
    }

    // Read a string value, interpreting backslash-escapes.

  }, {
    key: "readCodePoint",
    value: function readCodePoint() {
      var ch = this.input.charCodeAt(this.state.pos),
          code = void 0;

      if (ch === 123) {
        var codePos = ++this.state.pos;
        code = this.readHexChar(this.input.indexOf("}", this.state.pos) - this.state.pos);
        ++this.state.pos;
        if (code > 0x10FFFF) this.raise(codePos, "Code point out of bounds");
      } else {
        code = this.readHexChar(4);
      }
      return code;
    }
  }, {
    key: "readString",
    value: function readString(quote) {
      var out = "",
          chunkStart = ++this.state.pos;
      for (;;) {
        if (this.state.pos >= this.input.length) this.raise(this.state.start, "Unterminated string constant");
        var ch = this.input.charCodeAt(this.state.pos);
        if (ch === quote) break;
        if (ch === 92) {
          // '\'
          out += this.input.slice(chunkStart, this.state.pos);
          out += this.readEscapedChar(false);
          chunkStart = this.state.pos;
        } else {
          if ((0, _whitespace.isNewLine)(ch)) this.raise(this.state.start, "Unterminated string constant");
          ++this.state.pos;
        }
      }
      out += this.input.slice(chunkStart, this.state.pos++);
      return this.finishToken(_types.types.string, out);
    }

    // Reads template string tokens.

  }, {
    key: "readTmplToken",
    value: function readTmplToken() {
      var out = "",
          chunkStart = this.state.pos;
      for (;;) {
        if (this.state.pos >= this.input.length) this.raise(this.state.start, "Unterminated template");
        var ch = this.input.charCodeAt(this.state.pos);
        if (ch === 96 || ch === 36 && this.input.charCodeAt(this.state.pos + 1) === 123) {
          // '`', '${'
          if (this.state.pos === this.state.start && this.match(_types.types.template)) {
            if (ch === 36) {
              this.state.pos += 2;
              return this.finishToken(_types.types.dollarBraceL);
            } else {
              ++this.state.pos;
              return this.finishToken(_types.types.backQuote);
            }
          }
          out += this.input.slice(chunkStart, this.state.pos);
          return this.finishToken(_types.types.template, out);
        }
        if (ch === 92) {
          // '\'
          out += this.input.slice(chunkStart, this.state.pos);
          out += this.readEscapedChar(true);
          chunkStart = this.state.pos;
        } else if ((0, _whitespace.isNewLine)(ch)) {
          out += this.input.slice(chunkStart, this.state.pos);
          ++this.state.pos;
          switch (ch) {
            case 13:
              if (this.input.charCodeAt(this.state.pos) === 10) ++this.state.pos;
            case 10:
              out += "\n";
              break;
            default:
              out += String.fromCharCode(ch);
              break;
          }
          ++this.state.curLine;
          this.state.lineStart = this.state.pos;
          chunkStart = this.state.pos;
        } else {
          ++this.state.pos;
        }
      }
    }

    // Used to read escaped characters

  }, {
    key: "readEscapedChar",
    value: function readEscapedChar(inTemplate) {
      var ch = this.input.charCodeAt(++this.state.pos);
      ++this.state.pos;
      switch (ch) {
        case 110:
          return "\n"; // 'n' -> '\n'
        case 114:
          return "\r"; // 'r' -> '\r'
        case 120:
          return String.fromCharCode(this.readHexChar(2)); // 'x'
        case 117:
          return codePointToString(this.readCodePoint()); // 'u'
        case 116:
          return "\t"; // 't' -> '\t'
        case 98:
          return "\b"; // 'b' -> '\b'
        case 118:
          return "\u000b"; // 'v' -> '\u000b'
        case 102:
          return "\f"; // 'f' -> '\f'
        case 13:
          if (this.input.charCodeAt(this.state.pos) === 10) ++this.state.pos; // '\r\n'
        case 10:
          // ' \n'
          this.state.lineStart = this.state.pos;
          ++this.state.curLine;
          return "";
        default:
          if (ch >= 48 && ch <= 55) {
            var octalStr = this.input.substr(this.state.pos - 1, 3).match(/^[0-7]+/)[0];
            var octal = parseInt(octalStr, 8);
            if (octal > 255) {
              octalStr = octalStr.slice(0, -1);
              octal = parseInt(octalStr, 8);
            }
            if (octal > 0) {
              if (!this.state.containsOctal) {
                this.state.containsOctal = true;
                this.state.octalPosition = this.state.pos - 2;
              }
              if (this.state.strict || inTemplate) {
                this.raise(this.state.pos - 2, "Octal literal in strict mode");
              }
            }
            this.state.pos += octalStr.length - 1;
            return String.fromCharCode(octal);
          }
          return String.fromCharCode(ch);
      }
    }

    // Used to read character escape sequences ('\x', '\u', '\U').

  }, {
    key: "readHexChar",
    value: function readHexChar(len) {
      var codePos = this.state.pos;
      var n = this.readInt(16, len);
      if (n === null) this.raise(codePos, "Bad character escape sequence");
      return n;
    }

    // Read an identifier, and return it as a string. Sets `this.state.containsEsc`
    // to whether the word contained a '\u' escape.
    //
    // Incrementally adds only escaped chars, adding other chunks as-is
    // as a micro-optimization.

  }, {
    key: "readWord1",
    value: function readWord1() {
      this.state.containsEsc = false;
      var word = "",
          first = true,
          chunkStart = this.state.pos;
      while (this.state.pos < this.input.length) {
        var ch = this.fullCharCodeAtPos();
        if ((0, _identifier.isIdentifierChar)(ch)) {
          this.state.pos += ch <= 0xffff ? 1 : 2;
        } else if (ch === 92) {
          // "\"
          this.state.containsEsc = true;

          word += this.input.slice(chunkStart, this.state.pos);
          var escStart = this.state.pos;

          if (this.input.charCodeAt(++this.state.pos) !== 117) {
            // "u"
            this.raise(this.state.pos, "Expecting Unicode escape sequence \\uXXXX");
          }

          ++this.state.pos;
          var esc = this.readCodePoint();
          if (!(first ? _identifier.isIdentifierStart : _identifier.isIdentifierChar)(esc, true)) {
            this.raise(escStart, "Invalid Unicode escape");
          }

          word += codePointToString(esc);
          chunkStart = this.state.pos;
        } else {
          break;
        }
        first = false;
      }
      return word + this.input.slice(chunkStart, this.state.pos);
    }

    // Read an identifier or keyword token. Will check for reserved
    // words when necessary.

  }, {
    key: "readWord",
    value: function readWord() {
      var word = this.readWord1();
      var type = _types.types.name;
      if (!this.state.containsEsc && this.isKeyword(word)) {
        type = _types.keywords[word];
      }
      return this.finishToken(type, word);
    }
  }, {
    key: "braceIsBlock",
    value: function braceIsBlock(prevType) {
      if (prevType === _types.types.colon) {
        var parent = this.curContext();
        if (parent === _context.types.braceStatement || parent === _context.types.braceExpression) {
          return !parent.isExpr;
        }
      }

      if (prevType === _types.types._return) {
        return _whitespace.lineBreak.test(this.input.slice(this.state.lastTokEnd, this.state.start));
      }

      if (prevType === _types.types._else || prevType === _types.types.semi || prevType === _types.types.eof || prevType === _types.types.parenR) {
        return true;
      }

      if (prevType === _types.types.braceL) {
        return this.curContext() === _context.types.braceStatement;
      }

      return !this.state.exprAllowed;
    }
  }, {
    key: "updateContext",
    value: function updateContext(prevType) {
      var update = void 0,
          type = this.state.type;
      if (type.keyword && prevType === _types.types.dot) {
        this.state.exprAllowed = false;
      } else if (update = type.updateContext) {
        update.call(this, prevType);
      } else {
        this.state.exprAllowed = type.beforeExpr;
      }
    }
  }]);
  return Tokenizer;
}();

exports.default = Tokenizer;