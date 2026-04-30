// Flags: --expose-internals --no-warnings

'use strict';

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const { internalBinding } = require('internal/test/binding');

const { getConfigJsonSchema } = internalBinding('options');
const schema = JSON.parse(getConfigJsonSchema());

test('schema root has the expected shape', () => {
  assert.strictEqual(
    schema.$schema,
    'https://json-schema.org/draft/2020-12/schema',
  );
  assert.strictEqual(schema.type, 'object');
  assert.strictEqual(schema.additionalProperties, false);
  assert.strictEqual(schema.properties.$schema.type, 'string');
  assert.strictEqual(schema.properties.nodeOptions.type, 'object');
});

test('boolean options map to type:boolean', () => {
  const env = schema.properties.nodeOptions.properties;
  assert.strictEqual(env.addons?.type, 'boolean');
  assert.strictEqual(env['preserve-symlinks']?.type, 'boolean');
});

test('numeric options map to type:number', () => {
  const env = schema.properties.nodeOptions.properties;
  assert.strictEqual(env['max-http-header-size']?.type, 'number');
});

test('string-or-array options use oneOf', () => {
  const opt = schema.properties.nodeOptions.properties.import;
  assert.ok(Array.isArray(opt?.oneOf));
  assert.strictEqual(opt.oneOf.length, 2);
  assert.strictEqual(opt.oneOf[0].type, 'string');
  assert.strictEqual(opt.oneOf[1].type, 'array');
  assert.strictEqual(opt.oneOf[1].minItems, 1);
  assert.strictEqual(opt.oneOf[1].items.type, 'string');
});

test('namespaces are exposed at the root', () => {
  assert.strictEqual(schema.properties.test?.type, 'object');
  assert.strictEqual(schema.properties.permission?.type, 'object');
  assert.strictEqual(schema.properties.watch?.type, 'object');
});

test('namespace options keep their type', () => {
  const test_ns = schema.properties.test.properties;
  assert.strictEqual(test_ns['test-concurrency']?.type, 'number');
  assert.strictEqual(test_ns['test-only']?.type, 'boolean');
});
