'use strict';

// Runtime behavior of compiled pattern constraints. Schema construction
// already validates that the source is a valid RegExp; these tests cover what
// happens when that compiled pattern is applied to real input across many
// calls, edge strings, and reused Schema instances.

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('repeated validation is stable (no lastIndex leak)', () => {
  // If the internal RegExp were ever compiled with the `g` flag, its
  // `lastIndex` would carry over across calls and validation would flip
  // between pass and fail on the same input. Pin the stateless contract.
  const schema = new Schema({ type: 'string', pattern: '^[a-z]+$' });
  for (let i = 0; i < 5; i++) {
    assert.strictEqual(schema.validate('hello').valid, true);
  }
  for (let i = 0; i < 5; i++) {
    assert.strictEqual(schema.validate('Hello').valid, false);
  }
});

test('same Schema instance reused across different values', () => {
  const schema = new Schema({ type: 'string', pattern: '\\d+' });
  const inputs = [
    ['abc123', true],
    ['no-digits', false],
    ['42', true],
    ['', false],
  ];
  for (const [value, expected] of inputs) {
    assert.strictEqual(schema.validate(value).valid, expected);
  }
});

test('pattern anchors are not multiline by default', () => {
  // Patterns are compiled with no flags. `^`/`$` match start/end of input,
  // not start/end of line. Pin that behavior so adding the `m` flag becomes
  // a conscious decision rather than a silent change.
  const schema = new Schema({ type: 'string', pattern: '^abc$' });
  assert.strictEqual(schema.validate('abc').valid, true);
  assert.strictEqual(schema.validate('abc\n').valid, false);
  assert.strictEqual(schema.validate('\nabc').valid, false);
});

test('pattern dot does not match newline', () => {
  // Default RegExp semantics: `.` does not match '\n'. Pin it so adding the
  // `s` (dotAll) flag would be a conscious change.
  const schema = new Schema({ type: 'string', pattern: '^a.b$' });
  assert.strictEqual(schema.validate('aXb').valid, true);
  assert.strictEqual(schema.validate('a\nb').valid, false);
});

test('pattern error message includes the original source', () => {
  const schema = new Schema({ type: 'string', pattern: '^[A-Z]+$' });
  const result = schema.validate('lower');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'PATTERN_MISMATCH');
  assert.ok(result.errors[0].message.includes('^[A-Z]+$'));
});

test('pattern with backreference at runtime', () => {
  const schema = new Schema({ type: 'string', pattern: '^(.)\\1$' });
  assert.strictEqual(schema.validate('aa').valid, true);
  assert.strictEqual(schema.validate('ab').valid, false);
});

test('pattern with lookahead at runtime', () => {
  const schema = new Schema({
    type: 'string',
    pattern: '^(?=.*\\d)(?=.*[a-z]).+$',
  });
  assert.strictEqual(schema.validate('abc1').valid, true);
  assert.strictEqual(schema.validate('abc').valid, false);
  assert.strictEqual(schema.validate('123').valid, false);
});

test('pattern against empty string', () => {
  const schema = new Schema({ type: 'string', pattern: '.+' });
  const result = schema.validate('');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'PATTERN_MISMATCH');
});

test('pattern compiled once is reused for every item in an array', () => {
  const schema = new Schema({
    type: 'array',
    items: { type: 'string', pattern: '^[a-z]+$' },
  });
  const result = schema.validate(['good', 'BAD', 'also', '123']);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 2);
  const paths = result.errors.map((e) => e.path);
  assert.ok(paths.includes('[1]'));
  assert.ok(paths.includes('[3]'));
});
