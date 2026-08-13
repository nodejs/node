// Flags: --expose-internals

import '../common/index.mjs';
import assert from 'node:assert';
import { test } from 'node:test';
import tagFilter from 'internal/test_runner/tag_filter';

const { evaluateTagFilters, parseTagFilterExpression } = tagFilter;

function tagSet(...tags) {
  return new Set(tags);
}

function matches(expr, ...tags) {
  return evaluateTagFilters([parseTagFilterExpression(expr, 'expr')], tagSet(...tags));
}

test('single identifier', () => {
  assert.strictEqual(matches('db', 'db'), true);
  assert.strictEqual(matches('db', 'other'), false);
  assert.strictEqual(matches('db'), false);
});

test('case-insensitive match', () => {
  // The evaluator expects an already-lowercased tag set, so 'DB' in the set
  // must not match 'db' in the filter.
  assert.strictEqual(matches('DB', 'db'), true);
  assert.strictEqual(matches('db', 'DB'), false);
});

test('and / && have higher precedence than or / ||', () => {
  for (const expr of ['a or b and c', 'a || b && c']) {
    assert.strictEqual(matches(expr, 'a'), true);
    assert.strictEqual(matches(expr, 'b'), false);
    assert.strictEqual(matches(expr, 'c'), false);
    assert.strictEqual(matches(expr, 'a', 'b'), true);
    assert.strictEqual(matches(expr, 'a', 'c'), true);
    assert.strictEqual(matches(expr, 'b', 'c'), true);
    assert.strictEqual(matches(expr, 'a', 'b', 'c'), true);
  }
});

test('not has higher precedence than and', () => {
  for (const expr of ['not a and b', '!a && b']) {
    assert.strictEqual(matches(expr, 'b'), true);
    assert.strictEqual(matches(expr, 'a', 'b'), false);
  }
});

test('not has higher precedence than or', () => {
  for (const expr of ['not a or b', '!a || b']) {
    assert.strictEqual(matches(expr), true);
    assert.strictEqual(matches(expr, 'a'), false);
    assert.strictEqual(matches(expr, 'b'), true);
    assert.strictEqual(matches(expr, 'a', 'b'), true);
  }
});

test('parentheses override precedence', () => {
  for (const expr of ['(a or b) and c', '(a || b) && c']) {
    assert.strictEqual(matches(expr, 'a'), false);
    assert.strictEqual(matches(expr, 'c'), false);
    assert.strictEqual(matches(expr, 'a', 'c'), true);
    assert.strictEqual(matches(expr, 'b', 'c'), true);
  }
});

test('double negation', () => {
  for (const expr of ['not not a', '!!a']) {
    assert.strictEqual(matches(expr, 'a'), true);
    assert.strictEqual(matches(expr), false);
  }
});

test('mixing word and punctuation forms in one expression', () => {
  for (const expr of ['unit && not flaky', 'unit and !flaky']) {
    assert.strictEqual(matches(expr, 'unit'), true);
    assert.strictEqual(matches(expr, 'unit', 'flaky'), false);
  }
});

test('mixed forms inside parentheses, word and punct equivalence', () => {
  // Each pair below is the same expression in word form and punctuation form.
  for (const expr of ['a and (b || not c)', 'a && (b or not c)']) {
    assert.strictEqual(matches(expr, 'a'), true);
    assert.strictEqual(matches(expr, 'a', 'b'), true);
    assert.strictEqual(matches(expr, 'a', 'c'), false);
    assert.strictEqual(matches(expr, 'a', 'b', 'c'), true);
    assert.strictEqual(matches(expr, 'b'), false);
  }

  for (const expr of ['(a || b) && !c', '(a or b) and not c']) {
    assert.strictEqual(matches(expr, 'a'), true);
    assert.strictEqual(matches(expr, 'b'), true);
    assert.strictEqual(matches(expr, 'a', 'c'), false);
    assert.strictEqual(matches(expr, 'c'), false);
  }

  for (const expr of ['not (a or b) and c', '!(a || b) && c']) {
    assert.strictEqual(matches(expr, 'c'), true);
    assert.strictEqual(matches(expr, 'a', 'c'), false);
    assert.strictEqual(matches(expr, 'b', 'c'), false);
  }

  for (const expr of ['!(a && b) || c', 'not (a and b) or c']) {
    assert.strictEqual(matches(expr, 'a'), true);
    assert.strictEqual(matches(expr, 'a', 'b'), false);
    assert.strictEqual(matches(expr, 'a', 'b', 'c'), true);
  }
});

test('nested parentheses and wildcards mixed with word forms', () => {
  for (const expr of [
    '(db* or unit) and not (slow || flaky)',
    '(db* || unit) && !(slow or flaky)',
  ]) {
    assert.strictEqual(matches(expr, 'db'), true);
    assert.strictEqual(matches(expr, 'db:postgres'), true);
    assert.strictEqual(matches(expr, 'unit'), true);
    assert.strictEqual(matches(expr, 'unit', 'slow'), false);
    assert.strictEqual(matches(expr, 'db', 'flaky'), false);
    assert.strictEqual(matches(expr, 'mongo'), false);
  }
});

test("'notslow' is one identifier, 'not slow' is two tokens", () => {
  assert.strictEqual(matches('notslow', 'notslow'), true);
  assert.strictEqual(matches('notslow', 'slow'), false);
  assert.strictEqual(matches('not slow'), true);
  assert.strictEqual(matches('not slow', 'slow'), false);
});

test('punctuation does not need whitespace separation', () => {
  assert.strictEqual(matches('!a', 'b'), true);
  assert.strictEqual(matches('!a', 'a'), false);
  assert.strictEqual(matches('a&&b', 'a', 'b'), true);
  assert.strictEqual(matches('a||b', 'a'), true);
});

test('wildcard expands across allowed alphabet', () => {
  assert.strictEqual(matches('db*', 'db'), true);
  assert.strictEqual(matches('db*', 'db1'), true);
  assert.strictEqual(matches('db*', 'db:postgres'), true);
  assert.strictEqual(matches('db*', 'mongo'), false);
});

test('bare * matches any tagged test', () => {
  assert.strictEqual(matches('*', 'anything'), true);
  assert.strictEqual(matches('*', 'a', 'b'), true);
  assert.strictEqual(matches('*'), false);
});

test('wildcard in middle and at start', () => {
  assert.strictEqual(matches('*db', 'mydb'), true);
  assert.strictEqual(matches('*db', 'db'), true);
  assert.strictEqual(matches('*db', 'dbmy'), false);
  assert.strictEqual(matches('a*z', 'abz'), true);
  assert.strictEqual(matches('a*z', 'az'), true);
  assert.strictEqual(matches('a*z', 'abc'), false);
});

test('regex metacharacters in tags are escaped', () => {
  // The literal `.` in the filter must match a literal dot only, not any
  // arbitrary character (which would be the unescaped regex meaning).
  assert.strictEqual(matches('a.b*', 'a.b'), true);
  assert.strictEqual(matches('a.b*', 'axb'), false);
});

test('wildcard case-folding matches literal-tag case-folding for non-ASCII', () => {
  // Authoring a tag 'İ' (U+0130) goes through String.prototype.toLowerCase
  // and stores 'i̇' (two code points). Wildcard matching must use the same
  // fold so authors see consistent behavior across literal and wildcard
  // filters.
  const folded = 'İ'.toLowerCase();
  assert.strictEqual(matches('İ', folded), true);
  assert.strictEqual(matches('İ*', folded), true);
  assert.strictEqual(matches('İ*', `${folded}foo`), true);
  assert.strictEqual(matches('*İ', `foo${folded}`), true);
});

test('untagged test fails any include expression', () => {
  for (const expr of ['a', 'a or b', 'a || b', 'a and b', 'a && b', '*']) {
    assert.strictEqual(matches(expr), false);
  }
});

test("'not X' against an untagged test is true", () => {
  for (const expr of ['not a', '!a']) {
    assert.strictEqual(matches(expr), true);
  }
});

test('multiple filters AND together', () => {
  const f1 = parseTagFilterExpression('a', 'expr');
  const f2 = parseTagFilterExpression('not b', 'expr');
  assert.strictEqual(evaluateTagFilters([f1, f2], tagSet('a')), true);
  assert.strictEqual(evaluateTagFilters([f1, f2], tagSet('a', 'b')), false);
  assert.strictEqual(evaluateTagFilters([f1, f2], tagSet('b')), false);
  assert.strictEqual(evaluateTagFilters([f1, f2], tagSet()), false);
});

test('empty filter list always passes', () => {
  assert.strictEqual(evaluateTagFilters([], tagSet()), true);
  assert.strictEqual(evaluateTagFilters([], tagSet('a')), true);
});

// --- Malformed expressions -------------------------------------------------

function expectInvalid(expr) {
  assert.throws(
    () => parseTagFilterExpression(expr, 'expr'),
    { code: 'ERR_INVALID_ARG_VALUE' },
    `expected '${expr}' to be invalid`,
  );
}

test('rejects empty / whitespace-only expressions', () => {
  for (const expr of ['', '   ', '\t\n']) {
    expectInvalid(expr);
  }
});

test('rejects unbalanced parentheses', () => {
  for (const expr of ['(', '((', '(a', 'a)', '((a)', '(a))']) {
    expectInvalid(expr);
  }
});

test('rejects dangling operators', () => {
  for (const expr of ['not', 'a and', 'a or', '&& a', '|| a', 'a &&', 'a ||']) {
    expectInvalid(expr);
  }
});

test('rejects two adjacent identifiers', () => {
  expectInvalid('foo bar');
});

test('rejects single & and single |', () => {
  for (const expr of ['a & b', 'a | b']) {
    expectInvalid(expr);
  }
});

test('rejects empty parentheses', () => {
  expectInvalid('()');
});

test('rejects non-string expressions', () => {
  for (const expr of [42, null, undefined, true, [], {}, Symbol('s')]) {
    assert.throws(
      () => parseTagFilterExpression(expr, 'expr'),
      { code: 'ERR_INVALID_ARG_TYPE' },
      `expected ${String(expr)} to be rejected`,
    );
  }
});
