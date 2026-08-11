// Flags: --expose-internals

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { createRequire } from 'node:module';
import { hasFIPS } from '../common/crypto.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

if (!hasFIPS(3))
  common.skip('requires OpenSSL >= 3 in FIPS mode');

const require = createRequire(import.meta.url);
const { internalBinding } = require('internal/test/binding');
const { getCryptoKeyHandle } = require('internal/crypto/keys');
const {
  CShakeJob,
  KangarooTwelveJob,
  KmacJob,
  TurboShakeJob,
  kCryptoJobWebCrypto,
  kSignJobModeSign,
} = internalBinding('crypto');
const { subtle } = globalThis.crypto;
const { SubtleCrypto } = globalThis;
const data = new Uint8Array();

async function assertFipsException(operation, algorithm, fn, message) {
  assert.strictEqual(SubtleCrypto.supports(operation, algorithm), false);
  await assert.rejects(fn(), {
    name: 'NotSupportedError',
    message,
  });
}

for (const algorithm of [
  { name: 'turboshake128', outputLength: 128 },
  { name: 'TurboSHAKE256', outputLength: 256 },
  { name: 'KT128', outputLength: 128 },
  { name: 'KT256', outputLength: 256, customization: data },
]) {
  await assertFipsException(
    'digest',
    algorithm,
    () => subtle.digest(algorithm, data),
    'Unrecognized algorithm name');
}

for (const createJob of [
  () => new TurboShakeJob(
    kCryptoJobWebCrypto, 'TurboSHAKE128', 0x1f, 16, data),
  () => new KangarooTwelveJob(
    kCryptoJobWebCrypto, 'KT128', undefined, 16, data),
  () => new CShakeJob(
    kCryptoJobWebCrypto,
    'cSHAKE128',
    data,
    Buffer.from('KMAC'),
    undefined,
    128),
]) {
  assert.throws(createJob, {
    code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
    message: 'Unsupported crypto operation',
  });
}

const emptyCShake = {
  name: 'cSHAKE128',
  outputLength: 256,
  customization: data,
  functionName: data,
};
assert.strictEqual(SubtleCrypto.supports('digest', emptyCShake), true);

for (const length of [1, 513]) {
  const algorithm = {
    name: 'cSHAKE128',
    outputLength: 256,
    customization: new Uint8Array(length),
  };
  await assertFipsException(
    'digest',
    algorithm,
    () => subtle.digest(algorithm, data),
    'Unsupported CShakeParams customization');
}

const functionName = {
  name: 'cSHAKE256',
  outputLength: 256,
  functionName: Buffer.from('KMAC'),
};
await assertFipsException(
  'digest',
  functionName,
  () => subtle.digest(functionName, data),
  'Unsupported CShakeParams functionName');

const bothCShakeParams = {
  ...functionName,
  customization: new Uint8Array(1),
};
await assertFipsException(
  'digest',
  bothCShakeParams,
  () => subtle.digest(bothCShakeParams, data),
  'Unsupported CShakeParams customization');

for (const length of [0, 24, 33]) {
  const algorithm = { name: 'KMAC128', length };
  await assertFipsException(
    'generateKey',
    algorithm,
    () => subtle.generateKey(algorithm, false, ['sign', 'verify']),
    'Invalid key length');
  await assertFipsException(
    'importKey',
    algorithm,
    () => subtle.importKey(
      'raw-secret',
      new Uint8Array(length === 24 ? 4 : Math.ceil(length / 8)),
      algorithm,
      false,
      ['sign', 'verify']),
    'Invalid key length');
}

const minimumKmac = { name: 'KMAC128', length: 32 };
assert.strictEqual(
  SubtleCrypto.supports('generateKey', minimumKmac), true);
assert.strictEqual(
  SubtleCrypto.supports('importKey', minimumKmac), true);
await assert.rejects(
  subtle.importKey(
    'raw-secret',
    new Uint8Array(5),
    minimumKmac,
    false,
    ['sign', 'verify']), {
    name: 'DataError',
    message: 'Invalid key length',
  });

for (const length of [0, 3]) {
  await assert.rejects(
    subtle.importKey(
      'raw-secret',
      new Uint8Array(length),
      'KMAC128',
      false,
      ['sign', 'verify']), {
      name: 'NotSupportedError',
      message: 'Invalid key length',
    });
}
const key = await subtle.importKey(
  'raw-secret',
  new Uint8Array(4),
  'KMAC128',
  false,
  ['sign', 'verify']);
assert.strictEqual(key.algorithm.length, 32);

await assert.rejects(
  new KmacJob(
    kCryptoJobWebCrypto,
    kSignJobModeSign,
    getCryptoKeyHandle(key),
    'KMAC128',
    undefined,
    32,
    9,
    data,
    undefined).run(),
  (err) => {
    assert.strictEqual(err.name, 'OperationError');
    assert.strictEqual(err.cause?.code, 'ERR_CRYPTO_OPERATION_FAILED');
    return true;
  });

const minimumOutput = { name: 'KMAC128', outputLength: 8 };
assert.strictEqual(SubtleCrypto.supports('sign', minimumOutput), true);
assert.strictEqual(SubtleCrypto.supports('verify', minimumOutput), true);
for (const outputLength of [0, 9]) {
  const algorithm = { name: 'KMAC128', outputLength };
  await assertFipsException(
    'sign',
    algorithm,
    () => subtle.sign(algorithm, key, data),
    'Invalid KmacParams outputLength');
  await assertFipsException(
    'verify',
    algorithm,
    () => subtle.verify(algorithm, key, data, data),
    'Invalid KmacParams outputLength');
}
