'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  createCipheriv,
  createHash,
  getCiphers,
  getCipherInfo,
} = require('crypto');
const { hasFIPS, hasOpenSSL, isBoringSSL } = require('../common/crypto');

const assert = require('assert');

const ciphers = getCiphers();

assert.strictEqual(getCipherInfo(-1), undefined);
assert.strictEqual(getCipherInfo('cipher that does not exist'), undefined);
if (hasOpenSSL(3)) {
  assert.deepStrictEqual(
    ciphers.filter((cipher) => cipher.includes('cbc-hmac')), []);
  for (const cipher of [
    'null',
    'aes-128-cbc-hmac-sha1',
    'aes-256-cbc-hmac-sha1',
    'aes-128-cbc-hmac-sha256',
    'aes-256-cbc-hmac-sha256',
    'aes-128-cbc-hmac-sha1-etm',
    'aes-192-cbc-hmac-sha1-etm',
    'aes-256-cbc-hmac-sha1-etm',
    'aes-128-cbc-hmac-sha256-etm',
    'aes-192-cbc-hmac-sha256-etm',
    'aes-256-cbc-hmac-sha256-etm',
    'aes-128-cbc-hmac-sha512-etm',
    'aes-192-cbc-hmac-sha512-etm',
    'aes-256-cbc-hmac-sha512-etm',
  ]) {
    assert(!ciphers.includes(cipher));
    assert.strictEqual(getCipherInfo(cipher), undefined);
    assert.throws(
      () => createCipheriv(cipher, Buffer.alloc(16), Buffer.alloc(16)), {
        code: 'ERR_CRYPTO_UNKNOWN_CIPHER',
      });
  }
}

if (ciphers.includes('aes-128-wrap-inv')) {
  const alias = 'aes128-wrap-inv';
  assert(ciphers.includes(alias));
  assert.deepStrictEqual(getCipherInfo(alias),
                         getCipherInfo('aes-128-wrap-inv'));
}
assert(!ciphers.some((cipher) => /^\d+(?:\.\d+)+$/.test(cipher)));

if (!isBoringSSL) {
  // A failed provider fetch must not contaminate the OpenSSL error queue.
  assert.throws(() => createHash('sha256', { outputLength: 28 }), {
    code: 'ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH',
  });
}

for (const cipher of ciphers) {
  const info = getCipherInfo(cipher);
  if (isBoringSSL && !info) {
    // BoringSSL reports some legacy ciphers in getCiphers() but returns no
    // info for them (e.g. des-ede3, des-ede3-ecb, rc2-40-cbc).
    common.printSkipMessage(`Skipping unsupported ${cipher} test case`);
    continue;
  }
  assert(info);
  if (info.nid !== undefined) {
    const info2 = getCipherInfo(info.nid);
    assert.deepStrictEqual(info, info2);
  }
}

const info = getCipherInfo('aes-128-cbc');
assert.strictEqual(info.name, 'aes-128-cbc');
assert.strictEqual(info.nid, 419);
assert.strictEqual(info.blockSize, 16);
assert.strictEqual(info.ivLength, 16);
assert.strictEqual(info.keyLength, 16);
assert.strictEqual(info.mode, 'cbc');

[null, undefined, [], {}].forEach((arg) => {
  assert.throws(() => getCipherInfo(arg), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[null, '', 1, true].forEach((options) => {
  assert.throws(
    () => getCipherInfo('aes-192-cbc', options), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
});

[null, '', {}, [], true].forEach((len) => {
  assert.throws(
    () => getCipherInfo('aes-192-cbc', { keyLength: len }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  assert.throws(
    () => getCipherInfo('aes-192-cbc', { ivLength: len }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
});

assert(!getCipherInfo('aes-128-cbc', { keyLength: 12 }));
assert(getCipherInfo('aes-128-cbc', { keyLength: 16 }));
assert(!getCipherInfo('aes-128-cbc', { ivLength: 12 }));
assert(getCipherInfo('aes-128-cbc', { ivLength: 16 }));

assert(!getCipherInfo('aes-128-ccm', { ivLength: 1 }));
assert(!getCipherInfo('aes-128-ccm', { ivLength: 14 }));
if (!isBoringSSL) {
  for (let n = 7; n <= 13; n++)
    assert(getCipherInfo('aes-128-ccm', { ivLength: n }));
} else {
  common.printSkipMessage('Skipping unsupported aes-128-ccm test cases');
}

assert(!getCipherInfo('aes-128-ocb', { ivLength: 16 }));
if (hasFIPS(3)) {
  assert.strictEqual(
    getCipherInfo('aes-128-ocb', { ivLength: 12 }), undefined);
} else if (!isBoringSSL) {
  for (let n = 1; n < 16; n++)
    assert(getCipherInfo('aes-128-ocb', { ivLength: n }));
} else {
  common.printSkipMessage('Skipping unsupported aes-128-ocb test cases');
}

if (ciphers.includes('aes-128-cbc-cts')) {
  const info = getCipherInfo('aes-128-cbc-cts');
  assert.strictEqual(info.name, 'aes-128-cbc-cts');
  assert.strictEqual(info.mode, 'cbc');
  assert.strictEqual(info.keyLength, 16);
  assert.strictEqual(info.blockSize, 16);
  assert.strictEqual(info.ivLength, 16);
  assert(getCipherInfo('aes-128-cbc-cts', { ivLength: 16 }));
  assert(!getCipherInfo('aes-128-cbc-cts', { ivLength: 15 }));
} else {
  common.printSkipMessage('Skipping unsupported aes-128-cbc-cts test cases');
}

if (ciphers.includes('aes-128-siv')) {
  const info = getCipherInfo('aes-128-siv');
  assert.strictEqual(info.name, 'aes-128-siv');
  assert.strictEqual(info.mode, 'siv');
  assert.strictEqual(info.keyLength, 32);
  assert.strictEqual(info.ivLength, undefined);
  assert(getCipherInfo('aes-128-siv', { ivLength: 0 }));
  assert(!getCipherInfo('aes-128-siv', { ivLength: 1 }));
} else {
  common.printSkipMessage('Skipping unsupported aes-128-siv test cases');
}

if (ciphers.includes('aes-128-gcm-siv')) {
  const info = getCipherInfo('aes-128-gcm-siv');
  assert.strictEqual(info.name, 'aes-128-gcm-siv');
  assert.strictEqual(info.mode, 'gcm-siv');
  assert.strictEqual(info.keyLength, 16);
  assert.strictEqual(info.ivLength, 12);
  assert(getCipherInfo('aes-128-gcm-siv', { ivLength: 12 }));
  assert(!getCipherInfo('aes-128-gcm-siv', { ivLength: 11 }));
} else {
  common.printSkipMessage('Skipping unsupported aes-128-gcm-siv test cases');
}

for (const [name, mode, keyLength, ivLength] of [
  ['sm4-gcm', 'gcm', 16, 12],
  ['sm4-ccm', 'ccm', 16, 12],
  ['sm4-xts', 'xts', 32, 16],
]) {
  if (ciphers.includes(name)) {
    const info = getCipherInfo(name);
    assert.strictEqual(info.name, name);
    assert.strictEqual(info.mode, mode);
    assert.strictEqual(info.nid, undefined);
    assert.strictEqual(info.keyLength, keyLength);
    assert.strictEqual(info.ivLength, ivLength);
  } else {
    common.printSkipMessage(`Skipping unsupported ${name} test cases`);
  }
}
