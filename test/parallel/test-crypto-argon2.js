'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (!hasOpenSSL(3, 2))
  common.skip('requires OpenSSL >= 3.2');

const assert = require('node:assert');
const crypto = require('node:crypto');

function runArgon2(algorithm, options) {
  const syncResult = crypto.argon2Sync(algorithm, options);

  crypto.argon2(algorithm, options,
                common.mustSucceed((asyncResult) => {
                  assert.deepStrictEqual(asyncResult, syncResult);
                }));

  return syncResult;
}

const message = Buffer.alloc(32, 0x01);
const nonce = Buffer.alloc(16, 0x02);
const secret = Buffer.alloc(8, 0x03);
const associatedData = Buffer.alloc(12, 0x04);
const defaults = { message, nonce, parallelism: 4, tagLength: 32, memory: 64 << 10, passes: 3 };

const good = [
  [
    'argon2d',
    {},
    'bf37a2a7530e053a8bd2784a9d50dc7de451e33bd581096922bc7f9ef66020ed',
  ],
  [
    'argon2i',
    {},
    '96334882febdd85eb9b2cf367479fbad2d5b87cf79f9076f51b23589560a2e0a',
  ],
  [
    'argon2id',
    {},
    'fedd802b86b17230843c6d3f025c81d3f472fbf9daaf26897fa88844732167ec',
  ],
  [
    'argon2id',
    { secret },
    '71d8a1361e3660f8e8158021a6870f223d42342c6a194429252c6d6ad19c25d6',
  ],
  [
    'argon2id',
    { associatedData },
    '7da97d5dd2ba433dbb1b26e00a496caa673501b9a437e863855201eb51cea6ef',
  ],
  [
    'argon2id',
    { secret, associatedData },
    '47a677ebc68412b8e17949c921fdca065352114ffb2a09eebe61adc451eecf1e',
  ],
  [
    'argon2id',
    { passes: 2 },
    '2139b90863e8bbffff1459c9f08f6460db08def592991e78f11b6c5765bd1fac',
  ],
  [
    'argon2id',
    { memory: 32 << 10 },
    '2beec876bf5b1098e8349d094385eda1dd472733d103e0f1afad4d28f948e808',
  ],
];

// Test vectors that should fail.
const bad = [
  ['argon2id', { nonce: nonce.subarray(0, 7) }, 'parameters.nonce.byteLength'], // nonce.byteLength < 8
  ['argon2id', { tagLength: 3 }, 'parameters.tagLength'], // tagLength < 4
  ['argon2id', { tagLength: 2 ** 32 }, 'parameters.tagLength'], // tagLength > 2^(32)-1
  ['argon2id', { passes: 0 }, 'parameters.passes'], // passes < 2
  ['argon2id', { passes: 2 ** 32 }, 'parameters.passes'], // passes > 2^(32)-1
  ['argon2id', { parallelism: 0 }, 'parameters.parallelism'], // parallelism < 1
  ['argon2id', { parallelism: 2 ** 24 }, 'parameters.parallelism'], // Parallelism > 2^(24)-1
  ['argon2id', { parallelism: 4, memory: 16 }, 'parameters.memory'], // Memory < 8 * parallelism
  ['argon2id', { memory: 2 ** 32 }, 'parameters.memory'], // memory > 2^(32)-1
];

for (const [algorithm, overrides, expected] of good) {
  const parameters = { ...defaults, ...overrides };
  const actual = runArgon2(algorithm, parameters);
  assert.strictEqual(actual.toString('hex'), expected);
}

for (const [algorithm, parameters, param] of bad) {
  const expected = {
    code: 'ERR_OUT_OF_RANGE',
    message: new RegExp(`The value of "${param}" is out of range`),
  };
  const params = { ...defaults, ...parameters };
  assert.throws(() => crypto.argon2(algorithm, params, () => {}), expected);
  assert.throws(() => crypto.argon2Sync(algorithm, params), expected);
}

{
  const expected = '07a0f408fd6bb1190be8e1600f680304e4680f72af121c3399308f48aba67ce4';
  const actual = crypto.argon2Sync('argon2id', { ...defaults, message: '', tagLength: 32 });
  assert.strictEqual(actual.toString('hex'), expected);
}

for (const key of Object.keys(defaults)) {
  const parameters = { ...defaults };
  delete parameters[key];
  assert.throws(() => crypto.argon2('argon2id', parameters, () => {}), { code: 'ERR_INVALID_ARG_TYPE', message: new RegExp(`"parameters\\.${key}"`) });
  assert.throws(() => crypto.argon2Sync('argon2id', parameters), { code: 'ERR_INVALID_ARG_TYPE', message: new RegExp(`"parameters\\.${key}"`) });
}

{
  const expected = { code: 'ERR_INVALID_ARG_TYPE' };
  assert.throws(() => crypto.argon2(), expected);
  assert.throws(() => crypto.argon2('argon2id', null), expected);
  assert.throws(() => crypto.argon2('argon2id', defaults, null), expected);
  assert.throws(() => crypto.argon2('argon2id', defaults, {}), expected);
}
