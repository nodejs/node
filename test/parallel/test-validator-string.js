'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('minLength - passes at boundary', () => {
  const schema = new Schema({ type: 'string', minLength: 3 });
  assert.strictEqual(schema.validate('abc').valid, true);
});

test('minLength - fails below', () => {
  const schema = new Schema({ type: 'string', minLength: 3 });
  const result = schema.validate('ab');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'STRING_TOO_SHORT');
});

test('minLength - zero allows empty string', () => {
  const schema = new Schema({ type: 'string', minLength: 0 });
  assert.strictEqual(schema.validate('').valid, true);
});

test('maxLength - passes at boundary', () => {
  const schema = new Schema({ type: 'string', maxLength: 5 });
  assert.strictEqual(schema.validate('abcde').valid, true);
});

test('maxLength - fails above', () => {
  const schema = new Schema({ type: 'string', maxLength: 5 });
  const result = schema.validate('abcdef');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'STRING_TOO_LONG');
});

test('pattern - matches', () => {
  const schema = new Schema({ type: 'string', pattern: '^[a-z]+$' });
  assert.strictEqual(schema.validate('hello').valid, true);
});

test('pattern - mismatches', () => {
  const schema = new Schema({ type: 'string', pattern: '^[a-z]+$' });
  const result = schema.validate('Hello123');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'PATTERN_MISMATCH');
});

test('pattern - email-like', () => {
  const schema = new Schema({ type: 'string', pattern: '^[^@]+@[^@]+$' });
  assert.strictEqual(schema.validate('user@example.com').valid, true);
  assert.strictEqual(schema.validate('invalid').valid, false);
});

test('enum - value in list passes', () => {
  const schema = new Schema({ type: 'string', enum: ['red', 'green', 'blue'] });
  assert.strictEqual(schema.validate('red').valid, true);
  assert.strictEqual(schema.validate('green').valid, true);
});

test('enum - value not in list fails', () => {
  const schema = new Schema({ type: 'string', enum: ['red', 'green', 'blue'] });
  const result = schema.validate('yellow');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'ENUM_MISMATCH');
});

test('minLength === maxLength pins exact length', () => {
  const schema = new Schema({ type: 'string', minLength: 5, maxLength: 5 });
  assert.strictEqual(schema.validate('abcde').valid, true);

  const tooShort = schema.validate('abcd');
  assert.strictEqual(tooShort.valid, false);
  assert.strictEqual(tooShort.errors[0].code, 'STRING_TOO_SHORT');

  const tooLong = schema.validate('abcdef');
  assert.strictEqual(tooLong.valid, false);
  assert.strictEqual(tooLong.errors[0].code, 'STRING_TOO_LONG');
});

test('combined constraints - all pass', () => {
  const schema = new Schema({
    type: 'string',
    minLength: 1,
    maxLength: 10,
    pattern: '^[a-z]+$',
  });
  assert.strictEqual(schema.validate('hello').valid, true);
});

test('combined constraints - multiple failures', () => {
  const schema = new Schema({
    type: 'string',
    minLength: 5,
    pattern: '^[0-9]+$',
  });
  const result = schema.validate('ab');
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 2);
  const errorCodes = result.errors.map((e) => e.code);
  assert.ok(errorCodes.includes('STRING_TOO_SHORT'));
  assert.ok(errorCodes.includes('PATTERN_MISMATCH'));
});
