'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('requires OpenSSL >= 3.2');

const assert = require('node:assert');
const crypto = require('node:crypto');

function runArgon2(options) {
  const syncResult =
    crypto.argon2Sync(options);

  crypto.argon2(options,
                common.mustSucceed((asyncResult) => {
                  assert.deepStrictEqual(asyncResult, syncResult);
                }));

  return syncResult;
}

const message = Buffer.alloc(32, 0x01);
const nonce = Buffer.alloc(16, 0x02);
const secret = Buffer.alloc(8, 0x03);
const associatedData = Buffer.alloc(12, 0x04);
const defaults = { message, nonce, parallelism: 4, tagLength: 32, memory: 64 << 10, passes: 3, type: 'argon2id' };

const good = [
  [
    { type: 'argon2d' },
    'bf37a2a7530e053a8bd2784a9d50dc7de451e33bd581096922bc7f9ef66020ed',
  ],
  [
    { type: 'argon2i' },
    '96334882febdd85eb9b2cf367479fbad2d5b87cf79f9076f51b23589560a2e0a',
  ],
  [
    { type: 'argon2id' },
    'fedd802b86b17230843c6d3f025c81d3f472fbf9daaf26897fa88844732167ec',
  ],
  [
    { secret },
    '71d8a1361e3660f8e8158021a6870f223d42342c6a194429252c6d6ad19c25d6',
  ],
  [
    { associatedData },
    '7da97d5dd2ba433dbb1b26e00a496caa673501b9a437e863855201eb51cea6ef',
  ],
  [
    { secret, associatedData },
    '47a677ebc68412b8e17949c921fdca065352114ffb2a09eebe61adc451eecf1e',
  ],
  [
    { passes: 2 },
    '2139b90863e8bbffff1459c9f08f6460db08def592991e78f11b6c5765bd1fac',
  ],
  [
    { memory: 32 << 10 },
    '2beec876bf5b1098e8349d094385eda1dd472733d103e0f1afad4d28f948e808',
  ],
];

// Test vectors that should fail.
const bad = [
  [{ nonce: nonce.subarray(0, 7) }, 'nonce.byteLength'], // nonce.byteLength < 8
  [{ tagLength: 3 }, 'tagLength'], // tagLength < 4
  [{ tagLength: 2 ** 32 }, 'tagLength'], // tagLength > 2^(32)-1
  [{ passes: 0 }, 'passes'], // passes < 2
  [{ passes: 2 ** 32 }, 'passes'], // passes > 2^(32)-1
  [{ parallelism: 0 }, 'parallelism'], // parallelism < 1
  [{ parallelism: 2 ** 24 }, 'parallelism'], // Parallelism > 2^(24)-1
  [{ parallelism: 4, memory: 16 }, 'memory'], // Memory < 8 * parallelism
  [{ memory: 2 ** 32 }, 'memory'], // memory > 2^(32)-1
];

for (const [overrides, expected] of good) {
  const options = { ...defaults, ...overrides };
  const actual = runArgon2(options);
  assert.strictEqual(actual.toString('hex'), expected);
}

for (const [options, param] of bad) {
  const expected = {
    code: 'ERR_OUT_OF_RANGE',
    message: new RegExp(`The value of "${param}" is out of range`),
  };
  const optionsWithDefaults = { ...defaults, ...options };
  assert.throws(() => crypto.argon2(optionsWithDefaults, () => {}), expected);
  assert.throws(() => crypto.argon2Sync(optionsWithDefaults), expected);
}

{
  const expected = '07a0f408fd6bb1190be8e1600f680304e4680f72af121c3399308f48aba67ce4';
  const actual = crypto.argon2Sync({ ...defaults, message: '', tagLength: 32 });
  assert.strictEqual(actual.toString('hex'), expected);
}

for (const key of Object.keys(defaults)) {
  const options = { ...defaults };
  delete options[key];
  assert.throws(() => crypto.argon2(options, () => {}), { code: 'ERR_INVALID_ARG_TYPE', message: new RegExp(`"${key}"`) });
  assert.throws(() => crypto.argon2Sync(options), { code: 'ERR_INVALID_ARG_TYPE', message: new RegExp(`"${key}"`) });
}

{
  const expected = { code: 'ERR_INVALID_ARG_TYPE' };
  assert.throws(() => crypto.argon2(null), expected);
  assert.throws(() => crypto.argon2(defaults, null), expected);
  assert.throws(() => crypto.argon2(defaults, {}), expected);
}
