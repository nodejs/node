'use strict';

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const { Schema } = require('node:validator');

test('applyDefaults - applies defaults for missing properties', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      host: { type: 'string', default: 'localhost' },
      port: { type: 'integer', default: 3000 },
    },
  });
  const result = schema.applyDefaults({});
  assert.strictEqual(result.host, 'localhost');
  assert.strictEqual(result.port, 3000);
});

test('applyDefaults - does not overwrite existing values', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      host: { type: 'string', default: 'localhost' },
    },
  });
  const result = schema.applyDefaults({ host: 'example.com' });
  assert.strictEqual(result.host, 'example.com');
});

test('applyDefaults - does not mutate input', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      host: { type: 'string', default: 'localhost' },
    },
  });
  const input = {};
  schema.applyDefaults(input);
  assert.strictEqual(input.host, undefined);
});

test('applyDefaults - applies for undefined values', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      host: { type: 'string', default: 'localhost' },
    },
  });
  const result = schema.applyDefaults({ host: undefined });
  assert.strictEqual(result.host, 'localhost');
});

test('applyDefaults - does not apply for null values', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      host: { type: 'string', default: 'localhost' },
    },
  });
  const result = schema.applyDefaults({ host: null });
  assert.strictEqual(result.host, null);
});

test('applyDefaults - nested defaults', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      config: {
        type: 'object',
        properties: {
          timeout: { type: 'integer', default: 5000 },
          retries: { type: 'integer', default: 3 },
        },
      },
    },
  });
  const result = schema.applyDefaults({ config: { retries: 1 } });
  assert.strictEqual(result.config.timeout, 5000);
  assert.strictEqual(result.config.retries, 1);
});

test('applyDefaults - preserves extra properties', () => {
  const schema = new Schema({
    type: 'object',
    properties: {
      name: { type: 'string', default: 'unknown' },
    },
  });
  const result = schema.applyDefaults({ extra: 'value' });
  assert.strictEqual(result.name, 'unknown');
  assert.strictEqual(result.extra, 'value');
});

test('applyDefaults - returns non-object data as-is', () => {
  const schema = new Schema({ type: 'string' });
  assert.strictEqual(schema.applyDefaults('hello'), 'hello');
});

test('applyDefaults - array items defaults', () => {
  const schema = new Schema({
    type: 'array',
    items: {
      type: 'object',
      properties: {
        enabled: { type: 'boolean', default: true },
      },
    },
  });
  const result = schema.applyDefaults([{ enabled: false }, {}]);
  assert.strictEqual(result[0].enabled, false);
  assert.strictEqual(result[1].enabled, true);
});
