'use strict';

require('../common');

const { test, describe } = require('node:test');
const assert = require('assert');
const { TextEncoder } = require('node:util');

const { parseFromBuffer } = require('node:json');

// TODO(araujogui): should I use Buffer.from instead?
const encoder = new TextEncoder();

const toArrayBuffer = (str) => encoder.encode(str).buffer;

describe('node:json', () => {
  test('throws TypeError when called without arguments', () => {
    assert.throws(() => parseFromBuffer(), {
      name: 'TypeError',
    });
  });

  test('throws TypeError when called with number', () => {
    assert.throws(() => parseFromBuffer(123), {
      name: 'TypeError',
    });
  });

  test('throws TypeError when called with object', () => {
    assert.throws(() => parseFromBuffer({}), {
      name: 'TypeError',
    });
  });

  test('throws TypeError when called with array', () => {
    assert.throws(() => parseFromBuffer([]), {
      name: 'TypeError',
    });
  });

  test('throws TypeError when called with null', () => {
    assert.throws(() => parseFromBuffer(null), {
      name: 'TypeError',
    });
  });

  test('throws SyntaxError on invalid string JSON', () => {
    assert.throws(() => parseFromBuffer(toArrayBuffer('not valid json')), {
      name: 'SyntaxError',
    });
  });

  test('throws SyntaxError on invalid object JSON', () => {
    assert.throws(() => parseFromBuffer(toArrayBuffer('{"key": "property}')), {
      name: 'SyntaxError',
    });
  });

  test('throws SyntaxError on invalid array JSON', () => {
    assert.throws(() => parseFromBuffer(toArrayBuffer('[1,2,3,]')), {
      name: 'SyntaxError',
    });
  });

  test('parses simple object', () => {
    const result = parseFromBuffer(toArrayBuffer('{"key":"value"}'));
    assert.deepStrictEqual(result, { key: 'value' });
  });

  test('parses object with multiple properties', () => {
    const result = parseFromBuffer(
      toArrayBuffer('{"name":"John","age":30,"active":true}')
    );
    assert.deepStrictEqual(result, { name: 'John', age: 30, active: true });
  });

  test('parses nested objects', () => {
    const result = parseFromBuffer(
      toArrayBuffer('{"user":{"name":"Jane","address":{"city":"NYC"}}}')
    );
    assert.deepStrictEqual(result, {
      user: {
        name: 'Jane',
        address: { city: 'NYC' },
      },
    });
  });

  test('parses empty object', () => {
    const result = parseFromBuffer(toArrayBuffer('{}'));
    assert.deepStrictEqual(result, {});
  });

  test('parses simple array', () => {
    const result = parseFromBuffer(toArrayBuffer('[1,2,3]'));
    assert.deepStrictEqual(result, [1, 2, 3]);
  });

  test('parses array of objects', () => {
    const result = parseFromBuffer(toArrayBuffer('[{"id":1},{"id":2}]'));
    assert.deepStrictEqual(result, [{ id: 1 }, { id: 2 }]);
  });

  test('parses empty array', () => {
    const result = parseFromBuffer(toArrayBuffer('[]'));
    assert.deepStrictEqual(result, []);
  });

  test('parses nested arrays', () => {
    const result = parseFromBuffer(toArrayBuffer('[[1,2],[3,4]]'));
    assert.deepStrictEqual(result, [[1, 2], [3, 4]]);
  });

  test('parses string value', () => {
    const result = parseFromBuffer(toArrayBuffer('"hello"'));
    assert.strictEqual(result, 'hello');
  });

  test('parses integer', () => {
    const result = parseFromBuffer(toArrayBuffer('42'));
    assert.strictEqual(result, 42);
  });

  test('parses negative number', () => {
    const result = parseFromBuffer(toArrayBuffer('-17'));
    assert.strictEqual(result, -17);
  });

  test('parses floating point number', () => {
    const result = parseFromBuffer(toArrayBuffer('3.14159'));
    assert.strictEqual(result, 3.14159);
  });

  test('parses boolean true', () => {
    const result = parseFromBuffer(toArrayBuffer('true'));
    assert.strictEqual(result, true);
  });

  test('parses boolean false', () => {
    const result = parseFromBuffer(toArrayBuffer('false'));
    assert.strictEqual(result, false);
  });

  test('parses null', () => {
    const result = parseFromBuffer(toArrayBuffer('null'));
    assert.strictEqual(result, null);
  });

  test('parses escaped quotes', () => {
    const result = parseFromBuffer(toArrayBuffer('{"message":"Hello \\"World\\""}'));
    assert.strictEqual(result.message, 'Hello "World"');
  });

  test('parses escaped backslash', () => {
    const result = parseFromBuffer(toArrayBuffer('{"path":"C:\\\\Users"}'));
    assert.strictEqual(result.path, 'C:\\Users');
  });

  test('parses escaped newline', () => {
    const result = parseFromBuffer(toArrayBuffer('{"text":"line1\\nline2"}'));
    assert.strictEqual(result.text, 'line1\nline2');
  });
});
