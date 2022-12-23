'use strict';

require('../common');
const assert = require('assert');
const { isUtf8, Buffer } = require('buffer');
const { TextEncoder } = require('util');

const encoder = new TextEncoder();

assert.strictEqual(isUtf8(encoder.encode('hello')), true);
assert.strictEqual(isUtf8(encoder.encode('ğ')), true);
assert.strictEqual(isUtf8(Buffer.from([0xf8])), false);
assert.strictEqual(isUtf8(encoder.encode('aé日')), true);

[
  null,
  undefined,
  'hello',
  true,
  false,
].forEach((input) => {
  assert.throws(
    () => { isUtf8(input); },
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
    () => { isUtf8(arrayBuffer); },
    {
      code: 'ERR_BUFFER_CONTEXT_NOT_AVAILABLE'
    }
  );
}
