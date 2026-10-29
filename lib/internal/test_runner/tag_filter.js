'use strict';
const {
  ArrayPrototypePush,
  ObjectFreeze,
  RegExp,
  RegExpPrototypeExec,
  SafeSet,
  StringPrototypeCharCodeAt,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const {
  emitExperimentalWarning,
} = require('internal/util');
const {
  validateArray,
  validateString,
} = require('internal/validators');

const kCharAmpersand = 0x26;
const kCharBackslash = 0x5C;
const kCharCarriageReturn = 0x0D;
const kCharCaret = 0x5E;
const kCharDollar = 0x24;
const kCharDot = 0x2E;
const kCharExclaim = 0x21;
const kCharFormFeed = 0x0C;
const kCharLBrace = 0x7B;
const kCharLBracket = 0x5B;
const kCharLineFeed = 0x0A;
const kCharLParen = 0x28;
const kCharPipe = 0x7C;
const kCharPlus = 0x2B;
const kCharQuestion = 0x3F;
const kCharRBrace = 0x7D;
const kCharRBracket = 0x5D;
const kCharRParen = 0x29;
const kCharSpace = 0x20;
const kCharStar = 0x2A;
const kCharTab = 0x09;
const kCharVerticalTab = 0x0B;

// Single `&` and `|` are allowed here: standalone they have no operator
// meaning, only the doubled forms `&&` and `||` do.
const kTagForbiddenChars = new SafeSet([
  kCharTab, kCharLineFeed, kCharVerticalTab, kCharFormFeed, kCharCarriageReturn, kCharSpace,
  kCharAmpersand, kCharExclaim, kCharLParen, kCharPipe, kCharRParen, kCharStar,
]);

const kReservedWords = new SafeSet(['and', 'or', 'not']);

/**
 * Frozen, shared empty array used as the canonical "no tags" value for
 * tests that don't declare their own and have no tagged ancestors.
 * Sharing one frozen instance avoids per-test allocation.
 * @type {readonly string[]}
 */
const kEmptyTagArray = ObjectFreeze([]);

function isWhitespace(ch) {
  return ch === kCharTab || ch === kCharLineFeed || ch === kCharVerticalTab ||
         ch === kCharFormFeed || ch === kCharCarriageReturn || ch === kCharSpace;
}

// `*` is intentionally not a break char: it's allowed inside an identifier
// and becomes a wildcard.
function isIdentBreak(ch) {
  return isWhitespace(ch) ||
         ch === kCharAmpersand || ch === kCharPipe ||
         ch === kCharExclaim || ch === kCharLParen || ch === kCharRParen;
}

/**
 * Validate and canonicalize a `tags` option value supplied to `test()`,
 * `it()`, `suite()`, or `describe()`.
 *
 * Each tag must be a non-empty string that contains no whitespace, no
 * operator characters (`& | ! ( ) *`), and is not the reserved word
 * `'and'`, `'or'`, or `'not'` in any casing. Tags are canonicalized to
 * lowercase and deduplicated on the lowercased form, preserving the
 * first-seen declaration order.
 *
 * Calling this with a non-empty array also emits the one-shot
 * `ExperimentalWarning` for the test-tags feature.
 * @param {unknown} tags - The value supplied by the user for `options.tags`.
 * @param {string} optName - Option name to embed in thrown error messages
 *   (typically `'options.tags'`).
 * @returns {string[]} Lowercased, deduplicated tags in declaration order.
 *   Returns an empty array when `tags` is `[]`.
 * @throws {ERR_INVALID_ARG_TYPE} If `tags` is not an array, or any element
 *   is not a string.
 * @throws {ERR_INVALID_ARG_VALUE} If any element is empty, contains a
 *   forbidden character, or is a reserved word in any casing.
 */
function validateAndCanonicalizeTagValues(tags, optName) {
  validateArray(tags, optName);

  if (tags.length === 0) {
    return kEmptyTagArray;
  }

  emitExperimentalWarning('Test tags');

  const seen = new SafeSet();
  const result = [];

  for (let i = 0; i < tags.length; i++) {
    const tag = tags[i];
    const elemName = `${optName}[${i}]`;

    validateString(tag, elemName);

    if (tag.length === 0) {
      throw new ERR_INVALID_ARG_VALUE(elemName, tag, 'must not be empty');
    }

    for (let j = 0; j < tag.length; j++) {
      const ch = StringPrototypeCharCodeAt(tag, j);
      if (kTagForbiddenChars.has(ch)) {
        throw new ERR_INVALID_ARG_VALUE(
          elemName,
          tag,
          'contains a forbidden character. Tags must not contain whitespace, ' +
          'operator characters (&, |, !, (, ), *), or be the reserved words ' +
          '\'and\', \'or\', or \'not\'',
        );
      }
    }

    const lower = StringPrototypeToLowerCase(tag);

    if (kReservedWords.has(lower)) {
      throw new ERR_INVALID_ARG_VALUE(
        elemName,
        tag,
        'must not be the reserved word \'and\', \'or\', or \'not\'',
      );
    }

    if (!seen.has(lower)) {
      seen.add(lower);
      ArrayPrototypePush(result, lower);
    }
  }

  return ObjectFreeze(result);
}

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------

// Token types
const T_AND = 'AND';
const T_OR = 'OR';
const T_NOT = 'NOT';
const T_LPAREN = 'LPAREN';
const T_RPAREN = 'RPAREN';
const T_IDENT = 'IDENT';
const T_EOF = 'EOF';

function lex(expr, name) {
  const tokens = [];
  let i = 0;
  const len = expr.length;

  while (i < len) {
    const ch = StringPrototypeCharCodeAt(expr, i);

    if (isWhitespace(ch)) {
      i++;
      continue;
    }

    if (ch === kCharAmpersand) {
      if (i + 1 < len && StringPrototypeCharCodeAt(expr, i + 1) === kCharAmpersand) {
        ArrayPrototypePush(tokens, { __proto__: null, type: T_AND, pos: i });
        i += 2;
      } else {
        throw new ERR_INVALID_ARG_VALUE(
          name, expr,
          `unexpected '&' at position ${i}; use '&&' or 'and'`,
        );
      }
      continue;
    }

    if (ch === kCharPipe) {
      if (i + 1 < len && StringPrototypeCharCodeAt(expr, i + 1) === kCharPipe) {
        ArrayPrototypePush(tokens, { __proto__: null, type: T_OR, pos: i });
        i += 2;
      } else {
        throw new ERR_INVALID_ARG_VALUE(
          name, expr,
          `unexpected '|' at position ${i}; use '||' or 'or'`,
        );
      }
      continue;
    }

    if (ch === kCharExclaim) {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_NOT, pos: i });
      i++;
      continue;
    }

    if (ch === kCharLParen) {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_LPAREN, pos: i });
      i++;
      continue;
    }

    if (ch === kCharRParen) {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_RPAREN, pos: i });
      i++;
      continue;
    }

    const start = i;
    while (i < len && !isIdentBreak(StringPrototypeCharCodeAt(expr, i))) {
      i++;
    }
    const ident = StringPrototypeSlice(expr, start, i);
    const lower = StringPrototypeToLowerCase(ident);

    if (lower === 'and') {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_AND, pos: start });
    } else if (lower === 'or') {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_OR, pos: start });
    } else if (lower === 'not') {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_NOT, pos: start });
    } else {
      ArrayPrototypePush(tokens, { __proto__: null, type: T_IDENT, value: ident, pos: start });
    }
  }

  ArrayPrototypePush(tokens, { __proto__: null, type: T_EOF, pos: len });
  return tokens;
}

// ---------------------------------------------------------------------------
// Regex escaping for wildcard tags
// ---------------------------------------------------------------------------

// `*` is the only metacharacter intentionally not escaped: the caller
// handles it as a wildcard.
function escapeForRegExp(str) {
  let result = '';
  for (let i = 0; i < str.length; i++) {
    const ch = StringPrototypeCharCodeAt(str, i);
    if (
      ch === kCharLBracket || ch === kCharRBracket ||
      ch === kCharLParen || ch === kCharRParen ||
      ch === kCharLBrace || ch === kCharRBrace ||
      ch === kCharCaret || ch === kCharDollar ||
      ch === kCharDot || ch === kCharPlus ||
      ch === kCharQuestion || ch === kCharBackslash ||
      ch === kCharPipe
    ) {
      result += '\\';
    }
    result += StringPrototypeSlice(str, i, i + 1);
  }
  return result;
}

function buildWildcardRegex(ident) {
  // Lowercase the source first and drop the /i flag, so wildcard matching
  // uses the same casing semantics as literal matching (which lowercases the
  // tag value at validation time). Using /i would diverge for non-ASCII
  // characters whose String.prototype.toLowerCase() output differs from
  // RegExp /i case folding (e.g. Turkish I-with-dot U+0130).
  const parts = StringPrototypeSplit(StringPrototypeToLowerCase(ident), '*');
  let pattern = '^';
  for (let i = 0; i < parts.length; i++) {
    if (i > 0) {
      pattern += '.*';
    }
    pattern += escapeForRegExp(parts[i]);
  }
  pattern += '$';
  return new RegExp(pattern);
}

function makeTagNode(ident) {
  for (let i = 0; i < ident.length; i++) {
    if (StringPrototypeCharCodeAt(ident, i) === kCharStar) {
      return { __proto__: null, type: 'wildcard', re: buildWildcardRegex(ident) };
    }
  }
  return { __proto__: null, type: 'tag', value: StringPrototypeToLowerCase(ident) };
}

// ---------------------------------------------------------------------------
// Parser - recursive descent
// Precedence: not > and > or  (standard Boolean)
// ---------------------------------------------------------------------------

/**
 * Parse a tag filter expression string into an AST suitable for
 * `evaluateTagFilters`.
 *
 * Supported syntax:
 *   - Identifiers: any non-whitespace, non-operator characters; case-
 *     insensitive when matched. A `*` inside an identifier becomes a
 *     wildcard that matches any sequence of characters; a bare `*`
 *     matches any non-empty tag.
 *   - Boolean operators: `and`/`&&`, `or`/`||`, `not`/`!`, with the
 *     standard precedence `not > and > or` and left-associative binary
 *     operators. Word forms must be whitespace-separated; punctuation
 *     forms do not.
 *   - Parentheses for grouping.
 * @param {string} expr - The expression to parse.
 * @param {string} name - Option name to embed in thrown error messages
 *   (typically `'--experimental-test-tag-filter[<i>]'` or
 *   `'options.testTagFilters[<i>]'`).
 * @returns {object} An opaque AST node. The shape is internal; pass it
 *   only to `evaluateTagFilters`.
 * @throws {ERR_INVALID_ARG_TYPE} If `expr` is not a string.
 * @throws {ERR_INVALID_ARG_VALUE} If `expr` is empty, contains only
 *   whitespace, or has a syntax error (unbalanced parentheses, dangling
 *   operator, adjacent identifiers, etc.). The error message includes
 *   the offending position.
 */
function parseTagFilterExpression(expr, name) {
  validateString(expr, name);

  if (StringPrototypeTrim(expr).length === 0) {
    throw new ERR_INVALID_ARG_VALUE(name, expr, 'must not be empty');
  }

  const tokens = lex(expr, name);
  let pos = 0;

  function peek() {
    return tokens[pos];
  }

  function consume() {
    return tokens[pos++];
  }

  function parseExpr() {
    return parseOr();
  }

  function parseOr() {
    let left = parseAnd();
    while (peek().type === T_OR) {
      consume();
      const right = parseAnd();
      left = { __proto__: null, type: 'or', left, right };
    }
    return left;
  }

  function parseAnd() {
    let left = parseNot();
    while (peek().type === T_AND) {
      consume();
      const right = parseNot();
      left = { __proto__: null, type: 'and', left, right };
    }
    return left;
  }

  function parseNot() {
    if (peek().type === T_NOT) {
      const { pos: notPos } = consume();
      if (peek().type === T_EOF) {
        throw new ERR_INVALID_ARG_VALUE(
          name, expr,
          `expected operand after 'not' at position ${notPos}`,
        );
      }
      const operand = parseNot();
      return { __proto__: null, type: 'not', operand };
    }

    if (peek().type === T_LPAREN) {
      const { pos: lPos } = consume();
      if (peek().type === T_EOF) {
        throw new ERR_INVALID_ARG_VALUE(
          name, expr,
          `unmatched '(' at position ${lPos}`,
        );
      }
      const inner = parseExpr();
      if (peek().type !== T_RPAREN) {
        throw new ERR_INVALID_ARG_VALUE(
          name, expr,
          `expected ')' at position ${peek().pos}`,
        );
      }
      consume();
      return inner;
    }

    if (peek().type === T_IDENT) {
      const { value } = consume();
      return makeTagNode(value);
    }

    const tok = peek();
    throw new ERR_INVALID_ARG_VALUE(
      name, expr,
      `unexpected token '${tok.type}' at position ${tok.pos}`,
    );
  }

  const ast = parseExpr();

  if (peek().type !== T_EOF) {
    const tok = peek();
    throw new ERR_INVALID_ARG_VALUE(
      name, expr,
      `unexpected token at position ${tok.pos}`,
    );
  }

  return ast;
}

// ---------------------------------------------------------------------------
// Evaluator
// ---------------------------------------------------------------------------

/**
 * Evaluate a tag filter AST against a set of test tags.
 *
 * Untagged tests have an empty SafeSet. The semantics are:
 *   - An include expression (tag/wildcard/and/or) evaluates false for empty set.
 *   - 'not X' evaluates true when X is false (so untagged tests pass 'not X').
 * @param {object} ast - AST node from parseTagFilterExpression.
 * @param {SafeSet} tags - Lowercased canonical tags for the test.
 * @returns {boolean}
 */
function evaluateTagFilter(ast, tags) {
  const { type } = ast;

  if (type === 'or') {
    return evaluateTagFilter(ast.left, tags) || evaluateTagFilter(ast.right, tags);
  }

  if (type === 'and') {
    return evaluateTagFilter(ast.left, tags) && evaluateTagFilter(ast.right, tags);
  }

  if (type === 'not') {
    return !evaluateTagFilter(ast.operand, tags);
  }

  if (type === 'tag') {
    return tags.has(ast.value);
  }

  // type === 'wildcard'
  let matched = false;
  tags.forEach((tag) => {
    if (!matched && RegExpPrototypeExec(ast.re, tag) !== null) {
      matched = true;
    }
  });
  return matched;
}

/**
 * Evaluate one or more tag filter ASTs against a test's tag set.
 *
 * Multiple ASTs compose by AND: a test passes only when every AST
 * evaluates true for it. An empty `asts` array always passes.
 *
 * Untagged tests (an empty `tags` set) fail any include expression
 * (`tag`, `wildcard`, `and`, `or`) and pass `not X`, so excluding tags
 * does not accidentally remove untagged tests.
 * @param {object[]} asts - ASTs returned by `parseTagFilterExpression`.
 * @param {SafeSet<string>} tags - The test's lowercased canonical tag
 *   set (use `Set` in non-internal contexts; both work).
 * @returns {boolean} `true` if the test should run.
 */
function evaluateTagFilters(asts, tags) {
  for (let i = 0; i < asts.length; i++) {
    if (!evaluateTagFilter(asts[i], tags)) {
      return false;
    }
  }
  return true;
}

module.exports = {
  evaluateTagFilters,
  kEmptyTagArray,
  parseTagFilterExpression,
  validateAndCanonicalizeTagValues,
};
