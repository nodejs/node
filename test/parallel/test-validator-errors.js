'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema, codes } = require('node:validator');

test('INVALID_TYPE produced for wrong type', () => {
  const schema = new Schema({ type: 'string' });
  const result = schema.validate(42);
  assert.strictEqual(result.errors[0].code, codes.INVALID_TYPE);
});

test('MISSING_REQUIRED produced for missing property', () => {
  const schema = new Schema({
    type: 'object',
    required: ['name'],
    properties: { name: { type: 'string' } },
  });
  const result = schema.validate({});
  assert.strictEqual(result.errors[0].code, codes.MISSING_REQUIRED);
});

test('STRING_TOO_SHORT produced', () => {
  const schema = new Schema({ type: 'string', minLength: 5 });
  const result = schema.validate('hi');
  assert.strictEqual(result.errors[0].code, codes.STRING_TOO_SHORT);
});

test('STRING_TOO_LONG produced', () => {
  const schema = new Schema({ type: 'string', maxLength: 2 });
  const result = schema.validate('hello');
  assert.strictEqual(result.errors[0].code, codes.STRING_TOO_LONG);
});

test('PATTERN_MISMATCH produced', () => {
  const schema = new Schema({ type: 'string', pattern: '^[0-9]+$' });
  const result = schema.validate('abc');
  assert.strictEqual(result.errors[0].code, codes.PATTERN_MISMATCH);
});

test('ENUM_MISMATCH produced', () => {
  const schema = new Schema({ type: 'string', enum: ['a', 'b'] });
  const result = schema.validate('c');
  assert.strictEqual(result.errors[0].code, codes.ENUM_MISMATCH);
});

test('NUMBER_TOO_SMALL produced', () => {
  const schema = new Schema({ type: 'number', minimum: 10 });
  const result = schema.validate(5);
  assert.strictEqual(result.errors[0].code, codes.NUMBER_TOO_SMALL);
});

test('NUMBER_TOO_LARGE produced', () => {
  const schema = new Schema({ type: 'number', maximum: 10 });
  const result = schema.validate(15);
  assert.strictEqual(result.errors[0].code, codes.NUMBER_TOO_LARGE);
});

test('NUMBER_NOT_MULTIPLE produced', () => {
  const schema = new Schema({ type: 'number', multipleOf: 3 });
  const result = schema.validate(7);
  assert.strictEqual(result.errors[0].code, codes.NUMBER_NOT_MULTIPLE);
});

test('NOT_INTEGER produced', () => {
  const schema = new Schema({ type: 'integer' });
  const result = schema.validate(3.14);
  assert.strictEqual(result.errors[0].code, codes.NOT_INTEGER);
});

test('ARRAY_TOO_SHORT produced', () => {
  const schema = new Schema({ type: 'array', minItems: 2 });
  const result = schema.validate([1]);
  assert.strictEqual(result.errors[0].code, codes.ARRAY_TOO_SHORT);
});

test('ARRAY_TOO_LONG produced', () => {
  const schema = new Schema({ type: 'array', maxItems: 1 });
  const result = schema.validate([1, 2]);
  assert.strictEqual(result.errors[0].code, codes.ARRAY_TOO_LONG);
});

test('ADDITIONAL_PROPERTY produced', () => {
  const schema = new Schema({
    type: 'object',
    additionalProperties: false,
    properties: { a: { type: 'string' } },
  });
  const result = schema.validate({ a: 'ok', b: 'extra' });
  assert.strictEqual(result.errors[0].code, codes.ADDITIONAL_PROPERTY);
});

test('multiple errors collected', () => {
  const schema = new Schema({
    type: 'object',
    required: ['a', 'b'],
    properties: {
      a: { type: 'string' },
      b: { type: 'number' },
      c: { type: 'boolean' },
    },
  });
  const result = schema.validate({ c: 'not_boolean' });
  assert.strictEqual(result.valid, false);
  assert.ok(result.errors.length >= 3);
});

test('error path uses dot notation', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      a: {
        type: 'object',
        properties: {
          b: { type: 'string' },
        },
      },
    },
  });
  const result = schema.validate({ a: { b: 42 } });
  assert.strictEqual(result.errors[0].path, 'a.b');
});

test('error path uses bracket notation for arrays', () => {
  const schema = new Schema({
    type: 'array',
    items: { type: 'string' },
  });
  const result = schema.validate(['ok', 42]);
  assert.strictEqual(result.errors[0].path, '[1]');
});
