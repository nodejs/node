'use strict';

const common = require('../common');
const assert = require('assert');

// utf8, ucs2, ascii, latin1, utf16le
const encodings = ['utf8', 'utf-8', 'ucs2', 'ucs-2', 'ascii', 'latin1',
                   'binary', 'utf16le', 'utf-16le'];

encodings
  .reduce((es, e) => es.concat(e, e.toUpperCase()), [])
  .forEach((encoding) => {
    assert.strictEqual(Buffer.from('foo', encoding).toString(encoding), 'foo');
  });

// base64
['base64', 'BASE64'].forEach((encoding) => {
  assert.strictEqual(Buffer.from('Zm9v', encoding).toString(encoding), 'Zm9v');
});

// hex
['hex', 'HEX'].forEach((encoding) => {
  assert.strictEqual(Buffer.from('666f6f', encoding).toString(encoding),
                     '666f6f');
});

// Invalid encodings
for (let i = 1; i < 10; i++) {
  const encoding = String(i).repeat(i);
  const error = common.expectsError({
    code: 'ERR_UNKNOWN_ENCODING',
    name: 'TypeError',
    message: `Unknown encoding: ${encoding}`
  });
  assert.ok(!Buffer.isEncoding(encoding));
  assert.throws(() => Buffer.from('foo').toString(encoding), error);
}
