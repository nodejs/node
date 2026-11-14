'use strict';

require('../common');
const assert = require('assert');
const { isAscii, Buffer } = require('buffer');
const { TextEncoder } = require('util');

const encoder = new TextEncoder();

assert.strictEqual(isAscii(encoder.encode('hello')), true);
assert.strictEqual(isAscii(encoder.encode('ğ')), false);
assert.strictEqual(isAscii(Buffer.from([])), true);
assert.strictEqual(isAscii(new DataView(new ArrayBuffer(0))), true);
assert.strictEqual(isAscii(new DataView(encoder.encode('hello').buffer)), true);
assert.strictEqual(isAscii(new DataView(encoder.encode('ğ').buffer)), false);

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
  const dataView = new DataView(arrayBuffer);
  const inputs = [
    arrayBuffer,
    typedArray,
    Buffer.from(arrayBuffer),
    dataView,
  ];
  for (const input of inputs) {
    assert.strictEqual(isAscii(input), false);
  }
  structuredClone(arrayBuffer, { transfer: [arrayBuffer] });
  for (const input of inputs) {
    assert.strictEqual(isAscii(input), true);
  }
}

{
  const buffer = Buffer.from('hello world');
  const view = new DataView(buffer.buffer, buffer.byteOffset, 5);
  assert.strictEqual(isAscii(view), true);
}
