'use strict';

// A YAML 1.2 parser for the core schema that builds JavaScript values
// directly, in one pass over the text. It follows the structure of the
// specification: documents and directives, block collections driven by
// indentation, flow collections, and the scalar styles.

const {
  ArrayIsArray,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  JSONStringify,
  NumberParseFloat,
  NumberParseInt,
  NumberPrototypeToString,
  ObjectDefineProperty,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  SafeMap,
  SafeSet,
  String,
  StringFromCharCode,
  StringFromCodePoint,
  StringPrototypeCharCodeAt,
  StringPrototypeRepeat,
  StringPrototypeSlice,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_YAML,
  },
} = require('internal/errors');

const kMaxDepth = 1000;
const kMaxKeyLength = 1024;

const kEOF = -1;
const kTab = 0x09;
const kLF = 0x0A;
const kCR = 0x0D;
const kSpace = 0x20;
const kBang = 0x21;
const kDoubleQuote = 0x22;
const kHash = 0x23;
const kPercent = 0x25;
const kAmpersand = 0x26;
const kSingleQuote = 0x27;
const kAsterisk = 0x2A;
const kPlus = 0x2B;
const kComma = 0x2C;
const kMinus = 0x2D;
const kDot = 0x2E;
const kSlash = 0x2F;
const kZero = 0x30;
const kNine = 0x39;
const kColon = 0x3A;
const kLess = 0x3C;
const kGreater = 0x3E;
const kQuestion = 0x3F;
const kAt = 0x40;
const kLeftBracket = 0x5B;
const kBackslash = 0x5C;
const kRightBracket = 0x5D;
const kBacktick = 0x60;
const kLeftBrace = 0x7B;
const kPipe = 0x7C;
const kRightBrace = 0x7D;
const kBOM = 0xFEFF;

const kClip = 0;
const kStrip = 1;
const kKeep = 2;

// How a node was written, which decides how a tag applies to it.
const kPlain = 0;
const kQuoted = 1;
const kSequence = 2;
const kMapping = 3;

const kCoreTagPrefix = 'tag:yaml.org,2002:';

// Returned by the compose functions when there is no node at the position.
const kNoNode = Symbol('kNoNode');

class State {
  constructor(input) {
    this.input = input;
    this.length = input.length;
    this.pos = 0;
    this.line = 0;
    this.lineStart = 0;
    // Number of leading spaces of the current line.
    this.lineIndent = 0;
    // Whether the current line has content after a tab in its indentation.
    this.tabIndented = false;
    this.depth = 0;
    this.anchors = new SafeMap();
    this.tagHandles = new SafeMap();
    this.version = null;
    this.explicitEnd = false;
    // Set by the scalar readers: whether the scalar spanned several lines.
    this.multiline = false;
    // Set by readFlowNode: whether the node was quoted or a flow collection,
    // which allows ":" to follow it without a space.
    this.lastJsonLike = false;
  }
}

function at(s, pos) {
  return pos < s.length ? StringPrototypeCharCodeAt(s.input, pos) : kEOF;
}

function fail(s, message, pos = s.pos) {
  let line = s.line;
  let lineStart = s.lineStart;
  if (pos < lineStart) {
    // The error is on an earlier line: recount from the start.
    line = 0;
    lineStart = 0;
    for (let i = 0; i < pos; i++) {
      const c = StringPrototypeCharCodeAt(s.input, i);
      if (c === kLF || (c === kCR && at(s, i + 1) !== kLF)) {
        line++;
        lineStart = i + 1;
      }
    }
  }
  throw new ERR_INVALID_YAML(
    `${message} (line ${line + 1}, column ${pos - lineStart + 1})`);
}

function isWhite(c) {
  return c === kSpace || c === kTab;
}

function isBreak(c) {
  return c === kLF || c === kCR;
}

function isBlank(c) {
  return c === kSpace || c === kTab || c === kLF || c === kCR || c === kEOF;
}

function isFlowIndicator(c) {
  return c === kComma || c === kLeftBracket || c === kRightBracket ||
    c === kLeftBrace || c === kRightBrace;
}

function isDigit(c) {
  return c >= kZero && c <= kNine;
}

function isHexDigit(c) {
  return isDigit(c) || (c >= 0x41 && c <= 0x46) || (c >= 0x61 && c <= 0x66);
}

// Consumes one line break and the indentation of the next line.
function consumeBreak(s) {
  const c = at(s, s.pos);
  s.pos++;
  if (c === kCR && at(s, s.pos) === kLF) s.pos++;
  s.line++;
  s.lineStart = s.pos;
  let indent = 0;
  while (at(s, s.pos) === kSpace) {
    s.pos++;
    indent++;
  }
  s.lineIndent = indent;
  s.tabIndented = false;
  if (at(s, s.pos) === kTab) {
    let p = s.pos;
    while (isWhite(at(s, p))) p++;
    const n = at(s, p);
    s.tabIndented = n !== kEOF && !isBreak(n) && n !== kHash;
  }
}

function skipWhite(s) {
  while (isWhite(at(s, s.pos))) s.pos++;
}

function skipToEndOfLine(s) {
  while (s.pos < s.length && !isBreak(at(s, s.pos))) s.pos++;
}

function atDocumentMarker(s, pos = s.pos) {
  if (pos !== s.lineStart) return false;
  const c = at(s, pos);
  if (c !== kMinus && c !== kDot) return false;
  return at(s, pos + 1) === c && at(s, pos + 2) === c && isBlank(at(s, pos + 3));
}

// Skips whitespace, comments and line breaks. Returns the number of line
// breaks crossed. A comment must be separated from content by whitespace.
function skipSeparation(s) {
  let breaks = 0;
  for (;;) {
    const c = at(s, s.pos);
    if (isWhite(c)) {
      s.pos++;
    } else if (c === kHash) {
      if (s.pos > 0 && !isBlank(at(s, s.pos - 1))) {
        fail(s, 'Comments must be separated from other tokens by whitespace');
      }
      skipToEndOfLine(s);
    } else if (isBreak(c)) {
      consumeBreak(s);
      breaks++;
    } else {
      return breaks;
    }
  }
}

function setProperty(s, map, key, value, keyPos) {
  if (ObjectPrototypeHasOwnProperty(map, key)) {
    fail(s, `Duplicate mapping key "${key}"`, keyPos);
  }
  ObjectDefineProperty(map, key, {
    __proto__: null,
    value,
    writable: true,
    enumerable: true,
    configurable: true,
  });
}

// Mapping keys become property names: scalars as their string form,
// collections as their JSON text, bounded in size since aliases can make a
// small key expand to a huge text.
function keyToString(s, key, pos) {
  if (typeof key === 'string') return key;
  if (key === null || typeof key === 'number' || typeof key === 'boolean') {
    return String(key);
  }
  const text = { __proto__: null, value: '', budget: kMaxKeyLength };
  appendKeyJSON(s, text, key, pos);
  return text.value;
}

function appendKeyText(s, text, part, pos) {
  text.budget -= part.length;
  if (text.budget < 0) {
    fail(s, `A collection key must not exceed ${kMaxKeyLength} characters`, pos);
  }
  text.value += part;
}

function appendKeyJSON(s, text, value, pos) {
  if (value === null || typeof value !== 'object') {
    appendKeyText(s, text, JSONStringify(value), pos);
  } else if (ArrayIsArray(value)) {
    appendKeyText(s, text, '[', pos);
    for (let i = 0; i < value.length; i++) {
      if (i > 0) appendKeyText(s, text, ',', pos);
      appendKeyJSON(s, text, value[i], pos);
    }
    appendKeyText(s, text, ']', pos);
  } else {
    appendKeyText(s, text, '{', pos);
    const keys = ObjectKeys(value);
    for (let i = 0; i < keys.length; i++) {
      if (i > 0) appendKeyText(s, text, ',', pos);
      appendKeyText(s, text, `${JSONStringify(keys[i])}:`, pos);
      appendKeyJSON(s, text, value[keys[i]], pos);
    }
    appendKeyText(s, text, '}', pos);
  }
}

function enter(s) {
  if (++s.depth > kMaxDepth) {
    fail(s, `Nesting deeper than ${kMaxDepth} levels`);
  }
}

// --- Scalars ---------------------------------------------------------------

// Resolves a plain scalar with the core schema.
function resolvePlain(text) {
  const length = text.length;
  if (length === 0) return null;
  const c = StringPrototypeCharCodeAt(text, 0);
  switch (c) {
    case 0x7E:  // ~
      if (length === 1) return null;
      break;
    case 0x6E: case 0x4E:  // n N
      if (text === 'null' || text === 'Null' || text === 'NULL') return null;
      break;
    case 0x74: case 0x54:  // t T
      if (text === 'true' || text === 'True' || text === 'TRUE') return true;
      break;
    case 0x66: case 0x46:  // f F
      if (text === 'false' || text === 'False' || text === 'FALSE') return false;
      break;
    case kDot: case kMinus: case kPlus: {
      const special = parseSpecialFloat(text);
      if (special !== undefined) return special;
      break;
    }
  }
  if (c === kDot || c === kMinus || c === kPlus || isDigit(c)) {
    const number = parseNumber(text);
    if (number !== undefined) return number;
  }
  return text;
}

function parseSpecialFloat(text) {
  let i = 0;
  const first = StringPrototypeCharCodeAt(text, 0);
  if (first === kMinus || first === kPlus) i = 1;
  if (StringPrototypeCharCodeAt(text, i) !== kDot) return undefined;
  const word = StringPrototypeSlice(text, i + 1);
  if (word === 'inf' || word === 'Inf' || word === 'INF') {
    return first === kMinus ? -Infinity : Infinity;
  }
  if (i === 0 && (word === 'nan' || word === 'NaN' || word === 'NAN')) {
    return NaN;
  }
  return undefined;
}

// Core schema integers and floats, or undefined.
function parseNumber(text) {
  const length = text.length;
  let i = 0;
  let c = StringPrototypeCharCodeAt(text, 0);
  const negative = c === kMinus;
  if (c === kMinus || c === kPlus) {
    i++;
    c = StringPrototypeCharCodeAt(text, i);
  }
  if (c === kZero && i === 0 && length > 2) {
    const base = StringPrototypeCharCodeAt(text, 1);
    if (base === 0x78) {  // 0x
      for (let j = 2; j < length; j++) {
        if (!isHexDigit(StringPrototypeCharCodeAt(text, j))) return undefined;
      }
      return NumberParseInt(StringPrototypeSlice(text, 2), 16);
    }
    if (base === 0x6F) {  // 0o
      for (let j = 2; j < length; j++) {
        const d = StringPrototypeCharCodeAt(text, j);
        if (d < kZero || d > 0x37) return undefined;
      }
      return NumberParseInt(StringPrototypeSlice(text, 2), 8);
    }
  }
  let digits = 0;
  while (i < length && isDigit(StringPrototypeCharCodeAt(text, i))) {
    i++;
    digits++;
  }
  let isFloat = false;
  if (i < length && StringPrototypeCharCodeAt(text, i) === kDot) {
    isFloat = true;
    i++;
    while (i < length && isDigit(StringPrototypeCharCodeAt(text, i))) {
      i++;
      digits++;
    }
  }
  if (digits === 0) return undefined;
  if (i < length) {
    c = StringPrototypeCharCodeAt(text, i);
    if (c !== 0x65 && c !== 0x45) return undefined;  // e E
    isFloat = true;
    i++;
    c = StringPrototypeCharCodeAt(text, i);
    if (c === kMinus || c === kPlus) i++;
    let exponentDigits = 0;
    while (i < length && isDigit(StringPrototypeCharCodeAt(text, i))) {
      i++;
      exponentDigits++;
    }
    if (exponentDigits === 0 || i < length) return undefined;
  }
  if (isFloat) return NumberParseFloat(text);
  const value = NumberParseInt(text, 10);
  // -0 is an integer in YAML; keep it a plain 0 like JSON.parse does.
  return negative && value === 0 ? 0 : value;
}

// Whether text has the form of a core schema integer: decimal with an
// optional sign, 0x hexadecimal or 0o octal.
function isIntegerText(text) {
  const length = text.length;
  let i = 0;
  const c = StringPrototypeCharCodeAt(text, 0);
  if (c === kMinus || c === kPlus) i = 1;
  if (c === kZero && length > 2) {
    const base = StringPrototypeCharCodeAt(text, 1);
    if (base === 0x78 || base === 0x6F) {
      for (let j = 2; j < length; j++) {
        const d = StringPrototypeCharCodeAt(text, j);
        if (base === 0x78 ? !isHexDigit(d) : (d < kZero || d > 0x37)) return false;
      }
      return true;
    }
  }
  if (i === length) return false;
  for (; i < length; i++) {
    if (!isDigit(StringPrototypeCharCodeAt(text, i))) return false;
  }
  return true;
}

// Characters that never start a plain scalar: whitespace, the indicators of
// the other node kinds, comments and the reserved indicators.
const kNoPlainStart = new SafeSet([
  kEOF, kSpace, kTab, kLF, kCR, kHash, kAmpersand, kAsterisk, kBang, kPipe,
  kGreater, kSingleQuote, kDoubleQuote, kPercent, kAt, kBacktick,
]);

function canStartPlain(s, c, inFlow) {
  if (kNoPlainStart.has(c)) return false;
  if (c === kMinus || c === kQuestion || c === kColon) {
    const next = at(s, s.pos + 1);
    return !isBlank(next) && !(inFlow && isFlowIndicator(next));
  }
  if (isFlowIndicator(c)) return !inFlow;
  return true;
}

// A line break inside a scalar folds into a space, or into one newline per
// empty line that follows it.
function foldBreaks(count) {
  return count === 1 ? ' ' : StringPrototypeRepeat('\n', count - 1);
}

// Reads a plain scalar and returns its text. In block context the
// continuation lines must be indented more than the parent; in flow context
// the scalar also ends at flow indicators.
function readPlain(s, parentIndent, inFlow, singleLine) {
  const startLine = s.line;
  let result = '';
  let segmentStart = s.pos;
  let segmentEnd = s.pos;
  let pendingBreaks = 0;

  const flush = () => {
    if (segmentEnd > segmentStart) {
      result += (pendingBreaks > 0 ? foldBreaks(pendingBreaks) : '') +
        StringPrototypeSlice(s.input, segmentStart, segmentEnd);
    }
  };

  for (;;) {
    const c = at(s, s.pos);
    if (c === kEOF || isBreak(c)) {
      flush();
      if (c === kEOF || singleLine) break;
      // Look at the following lines without committing to them.
      const savedLine = s.line;
      const savedLineStart = s.lineStart;
      const savedIndent = s.lineIndent;
      const savedTab = s.tabIndented;
      let breaks = 0;
      let continues = false;
      while (isBreak(at(s, s.pos))) {
        consumeBreak(s);
        breaks++;
        let p = s.pos;
        while (isWhite(at(s, p))) p++;
        const n = at(s, p);
        if (n === kEOF) break;
        if (isBreak(n)) {
          // A blank line.
          s.pos = p;
          continue;
        }
        if (atDocumentMarker(s) || n === kHash) break;
        if (s.lineIndent <= parentIndent) break;
        if (inFlow && isFlowIndicator(n)) break;
        s.pos = p;
        continues = true;
        break;
      }
      if (!continues) {
        s.line = savedLine;
        s.lineStart = savedLineStart;
        s.lineIndent = savedIndent;
        s.tabIndented = savedTab;
        break;
      }
      pendingBreaks = breaks;
      segmentStart = s.pos;
      segmentEnd = s.pos;
      continue;
    }
    if (c === kColon) {
      const next = at(s, s.pos + 1);
      if (isBlank(next) || (inFlow && isFlowIndicator(next))) {
        flush();
        break;
      }
    } else if (c === kHash) {
      if (isWhite(at(s, s.pos - 1))) {
        flush();
        break;
      }
    } else if (inFlow && isFlowIndicator(c)) {
      flush();
      break;
    }
    s.pos++;
    if (!isWhite(c)) segmentEnd = s.pos;
  }
  // The scalar ends after its last non-blank character.
  s.pos = segmentEnd;
  s.multiline = s.line !== startLine;
  return result;
}

function trimTrailingWhite(text) {
  let end = text.length;
  while (end > 0 && isWhite(StringPrototypeCharCodeAt(text, end - 1))) end--;
  return StringPrototypeSlice(text, 0, end);
}

// Called at a line break inside a quoted scalar. Consumes the breaks and
// blank lines and returns their number. Continuation lines must be indented
// more than the parent node.
function skipQuotedBreaks(s, parentIndent) {
  let breaks = 0;
  while (isBreak(at(s, s.pos))) {
    consumeBreak(s);
    breaks++;
    skipWhite(s);
    const c = at(s, s.pos);
    if (c === kEOF) fail(s, 'Unterminated quoted scalar');
    if (isBreak(c)) continue;
    if (atDocumentMarker(s)) {
      fail(s, 'Unexpected document marker inside a quoted scalar');
    }
    if (s.lineIndent <= parentIndent) fail(s, 'Bad indentation in a quoted scalar');
  }
  return breaks;
}

function readSingleQuoted(s, parentIndent) {
  const startLine = s.line;
  const startPos = s.pos;
  s.pos++;  // Opening quote.
  let result = '';
  let segmentStart = s.pos;
  for (;;) {
    const c = at(s, s.pos);
    if (c === kEOF) {
      fail(s, 'Unterminated single-quoted scalar', startPos);
    } else if (c === kSingleQuote) {
      result += StringPrototypeSlice(s.input, segmentStart, s.pos);
      s.pos++;
      if (at(s, s.pos) === kSingleQuote) {
        result += "'";
        s.pos++;
        segmentStart = s.pos;
        continue;
      }
      break;
    } else if (isBreak(c)) {
      result += trimTrailingWhite(StringPrototypeSlice(s.input, segmentStart, s.pos));
      result += foldBreaks(skipQuotedBreaks(s, parentIndent));
      segmentStart = s.pos;
    } else {
      s.pos++;
    }
  }
  s.multiline = s.line !== startLine;
  return result;
}

function readDoubleQuoted(s, parentIndent) {
  const startLine = s.line;
  const startPos = s.pos;
  s.pos++;  // Opening quote.
  let result = '';
  let segmentStart = s.pos;
  for (;;) {
    const c = at(s, s.pos);
    if (c === kEOF) {
      fail(s, 'Unterminated double-quoted scalar', startPos);
    } else if (c === kDoubleQuote) {
      result += StringPrototypeSlice(s.input, segmentStart, s.pos);
      s.pos++;
      break;
    } else if (c === kBackslash) {
      result += StringPrototypeSlice(s.input, segmentStart, s.pos);
      s.pos++;
      const e = at(s, s.pos);
      if (isBreak(e)) {
        // Escaped line break: join with the next line without a space. Empty
        // lines in between are kept.
        const breaks = skipQuotedBreaks(s, parentIndent);
        if (breaks > 1) result += StringPrototypeRepeat('\n', breaks - 1);
      } else {
        result += readEscape(s, e);
      }
      segmentStart = s.pos;
    } else if (isBreak(c)) {
      result += trimTrailingWhite(StringPrototypeSlice(s.input, segmentStart, s.pos));
      result += foldBreaks(skipQuotedBreaks(s, parentIndent));
      segmentStart = s.pos;
    } else {
      s.pos++;
    }
  }
  s.multiline = s.line !== startLine;
  return result;
}

function readEscape(s, c) {
  s.pos++;
  switch (c) {
    case 0x30: return '\0';
    case 0x61: return '\x07';
    case 0x62: return '\b';
    case 0x74: case kTab: return '\t';
    case 0x6E: return '\n';
    case 0x76: return '\v';
    case 0x66: return '\f';
    case 0x72: return '\r';
    case 0x65: return '\x1B';
    case kSpace: return ' ';
    case kDoubleQuote: return '"';
    case kSlash: return '/';
    case kBackslash: return '\\';
    case 0x4E: return '\x85';
    case 0x5F: return '\xA0';
    case 0x4C: return '\u2028';
    case 0x50: return '\u2029';
    case 0x78: return readHexEscape(s, 2);
    case 0x75: return readHexEscape(s, 4);
    case 0x55: return readHexEscape(s, 8);
  }
  fail(s, `Unknown escape sequence "\\${c === kEOF ? '' : StringFromCharCode(c)}"`, s.pos - 2);
}

function readHexEscape(s, digits) {
  let value = 0;
  for (let i = 0; i < digits; i++) {
    const c = at(s, s.pos);
    if (!isHexDigit(c)) fail(s, 'Invalid hexadecimal escape sequence');
    value = value * 16 + NumberParseInt(StringFromCharCode(c), 16);
    s.pos++;
  }
  if (value > 0x10FFFF || (digits === 8 && value >= 0xD800 && value <= 0xDFFF)) {
    fail(s, 'Invalid code point in escape sequence', s.pos - digits);
  }
  return value > 0xFFFF ? StringFromCodePoint(value) : StringFromCharCode(value);
}

// Reads a literal (|) or folded (>) block scalar. parentIndent is the
// indentation of the node that owns it.
function readBlockScalar(s, parentIndent) {
  const folded = at(s, s.pos) === kGreater;
  const headerPos = s.pos;
  s.pos++;
  let chomping = kClip;
  let indicator = 0;
  for (let i = 0; i < 2; i++) {
    const c = at(s, s.pos);
    if (c === kMinus || c === kPlus) {
      if (chomping !== kClip) fail(s, 'Repeated chomping indicator');
      chomping = c === kMinus ? kStrip : kKeep;
      s.pos++;
    } else if (isDigit(c)) {
      if (indicator !== 0) fail(s, 'Repeated indentation indicator');
      if (c === kZero) fail(s, 'Indentation indicator must be between 1 and 9');
      indicator = c - kZero;
      s.pos++;
    }
  }
  skipWhite(s);
  if (at(s, s.pos) === kHash) {
    if (!isBlank(at(s, s.pos - 1))) {
      fail(s, 'Comments must be separated from other tokens by whitespace');
    }
    skipToEndOfLine(s);
  }
  const afterHeader = at(s, s.pos);
  if (afterHeader !== kEOF && !isBreak(afterHeader)) {
    fail(s, 'Unexpected characters after block scalar header', headerPos);
  }
  s.multiline = true;
  if (afterHeader === kEOF) return '';
  consumeBreak(s);

  let contentIndent = indicator > 0 ? parentIndent + indicator : -1;

  // Leading empty lines: lines of spaces only. With auto-detection they must
  // not be indented more than the content.
  let leadingEmpty = 0;
  let maxLeadingSpaces = 0;
  for (;;) {
    const c = at(s, s.pos);
    if (c === kEOF && s.pos === s.lineStart) break;
    if (c !== kEOF && !isBreak(c)) break;
    if (s.lineIndent > maxLeadingSpaces) maxLeadingSpaces = s.lineIndent;
    leadingEmpty++;
    if (c === kEOF) break;
    consumeBreak(s);
  }
  if (at(s, s.pos) === kEOF) {
    return chomping === kKeep ? StringPrototypeRepeat('\n', leadingEmpty) : '';
  }
  if (contentIndent < 0) {
    // Auto-detect from the first non-empty line.
    if (s.lineIndent <= parentIndent || atDocumentMarker(s)) {
      contentIndent = parentIndent + 1;
    } else {
      contentIndent = s.lineIndent;
      if (maxLeadingSpaces > contentIndent) {
        fail(s, 'A leading empty line is indented more than the block scalar content');
      }
    }
  }

  let result = '';
  let emptyLines = leadingEmpty;
  let hasContent = false;
  let prevMoreIndented = false;
  for (;;) {
    const c = at(s, s.pos);
    if (c === kEOF && s.pos === s.lineStart) break;
    if (s.lineIndent < contentIndent) {
      // Less indented: an empty line of spaces, or the end of the scalar.
      if (c === kTab) fail(s, 'Tabs cannot be used for indentation');
      if (c !== kEOF && !isBreak(c)) break;
      emptyLines++;
      if (c === kEOF) break;
      consumeBreak(s);
      continue;
    }
    if (atDocumentMarker(s)) break;
    // A content line: everything after contentIndent spaces.
    const textStart = s.lineStart + contentIndent;
    let end = textStart;
    while (end < s.length && !isBreak(at(s, end))) end++;
    const text = StringPrototypeSlice(s.input, textStart, end);
    if (text.length === 0) {
      emptyLines++;
    } else {
      const moreIndented = isWhite(StringPrototypeCharCodeAt(text, 0));
      if (hasContent) {
        if (folded && !moreIndented && !prevMoreIndented) {
          result += emptyLines === 0 ? ' ' : StringPrototypeRepeat('\n', emptyLines);
        } else {
          result += StringPrototypeRepeat('\n', emptyLines + 1);
        }
      } else if (emptyLines > 0) {
        result += StringPrototypeRepeat('\n', emptyLines);
      }
      result += text;
      hasContent = true;
      emptyLines = 0;
      prevMoreIndented = moreIndented;
    }
    s.pos = end;
    if (at(s, s.pos) === kEOF) break;
    consumeBreak(s);
  }
  if (!hasContent) {
    // Only empty lines: keep yields the line breaks, the others nothing.
    return chomping === kKeep ? StringPrototypeRepeat('\n', emptyLines) : '';
  }
  if (chomping === kClip) {
    result += '\n';
  } else if (chomping === kKeep) {
    result += StringPrototypeRepeat('\n', emptyLines + 1);
  }
  return result;
}

// --- Properties, tags, anchors, aliases -------------------------------------

function readAnchorName(s) {
  const start = s.pos;
  for (;;) {
    const c = at(s, s.pos);
    if (isBlank(c) || isFlowIndicator(c)) break;
    s.pos++;
  }
  if (s.pos === start) fail(s, 'Anchor name must not be empty');
  return StringPrototypeSlice(s.input, start, s.pos);
}

function decodeTagEscapes(s, text) {
  let result = '';
  for (let i = 0; i < text.length; i++) {
    const c = StringPrototypeCharCodeAt(text, i);
    if (c !== kPercent) {
      result += StringFromCharCode(c);
      continue;
    }
    const hex = StringPrototypeSlice(text, i + 1, i + 3);
    if (hex.length !== 2 || !isHexDigit(StringPrototypeCharCodeAt(hex, 0)) ||
        !isHexDigit(StringPrototypeCharCodeAt(hex, 1))) {
      fail(s, 'Invalid escape in tag');
    }
    result += StringFromCharCode(NumberParseInt(hex, 16));
    i += 2;
  }
  return result;
}

// Reads a tag property and returns the resolved tag.
function readTag(s, inFlow) {
  const start = s.pos;
  s.pos++;  // !
  if (at(s, s.pos) === kLess) {
    s.pos++;
    const uriStart = s.pos;
    while (at(s, s.pos) !== kGreater) {
      if (isBlank(at(s, s.pos))) fail(s, 'Unterminated verbatim tag', start);
      s.pos++;
    }
    const uri = StringPrototypeSlice(s.input, uriStart, s.pos);
    s.pos++;
    if (uri.length === 0 || uri === '!') fail(s, 'Verbatim tag must not be empty', start);
    if (!isBlank(at(s, s.pos)) && !isFlowIndicator(at(s, s.pos))) {
      fail(s, 'Unexpected characters after tag');
    }
    return uri;
  }
  // The handle is "!", "!!" or "!name!".
  let handleEnd = s.pos;
  if (at(s, handleEnd) === kBang) {
    handleEnd++;
  } else {
    let p = handleEnd;
    while (!isBlank(at(s, p)) && !isFlowIndicator(at(s, p)) && at(s, p) !== kBang) p++;
    if (at(s, p) === kBang) handleEnd = p + 1;
  }
  const handle = StringPrototypeSlice(s.input, start, handleEnd);
  s.pos = handleEnd;
  const suffixStart = s.pos;
  while (!isBlank(at(s, s.pos)) && !isFlowIndicator(at(s, s.pos))) {
    if (at(s, s.pos) === kBang) fail(s, 'Invalid character in tag');
    s.pos++;
  }
  const suffix = decodeTagEscapes(s, StringPrototypeSlice(s.input, suffixStart, s.pos));
  if (!isBlank(at(s, s.pos)) && !(inFlow && isFlowIndicator(at(s, s.pos)))) {
    fail(s, 'Invalid character in tag');
  }
  if (handle === '!') {
    if (suffix.length === 0) return '!';
    const prefix = s.tagHandles.get('!');
    return prefix === undefined ? `!${suffix}` : prefix + suffix;
  }
  if (suffix.length === 0) fail(s, 'Tag suffix must not be empty', start);
  if (handle === '!!') {
    const prefix = s.tagHandles.get('!!');
    return (prefix === undefined ? kCoreTagPrefix : prefix) + suffix;
  }
  const prefix = s.tagHandles.get(handle);
  if (prefix === undefined) fail(s, `Undeclared tag handle "${handle}"`, start);
  return prefix + suffix;
}

// Reads the optional anchor and tag before a node. Returns null when there
// are none.
function readProperties(s, inFlow) {
  let anchor = null;
  let tag = null;
  for (;;) {
    const c = at(s, s.pos);
    if (c === kAmpersand) {
      if (anchor !== null) fail(s, 'A node can have only one anchor');
      s.pos++;
      anchor = readAnchorName(s);
    } else if (c === kBang) {
      if (tag !== null) fail(s, 'A node can have only one tag');
      tag = readTag(s, inFlow);
    } else {
      break;
    }
    skipWhite(s);
    const n = at(s, s.pos);
    // Properties may be on a line of their own; the caller skips on.
    if (isBreak(n) || n === kHash || n === kEOF) break;
  }
  if (anchor === null && tag === null) return null;
  return { __proto__: null, anchor, tag };
}

// Joins two groups of properties of the same node.
function mergeProperties(s, a, b, pos) {
  if (a === null) return b;
  if (b === null) return a;
  if (a.anchor !== null && b.anchor !== null) fail(s, 'A node can have only one anchor', pos);
  if (a.tag !== null && b.tag !== null) fail(s, 'A node can have only one tag', pos);
  return {
    __proto__: null,
    anchor: a.anchor !== null ? a.anchor : b.anchor,
    tag: a.tag !== null ? a.tag : b.tag,
  };
}

function isCoreTag(tag) {
  return tag.length > kCoreTagPrefix.length &&
    StringPrototypeSlice(tag, 0, kCoreTagPrefix.length) === kCoreTagPrefix;
}

// Resolves a scalar, applying its tag, and registers its anchor.
function finishScalar(s, text, kind, props, pos) {
  const tag = props === null ? null : props.tag;
  let value;
  if (tag === null) {
    value = kind === kPlain ? resolvePlain(text) : text;
  } else if (tag === '!' || tag === `${kCoreTagPrefix}str`) {
    value = text;
  } else if (tag === `${kCoreTagPrefix}int`) {
    if (!isIntegerText(text)) fail(s, `"${text}" is not an integer`, pos);
    value = parseNumber(text);
  } else if (tag === `${kCoreTagPrefix}float`) {
    value = parseSpecialFloat(text);
    if (value === undefined) value = parseNumber(text);
    if (typeof value !== 'number') fail(s, `"${text}" is not a float`, pos);
  } else if (tag === `${kCoreTagPrefix}bool`) {
    value = resolvePlain(text);
    if (typeof value !== 'boolean') fail(s, `"${text}" is not a boolean`, pos);
  } else if (tag === `${kCoreTagPrefix}null`) {
    value = resolvePlain(text);
    if (value !== null) fail(s, `"${text}" is not null`, pos);
  } else if (tag === `${kCoreTagPrefix}map` || tag === `${kCoreTagPrefix}seq`) {
    fail(s, `Tag ${tag} cannot be applied to a scalar`, pos);
  } else {
    // A tag outside the core schema: the scalar stays a string.
    value = text;
  }
  if (props !== null && props.anchor !== null) s.anchors.set(props.anchor, value);
  return value;
}

const kScalarTags = ['str', 'int', 'float', 'bool', 'null'];

function checkCollectionTag(s, tag, kind, pos) {
  if (tag === null || !isCoreTag(tag)) return;
  const name = StringPrototypeSlice(tag, kCoreTagPrefix.length);
  const other = kind === kSequence ? 'map' : 'seq';
  if (name === other || ArrayPrototypeIncludes(kScalarTags, name)) {
    fail(s, `Tag ${tag} cannot be applied to a ${kind === kSequence ? 'sequence' : 'mapping'}`, pos);
  }
}

// Applies properties that were read on a line before a flow collection.
function applyCollectionProps(s, value, props, kind, pos) {
  if (props === null) return value;
  checkCollectionTag(s, props.tag, kind, pos);
  if (props.anchor !== null) s.anchors.set(props.anchor, value);
  return value;
}

function readAlias(s) {
  const start = s.pos;
  s.pos++;  // *
  const name = readAnchorName(s);
  if (!s.anchors.has(name)) fail(s, `Unknown anchor "${name}"`, start);
  return s.anchors.get(name);
}

// --- Flow collections ---------------------------------------------------------

// Skips whitespace, breaks and comments inside a flow collection. The lines
// of the collection must be indented more than the enclosing block node.
function skipFlowSeparation(s, parentIndent) {
  const breaks = skipSeparation(s);
  if (at(s, s.pos) === kEOF) fail(s, 'Unterminated flow collection');
  if (atDocumentMarker(s)) fail(s, 'Unexpected document marker inside a flow collection');
  if (breaks > 0 && s.lineIndent <= parentIndent) fail(s, 'Bad indentation in a flow collection');
}

function readFlowSequence(s, parentIndent, props, lineAnchor = null) {
  enter(s);
  const start = s.pos;
  s.pos++;  // [
  const result = [];
  if (lineAnchor !== null) s.anchors.set(lineAnchor, result);
  if (props !== null && props.anchor !== null) s.anchors.set(props.anchor, result);
  for (;;) {
    skipFlowSeparation(s, parentIndent);
    if (at(s, s.pos) === kRightBracket) {
      s.pos++;
      break;
    }
    const entryPos = s.pos;
    let explicit = false;
    if (at(s, s.pos) === kQuestion && isBlank(at(s, s.pos + 1))) {
      explicit = true;
      s.pos++;
      skipFlowSeparation(s, parentIndent);
    }
    const entry = readFlowPair(s, parentIndent, explicit);
    if (entry === kNoNode) fail(s, 'Unexpected character in flow sequence', entryPos);
    ArrayPrototypePush(result, entry);
    skipFlowSeparation(s, parentIndent);
    const c = at(s, s.pos);
    if (c === kComma) {
      s.pos++;
      continue;
    }
    if (c === kRightBracket) {
      s.pos++;
      break;
    }
    fail(s, 'Expected "," or "]" in flow sequence');
  }
  if (props !== null) checkCollectionTag(s, props.tag, kSequence, start);
  s.depth--;
  s.multiline = false;
  return result;
}

// Reads one flow sequence entry: a node, or a single-pair mapping "key: value".
function readFlowPair(s, parentIndent, explicit) {
  const keyPos = s.pos;
  let key = readFlowNode(s, parentIndent);
  const jsonLike = s.lastJsonLike;
  const multiline = s.multiline;
  skipWhite(s);
  if (at(s, s.pos) === kColon) {
    const next = at(s, s.pos + 1);
    if (isBlank(next) || isFlowIndicator(next) || jsonLike) {
      if (multiline && !jsonLike && !explicit) {
        fail(s, 'Implicit mapping keys must be on a single line', keyPos);
      }
      if (key === kNoNode) key = null;
      s.pos++;
      skipFlowSeparation(s, parentIndent);
      let value = kNoNode;
      if (at(s, s.pos) !== kComma && at(s, s.pos) !== kRightBracket) {
        value = readFlowNode(s, parentIndent);
      }
      if (value === kNoNode) value = null;
      const pair = {};
      setProperty(s, pair, keyToString(s, key, keyPos), value, keyPos);
      return pair;
    }
  }
  if (explicit) {
    const pair = {};
    setProperty(s, pair, keyToString(s, key === kNoNode ? null : key, keyPos), null, keyPos);
    return pair;
  }
  return key;
}

function readFlowMapping(s, parentIndent, props, lineAnchor = null) {
  enter(s);
  const start = s.pos;
  s.pos++;  // {
  const result = {};
  if (lineAnchor !== null) s.anchors.set(lineAnchor, result);
  if (props !== null && props.anchor !== null) s.anchors.set(props.anchor, result);
  for (;;) {
    skipFlowSeparation(s, parentIndent);
    if (at(s, s.pos) === kRightBrace) {
      s.pos++;
      break;
    }
    let explicit = false;
    if (at(s, s.pos) === kQuestion && isBlank(at(s, s.pos + 1))) {
      explicit = true;
      s.pos++;
      skipFlowSeparation(s, parentIndent);
    }
    const keyPos = s.pos;
    let key = kNoNode;
    const c0 = at(s, s.pos);
    const c1 = at(s, s.pos + 1);
    if (!(c0 === kColon && (isBlank(c1) || isFlowIndicator(c1)))) {
      key = readFlowNode(s, parentIndent);
    }
    const jsonLike = s.lastJsonLike;
    if (key === kNoNode) {
      if (!explicit && at(s, s.pos) !== kColon) {
        fail(s, 'Unexpected character in flow mapping', keyPos);
      }
      key = null;
    }
    skipFlowSeparation(s, parentIndent);
    let value = null;
    if (at(s, s.pos) === kColon) {
      const next = at(s, s.pos + 1);
      if (!(isBlank(next) || isFlowIndicator(next) || jsonLike)) {
        fail(s, 'Expected whitespace after ":" in flow mapping');
      }
      s.pos++;
      skipFlowSeparation(s, parentIndent);
      const c = at(s, s.pos);
      if (c !== kComma && c !== kRightBrace) {
        value = readFlowNode(s, parentIndent);
        if (value === kNoNode) value = null;
      }
    }
    setProperty(s, result, keyToString(s, key, keyPos), value, keyPos);
    skipFlowSeparation(s, parentIndent);
    const c = at(s, s.pos);
    if (c === kComma) {
      s.pos++;
      continue;
    }
    if (c === kRightBrace) {
      s.pos++;
      break;
    }
    fail(s, 'Expected "," or "}" in flow mapping');
  }
  if (props !== null) checkCollectionTag(s, props.tag, kMapping, start);
  s.depth--;
  s.multiline = false;
  return result;
}

// Reads a node inside a flow collection. Returns kNoNode when the position
// holds no node (an empty entry or value).
function readFlowNode(s, parentIndent) {
  s.lastJsonLike = false;
  const props = readProperties(s, true);
  if (props !== null) skipFlowSeparation(s, parentIndent);
  const pos = s.pos;
  const c = at(s, s.pos);
  switch (c) {
    case kAsterisk:
      if (props !== null) fail(s, 'An alias cannot have properties');
      return readAlias(s);
    case kLeftBracket: {
      const value = readFlowSequence(s, parentIndent, props);
      s.lastJsonLike = true;
      return value;
    }
    case kLeftBrace: {
      const value = readFlowMapping(s, parentIndent, props);
      s.lastJsonLike = true;
      return value;
    }
    case kSingleQuote:
      s.lastJsonLike = true;
      return finishScalar(s, readSingleQuoted(s, parentIndent), kQuoted, props, pos);
    case kDoubleQuote:
      s.lastJsonLike = true;
      return finishScalar(s, readDoubleQuoted(s, parentIndent), kQuoted, props, pos);
    case kPipe: case kGreater:
      fail(s, 'Block scalars are not allowed in flow collections');
  }
  if (canStartPlain(s, c, true)) {
    return finishScalar(s, readPlain(s, parentIndent, true, false), kPlain, props, pos);
  }
  if (props !== null) {
    // Properties with no content: an empty scalar.
    return finishScalar(s, '', kPlain, props, pos);
  }
  return kNoNode;
}

// --- Block collections -----------------------------------------------------

// Whether the current position starts a block sequence entry.
function atSequenceEntry(s) {
  return at(s, s.pos) === kMinus && isBlank(at(s, s.pos + 1));
}

// Whether the current line is indented enough for a node of a collection at
// parentIndent. Only spaces count as indentation.
function indentedEnough(s, parentIndent, allowSameIndentSequence) {
  if (s.lineIndent > parentIndent) return true;
  return allowSameIndentSequence && s.lineIndent === parentIndent && atSequenceEntry(s);
}

// Whether the whitespace before pos on its line holds a tab.
function tabBefore(s, pos) {
  for (let p = pos - 1; p >= s.lineStart && isWhite(at(s, p)); p--) {
    if (at(s, p) === kTab) return true;
  }
  return false;
}

// Composes a node in block context. parentIndent is the indentation of the
// enclosing collection (-1 at the document root). The node may start on the
// current line or on a following line indented more than the parent. With
// allowSameIndentSequence, a block sequence may start at the parent's own
// indentation, as a mapping value. With compact, a block collection may start
// on the current line, after "-", "?" or an explicit ":". Returns kNoNode
// when there is no node.
function composeBlock(s, parentIndent, allowSameIndentSequence, compact) {
  let breaks = skipSeparation(s);
  if (at(s, s.pos) === kEOF || atDocumentMarker(s)) return kNoNode;
  if (breaks > 0 && !indentedEnough(s, parentIndent, allowSameIndentSequence)) {
    return kNoNode;
  }
  // Properties followed by a line break belong to the node on the next line,
  // or to the collection whose first key follows them. Properties on the
  // node's own line belong to that node, or to the first key.
  let lineProps = null;
  let propsStart = s.pos;
  let props = readProperties(s, false);
  while (props !== null) {
    const more = skipSeparation(s);
    if (more === 0 && at(s, s.pos) !== kEOF) break;
    breaks += more;
    lineProps = mergeProperties(s, lineProps, props, propsStart);
    props = null;
    if (at(s, s.pos) === kEOF || atDocumentMarker(s) ||
        !indentedEnough(s, parentIndent, allowSameIndentSequence)) {
      // Properties with no content: an empty scalar node.
      return finishScalar(s, '', kPlain, lineProps, propsStart);
    }
    propsStart = s.pos;
    props = readProperties(s, false);
  }
  const pos = s.pos;
  const startLine = s.line;
  const nodeStart = props === null ? pos : propsStart;
  const indent = nodeStart - s.lineStart;
  // A block collection cannot start on the line of the parent indicator
  // unless the compact form is allowed, and never after a tab.
  const collectionAllowed = breaks > 0 || compact;
  const collectionAfterTab = tabBefore(s, nodeStart);
  const c = at(s, s.pos);
  let value;
  switch (c) {
    case kAsterisk: {
      if (props !== null) fail(s, 'An alias cannot have properties', propsStart);
      value = readAlias(s);
      let p = s.pos;
      while (isWhite(at(s, p))) p++;
      if (at(s, p) === kColon && isBlank(at(s, p + 1))) {
        if (collectionAfterTab) fail(s, 'Tabs cannot be used for indentation', pos);
        if (!collectionAllowed) fail(s, 'A block mapping is not allowed here', pos);
        s.pos = p;
        return readBlockMapping(s, indent, lineProps, value, pos);
      }
      if (lineProps !== null) fail(s, 'An alias cannot have properties', pos);
      return value;
    }
    case kLeftBracket:
      value = readFlowSequence(s, parentIndent, props, lineProps === null ? null : lineProps.anchor);
      break;
    case kLeftBrace:
      value = readFlowMapping(s, parentIndent, props, lineProps === null ? null : lineProps.anchor);
      break;
    case kSingleQuote:
      value = readSingleQuoted(s, parentIndent);
      break;
    case kDoubleQuote:
      value = readDoubleQuoted(s, parentIndent);
      break;
    case kPipe: case kGreater:
      return finishScalar(s, readBlockScalar(s, parentIndent),
                          kQuoted, mergeProperties(s, lineProps, props, pos), pos);
    case kMinus:
      if (isBlank(at(s, s.pos + 1))) {
        if (collectionAfterTab) fail(s, 'Tabs cannot be used for indentation', pos);
        if (!collectionAllowed || props !== null) {
          fail(s, 'A block sequence is not allowed here', pos);
        }
        return readBlockSequence(s, indent, lineProps);
      }
      break;
    case kQuestion:
      if (isBlank(at(s, s.pos + 1))) {
        if (collectionAfterTab) fail(s, 'Tabs cannot be used for indentation', pos);
        if (!collectionAllowed || props !== null) {
          fail(s, 'A block mapping is not allowed here', pos);
        }
        return readBlockMapping(s, indent, lineProps, kNoNode, pos);
      }
      break;
    case kColon:
      if (isBlank(at(s, s.pos + 1))) {
        // An empty key, which takes the properties on its line.
        if (collectionAfterTab) fail(s, 'Tabs cannot be used for indentation', pos);
        if (!collectionAllowed) fail(s, 'A block mapping is not allowed here', pos);
        const key = finishScalar(s, '', kPlain, props, pos);
        return readBlockMapping(s, indent, lineProps, key, pos);
      }
      break;
  }
  if (value === undefined) {
    if (!canStartPlain(s, c, false)) {
      fail(s, `Unexpected character "${StringFromCharCode(c)}"`);
    }
    value = readPlain(s, parentIndent, false, false);
  }
  // A scalar or flow collection on one line followed by ":" is the first
  // key of a block mapping.
  let p = s.pos;
  while (isWhite(at(s, p))) p++;
  if (at(s, p) === kColon && isBlank(at(s, p + 1))) {
    if (s.line !== startLine) fail(s, 'Implicit mapping keys must be on a single line', pos);
    if (collectionAfterTab) fail(s, 'Tabs cannot be used for indentation', pos);
    if (!collectionAllowed) fail(s, 'A block mapping is not allowed here', pos);
    s.pos = p;
    const key = finishNode(s, value, c, props, pos);
    return readBlockMapping(s, indent, lineProps, key, pos);
  }
  if (c === kLeftBracket || c === kLeftBrace) {
    const kind = c === kLeftBracket ? kSequence : kMapping;
    return applyCollectionProps(s, value, lineProps, kind, pos);
  }
  return finishNode(s, value, c, mergeProperties(s, lineProps, props, pos), pos);
}

// Resolves a node that was read as raw scalar text or as a flow collection.
function finishNode(s, value, c, props, pos) {
  if (c === kLeftBracket || c === kLeftBrace) return value;
  const kind = c === kSingleQuote || c === kDoubleQuote ? kQuoted : kPlain;
  return finishScalar(s, value, kind, props, pos);
}

// Reads a block sequence whose entries are at the given indentation.
function readBlockSequence(s, indent, props) {
  enter(s);
  const start = s.pos;
  const result = [];
  if (props !== null && props.anchor !== null) s.anchors.set(props.anchor, result);
  for (;;) {
    // At "-".
    s.pos++;
    const entry = composeBlock(s, indent, false, true);
    ArrayPrototypePush(result, entry === kNoNode ? null : entry);
    if (!nextLineAt(s, indent) || !atSequenceEntry(s)) break;
  }
  if (props !== null) checkCollectionTag(s, props.tag, kSequence, start);
  s.depth--;
  s.multiline = true;
  return result;
}

// After a collection entry, moves to the next line at the collection's
// indentation. Returns false when the collection ends there. Lines indented
// more than the collection are an error.
function nextLineAt(s, indent) {
  const pos = s.pos;
  const breaks = skipSeparation(s);
  if (at(s, s.pos) === kEOF || atDocumentMarker(s)) return false;
  const atLineContent = s.pos === s.lineStart + s.lineIndent;
  if (!atLineContent) {
    if (tabBefore(s, s.pos)) fail(s, 'Tabs cannot be used for indentation');
    if (breaks === 0) fail(s, 'Unexpected content after a collection entry', pos);
  }
  if (s.lineIndent < indent) return false;
  if (s.lineIndent > indent) fail(s, 'Bad indentation');
  return true;
}

// Reads a block mapping at the given indentation. firstKey is the already
// read first key, with the position at its ":", or kNoNode when the mapping
// starts with "?".
function readBlockMapping(s, indent, props, firstKey, start) {
  enter(s);
  const result = {};
  if (props !== null && props.anchor !== null) s.anchors.set(props.anchor, result);
  let key = firstKey;
  let keyPos = start;
  let first = true;
  for (;;) {
    if (!first || key === kNoNode) {
      keyPos = s.pos;
      if (at(s, s.pos) === kQuestion && isBlank(at(s, s.pos + 1))) {
        // Explicit key, possibly a collection on the following lines. Its
        // value, if any, starts with ":" at the mapping indentation.
        s.pos++;
        key = composeBlock(s, indent, true, true);
        if (key === kNoNode) key = null;
        if (!nextLineAt(s, indent)) {
          setProperty(s, result, keyToString(s, key, keyPos), null, keyPos);
          break;
        }
        if (!(at(s, s.pos) === kColon && isBlank(at(s, s.pos + 1)))) {
          setProperty(s, result, keyToString(s, key, keyPos), null, keyPos);
          first = false;
          continue;
        }
        s.pos++;
        let value = composeBlock(s, indent, true, true);
        if (value === kNoNode) value = null;
        setProperty(s, result, keyToString(s, key, keyPos), value, keyPos);
        if (!nextLineAt(s, indent)) break;
        first = false;
        continue;
      }
      key = readImplicitKey(s, indent);
    }
    first = false;
    // At ":".
    s.pos++;
    let value = composeBlock(s, indent, true, false);
    if (value === kNoNode) value = null;
    setProperty(s, result, keyToString(s, key, keyPos), value, keyPos);
    if (!nextLineAt(s, indent)) break;
    if (atSequenceEntry(s)) fail(s, 'Unexpected sequence entry in a mapping');
  }
  if (props !== null) checkCollectionTag(s, props.tag, kMapping, start);
  s.depth--;
  s.multiline = true;
  return result;
}

// Reads an implicit key: a single-line node followed by ":", and leaves the
// position at the ":".
function readImplicitKey(s, indent) {
  const propsStart = s.pos;
  const props = readProperties(s, false);
  const pos = s.pos;
  const c = at(s, s.pos);
  let value;
  switch (c) {
    case kAsterisk:
      if (props !== null) fail(s, 'An alias cannot have properties', propsStart);
      value = readAlias(s);
      skipWhite(s);
      if (at(s, s.pos) !== kColon || !isBlank(at(s, s.pos + 1))) {
        fail(s, 'Expected ":" after mapping key', pos);
      }
      return value;
    case kColon:
      // An empty key.
      if (isBlank(at(s, s.pos + 1))) return finishScalar(s, '', kPlain, props, pos);
      break;
    case kLeftBracket:
      value = readFlowSequence(s, indent, props);
      break;
    case kLeftBrace:
      value = readFlowMapping(s, indent, props);
      break;
    case kSingleQuote:
      value = readSingleQuoted(s, indent);
      break;
    case kDoubleQuote:
      value = readDoubleQuoted(s, indent);
      break;
  }
  if (value === undefined) {
    if (!canStartPlain(s, c, false)) fail(s, 'Expected a mapping key', pos);
    value = readPlain(s, indent, false, true);
  }
  if (s.multiline) fail(s, 'Implicit mapping keys must be on a single line', pos);
  skipWhite(s);
  if (at(s, s.pos) !== kColon || !isBlank(at(s, s.pos + 1))) {
    fail(s, 'Expected ":" after mapping key', pos);
  }
  return finishNode(s, value, c, props, pos);
}

// --- Documents -------------------------------------------------------------

function readDirective(s) {
  const start = s.pos;
  s.pos++;  // %
  const nameStart = s.pos;
  while (!isBlank(at(s, s.pos))) s.pos++;
  const name = StringPrototypeSlice(s.input, nameStart, s.pos);
  const args = [];
  for (;;) {
    skipWhite(s);
    const c = at(s, s.pos);
    if (c === kEOF || isBreak(c) || c === kHash) break;
    const argStart = s.pos;
    while (!isBlank(at(s, s.pos))) s.pos++;
    ArrayPrototypePush(args, StringPrototypeSlice(s.input, argStart, s.pos));
  }
  if (at(s, s.pos) === kHash) skipToEndOfLine(s);
  if (name === 'YAML') {
    if (s.version !== null) fail(s, 'Repeated %YAML directive', start);
    if (args.length !== 1) fail(s, 'The %YAML directive takes one argument', start);
    const version = args[0];
    let valid = version.length >= 3 &&
      isDigit(StringPrototypeCharCodeAt(version, 0)) &&
      StringPrototypeCharCodeAt(version, 1) === kDot;
    for (let i = 2; valid && i < version.length; i++) {
      valid = isDigit(StringPrototypeCharCodeAt(version, i));
    }
    if (!valid) fail(s, `Invalid YAML version "${version}"`, start);
    if (StringPrototypeCharCodeAt(version, 0) !== 0x31) {
      fail(s, `Unsupported YAML version "${version}"`, start);
    }
    s.version = version;
  } else if (name === 'TAG') {
    if (args.length !== 2) fail(s, 'The %TAG directive takes two arguments', start);
    const { 0: handle, 1: prefix } = args;
    if (StringPrototypeCharCodeAt(handle, 0) !== kBang ||
        (handle.length > 1 && StringPrototypeCharCodeAt(handle, handle.length - 1) !== kBang)) {
      fail(s, `Invalid tag handle "${handle}"`, start);
    }
    if (s.tagHandles.has(handle)) fail(s, `Repeated tag handle "${handle}"`, start);
    s.tagHandles.set(handle, decodeTagEscapes(s, prefix));
  }
  // Other directives are reserved and ignored.
}

// Reads one document. Returns kNoNode at the end of the stream.
function readDocument(s) {
  s.anchors.clear();
  s.tagHandles.clear();
  s.version = null;
  let hasDirectives = false;
  for (;;) {
    skipSeparation(s);
    if (at(s, s.pos) === kPercent && s.pos === s.lineStart) {
      readDirective(s);
      hasDirectives = true;
      continue;
    }
    break;
  }
  let explicitStart = false;
  if (at(s, s.pos) === kEOF) {
    if (hasDirectives) fail(s, 'Directives must be followed by "---"');
    return kNoNode;
  }
  if (atDocumentMarker(s)) {
    if (at(s, s.pos) === kDot) {
      if (hasDirectives) fail(s, 'Directives must be followed by "---"');
      skipDocumentEnd(s);
      return kNoNode;
    }
    s.pos += 3;
    explicitStart = true;
  } else if (hasDirectives) {
    fail(s, 'Directives must be followed by "---"');
  }
  let root = composeBlock(s, -1, true, !explicitStart);
  if (root === kNoNode) root = null;
  // After the root node: whitespace, comments, then a document marker or EOF.
  skipSeparation(s);
  const c = at(s, s.pos);
  if (c !== kEOF && !atDocumentMarker(s)) {
    fail(s, 'Unexpected content after the document root');
  }
  s.explicitEnd = false;
  if (c === kDot) {
    skipDocumentEnd(s);
    s.explicitEnd = true;
  }
  return root;
}

// Consumes "..." markers. Only a comment may follow one on its line.
function skipDocumentEnd(s) {
  for (;;) {
    s.pos += 3;
    skipWhite(s);
    const c = at(s, s.pos);
    if (c === kHash) skipToEndOfLine(s);
    else if (c !== kEOF && !isBreak(c)) fail(s, 'Unexpected content after "..."');
    skipSeparation(s);
    if (!(atDocumentMarker(s) && at(s, s.pos) === kDot)) break;
  }
}

function skipBOM(s) {
  if (at(s, s.pos) === kBOM) {
    s.pos++;
    s.lineStart = s.pos;
    while (at(s, s.pos) === kSpace) {
      s.pos++;
      s.lineIndent++;
    }
  }
}

// YAML allows tab, line feed, carriage return, the printable characters of
// the BMP except the C1 controls other than NEL, and the supplementary planes,
// which are surrogate pairs here.
function checkPrintable(s) {
  for (let i = 0; i < s.length; i++) {
    const c = StringPrototypeCharCodeAt(s.input, i);
    if (c >= kSpace && c < 0x7F) continue;
    if (c === kTab || c === kLF || c === kCR || c === 0x85) continue;
    if ((c >= 0xA0 && c < 0xD800) || (c >= 0xE000 && c < 0xFFFE)) continue;
    if (c >= 0xD800 && c < 0xDC00) {
      const next = at(s, i + 1);
      if (next >= 0xDC00 && next < 0xE000) {
        i++;
        continue;
      }
    }
    fail(s, `Character U+${NumberPrototypeToString(c, 16)} is not allowed in YAML`, i);
  }
}

// Parses a YAML stream and returns its documents as an array.
function parseDocuments(text) {
  const s = new State(text);
  checkPrintable(s);
  const documents = [];
  while (at(s, s.pos) === kSpace) {
    s.pos++;
    s.lineIndent++;
  }
  skipBOM(s);
  for (;;) {
    const document = readDocument(s);
    if (document === kNoNode) {
      if (at(s, s.pos) === kEOF) break;
      // A "..." before any document.
      continue;
    }
    ArrayPrototypePush(documents, document);
    if (at(s, s.pos) === kEOF) break;
    // A byte order mark may also precede each document.
    if (s.explicitEnd) skipBOM(s);
  }
  return documents;
}

module.exports = {
  parseDocuments,
};
