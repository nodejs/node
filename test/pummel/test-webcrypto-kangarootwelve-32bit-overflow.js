'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

// KangarooTwelve: data + customization size_t overflow on 32-bit platforms.
// On 32-bit, msg_len + custom_len + LengthEncode(custom_len).size() can wrap
// size_t, causing an undersized allocation and heap buffer overflow.
// This test verifies the guard rejects such inputs.
// When kMaxLength < 2^32 two max-sized buffers can overflow a 32-bit size_t.

const { kMaxLength } = require('buffer');

if (kMaxLength >= 2 ** 32)
  common.skip('only relevant when kMaxLength < 2^32');

const assert = require('assert');
const { subtle } = globalThis.crypto;

let data, customization;
try {
  data = new Uint8Array(kMaxLength);
  customization = new Uint8Array(kMaxLength);
} catch {
  common.skip('insufficient memory to allocate test buffers');
}

(async () => {
  await assert.rejects(
    subtle.digest(
      { name: 'KT128', outputLength: 256, customization },
      data),
    { name: 'OperationError' });

  await assert.rejects(
    subtle.digest(
      { name: 'KT256', outputLength: 512, customization },
      data),
    { name: 'OperationError' });
})().then(common.mustCall());
