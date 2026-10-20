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
  // Detached array buffers and views are treated as empty.
  const arrayBuffer = new ArrayBuffer(1);
  const typedArray = new Uint8Array(arrayBuffer);
  typedArray[0] = 0xff;
  const inputs = [
    arrayBuffer,
    typedArray,
    Buffer.from(arrayBuffer),
  ];
  for (const input of inputs) {
    assert.strictEqual(isAscii(input), false);
  }
  structuredClone(arrayBuffer, { transfer: [arrayBuffer] });
  for (const input of inputs) {
    assert.strictEqual(isAscii(input), true);
  }
}
