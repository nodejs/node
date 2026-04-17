'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('properties - validates named properties', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      name: { type: 'string' },
      age: { type: 'number' },
    },
  });
  assert.strictEqual(schema.validate({ name: 'Alice', age: 30 }).valid, true);
});

test('properties - reports property errors', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      name: { type: 'string' },
      age: { type: 'number' },
    },
  });
  const result = schema.validate({ name: 42, age: 'thirty' });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 2);
  assert.strictEqual(result.errors[0].path, 'name');
  assert.strictEqual(result.errors[1].path, 'age');
});

test('required - missing required fails', () => {
  const schema = new Schema({
    type: 'object',
    required: ['name'],
    properties: {
      name: { type: 'string' },
      age: { type: 'number' },
    },
  });
  const result = schema.validate({ age: 30 });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'MISSING_REQUIRED');
  assert.strictEqual(result.errors[0].path, 'name');
});

test('required - present required passes', () => {
  const schema = new Schema({
    type: 'object',
    required: ['name'],
    properties: {
      name: { type: 'string' },
    },
  });
  assert.strictEqual(schema.validate({ name: 'Alice' }).valid, true);
});

test('optional properties can be omitted', () => {
  const schema = new Schema({
    type: 'object',
    required: ['name'],
    properties: {
      name: { type: 'string' },
      age: { type: 'number' },
    },
  });
  assert.strictEqual(schema.validate({ name: 'Alice' }).valid, true);
});

test('additionalProperties false - extra properties fail', () => {
  const schema = new Schema({
    type: 'object',
    additionalProperties: false,
    properties: {
      name: { type: 'string' },
    },
  });
  const result = schema.validate({ name: 'Alice', extra: true });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'ADDITIONAL_PROPERTY');
  assert.strictEqual(result.errors[0].path, 'extra');
});

test('additionalProperties true (default) - extra properties pass', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      name: { type: 'string' },
    },
  });
  assert.strictEqual(schema.validate({ name: 'Alice', extra: true }).valid, true);
});

test('multiple required fields missing', () => {
  const schema = new Schema({
    type: 'object',
    required: ['a', 'b', 'c'],
    properties: {
      a: { type: 'string' },
      b: { type: 'string' },
      c: { type: 'string' },
    },
  });
  const result = schema.validate({});
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors.length, 3);
});

test('empty object with no constraints is valid', () => {
  const schema = new Schema({ type: 'object', properties: {} });
  assert.strictEqual(schema.validate({}).valid, true);
});

test('property name containing "." produces literal dot in path', () => {
  // Documents current behavior: formatPath joins with a literal '.',
  // so a property named 'a.b' produces path 'a.b'. A future path-escape
  // change would flip this assertion.
  const schema = new Schema({
    type: 'object',
    properties: {
      'a.b': { type: 'string' },
    },
  });
  assert.strictEqual(schema.validate({ 'a.b': 'ok' }).valid, true);
  const result = schema.validate({ 'a.b': 42 });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, 'a.b');
  assert.strictEqual(result.errors[0].code, 'INVALID_TYPE');
});

test('property name containing "[" is preserved verbatim in path', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      'items[0]': { type: 'string' },
    },
  });
  assert.strictEqual(schema.validate({ 'items[0]': 'ok' }).valid, true);
  const result = schema.validate({ 'items[0]': 42 });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, 'items[0]');
});

test('additionalProperties false without properties rejects all keys', () => {
  const schema = new Schema({
    type: 'object',
    additionalProperties: false,
  });
  assert.strictEqual(schema.validate({}).valid, true);
  const result = schema.validate({ a: 1 });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'ADDITIONAL_PROPERTY');
  assert.strictEqual(result.errors[0].path, 'a');
});
