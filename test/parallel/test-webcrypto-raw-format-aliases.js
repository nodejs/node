'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasFIPS } = require('../common/crypto');
const { subtle } = globalThis.crypto;
const rejectsXCurves = hasFIPS(3, 5);

function getAlgorithmName(algorithm) {
  return typeof algorithm === 'string' ? algorithm : algorithm.name;
}

function assertImportNotSupported(format, keyData, algorithm, extractable, usages) {
  return assert.rejects(
    subtle.importKey(format, keyData, algorithm, extractable, usages),
    {
      name: 'NotSupportedError',
      message: `Unable to import ${getAlgorithmName(algorithm)} using ${format} format`,
    });
}

async function assertSecretKeyDoesNotAcceptRawPublic(algorithm) {
  const keyData = new Uint8Array(32);
  await assertImportNotSupported(
    'raw-public',
    keyData,
    algorithm,
    false,
    ['deriveBits']);
}

async function assertPublicKeyDoesNotAcceptRawSecret(
  algorithm,
  generateUsages,
  importUsages,
) {
  const { publicKey } = await subtle.generateKey(
    algorithm,
    true,
    generateUsages);
  const keyData = await subtle.exportKey('raw', publicKey);

  await assertImportNotSupported(
    'raw-secret',
    keyData,
    algorithm,
    true,
    importUsages);
}

const tests = [
  assertSecretKeyDoesNotAcceptRawPublic('HKDF'),
  assertSecretKeyDoesNotAcceptRawPublic('PBKDF2'),
  assertPublicKeyDoesNotAcceptRawSecret(
    { name: 'ECDSA', namedCurve: 'P-256' },
    ['sign', 'verify'],
    ['verify']),
  assertPublicKeyDoesNotAcceptRawSecret(
    { name: 'ECDH', namedCurve: 'P-256' },
    ['deriveBits'],
    []),
  assertPublicKeyDoesNotAcceptRawSecret(
    'Ed25519',
    ['sign', 'verify'],
    ['verify']),
];

if (rejectsXCurves) {
  tests.push(assert.rejects(
    subtle.generateKey('X25519', true, ['deriveBits']),
    (err) => err.name === 'OperationError' &&
             err.cause?.code === 'ERR_OSSL_EVP_UNSUPPORTED'));
} else {
  tests.push(assertPublicKeyDoesNotAcceptRawSecret(
    'X25519',
    ['deriveBits'],
    []));
}

Promise.all(tests).then(common.mustCall());
