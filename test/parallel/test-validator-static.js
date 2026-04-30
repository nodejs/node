'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('Schema.validate convenience method - valid', () => {
  const result = Schema.validate({ type: 'string' }, 'hello');
  assert.strictEqual(result.valid, true);
  assert.strictEqual(result.errors.length, 0);
});

test('Schema.validate convenience method - invalid', () => {
  const result = Schema.validate({ type: 'number', minimum: 0 }, -1);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].code, 'NUMBER_TOO_SMALL');
});

test('Schema.validate equivalent to new Schema().validate()', () => {
  const definition = {
    type: 'object',
    required: ['name'],
    properties: {
      name: { type: 'string', minLength: 1 },
      age: { type: 'integer', minimum: 0 },
    },
  };
  const data = { name: '', age: -1 };

  const staticResult = Schema.validate(definition, data);
  const instanceResult = new Schema(definition).validate(data);

  assert.strictEqual(staticResult.valid, instanceResult.valid);
  assert.strictEqual(staticResult.errors.length, instanceResult.errors.length);
  for (let i = 0; i < staticResult.errors.length; i++) {
    assert.strictEqual(staticResult.errors[i].path, instanceResult.errors[i].path);
    assert.strictEqual(staticResult.errors[i].code, instanceResult.errors[i].code);
  }
});

test('Schema.validate throws for invalid schema', () => {
  assert.throws(
    () => Schema.validate({ type: 'invalid' }, 'data'),
    { code: 'ERR_VALIDATOR_INVALID_SCHEMA' });
});

test('Schema.validate with complex schema', () => {
  const result = Schema.validate({
    type: 'object',
    required: ['items'],
    properties: {
      items: {
        type: 'array',
        minItems: 1,
        items: {
          type: 'object',
          required: ['id'],
          properties: {
            id: { type: 'integer', minimum: 1 },
            label: { type: 'string' },
          },
        },
      },
    },
  }, {
    items: [
      { id: 1, label: 'first' },
      { id: 2, label: 'second' },
    ],
  });
  assert.strictEqual(result.valid, true);
});
