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
const defaults = { message, nonce, parallelism: 1, tagLength: 64, memory: 8, passes: 3 };

const good = [
  // test vectors from RFC 9106 https://www.rfc-editor.org/rfc/rfc9106.html#name-test-vectors
  // and OpenSSL 3.2 https://github.com/openssl/openssl/blob/6dfa998f7ea150f9c6d4e4727cf6d5c82a68a8da/test/recipes/30-test_evp_data/evpkdf_argon2.txt
  //
  // OpenSSL defaults are:
  // - outlen: 64
  // - passes: 3
  // - parallelism: 1
  // - memory: 8
  // https://github.com/openssl/openssl/blob/6dfa998f7ea150f9c6d4e4727cf6d5c82a68a8da/providers/implementations/kdfs/argon2.c#L77-L82
  [
    'argon2d',
    { secret, associatedData, parallelism: 4, tagLength: 32, memory: 32 },
    '512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb',
  ],
  [
    'argon2i',
    { secret, associatedData, parallelism: 4, tagLength: 32, memory: 32 },
    'c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8',
  ],
  [
    'argon2id',
    { secret, associatedData, parallelism: 4, tagLength: 32, memory: 32 },
    '0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659',
  ],
  [
    'argon2d',
    { message: '1234567890', nonce: 'saltsalt' },
    'd16ad773b1c6400d3193bc3e66271603e9de72bace20af3f89c236f5434cdec99072ddfc6b9c77ea9f386c0e8d7cb0c37cec6ec3277a22c92d5be58ef67c7eaa',
  ],
  [
    'argon2id',
    { message: '', parallelism: 4, tagLength: 32, memory: 32 },
    '0a34f1abde67086c82e785eaf17c68382259a264f4e61b91cd2763cb75ac189a',
  ],
  [
    'argon2d',
    { message: '1234567890', nonce: 'saltsalt', parallelism: 2, memory: 65536 },
    '5ca0ab135de1241454840172696c301c7b8fd99a788cd11cf9699044cadf7fca0a6e3762cb3043a71adf6553db3fd7925101b0ccf8868b098492a4addb2486bc',
  ],
  [
    'argon2i',
    { parallelism: 4, tagLength: 32, memory: 32 },
    'a9a7510e6db4d588ba3414cd0e094d480d683f97b9ccb612a544fe8ef65ba8e0',
  ],
  [
    'argon2id',
    { parallelism: 4, tagLength: 32, memory: 32 },
    '03aab965c12001c9d7d0d2de33192c0494b684bb148196d73c1df1acaf6d0c2e',
  ],
  [
    'argon2d',
    { message: '1234567890', nonce: 'saltsalt', parallelism: 2, tagLength: 128, memory: 65536 },
    'a86c83a19f0b234ecba8c275d16d059153f961e4c39ec9b1be98b3e73d791789363682443ad594334048634e91c493affed0bc29fd329a0e553c00149d6db19af4e4a354aec14dbd575d78ba87d4a4bc4746666e7a4e6ee1572bbffc2eba308a2d825cb7b41fde3a95d5cff0dfa2d0fdd636b32aea8b4a3c532742d330bd1b90',
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

for (const [algorithm, overrides, param] of bad) {
  const expected = {
    code: 'ERR_OUT_OF_RANGE',
    message: new RegExp(`The value of "${param}" is out of range`),
  };
  const parameters = { ...defaults, ...overrides };
  assert.throws(() => crypto.argon2(algorithm, parameters, () => {}), expected);
  assert.throws(() => crypto.argon2Sync(algorithm, parameters), expected);
}

for (const key of Object.keys(defaults)) {
  const expected = {
    code: 'ERR_INVALID_ARG_TYPE',
    message: new RegExp(`"parameters.${key}"`),
  };
  const parameters = { ...defaults };
  delete parameters[key];
  assert.throws(() => crypto.argon2('argon2id', parameters, () => {}), expected);
  assert.throws(() => crypto.argon2Sync('argon2id', parameters), expected);
}

{
  const expected = { code: 'ERR_INVALID_ARG_TYPE' };
  assert.throws(() => crypto.argon2(), expected);
  assert.throws(() => crypto.argon2('argon2id', null), expected);
  assert.throws(() => crypto.argon2('argon2id', defaults, null), expected);
  assert.throws(() => crypto.argon2('argon2id', defaults, {}), expected);
}
