'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('nested object validation', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      address: {
        type: 'object',
        required: ['city'],
        properties: {
          street: { type: 'string' },
          city: { type: 'string' },
        },
      },
    },
  });

  assert.strictEqual(
    schema.validate({ address: { street: '123 Main', city: 'NYC' } }).valid,
    true);

  const result = schema.validate({ address: { street: 123 } });
  assert.strictEqual(result.valid, false);
  const paths = result.errors.map((e) => e.path);
  assert.ok(paths.includes('address.street'));
  assert.ok(paths.includes('address.city'));
});

test('array of objects', () => {
  const schema = new Schema({
    type: 'array',
    items: {
      type: 'object',
      required: ['name'],
      properties: {
        name: { type: 'string' },
      },
    },
  });

  assert.strictEqual(
    schema.validate([{ name: 'Alice' }, { name: 'Bob' }]).valid, true);

  const result = schema.validate([{ name: 'Alice' }, {}]);
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, '[1].name');
  assert.strictEqual(result.errors[0].code, 'MISSING_REQUIRED');
});

test('deeply nested error paths', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      users: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            contact: {
              type: 'object',
              properties: {
                email: { type: 'string', pattern: '^[^@]+@[^@]+$' },
              },
            },
          },
        },
      },
    },
  });

  const result = schema.validate({
    users: [
      { contact: { email: 'valid@test.com' } },
      { contact: { email: 'invalid' } },
    ],
  });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, 'users[1].contact.email');
});

test('three-level nesting', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      a: {
        type: 'object',
        properties: {
          b: {
            type: 'object',
            properties: {
              c: { type: 'string' },
            },
          },
        },
      },
    },
  });
  const result = schema.validate({ a: { b: { c: 42 } } });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, 'a.b.c');
});

test('nested object missing required in nested', () => {
  const schema = new Schema({
    type: 'object',
    required: ['config'],
    properties: {
      config: {
        type: 'object',
        required: ['host', 'port'],
        properties: {
          host: { type: 'string' },
          port: { type: 'integer' },
        },
      },
    },
  });

  const result = schema.validate({ config: { host: 'localhost' } });
  assert.strictEqual(result.valid, false);
  assert.strictEqual(result.errors[0].path, 'config.port');
  assert.strictEqual(result.errors[0].code, 'MISSING_REQUIRED');
});
