'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('minItems - passes at boundary', () => {
  const schema = new Schema({ type: 'array', minItems: 2 });
  assert.strictEqual(schema.validate([1, 2]).valid, true);
});

test('minItems - fails below', () => {
  const schema = new Schema({ type: 'array', minItems: 2 });
  const result = schema.validate([1]);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'ARRAY_TOO_SHORT');
});

test('maxItems - passes at boundary', () => {
  const schema = new Schema({ type: 'array', maxItems: 3 });
  assert.strictEqual(schema.validate([1, 2, 3]).valid, true);
});

test('maxItems - fails above', () => {
  const schema = new Schema({ type: 'array', maxItems: 3 });
  const result = schema.validate([1, 2, 3, 4]);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'ARRAY_TOO_LONG');
});

test('items - validates each item', () => {
  const schema = new Schema({
    type: 'array',
    items: { type: 'string' },
  });
  assert.strictEqual(schema.validate(['a', 'b', 'c']).valid, true);
});

test('items - reports errors with index path', () => {
  const schema = new Schema({
    type: 'array',
    items: { type: 'string' },
  });
  const result = schema.validate(['a', 42, 'c']);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 1);
  assert.strictEqual(result.errors[0].path, '[1]');
  assert.strictEqual(result.errors[0].code, 'INVALID_TYPE');
});

test('items - multiple errors', () => {
  const schema = new Schema({
    type: 'array',
    items: { type: 'number', minimum: 0 },
  });
  const result = schema.validate([1, -1, 'x', 3]);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 2);
});

test('minItems === maxItems pins exact length', () => {
  const schema = new Schema({ type: 'array', minItems: 2, maxItems: 2 });
  assert.strictEqual(schema.validate([1, 2]).valid, true);

  const tooShort = schema.validate([1]);
  assert.strictEqual(tooShort.valid, false);
  assert.strictEqual(tooShort.errors[0].code, 'ARRAY_TOO_SHORT');

  const tooLong = schema.validate([1, 2, 3]);
  assert.strictEqual(tooLong.valid, false);
  assert.strictEqual(tooLong.errors[0].code, 'ARRAY_TOO_LONG');
});

test('empty array is valid', () => {
  const schema = new Schema({ type: 'array', items: { type: 'string' } });
  assert.strictEqual(schema.validate([]).valid, true);
});

test('nested array schemas', () => {
  const schema = new Schema({
    type: 'array',
    items: {
      type: 'array',
      items: { type: 'number' },
    },
  });
  assert.strictEqual(schema.validate([[1, 2], [3, 4]]).valid, true);
  const result = schema.validate([[1, 'x']]);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, '[0][1]');
});
