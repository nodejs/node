'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('invalid type string', () => {
  assert.throws(() => new Schema({ type: 'invalid' }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('missing type', () => {
  assert.throws(() => new Schema({}), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('negative minLength', () => {
  assert.throws(() => new Schema({ type: 'string', minLength: -1 }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('non-integer minLength', () => {
  assert.throws(() => new Schema({ type: 'string', minLength: 1.5 }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('minLength greater than maxLength', () => {
  assert.throws(
    () => new Schema({ type: 'string', minLength: 10, maxLength: 5 }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('invalid pattern regex', () => {
  assert.throws(() => new Schema({ type: 'string', pattern: '[invalid' }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('pattern not a string', () => {
  assert.throws(() => new Schema({ type: 'string', pattern: 123 }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('enum not an array', () => {
  assert.throws(() => new Schema({ type: 'string', enum: 'red' }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('enum empty array', () => {
  assert.throws(() => new Schema({ type: 'string', enum: [] }), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('required not an array', () => {
  assert.throws(
    () => new Schema({
      type: 'object', required: 'name',
      properties: { name: { type: 'string' } },
    }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('required contains non-strings', () => {
  assert.throws(
    () => new Schema({
      type: 'object', required: [123],
      properties: { 123: { type: 'string' } },
    }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('properties not an object', () => {
  assert.throws(
    () => new Schema({ type: 'object', properties: 'bad' }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('properties is an array', () => {
  assert.throws(
    () => new Schema({ type: 'object', properties: [] }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('items not a schema object', () => {
  assert.throws(
    () => new Schema({ type: 'array', items: 'string' }),
    { code: 'ERR_INVALID_ARG_TYPE' });
});

test('additionalProperties not boolean', () => {
  assert.throws(
    () => new Schema({
      type: 'object', additionalProperties: 'no',
      properties: {},
    }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('multipleOf must be positive', () => {
  assert.throws(
    () => new Schema({ type: 'number', multipleOf: 0 }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
  assert.throws(
    () => new Schema({ type: 'number', multipleOf: -1 }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('minimum must be finite number', () => {
  assert.throws(
    () => new Schema({ type: 'number', minimum: 'zero' }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
  assert.throws(
    () => new Schema({ type: 'number', minimum: Infinity }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('minimum rejects NaN', () => {
  assert.throws(
    () => new Schema({ type: 'number', minimum: NaN }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('maximum rejects Infinity', () => {
  assert.throws(
    () => new Schema({ type: 'number', maximum: Infinity }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('maximum rejects NaN', () => {
  assert.throws(
    () => new Schema({ type: 'number', maximum: NaN }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('exclusiveMinimum rejects Infinity', () => {
  assert.throws(
    () => new Schema({ type: 'number', exclusiveMinimum: Infinity }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('exclusiveMaximum rejects -Infinity', () => {
  assert.throws(
    () => new Schema({ type: 'number', exclusiveMaximum: -Infinity }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('multipleOf rejects NaN', () => {
  assert.throws(
    () => new Schema({ type: 'number', multipleOf: NaN }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('multipleOf rejects Infinity', () => {
  assert.throws(
    () => new Schema({ type: 'number', multipleOf: Infinity }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('minItems greater than maxItems', () => {
  assert.throws(
    () => new Schema({ type: 'array', minItems: 10, maxItems: 5 }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('unknown constraint for type', () => {
  assert.throws(
    () => new Schema({ type: 'string', minimum: 0 }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
  assert.throws(
    () => new Schema({ type: 'number', minLength: 0 }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('required property not in properties', () => {
  assert.throws(
    () => new Schema({
      type: 'object',
      required: ['missing'],
      properties: { name: { type: 'string' } },
    }),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});
