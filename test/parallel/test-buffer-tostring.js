'use strict';

const common = require('../common');
const assert = require('assert');

// utf8, ucs2, ascii, latin1, utf16le
for (const encoding of ['utf8', 'utf-8', 'ucs2', 'ucs-2', 'ascii', 'latin1',
                        'binary', 'utf16le', 'utf-16le'].flatMap((e) => [e, e.toUpperCase()])) {
  assert.strictEqual(Buffer.from('foo', encoding).toString(encoding), 'foo');
}

// Ignore an incomplete trailing code unit when decoding unaligned UTF-16LE.
for (const size of [514, 516]) {
  const buffer = Buffer.alloc(size, 0x61);
  assert.strictEqual(buffer.toString('utf16le', 1),
                     '\u6161'.repeat((size - 1) >>> 1));
}

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
