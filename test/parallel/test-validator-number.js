'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('minimum - passes at boundary', () => {
  const schema = new Schema({ type: 'number', minimum: 0 });
  assert.strictEqual(schema.validate(0).valid, true);
});

test('minimum - fails below', () => {
  const schema = new Schema({ type: 'number', minimum: 0 });
  const result = schema.validate(-1);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NUMBER_TOO_SMALL');
});

test('maximum - passes at boundary', () => {
  const schema = new Schema({ type: 'number', maximum: 100 });
  assert.strictEqual(schema.validate(100).valid, true);
});

test('maximum - fails above', () => {
  const schema = new Schema({ type: 'number', maximum: 100 });
  const result = schema.validate(101);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NUMBER_TOO_LARGE');
});

test('exclusiveMinimum - fails at boundary', () => {
  const schema = new Schema({ type: 'number', exclusiveMinimum: 0 });
  const result = schema.validate(0);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NUMBER_TOO_SMALL');
});

test('exclusiveMinimum - passes above', () => {
  const schema = new Schema({ type: 'number', exclusiveMinimum: 0 });
  assert.strictEqual(schema.validate(0.001).valid, true);
});

test('exclusiveMaximum - fails at boundary', () => {
  const schema = new Schema({ type: 'number', exclusiveMaximum: 100 });
  const result = schema.validate(100);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NUMBER_TOO_LARGE');
});

test('exclusiveMaximum - passes below', () => {
  const schema = new Schema({ type: 'number', exclusiveMaximum: 100 });
  assert.strictEqual(schema.validate(99.999).valid, true);
});

test('multipleOf - multiples pass', () => {
  const schema = new Schema({ type: 'number', multipleOf: 3 });
  assert.strictEqual(schema.validate(0).valid, true);
  assert.strictEqual(schema.validate(9).valid, true);
  assert.strictEqual(schema.validate(-6).valid, true);
});

test('multipleOf - non-multiples fail', () => {
  const schema = new Schema({ type: 'number', multipleOf: 3 });
  const result = schema.validate(7);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NUMBER_NOT_MULTIPLE');
});

test('multipleOf - floating point', () => {
  const schema = new Schema({ type: 'number', multipleOf: 0.1 });
  assert.strictEqual(schema.validate(0.3).valid, true);
  assert.strictEqual(schema.validate(0.7).valid, true);
});

test('Infinity rejected', () => {
  const schema = new Schema({ type: 'number' });
  assert.strictEqual(schema.validate(Infinity).valid, false);
  assert.strictEqual(schema.validate(-Infinity).valid, false);
});

test('NaN rejected', () => {
  const schema = new Schema({ type: 'number' });
  assert.strictEqual(schema.validate(NaN).valid, false);
});

test('-0 satisfies minimum: 0 and maximum: 0', () => {
  const minSchema = new Schema({ type: 'number', minimum: 0 });
  assert.strictEqual(minSchema.validate(-0).valid, true);

  const maxSchema = new Schema({ type: 'number', maximum: 0 });
  assert.strictEqual(maxSchema.validate(-0).valid, true);
});

test('undefined with number constraints yields INVALID_TYPE, not a crash', () => {
  const schema = new Schema({ type: 'number', minimum: 0, maximum: 100 });
  const result = schema.validate(undefined);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 1);
  assert.strictEqual(result.errors[0].code, 'INVALID_TYPE');
});

test('combined number constraints', () => {
  const schema = new Schema({
    type: 'number',
    minimum: 0,
    maximum: 100,
    multipleOf: 5,
  });
  assert.strictEqual(schema.validate(25).valid, true);
  assert.strictEqual(schema.validate(0).valid, true);
  assert.strictEqual(schema.validate(100).valid, true);

  const r1 = schema.validate(-5);
  assert.strictEqual(r1.valid, false);

  const r2 = schema.validate(7);
  assert.strictEqual(r2.valid, false);
  assert.strictEqual(r2.errors[0].code, 'NUMBER_NOT_MULTIPLE');
});
