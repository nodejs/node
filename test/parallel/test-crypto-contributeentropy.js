'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const {
  contributeEntropy,
  randomFillSync,
} = require('crypto');

[1, 'hello', false, {}, []].forEach((i) => {
  assert.throws(() => contributeEntropy(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[
  Buffer.from('hello'),
  randomFillSync(new ArrayBuffer(10)),
  randomFillSync(new SharedArrayBuffer(10)),
  randomFillSync(new Uint8Array(10)),
  randomFillSync(new DataView(new ArrayBuffer(10)))
].forEach((i) => {
  const ret = contributeEntropy(i);
  assert.strictEqual(typeof ret, 'boolean');
});
