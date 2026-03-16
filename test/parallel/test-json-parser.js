'use strict';

require('../common');
const assert = require('node:assert/strict');
const { describe, test } = require('node:test');
const { Parser } = require('node:json');

describe('Parser', () => {
  describe('constructor', () => {
    test('creates a Parser for each type', () => {
      for (const type of ['string', 'number', 'integer', 'boolean', 'null', 'object', 'array']) {
        new Parser({ type });
      }
    });

    test('creates a Parser for object type with properties', () => {
      new Parser({
        type: 'object',
        properties: {
          name: { type: 'string' },
          age: { type: 'integer' },
        },
      });
    });

    test('creates a Parser for array type without items', () => {
      new Parser({ type: 'array' });
    });

    test('creates a Parser for array type with items', () => {
      new Parser({ type: 'array', items: { type: 'string' } });
    });

    test('creates a Parser for nested object schemas', () => {
      new Parser({
        type: 'object',
        properties: {
          address: {
            type: 'object',
            properties: {
              street: { type: 'string' },
              zip: { type: 'integer' },
            },
          },
        },
      });
    });

    test('throws TypeError when called without new', () => {
      assert.throws(
        () => Parser({ type: 'string' }),
        { name: 'TypeError' },
      );
    });

    test('throws TypeError when schema argument is missing', () => {
      assert.throws(() => new Parser(), { name: 'TypeError' });
    });

    test('throws TypeError when schema argument is not an object', () => {
      for (const bad of ['string', 42, true, null]) {
        assert.throws(
          () => new Parser(bad),
          { name: 'TypeError' },
          `bad schema: ${bad}`,
        );
      }
    });

    test('throws TypeError when type is missing', () => {
      assert.throws(
        () => new Parser({}),
        { name: 'TypeError', message: /type/ },
      );
    });

    test('throws TypeError for unsupported schema type', () => {
      assert.throws(
        () => new Parser({ type: 'unsupported' }),
        { name: 'TypeError', message: /unsupported/ },
      );
    });

    test('throws TypeError for $ref combinator', () => {
      assert.throws(
        () => new Parser({ type: '$ref' }),
        { name: 'TypeError' },
      );
    });

    test('throws TypeError for oneOf combinator', () => {
      assert.throws(
        () => new Parser({ 'oneOf': [ { 'type': 'number', 'multipleOf': 5 }, { 'type': 'number', 'multipleOf': 3 } ] }),
        { name: 'TypeError' },
      );
    });

    test('throws TypeError for anyOf combinator', () => {
      assert.throws(
        () => new Parser({ 'anyOf': [ { 'type': 'string', 'maxLength': 5 }, { 'type': 'number', 'minimum': 0 } ] }),
        { name: 'TypeError' },
      );
    });

    test('throws TypeError for allOf combinator', () => {
      assert.throws(
        () => new Parser({ 'allOf': [ { 'type': 'string' }, { 'maxLength': 5 } ] }),
        { name: 'TypeError' },
      );
    });

    test('throws TypeError for not combinator', () => {
      assert.throws(
        () => new Parser({ 'not': { 'type': 'string' } }),
        { name: 'TypeError' },
      );
    });
  });

  describe('parsing objects', () => {
    const personSchema = {
      type: 'object',
      properties: {
        name: { type: 'string' },
        age: { type: 'integer' },
        score: { type: 'number' },
        active: { type: 'boolean' },
      },
    };

    test('projects defined fields from a flat object', () => {
      const p = new Parser(personSchema);
      const result = p.parse('{"name":"Alice","age":30,"score":9.5,"active":true}');
      assert.deepStrictEqual(result, { name: 'Alice', age: 30, score: 9.5, active: true });
    });

    test('ignores fields not in the schema', () => {
      const p = new Parser(personSchema);
      const result = p.parse('{"name":"Bob","age":25,"extra":"ignored","other":99}');
      assert.deepStrictEqual(result, { name: 'Bob', age: 25 });
    });

    test('returns empty object when no schema fields are present in JSON', () => {
      const p = new Parser(personSchema);
      assert.deepStrictEqual(p.parse('{"unrelated":true,"foo":"bar"}'), {});
    });

    test('returns empty object for empty JSON object', () => {
      assert.deepStrictEqual(new Parser({ type: 'object' }).parse('{}'), {});
    });

    test('returns empty object when schema has no properties', () => {
      const p = new Parser({ type: 'object', properties: {} });
      assert.deepStrictEqual(p.parse('{"name":"Alice"}'), {});
    });

    test('handles partial field presence', () => {
      const p = new Parser(personSchema);
      assert.deepStrictEqual(p.parse('{"name":"Charlie"}'), { name: 'Charlie' });
    });

    test('parses nested objects', () => {
      const p = new Parser({
        type: 'object',
        properties: {
          user: {
            type: 'object',
            properties: {
              id: { type: 'integer' },
              email: { type: 'string' },
            },
          },
        },
      });
      const result = p.parse('{"user":{"id":1,"email":"a@b.com","ignored":true}}');
      assert.deepStrictEqual(result, { user: { id: 1, email: 'a@b.com' } });
    });

    test('parses null field', () => {
      const p = new Parser({ type: 'object', properties: { v: { type: 'null' } } });
      assert.deepStrictEqual(p.parse('{"v":null}'), { v: null });
    });

    test('parses array field', () => {
      const p = new Parser({
        type: 'object',
        properties: { tags: { type: 'array', items: { type: 'string' } } },
      });
      assert.deepStrictEqual(p.parse('{"tags":["x","y"]}'), { tags: ['x', 'y'] });
    });

    test('parse can be called multiple times on the same Parser instance', () => {
      const p = new Parser({ type: 'object', properties: { x: { type: 'integer' } } });
      assert.deepStrictEqual(p.parse('{"x":1}'), { x: 1 });
      assert.deepStrictEqual(p.parse('{"x":2}'), { x: 2 });
      assert.deepStrictEqual(p.parse('{"x":3,"y":99}'), { x: 3 });
    });

    test('throws for invalid JSON', () => {
      const p = new Parser({ type: 'object', properties: { x: { type: 'integer' } } });
      assert.throws(() => p.parse('not json'), { name: 'Error' });
    });

    test('throws TypeError when parse argument is not a string', () => {
      const p = new Parser({ type: 'object' });
      assert.throws(() => p.parse(42), { name: 'TypeError' });
      assert.throws(() => p.parse(null), { name: 'TypeError' });
    });

    test('throws when schema root is object but JSON is not', () => {
      const p = new Parser({ type: 'object' });
      assert.throws(() => p.parse('"a string"'), { name: 'Error' });
      assert.throws(() => p.parse('[1,2,3]'), { name: 'Error' });
    });

    test('throws on type mismatch for a field', () => {
      const p = new Parser({ type: 'object', properties: { x: { type: 'integer' } } });
      assert.throws(
        () => p.parse('{"x":"not-a-number"}'),
        { name: 'Error', message: /x/ },
      );
    });
  });

  describe('parsing scalars at root', () => {
    test('parses a root string', () => {
      assert.strictEqual(new Parser({ type: 'string' }).parse('"hello"'), 'hello');
    });

    test('parses a root integer', () => {
      assert.strictEqual(new Parser({ type: 'integer' }).parse('42'), 42);
    });

    test('parses a root number', () => {
      assert.strictEqual(new Parser({ type: 'number' }).parse('3.14'), 3.14);
    });

    test('parses a root boolean true', () => {
      assert.strictEqual(new Parser({ type: 'boolean' }).parse('true'), true);
    });

    test('parses a root boolean false', () => {
      assert.strictEqual(new Parser({ type: 'boolean' }).parse('false'), false);
    });

    test('parses a root null', () => {
      assert.strictEqual(new Parser({ type: 'null' }).parse('null'), null);
    });
  });

  describe('parsing arrays', () => {
    test('parses an array of strings', () => {
      const p = new Parser({ type: 'array', items: { type: 'string' } });
      assert.deepStrictEqual(p.parse('["a","b","c"]'), ['a', 'b', 'c']);
    });

    test('parses an array of integers', () => {
      const p = new Parser({ type: 'array', items: { type: 'integer' } });
      assert.deepStrictEqual(p.parse('[1,2,3]'), [1, 2, 3]);
    });

    test('parses an array of numbers', () => {
      const p = new Parser({ type: 'array', items: { type: 'number' } });
      assert.deepStrictEqual(p.parse('[1.1,2.2,3.3]'), [1.1, 2.2, 3.3]);
    });

    test('parses an array of booleans', () => {
      const p = new Parser({ type: 'array', items: { type: 'boolean' } });
      assert.deepStrictEqual(p.parse('[true,false,true]'), [true, false, true]);
    });

    test('parses an empty array', () => {
      const p = new Parser({ type: 'array', items: { type: 'string' } });
      assert.deepStrictEqual(p.parse('[]'), []);
    });

    test('parses an array of objects with projection', () => {
      const p = new Parser({
        type: 'array',
        items: {
          type: 'object',
          properties: {
            id: { type: 'integer' },
            name: { type: 'string' },
          },
        },
      });
      const result = p.parse('[{"id":1,"name":"a","extra":0},{"id":2,"name":"b"}]');
      assert.deepStrictEqual(result, [{ id: 1, name: 'a' }, { id: 2, name: 'b' }]);
    });

    test('throws when schema root is array but JSON is not', () => {
      const p = new Parser({ type: 'array', items: { type: 'string' } });
      assert.throws(() => p.parse('"a string"'), { name: 'Error' });
      assert.throws(() => p.parse('{"key":"value"}'), { name: 'Error' });
    });

    test('throws on item type mismatch inside an array', () => {
      const p = new Parser({ type: 'array', items: { type: 'integer' } });
      assert.throws(() => p.parse('["not-a-number"]'), { name: 'Error' });
    });
  });
});
