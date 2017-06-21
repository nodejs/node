'use strict';

require('../common');
const assert = require('assert');

const outsideBounds = /^RangeError: Attempt to write outside buffer bounds$/;

assert.throws(() => Buffer.alloc(9).write('foo', -1), outsideBounds);
assert.throws(() => Buffer.alloc(9).write('foo', 10), outsideBounds);

const resultMap = new Map([
  ['utf8', Buffer.from([102, 111, 111, 0, 0, 0, 0, 0, 0])],
  ['ucs2', Buffer.from([102, 0, 111, 0, 111, 0, 0, 0, 0])],
  ['ascii', Buffer.from([102, 111, 111, 0, 0, 0, 0, 0, 0])],
  ['latin1', Buffer.from([102, 111, 111, 0, 0, 0, 0, 0, 0])],
  ['binary', Buffer.from([102, 111, 111, 0, 0, 0, 0, 0, 0])],
  ['utf16le', Buffer.from([102, 0, 111, 0, 111, 0, 0, 0, 0])],
  ['base64', Buffer.from([102, 111, 111, 0, 0, 0, 0, 0, 0])],
  ['hex', Buffer.from([102, 111, 111, 0, 0, 0, 0, 0, 0])]
]);

// utf8, ucs2, ascii, latin1, utf16le
const encodings = ['utf8', 'utf-8', 'ucs2', 'ucs-2', 'ascii', 'latin1',
                   'binary', 'utf16le', 'utf-16le'];

encodings
  .reduce((es, e) => es.concat(e, e.toUpperCase()), [])
  .forEach((encoding) => {
    const buf = Buffer.alloc(9);
    const len = Buffer.byteLength('foo', encoding);
    assert.strictEqual(buf.write('foo', 0, len, encoding), len);

    if (encoding.includes('-'))
      encoding = encoding.replace('-', '');

    assert.deepStrictEqual(buf, resultMap.get(encoding.toLowerCase()));
  });

// base64
['base64', 'BASE64'].forEach((encoding) => {
  const buf = Buffer.alloc(9);
  const len = Buffer.byteLength('Zm9v', encoding);

  assert.strictEqual(buf.write('Zm9v', 0, len, encoding), len);
  assert.deepStrictEqual(buf, resultMap.get(encoding.toLowerCase()));
});

// hex
['hex', 'HEX'].forEach((encoding) => {
  const buf = Buffer.alloc(9);
  const len = Buffer.byteLength('666f6f', encoding);

  assert.strictEqual(buf.write('666f6f', 0, len, encoding), len);
  assert.deepStrictEqual(buf, resultMap.get(encoding.toLowerCase()));
});

// Invalid encodings
for (let i = 1; i < 10; i++) {
  const encoding = String(i).repeat(i);
  const error = new RegExp(`^TypeError: Unknown encoding: ${encoding}$`);

  assert.ok(!Buffer.isEncoding(encoding));
  assert.throws(() => Buffer.alloc(9).write('foo', encoding), error);
}
