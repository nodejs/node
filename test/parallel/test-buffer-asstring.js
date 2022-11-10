'use strict';

const common = require('../common');
const assert = require('assert');

// utf8, ucs2, ascii, latin1, utf16le
const encodings = ['utf8', 'utf-8', 'ucs2', 'ucs-2', 'ascii', 'latin1',
                   'binary', 'utf16le', 'utf-16le'];

[
  {},
  new Boolean(true),
  { valueOf() { return null; } },
  { valueOf() { return undefined; } },
  { valueOf: null },
  Object.create(null),
  new Number(true),
  Symbol(),
  5n,
  (one, two, three) => {},
  undefined,
  null,
].forEach((input) => {
  const errObj = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
  };
  assert.throws(() => Buffer.asString(input), errObj);
  assert.throws(() => Buffer.asString(input, 'hex'), errObj);
});

{
  const value = Buffer.from('hello world');
  encodings
    .reduce((es, e) => es.concat(e, e.toUpperCase()), [])
    .forEach((encoding) => {
      assert.strictEqual(Buffer.asString(value, encoding), value.toString(encoding));
    });
}


// Invalid encodings
for (let i = 1; i < 10; i++) {
  const encoding = String(i).repeat(i);
  const error = common.expectsError({
    code: 'ERR_UNKNOWN_ENCODING',
    name: 'TypeError',
    message: `Unknown encoding: ${encoding}`
  });
  assert.ok(!Buffer.isEncoding(encoding));
  assert.throws(() => Buffer.asString([], encoding), error);
}
