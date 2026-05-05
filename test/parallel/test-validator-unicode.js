'use strict';

// String length constraints in node:validator count UTF-16 code units, like
// JavaScript's `String.prototype.length`. These tests pin that behavior so
// surrogate-pair handling doesn't silently change.

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('surrogate pair counts as two code units for minLength', () => {
  const schema = new Schema({ type: 'string', minLength: 2 });
  // '🎉' is a surrogate pair: .length === 2
  assert.strictEqual(schema.validate('🎉').valid, true);
});

test('surrogate pair counts as two code units for maxLength', () => {
  const schema = new Schema({ type: 'string', maxLength: 1 });
  const result = schema.validate('🎉');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'STRING_TOO_LONG');
});

test('BMP code point counts as one code unit', () => {
  const schema = new Schema({ type: 'string', minLength: 1, maxLength: 1 });
  assert.strictEqual(schema.validate('é').valid, true);
  assert.strictEqual(schema.validate('漢').valid, true);
});

test('combining characters each count separately', () => {
  // 'é' composed as 'e' + U+0301 is length 2 in UTF-16.
  const composed = 'e\u0301';
  const schema = new Schema({ type: 'string', minLength: 2, maxLength: 2 });
  assert.strictEqual(schema.validate(composed).valid, true);
});

test('pattern with unicode literal matches', () => {
  const schema = new Schema({ type: 'string', pattern: '^漢字$' });
  assert.strictEqual(schema.validate('漢字').valid, true);
  assert.strictEqual(schema.validate('hello').valid, false);
});

test('pattern with character class and unicode', () => {
  const schema = new Schema({ type: 'string', pattern: '^[a-zé]+$' });
  assert.strictEqual(schema.validate('café').valid, true);
});

test('enum with unicode strings', () => {
  const schema = new Schema({
    type: 'string',
    enum: ['日本', '한국', 'Ελλάδα'],
  });
  assert.strictEqual(schema.validate('日本').valid, true);
  assert.strictEqual(schema.validate('China').valid, false);
});

test('property name with non-ASCII characters', () => {
  const schema = new Schema({
    type: 'object',
    required: ['名前'],
    properties: {
      名前: { type: 'string' },
    },
  });
  assert.strictEqual(schema.validate({ 名前: 'Alice' }).valid, true);
  const missing = schema.validate({});
  assert.strictEqual(missing.valid, false);
  assert.strictEqual(missing.errors[0].path, '名前');
  assert.strictEqual(missing.errors[0].code, 'MISSING_REQUIRED');
});

test('empty string is valid with minLength 0', () => {
  const schema = new Schema({ type: 'string', minLength: 0, maxLength: 10 });
  assert.strictEqual(schema.validate('').valid, true);
});

test('pattern does not anchor implicitly', () => {
  // The validator compiles `new RegExp(source)` — no anchors.
  // Pattern "cat" should match anywhere in the string.
  const schema = new Schema({ type: 'string', pattern: 'cat' });
  assert.strictEqual(schema.validate('concatenate').valid, true);
  assert.strictEqual(schema.validate('dog').valid, false);
});
