'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('type string - valid', () => {
  const schema = new Schema({ type: 'string' });
  assert.strictEqual(schema.validate('hello').valid, true);
  assert.strictEqual(schema.validate('').valid, true);
});

test('type string - invalid', () => {
  const schema = new Schema({ type: 'string' });
  assert.strictEqual(schema.validate(42).valid, false);
  assert.strictEqual(schema.validate(true).valid, false);
  assert.strictEqual(schema.validate(null).valid, false);
  assert.strictEqual(schema.validate(undefined).valid, false);
  assert.strictEqual(schema.validate([]).valid, false);
  assert.strictEqual(schema.validate({}).valid, false);
});

test('type number - valid', () => {
  const schema = new Schema({ type: 'number' });
  assert.strictEqual(schema.validate(42).valid, true);
  assert.strictEqual(schema.validate(3.14).valid, true);
  assert.strictEqual(schema.validate(0).valid, true);
  assert.strictEqual(schema.validate(-1).valid, true);
});

test('type number - invalid', () => {
  const schema = new Schema({ type: 'number' });
  assert.strictEqual(schema.validate('42').valid, false);
  assert.strictEqual(schema.validate(true).valid, false);
  assert.strictEqual(schema.validate(null).valid, false);
  assert.strictEqual(schema.validate(NaN).valid, false);
  assert.strictEqual(schema.validate(undefined).valid, false);
});

test('type integer - valid', () => {
  const schema = new Schema({ type: 'integer' });
  assert.strictEqual(schema.validate(42).valid, true);
  assert.strictEqual(schema.validate(0).valid, true);
  assert.strictEqual(schema.validate(-5).valid, true);
});

test('type integer - rejects floats', () => {
  const schema = new Schema({ type: 'integer' });
  const result = schema.validate(3.14);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NOT_INTEGER');
});

test('type integer - rejects non-numbers', () => {
  const schema = new Schema({ type: 'integer' });
  assert.strictEqual(schema.validate('42').valid, false);
  assert.strictEqual(schema.validate(NaN).valid, false);
});

test('type boolean - valid', () => {
  const schema = new Schema({ type: 'boolean' });
  assert.strictEqual(schema.validate(true).valid, true);
  assert.strictEqual(schema.validate(false).valid, true);
});

test('type boolean - invalid', () => {
  const schema = new Schema({ type: 'boolean' });
  assert.strictEqual(schema.validate(0).valid, false);
  assert.strictEqual(schema.validate(1).valid, false);
  assert.strictEqual(schema.validate('true').valid, false);
  assert.strictEqual(schema.validate(null).valid, false);
});

test('type null - valid', () => {
  const schema = new Schema({ type: 'null' });
  assert.strictEqual(schema.validate(null).valid, true);
});

test('type null - invalid', () => {
  const schema = new Schema({ type: 'null' });
  assert.strictEqual(schema.validate(undefined).valid, false);
  assert.strictEqual(schema.validate(0).valid, false);
  assert.strictEqual(schema.validate('').valid, false);
  assert.strictEqual(schema.validate(false).valid, false);
});

test('type object - valid', () => {
  const schema = new Schema({ type: 'object', properties: {} });
  assert.strictEqual(schema.validate({}).valid, true);
  assert.strictEqual(schema.validate({ a: 1 }).valid, true);
});

test('type object - rejects non-objects', () => {
  const schema = new Schema({ type: 'object', properties: {} });
  assert.strictEqual(schema.validate([]).valid, false);
  assert.strictEqual(schema.validate(null).valid, false);
  assert.strictEqual(schema.validate('string').valid, false);
  assert.strictEqual(schema.validate(42).valid, false);
});

test('type array - valid', () => {
  const schema = new Schema({ type: 'array' });
  assert.strictEqual(schema.validate([]).valid, true);
  assert.strictEqual(schema.validate([1, 2, 3]).valid, true);
});

test('type array - rejects non-arrays', () => {
  const schema = new Schema({ type: 'array' });
  assert.strictEqual(schema.validate({}).valid, false);
  assert.strictEqual(schema.validate('hello').valid, false);
  assert.strictEqual(schema.validate(42).valid, false);
  assert.strictEqual(schema.validate(null).valid, false);
});

test('error objects have correct structure', () => {
  const schema = new Schema({ type: 'string' });
  const result = schema.validate(42);
  assert.strictEqual(result.errors.length, 1);
  const error = result.errors[0];
  assert.strictEqual(typeof error.path, 'string');
  assert.strictEqual(typeof error.message, 'string');
  assert.strictEqual(typeof error.code, 'string');
  assert.strictEqual(error.code, 'INVALID_TYPE');
  assert.strictEqual(error.path, '');
});
