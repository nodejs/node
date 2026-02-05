'use strict';

require('../common');

const { test, describe } = require('node:test');
const assert = require('assert');

const { parse } = require('node:json');

describe('node:json', () => {
  test('throws TypeError when called without arguments', () => {
    assert.throws(
      () => parse(),
      {
        name: 'TypeError',
      }
    );
  });

  test('throws TypeError when called with number', () => {
    assert.throws(
      () => parse(123),
      {
        name: 'TypeError',
      }
    );
  });

  test('throws TypeError when called with object', () => {
    assert.throws(
      () => parse({}),
      {
        name: 'TypeError',
      }
    );
  });

  test('throws TypeError when called with array', () => {
    assert.throws(
      () => parse([]),
      {
        name: 'TypeError',
      }
    );
  });

  test('throws TypeError when called with null', () => {
    assert.throws(
      () => parse(null),
      {
        name: 'TypeError',
      }
    );
  });

  test('throws SyntaxError on invalid string JSON', () => {
    assert.throws(
      () => parse('not valid json'),
      {
        name: 'SyntaxError',
      }
    );
  });

  test('throws SyntaxError on invalid object JSON', () => {
    assert.throws(
      () => parse('{"key": "property}'),
      {
        name: 'SyntaxError',
      }
    );
  });

  test('throws SyntaxError on invalid array JSON', () => {
    assert.throws(
      () => parse('[1,2,3,]'),
      {
        name: 'SyntaxError',
      }
    );
  });

  test('parses simple object', () => {
    const result = parse('{"key":"value"}');
    assert.deepStrictEqual(result, { key: 'value' });
  });

  test('parses object with multiple properties', () => {
    const result = parse('{"name":"John","age":30,"active":true}');
    assert.deepStrictEqual(result, { name: 'John', age: 30, active: true });
  });

  test('parses nested objects', () => {
    const result = parse('{"user":{"name":"Jane","address":{"city":"NYC"}}}');
    assert.deepStrictEqual(result, {
      user: {
        name: 'Jane',
        address: { city: 'NYC' },
      },
    });
  });

  test('parses empty object', () => {
    const result = parse('{}');
    assert.deepStrictEqual(result, {});
  });

  test('parses simple array', () => {
    const result = parse('[1,2,3]');
    assert.deepStrictEqual(result, [1, 2, 3]);
  });

  test('parses array of objects', () => {
    const result = parse('[{"id":1},{"id":2}]');
    assert.deepStrictEqual(result, [{ id: 1 }, { id: 2 }]);
  });

  test('parses empty array', () => {
    const result = parse('[]');
    assert.deepStrictEqual(result, []);
  });

  test('parses nested arrays', () => {
    const result = parse('[[1,2],[3,4]]');
    assert.deepStrictEqual(result, [[1, 2], [3, 4]]);
  });

  test('parses string value', () => {
    const result = parse('"hello"');
    assert.strictEqual(result, 'hello');
  });

  test('parses integer', () => {
    const result = parse('42');
    assert.strictEqual(result, 42);
  });

  test('parses negative number', () => {
    const result = parse('-17');
    assert.strictEqual(result, -17);
  });

  test('parses floating point number', () => {
    const result = parse('3.14159');
    assert.strictEqual(result, 3.14159);
  });

  test('parses boolean true', () => {
    const result = parse('true');
    assert.strictEqual(result, true);
  });

  test('parses boolean false', () => {
    const result = parse('false');
    assert.strictEqual(result, false);
  });

  test('parses null', () => {
    const result = parse('null');
    assert.strictEqual(result, null);
  });

  test('parses escaped quotes', () => {
    const result = parse('{"message":"Hello \\"World\\""}');
    assert.strictEqual(result.message, 'Hello "World"');
  });

  test('parses escaped backslash', () => {
    const result = parse('{"path":"C:\\\\Users"}');
    assert.strictEqual(result.path, 'C:\\Users');
  });

  test('parses escaped newline', () => {
    const result = parse('{"text":"line1\\nline2"}');
    assert.strictEqual(result.text, 'line1\nline2');
  });
});
