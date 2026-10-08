'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');

for (const buffer of [
  new SharedArrayBuffer(8),
  new SharedArrayBuffer(8, { maxByteLength: 16 }),
  new ArrayBuffer(8, { maxByteLength: 16 }),
]) {
  for (const Type of [Uint8Array, Float32Array, DataView]) {
    assert.throws(() => crypto.getRandomValues(new Type(buffer)), TypeError);
  }
}

assert.throws(() => crypto.getRandomValues(new Proxy(new Uint8Array(8), {})),
              TypeError);
