'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

/**
 * Key a method on your object with this symbol and you can get special
 * formatting for that value! See ShellStringText, ShellStringUnquoted, or
 * shellStringSemicolon for examples.
 * @ignore
 */
const formatSymbol = Symbol('format');
/**
 * This symbol is for implementing advanced behaviors like the need for extra
 * carets in Windows shell strings that use pipes. If present, it's called in
 * an earlier phase than formatSymbol, and is passed a mutable context that can
 * be read during the format phase to influence formatting.
 * @ignore
 */
const preformatSymbol = Symbol('preformat');

// When minimum Node version becomes 6, replace calls to sticky with /.../y and
// inline execFrom.
let stickySupported = true;
try {
  new RegExp('', 'y');
} catch (e) {
  stickySupported = false;
}
const sticky = stickySupported ? source => new RegExp(source, 'y') : source => new RegExp(`^(?:${source})`);
const execFrom = stickySupported ? (re, haystack, index) => (re.lastIndex = index, re.exec(haystack)) : (re, haystack, index) => re.exec(haystack.substr(index));

function quoteForCmd(text, forceQuote) {
  let caretDepth = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : 0;
  // See the below blog post for an explanation of this function and
  // quoteForWin32:
  // https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/
  if (!text.length) {
    return '""';
  }
  if (/[\n\r]/.test(text)) {
    throw new Error("Line breaks can't be quoted on Windows");
  }
  const caretEscape = /["%]/.test(text);
  text = quoteForWin32(text, forceQuote || !caretEscape && /[&()<>^|]/.test(text));
  if (caretEscape) {
    // See Win32Context for explanation of what caretDepth is for.
    do {
      text = text.replace(/[\t "%&()<>^|]/g, '^$&');
    } while (caretDepth--);
  }
  return text;
}
const quoteForWin32 = (text, forceQuote) => forceQuote || /[\t "]/.test(text) ? `"${text.replace(/\\+(?=$|")/g, '$&$&').replace(/"/g, '\\"')}"` : text;
const cmdMetaChars = /[\t\n\r "%&()<>^|]/;
class Win32Context {
  constructor() {
    this.currentScope = newScope(null);
    this.scopesByObject = new Map();
    this.argDetectState = 0;
    this.argSet = new Set();
  }
  read(text) {
    // When cmd.exe executes a batch file, or pipes to or from one, it spawns a
    // second copy of itself to run the inner command. This necessitates
    // doubling up on carets so that escaped characters survive both cmd.exe
    // invocations. See:
    // https://stackoverflow.com/questions/8192318/why-does-delayed-expansion-fail-when-inside-a-piped-block-of-code#8194279
    // https://ss64.com/nt/syntax-redirection.html
    //
    // Parentheses can create an additional subshell, requiring additional
    // escaping... it's a mess.
    //
    // So here's what we do about it: we read all unquoted text in a shell
    // string and put it through this tiny parser that looks for pipes,
    // sequence operators (&, &&, ||), redirects, and parentheses. This can't
    // be part of the main Puka parsing, because it can be affected by
    // `unquoted(...)` values provided at evaluation time.
    //
    // Then, after associating each thing that needs to be quoted with a scope
    // (via `mark()`), and identifying whether or not it's an argument to a
    // command, we can determine the depth of caret escaping required in each
    // scope and pass it (via `Formatter::quote()`) to `quoteForCmd()`.
    //
    // See also `ShellStringText`, which holds the logic for the previous
    // paragraph.
    const length = text.length;
    for (let pos = 0, match; pos < length;) {
      while (match = execFrom(reUnimportant, text, pos)) {
        if (match[2] == null) {
          // (not whitespace)
          if (match[1] != null) {
            // (>&)
            this.argDetectState = this.argDetectState === 0 ? ADS_FLAG_INITIAL_REDIRECT : 0;
          } else if (this.argDetectState !== ADS_FLAG_ARGS) {
            this.argDetectState |= ADS_FLAG_WORD;
          }
        } else {
          // (whitespace)
          if ((this.argDetectState & ADS_FLAG_WORD) !== 0) {
            this.argDetectState = ADS_FLAG_ARGS & ~this.argDetectState >> 1;
          }
        }
        pos += match[0].length;
      }
      if (pos >= length) break;
      if (match = execFrom(reSeqOp, text, pos)) {
        this.seq();
        pos += match[0].length;
      } else {
        const char = text.charCodeAt(pos);
        if (char === CARET) {
          pos += 2;
        } else if (char === QUOTE) {
          // If you were foolish enough to leave a dangling quotation mark in
          // an unquoted span... you're likely to have bigger problems than
          // incorrect escaping. So we just do the simplest thing of looking for
          // the end quote only in this piece of text.
          pos += execFrom(reNotQuote, text, pos + 1)[0].length + 2;
        } else {
          if (char === OPEN_PAREN) {
            this.enterScope();
          } else if (char === CLOSE_PAREN) {
            this.exitScope();
          } else if (char === PIPE) {
            this.pipe();
          } else {
            // (char === '<' or '>')
            this.argDetectState = this.argDetectState === 0 ? ADS_FLAG_INITIAL_REDIRECT : 0;
          }
          pos++;
        }
      }
    }
  }
  enterScope() {
    this.currentScope = newScope(this.currentScope);
    this.argDetectState = 0;
  }
  exitScope() {
    this.currentScope = this.currentScope.parent || (this.currentScope.parent = newScope(null));
    this.argDetectState = ADS_FLAG_ARGS;
  }
  seq() {
    // | binds tighter than sequence operators, so the latter create new sibling
    // scopes for future |s to mutate.
    this.currentScope = newScope(this.currentScope.parent);
    this.argDetectState = 0;
  }
  pipe() {
    this.currentScope.depthDelta = 1;
    this.argDetectState = 0;
  }
  mark(obj) {
    this.scopesByObject.set(obj, this.currentScope);
    if (this.argDetectState === ADS_FLAG_ARGS) {
      this.argSet.add(obj);
    } else {
      this.argDetectState |= ADS_FLAG_WORD;
    }
  }
  at(obj) {
    const scope = this.scopesByObject.get(obj);
    return {
      depth: getDepth(scope),
      isArgument: this.argSet.has(obj),
      isNative: scope.isNative
    };
  }
}
// These flags span the Win32Context's argument detection state machine. WORD
// is set when the context is inside a word that is not an argument (meaning it
// is either the first word in the command, or it is the object of a redirect).
// ARGS is set when the context has reached the arguments of a command.
// INITIAL_REDIRECT tracks the edge case when a redirect occurs before the
// first word of the command (if this flag is set, reaching the end of a word
// should take the state machine back to 0 instead of setting ADS_FLAG_ARGS).
const ADS_FLAG_WORD = 0x1;
const ADS_FLAG_ARGS = 0x2;
const ADS_FLAG_INITIAL_REDIRECT = 0x4;
const getDepth = scope => scope === null ? 0 : scope.depth !== -1 ? scope.depth : scope.depth = getDepth(scope.parent) + scope.depthDelta;
const newScope = parent => ({
  parent,
  depthDelta: 0,
  depth: -1,
  isNative: false
});
const CARET = '^'.charCodeAt();
const QUOTE = '"'.charCodeAt();
const OPEN_PAREN = '('.charCodeAt();
const CLOSE_PAREN = ')'.charCodeAt();
const PIPE = '|'.charCodeAt();
const reNotQuote = sticky('[^"]*');
const reSeqOp = sticky('&&?|\\|\\|');
const reUnimportant = sticky('(\\d*>&)|[^\\s"$&()<>^|]+|(\\s+)');

const quoteForSh = (text, forceQuote) => text.length ? forceQuote || shMetaChars.test(text) ? `'${text.replace(/'/g, "'\\''")}'`.replace(/^(?:'')+(?!$)/, '').replace(/\\'''/g, "\\'") : text : "''";
const shMetaChars = /[\t\n\r "#$&'()*;<>?\\`|~]/;

/**
 * To get a Formatter, call `Formatter.for`.
 *
 * To create a new Formatter, pass an object to `Formatter.declare`.
 *
 * To set the global default Formatter, assign to `Formatter.default`.
 *
 * @class
 * @property {Formatter} default - The Formatter to be used when no platform
 * is provided—for example, when creating strings with `sh`.
 * @ignore
 */
function Formatter() {}
Object.assign(Formatter,
/** @lends Formatter */
{
  /**
   * Gets a Formatter that has been declared for the provided platform, or
   * the base `'sh'` formatter if there is no Formatter specific to this
   * platform, or the Formatter for the current platform if no specific platform
   * is provided.
   */
  for(platform) {
    return platform == null ? Formatter.default || (Formatter.default = Formatter.for(process.platform)) : Formatter._registry.get(platform) || Formatter._registry.get('sh');
  },
  /**
   * Creates a new Formatter or mutates the properties on an existing
   * Formatter. The `platform` key on the provided properties object determines
   * when the Formatter is retrieved.
   */
  declare(props) {
    const platform = props && props.platform || 'sh';
    const existingFormatter = Formatter._registry.get(platform);
    const formatter = Object.assign(existingFormatter || new Formatter(), props);
    formatter.emptyString === void 0 && (formatter.emptyString = formatter.quote('', true));
    existingFormatter || Formatter._registry.set(formatter.platform, formatter);
  },
  _registry: new Map(),
  prototype: {
    platform: 'sh',
    quote: quoteForSh,
    metaChars: shMetaChars,
    hasExtraMetaChars: false,
    statementSeparator: ';',
    createContext() {
      return defaultContext;
    }
  }
});
const defaultContext = {
  at() {}
};
Formatter.declare();
Formatter.declare({
  platform: 'win32',
  quote(text, forceQuote, opts) {
    const caretDepth = opts ? (opts.depth || 0) + (opts.isArgument && !opts.isNative ? 1 : 0) : 0;
    return quoteForCmd(text, forceQuote, caretDepth);
  },
  metaChars: cmdMetaChars,
  hasExtraMetaChars: true,
  statementSeparator: '&',
  createContext(root) {
    const context = new this.Context();
    root[preformatSymbol](context);
    return context;
  },
  Context: Win32Context
});

const isObject = any => any === Object(any);
function memoize(f) {
  const cache = new WeakMap();
  return arg => {
    let result = cache.get(arg);
    if (result === void 0) {
      result = f(arg);
      cache.set(arg, result);
    }
    return result;
  };
}

/**
 * Represents a contiguous span of text that may or must be quoted. The contents
 * may already contain quoted segments, which will always be quoted. If unquoted
 * segments also require quoting, the entire span will be quoted together.
 * @ignore
 */
class ShellStringText {
  constructor(contents, untested) {
    this.contents = contents;
    this.untested = untested;
  }
  [formatSymbol](formatter, context) {
    const unformattedContents = this.contents;
    const length = unformattedContents.length;
    const contents = new Array(length);
    for (let i = 0; i < length; i++) {
      const c = unformattedContents[i];
      contents[i] = isObject(c) && formatSymbol in c ? c[formatSymbol](formatter) : c;
    }
    for (let unquoted = true, i = 0; i < length; i++) {
      const content = contents[i];
      if (content === null) {
        unquoted = !unquoted;
      } else {
        if (unquoted && (formatter.hasExtraMetaChars || this.untested && this.untested.has(i)) && formatter.metaChars.test(content)) {
          return formatter.quote(contents.join(''), false, context.at(this));
        }
      }
    }
    const parts = [];
    for (let quoted = null, i = 0; i < length; i++) {
      const content = contents[i];
      if (content === null) {
        quoted = quoted ? (parts.push(formatter.quote(quoted.join(''), true, context.at(this))), null) : [];
      } else {
        (quoted || parts).push(content);
      }
    }
    const result = parts.join('');
    return result.length ? result : formatter.emptyString;
  }
  [preformatSymbol](context) {
    context.mark(this);
  }
}

/**
 * Represents a contiguous span of text that will not be quoted.
 * @ignore
 */
class ShellStringUnquoted {
  constructor(value) {
    this.value = value;
  }
  [formatSymbol]() {
    return this.value;
  }
  [preformatSymbol](context) {
    context.read(this.value);
  }
}

/**
 * Represents a semicolon... or an ampersand, on Windows.
 * @ignore
 */
const shellStringSemicolon = {
  [formatSymbol](formatter) {
    return formatter.statementSeparator;
  },
  [preformatSymbol](context) {
    context.seq();
  }
};

const PLACEHOLDER = {};
const parse = memoize(templateSpans => {
  // These are the token types our DSL can recognize. Their values won't escape
  // this function.
  const TOKEN_TEXT = 0;
  const TOKEN_QUOTE = 1;
  const TOKEN_SEMI = 2;
  const TOKEN_UNQUOTED = 3;
  const TOKEN_SPACE = 4;
  const TOKEN_REDIRECT = 5;
  const result = [];
  let placeholderCount = 0;
  let prefix = null;
  let onlyPrefixOnce = false;
  let contents = [];
  let quote = 0;
  const lastSpan = templateSpans.length - 1;
  for (let spanIndex = 0; spanIndex <= lastSpan; spanIndex++) {
    const templateSpan = templateSpans[spanIndex];
    const posEnd = templateSpan.length;
    let tokenStart = 0;
    if (spanIndex) {
      placeholderCount++;
      contents.push(PLACEHOLDER);
    }
    // For each span, we first do a recognizing pass in which we use regular
    // expressions to identify the positions of tokens in the text, and then
    // a second pass that actually splits the text into the minimum number of
    // substrings necessary.
    const recognized = []; // [type1, index1, type2, index2...]
    let firstWordBreak = -1;
    let lastWordBreak = -1;
    {
      let pos = 0,
          match;
      while (pos < posEnd) {
        if (quote) {
          if (match = execFrom(quote === CHAR_SQUO ? reQuotation1 : reQuotation2, templateSpan, pos)) {
            recognized.push(TOKEN_TEXT, pos);
            pos += match[0].length;
          }
          if (pos < posEnd) {
            recognized.push(TOKEN_QUOTE, pos++);
            quote = 0;
          }
        } else {
          if (match = execFrom(reRedirectOrSpace, templateSpan, pos)) {
            firstWordBreak < 0 && (firstWordBreak = pos);
            lastWordBreak = pos;
            recognized.push(match[1] ? TOKEN_REDIRECT : TOKEN_SPACE, pos);
            pos += match[0].length;
          }
          if (match = execFrom(reText, templateSpan, pos)) {
            const setBreaks = match[1] != null;
            setBreaks && firstWordBreak < 0 && (firstWordBreak = pos);
            recognized.push(setBreaks ? TOKEN_UNQUOTED : TOKEN_TEXT, pos);
            pos += match[0].length;
            setBreaks && (lastWordBreak = pos);
          }
          const char = templateSpan.charCodeAt(pos);
          if (char === CHAR_SEMI) {
            firstWordBreak < 0 && (firstWordBreak = pos);
            recognized.push(TOKEN_SEMI, pos++);
            lastWordBreak = pos;
          } else if (char === CHAR_SQUO || char === CHAR_DQUO) {
            recognized.push(TOKEN_QUOTE, pos++);
            quote = char;
          }
        }
      }
    }
    // Word breaks are only important if they separate words with placeholders,
    // so we can ignore the first/last break if this is the first/last span.
    spanIndex === 0 && (firstWordBreak = -1);
    spanIndex === lastSpan && (lastWordBreak = posEnd);
    // Here begins the second pass mentioned above. This loop runs one more
    // iteration than there are tokens in recognized, because it handles tokens
    // on a one-iteration delay; hence the i <= iEnd instead of i < iEnd.
    const iEnd = recognized.length;
    for (let i = 0, type = -1; i <= iEnd; i += 2) {
      let typeNext = -1,
          pos;
      if (i === iEnd) {
        pos = posEnd;
      } else {
        typeNext = recognized[i];
        pos = recognized[i + 1];
        // If the next token is space or redirect, but there's another word
        // break in this span, then we can handle that token the same way we
        // would handle unquoted text because it isn't being attached to a
        // placeholder.
        typeNext >= TOKEN_SPACE && pos !== lastWordBreak && (typeNext = TOKEN_UNQUOTED);
      }
      const breakHere = pos === firstWordBreak || pos === lastWordBreak;
      if (pos && (breakHere || typeNext !== type)) {
        let value = type === TOKEN_QUOTE ? null : type === TOKEN_SEMI ? shellStringSemicolon : templateSpan.substring(tokenStart, pos);
        if (type >= TOKEN_SEMI) {
          // This branch handles semicolons, unquoted text, spaces, and
          // redirects. shellStringSemicolon is already a formatSymbol object;
          // the rest need to be wrapped.
          type === TOKEN_SEMI || (value = new ShellStringUnquoted(value));
          // We don't need to check placeholderCount here like we do below;
          // that's only relevant during the first word break of the span, and
          // because this iteration of the loop is processing the token that
          // was checked for breaks in the previous iteration, it will have
          // already been handled. For the same reason, prefix is guaranteed to
          // be null.
          if (contents.length) {
            result.push(new ShellStringText(contents, null));
            contents = [];
          }
          // Only spaces and redirects become prefixes, but not if they've been
          // rewritten to unquoted above.
          if (type >= TOKEN_SPACE) {
            prefix = value;
            onlyPrefixOnce = type === TOKEN_SPACE;
          } else {
            result.push(value);
          }
        } else {
          contents.push(value);
        }
        tokenStart = pos;
      }
      if (breakHere) {
        if (placeholderCount) {
          result.push({
            contents,
            placeholderCount,
            prefix,
            onlyPrefixOnce
          });
        } else {
          // There's no prefix to handle in this branch; a prefix prior to this
          // span would mean placeholderCount > 0, and a prefix in this span
          // can't be created because spaces and redirects get rewritten to
          // unquoted before the last word break.
          contents.length && result.push(new ShellStringText(contents, null));
        }
        placeholderCount = 0;
        prefix = null;
        onlyPrefixOnce = false;
        contents = [];
      }
      type = typeNext;
    }
  }
  if (quote) {
    throw new SyntaxError(`String is missing a ${String.fromCharCode(quote)} character`);
  }
  return result;
});
const CHAR_SEMI = ';'.charCodeAt();
const CHAR_SQUO = "'".charCodeAt();
const CHAR_DQUO = '"'.charCodeAt();
const reQuotation1 = sticky("[^']+");
const reQuotation2 = sticky('[^"]+');
const reText = sticky('[^\\s"#$&\'();<>\\\\`|]+|([#$&()\\\\`|]+)');
const reRedirectOrSpace = sticky('(\\s*\\d*[<>]+\\s*)|\\s+');

class BitSet {
  constructor() {
    this.vector = new Int32Array(1);
  }
  has(n) {
    return (this.vector[n >>> 5] & 1 << n) !== 0;
  }
  add(n) {
    const i = n >>> 5,
          requiredLength = i + 1;
    let vector = this.vector,
        _vector = vector,
        length = _vector.length;
    if (requiredLength > length) {
      while (requiredLength > (length *= 2));
      const oldValues = vector;
      vector = new Int32Array(length);
      vector.set(oldValues);
      this.vector = vector;
    }
    vector[i] |= 1 << n;
  }
}

function evaluate(template, values) {
  values = values.map(toStringishArray);
  const children = [];
  let valuesStart = 0;
  for (let i = 0, iMax = template.length; i < iMax; i++) {
    const word = template[i];
    if (formatSymbol in word) {
      children.push(word);
      continue;
    }
    const contents = word.contents,
          placeholderCount = word.placeholderCount,
          prefix = word.prefix,
          onlyPrefixOnce = word.onlyPrefixOnce;
    const kMax = contents.length;
    const valuesEnd = valuesStart + placeholderCount;
    const tuples = cartesianProduct(values, valuesStart, valuesEnd);
    valuesStart = valuesEnd;
    for (let j = 0, jMax = tuples.length; j < jMax; j++) {
      const needSpace = j > 0;
      const tuple = tuples[j];
      (needSpace || prefix) && children.push(needSpace && (onlyPrefixOnce || !prefix) ? unquotedSpace : prefix);
      let interpolatedContents = [];
      let untested = null;
      let quoting = false;
      let tupleIndex = 0;
      for (let k = 0; k < kMax; k++) {
        const content = contents[k];
        if (content === PLACEHOLDER) {
          const value = tuple[tupleIndex++];
          if (quoting) {
            interpolatedContents.push(value);
          } else {
            if (isObject(value) && formatSymbol in value) {
              if (interpolatedContents.length) {
                children.push(new ShellStringText(interpolatedContents, untested));
                interpolatedContents = [];
                untested = null;
              }
              children.push(value);
            } else {
              (untested || (untested = new BitSet())).add(interpolatedContents.length);
              interpolatedContents.push(value);
            }
          }
        } else {
          interpolatedContents.push(content);
          content === null && (quoting = !quoting);
        }
      }
      if (interpolatedContents.length) {
        children.push(new ShellStringText(interpolatedContents, untested));
      }
    }
  }
  return children;
}
const primToStringish = value => value == null ? '' + value : value;
function toStringishArray(value) {
  let array;
  switch (true) {
    default:
      if (isObject(value)) {
        if (Array.isArray(value)) {
          array = value;
          break;
        }
        if (Symbol.iterator in value) {
          array = Array.from(value);
          break;
        }
      }
      array = [value];
  }
  return array.map(primToStringish);
}
function cartesianProduct(arrs, start, end) {
  const size = end - start;
  let resultLength = 1;
  for (let i = start; i < end; i++) {
    resultLength *= arrs[i].length;
  }
  if (resultLength > 1e6) {
    throw new RangeError("Far too many elements to interpolate");
  }
  const result = new Array(resultLength);
  const indices = new Array(size).fill(0);
  for (let i = 0; i < resultLength; i++) {
    const value = result[i] = new Array(size);
    for (let j = 0; j < size; j++) {
      value[j] = arrs[j + start][indices[j]];
    }
    for (let j = size - 1; j >= 0; j--) {
      if (++indices[j] < arrs[j + start].length) break;
      indices[j] = 0;
    }
  }
  return result;
}
const unquotedSpace = new ShellStringUnquoted(' ');

/**
 * A ShellString represents a shell command after it has been interpolated, but
 * before it has been formatted for a particular platform. ShellStrings are
 * useful if you want to prepare a command for a different platform than the
 * current one, for instance.
 *
 * To create a ShellString, use `ShellString.sh` the same way you would use
 * top-level `sh`.
 */
class ShellString {
  /** @hideconstructor */
  constructor(children) {
    this.children = children;
  }
  /**
   * `ShellString.sh` is a template tag just like `sh`; the only difference is
   * that this function returns a ShellString which has not yet been formatted
   * into a String.
   * @returns {ShellString}
   * @function sh
   * @static
   * @memberof ShellString
   */
  static sh(templateSpans) {
    for (var _len = arguments.length, values = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
      values[_key - 1] = arguments[_key];
    }
    return new ShellString(evaluate(parse(templateSpans), values));
  }
  /**
   * A method to format a ShellString into a regular String formatted for a
   * particular platform.
   *
   * @param {String} [platform] a value that `process.platform` might take:
   * `'win32'`, `'linux'`, etc.; determines how the string is to be formatted.
   * When omitted, effectively the same as `process.platform`.
   * @returns {String}
   */
  toString(platform) {
    return this[formatSymbol](Formatter.for(platform));
  }
  [formatSymbol](formatter) {
    let context = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : formatter.createContext(this);
    return this.children.map(child => child[formatSymbol](formatter, context)).join('');
  }
  [preformatSymbol](context) {
    const children = this.children;
    for (let i = 0, iMax = children.length; i < iMax; i++) {
      const child = children[i];
      if (preformatSymbol in child) {
        child[preformatSymbol](context);
      }
    }
  }
}

/**
 * A Windows-specific version of {@link quoteForShell}.
 * @param {String} text to be quoted
 * @param {Boolean} [forceQuote] whether to always add quotes even if the string
 * is already safe. Defaults to `false`.
 */

/**
 * A Unix-specific version of {@link quoteForShell}.
 * @param {String} text to be quoted
 * @param {Boolean} [forceQuote] whether to always add quotes even if the string
 * is already safe. Defaults to `false`.
 */

/**
 * Quotes a string for injecting into a shell command.
 *
 * This function is exposed for some hypothetical case when the `sh` DSL simply
 * won't do; `sh` is expected to be the more convenient option almost always.
 * Compare:
 *
 * ```javascript
 * console.log('cmd' + args.map(a => ' ' + quoteForShell(a)).join(''));
 * console.log(sh`cmd ${args}`); // same as above
 *
 * console.log('cmd' + args.map(a => ' ' + quoteForShell(a, true)).join(''));
 * console.log(sh`cmd "${args}"`); // same as above
 * ```
 *
 * Additionally, on Windows, `sh` checks the entire command string for pipes,
 * which subtly change how arguments need to be quoted. If your commands may
 * involve pipes, you are strongly encouraged to use `sh` and not try to roll
 * your own with `quoteForShell`.
 *
 * @param {String} text to be quoted
 * @param {Boolean} [forceQuote] whether to always add quotes even if the string
 * is already safe. Defaults to `false`.
 * @param {String} [platform] a value that `process.platform` might take:
 * `'win32'`, `'linux'`, etc.; determines how the string is to be formatted.
 * When omitted, effectively the same as `process.platform`.
 *
 * @returns {String} a string that is safe for the current (or specified)
 * platform.
 */
function quoteForShell(text, forceQuote, platform) {
  return Formatter.for(platform).quote(text, forceQuote);
}

/**
 * A string template tag for safely constructing cross-platform shell commands.
 *
 * An `sh` template is not actually treated as a literal string to be
 * interpolated; instead, it is a tiny DSL designed to make working with shell
 * strings safe, simple, and straightforward. To get started quickly, see the
 * examples below. {@link #the-sh-dsl More detailed documentation} is available
 * further down.
 *
 * @name sh
 * @example
 * const title = '"this" & "that"';
 * sh`script --title=${title}`; // => "script '--title=\"this\" & \"that\"'"
 * // Note: these examples show results for non-Windows platforms.
 * // On Windows, the above would instead be
 * // 'script ^^^"--title=\\^^^"this\\^^^" ^^^& \\^^^"that\\^^^"^^^"'.
 *
 * const names = ['file1', 'file 2'];
 * sh`rimraf ${names}.txt`; // => "rimraf file1.txt 'file 2.txt'"
 *
 * const cmd1 = ['cat', 'file 1.txt', 'file 2.txt'];
 * const cmd2 = ['use-input', '-abc'];
 * sh`${cmd1}|${cmd2}`; // => "cat 'file 1.txt' 'file 2.txt'|use-input -abc"
 *
 * @returns {String} - a string formatted for the platform Node is currently
 * running on.
 */
const sh = function () {
  return ShellString.sh.apply(ShellString, arguments).toString();
};

/**
 * This function permits raw strings to be interpolated into a `sh` template.
 *
 * **IMPORTANT**: If you're using Puka due to security concerns, make sure you
 * don't pass any untrusted content to `unquoted`. This may be obvious, but
 * stray punctuation in an `unquoted` section can compromise the safety of the
 * entire shell command.
 *
 * @param value - any value (it will be treated as a string)
 *
 * @example
 * const both = true;
 * sh`foo ${unquoted(both ? '&&' : '||')} bar`; // => 'foo && bar'
 */
const unquoted = value => new ShellStringUnquoted(value);

exports.Formatter = Formatter;
exports.ShellString = ShellString;
exports.ShellStringText = ShellStringText;
exports.ShellStringUnquoted = ShellStringUnquoted;
exports.quoteForCmd = quoteForCmd;
exports.quoteForSh = quoteForSh;
exports.quoteForShell = quoteForShell;
exports.sh = sh;
exports.shellStringSemicolon = shellStringSemicolon;
exports.formatSymbol = formatSymbol;
exports.preformatSymbol = preformatSymbol;
exports.unquoted = unquoted;
