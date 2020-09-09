'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { Buffer } = require('buffer');
const assert = require('assert');

const {
  randomFill,
  randomFillSync,
  randomBytes
} = require('crypto');

let kData;
try {
  kData = Buffer.alloc(2 ** 31 + 1);
} catch {
  common.skip('not enough memory');
}

assert.throws(() => randomFill(kData, common.mustNotCall()), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => randomFillSync(kData), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => randomBytes(2 ** 31 + 1), {
  code: 'ERR_OUT_OF_RANGE'
});
