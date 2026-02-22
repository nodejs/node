'use strict';
require('../common');

const { test, describe } = require('node:test');
const { Parser } = require('node:json_parser');

describe('LazyParser', () => {
  test('scalar root string returns plain JS string', (t) => {
    const parser = new Parser();
    t.assert.strictEqual(parser.parse('"hello"').root(), 'hello');
  });

  test('scalar root number returns plain JS number', (t) => {
    const parser = new Parser();
    t.assert.strictEqual(parser.parse('42').root(), 42);
  });

  test('scalar root boolean returns plain JS boolean', (t) => {
    const parser = new Parser();
    t.assert.strictEqual(parser.parse('true').root(), true);
    t.assert.strictEqual(parser.parse('false').root(), false);
  });

  test('scalar root null returns null', (t) => {
    const parser = new Parser();
    t.assert.strictEqual(parser.parse('null').root(), null);
  });

  test('compound root array returns document with type "array"', (t) => {
    const parser = new Parser();
    const doc = parser.parse('[1, 2, 3]');
    t.assert.strictEqual(doc.root().type(), 'array');
  });

  test('compound root object returns document with type "object"', (t) => {
    const parser = new Parser();
    const doc = parser.parse('{"a": 1}');
    t.assert.strictEqual(doc.root().type(), 'object');
  });

  test('array iteration returns JsonValue objects', (t) => {
    const parser = new Parser();
    const doc = parser.parse('[1, "hi", true, null]');
    const types = [];
    for (const val of doc.root().getArray()) {
      types.push(val.type());
    }
    t.assert.deepStrictEqual(types, ['number', 'string', 'boolean', 'null']);
  });

  test('JsonValue getString returns string', (t) => {
    const parser = new Parser();
    const doc = parser.parse('["hello"]');
    const iter = doc.root().getArray();
    const val = iter.next().value;
    t.assert.strictEqual(val.getString(), 'hello');
  });

  test('JsonValue getNumber returns number', (t) => {
    const parser = new Parser();
    const doc = parser.parse('[99.5]');
    const iter = doc.root().getArray();
    const val = iter.next().value;
    t.assert.strictEqual(val.getNumber(), 99.5);
  });

  test('JsonValue getBoolean returns boolean', (t) => {
    const parser = new Parser();
    const doc = parser.parse('[true]');
    const iter = doc.root().getArray();
    const val = iter.next().value;
    t.assert.strictEqual(val.getBoolean(), true);
  });

  test('JsonValue isNull returns true for null', (t) => {
    const parser = new Parser();
    const doc = parser.parse('[null]');
    const iter = doc.root().getArray();
    const val = iter.next().value;
    t.assert.strictEqual(val.isNull(), true);
  });

  test('array iterator done after exhaustion', (t) => {
    const parser = new Parser();
    const doc = parser.parse('[1]');
    const iter = doc.root().getArray();
    t.assert.strictEqual(iter.next().done, false);
    t.assert.strictEqual(iter.next().done, true);
  });
});
