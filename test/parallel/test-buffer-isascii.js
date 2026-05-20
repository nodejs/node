'use strict';

require('../common');
const assert = require('assert');
const { isAscii, Buffer } = require('buffer');
const { TextEncoder } = require('util');

const encoder = new TextEncoder();

assert.strictEqual(isAscii(encoder.encode('hello')), true);
assert.strictEqual(isAscii(encoder.encode('ğ')), false);
assert.strictEqual(isAscii(Buffer.from([])), true);

[
  undefined,
  '', 'hello',
  false, true,
  0, 1,
  0n, 1n,
  Symbol(),
  () => {},
  {}, [], null,
].forEach((input) => {
  assert.throws(
    () => { isAscii(input); },
    {
      code: 'ERR_INVALID_ARG_TYPE',
    },
  );
});

{
  // Test with detached array buffers
  const arrayBuffer = new ArrayBuffer(1024);
  structuredClone(arrayBuffer, { transfer: [arrayBuffer] });
  assert.throws(
    () => { isAscii(arrayBuffer); },
    {
      code: 'ERR_INVALID_STATE'
    }
  );
}
