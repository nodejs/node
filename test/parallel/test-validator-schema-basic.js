'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const { test } = require('node:test');

test('require node:validator works', () => {
  const validator = require('node:validator');
  assert.ok(validator);
  assert.ok(validator.Schema);
  assert.ok(validator.codes);
});

test('require validator without scheme throws', () => {
  assert.throws(
    () => require('validator'),
    { code: 'MODULE_NOT_FOUND' },
  );
});

test('Schema constructor with valid definition', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'string' });
  assert.ok(schema);
});

test('Schema constructor throws for non-object', () => {
  const { Schema } = require('node:validator');
  assert.throws(() => new Schema('string'), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => new Schema(null), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => new Schema(42), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => new Schema([1, 2]), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});

test('validate returns correct structure', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'string' });
  const result = schema.validate('hello');
  assert.strictEqual(typeof result.valid, 'boolean');
  assert.ok(Array.isArray(result.errors));
  assert.strictEqual(result.valid, true);
  assert.strictEqual(result.errors.length, 0);
});

test('validate result is frozen', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'string' });
  const result = schema.validate('hello');
  assert.ok(Object.isFrozen(result));
  assert.ok(Object.isFrozen(result.errors));
});

test('toJSON returns a frozen copy of the definition', () => {
  const { Schema } = require('node:validator');
  const definition = { type: 'string', minLength: 1 };
  const schema = new Schema(definition);
  const json = schema.toJSON();
  assert.deepStrictEqual(json, definition);
  assert.ok(Object.isFrozen(json));
  assert.strictEqual(schema.toJSON(), json);
});

test('codes export has all expected codes', () => {
  const { codes } = require('node:validator');
  assert.strictEqual(codes.INVALID_TYPE, 'INVALID_TYPE');
  assert.strictEqual(codes.MISSING_REQUIRED, 'MISSING_REQUIRED');
  assert.strictEqual(codes.STRING_TOO_SHORT, 'STRING_TOO_SHORT');
  assert.strictEqual(codes.STRING_TOO_LONG, 'STRING_TOO_LONG');
  assert.strictEqual(codes.PATTERN_MISMATCH, 'PATTERN_MISMATCH');
  assert.strictEqual(codes.ENUM_MISMATCH, 'ENUM_MISMATCH');
  assert.strictEqual(codes.NUMBER_TOO_SMALL, 'NUMBER_TOO_SMALL');
  assert.strictEqual(codes.NUMBER_TOO_LARGE, 'NUMBER_TOO_LARGE');
  assert.strictEqual(codes.NUMBER_NOT_MULTIPLE, 'NUMBER_NOT_MULTIPLE');
  assert.strictEqual(codes.NOT_INTEGER, 'NOT_INTEGER');
  assert.strictEqual(codes.ARRAY_TOO_SHORT, 'ARRAY_TOO_SHORT');
  assert.strictEqual(codes.ARRAY_TOO_LONG, 'ARRAY_TOO_LONG');
  assert.strictEqual(codes.ADDITIONAL_PROPERTY, 'ADDITIONAL_PROPERTY');
});

test('codes export is frozen', () => {
  const { codes } = require('node:validator');
  assert.ok(Object.isFrozen(codes));
});

test('toJSON preserves -0 (not coerced to 0)', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'number', default: -0 });
  const json = schema.toJSON();
  assert.strictEqual(Object.is(json.default, -0), true);
});

test('toJSON keeps `default` by reference', () => {
  const { Schema } = require('node:validator');
  const defaults = { nested: { a: 1 } };
  const schema = new Schema({ type: 'object', default: defaults });
  assert.strictEqual(schema.toJSON().default, defaults);
});

test('toJSON result is deeply frozen', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({
    type: 'object',
    required: ['name'],
    properties: {
      name: { type: 'string' },
      tags: { type: 'array', items: { type: 'string' } },
    },
  });
  const json = schema.toJSON();
  assert.ok(Object.isFrozen(json));
  assert.ok(Object.isFrozen(json.properties));
  assert.ok(Object.isFrozen(json.properties.name));
  assert.ok(Object.isFrozen(json.properties.tags));
  assert.ok(Object.isFrozen(json.properties.tags.items));
  assert.ok(Object.isFrozen(json.required));
});

test('circular schema throws ERR_VALIDATOR_INVALID_SCHEMA', () => {
  const { Schema } = require('node:validator');
  const def = { type: 'object', properties: {} };
  def.properties.self = def;
  assert.throws(() => new Schema(def), {
    code: 'ERR_VALIDATOR_INVALID_SCHEMA',
  });
});

test('Schema instance has Symbol.toStringTag "Schema"', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'string' });
  assert.strictEqual(
    Object.prototype.toString.call(schema), '[object Schema]');
});

test('util.inspect(schema) output starts with "Schema "', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'string', minLength: 1 });
  const output = util.inspect(schema);
  assert.ok(output.startsWith('Schema '),
            `expected output to start with "Schema ", got: ${output}`);
});

test('util.inspect(schema, { depth: -1 }) returns "Schema [Object]"', () => {
  const { Schema } = require('node:validator');
  const schema = new Schema({ type: 'string' });
  assert.strictEqual(util.inspect(schema, { depth: -1 }), 'Schema [Object]');
});

test('shared-but-acyclic subgraph does not false-trigger circular check', () => {
  const { Schema } = require('node:validator');
  // Same leaf schema object referenced under two different property names.
  // The seen-set must be path-local, not global, or this throws.
  const leaf = { type: 'string' };
  const schema = new Schema({
    type: 'object',
    properties: {
      a: leaf,
      b: leaf,
    },
  });
  assert.strictEqual(schema.validate({ a: 'x', b: 'y' }).valid, true);
});
