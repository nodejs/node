'use strict';

const {
  SafeSet,
} = primordials;

const {
  ChaCha20Poly1305CipherJob,
  SecretKeyGenJob,
  kCryptoJobAsync,
} = internalBinding('crypto');

const {
  getUsagesMask,
  hasAnyNotIn,
  jobPromise,
  kHandle,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  InternalCryptoKey,
  getCryptoKeyHandle,
} = require('internal/crypto/keys');

const {
  importJwkSecretKey,
  importSecretKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

function validateKeyLength(length) {
  if (length !== 256)
    throw lazyDOMException('Invalid key length', 'DataError');
}

function c20pCipher(mode, key, data, algorithm) {
  return jobPromise(() => new ChaCha20Poly1305CipherJob(
    kCryptoJobAsync,
    mode,
    getCryptoKeyHandle(key),
    data,
    algorithm.iv,
    algorithm.additionalData));
}

async function c20pGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const checkUsages = ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'];

  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, checkUsages)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${algorithm.name} key`,
      'SyntaxError');
  }

  const handle = await jobPromise(() => new SecretKeyGenJob(kCryptoJobAsync, 256));

  return new InternalCryptoKey(
    handle,
    { name },
    getUsagesMask(usagesSet),
    extractable);
}

function c20pImportKey(
  algorithm,
  format,
  keyData,
  extractable,
  keyUsages) {
  const { name } = algorithm;
  const checkUsages = ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'];

  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, checkUsages)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${algorithm.name} key`,
      'SyntaxError');
  }

  let handle;
  switch (format) {
    case 'KeyObject': {
      handle = keyData[kHandle];
      break;
    }
    case 'raw-secret': {
      handle = importSecretKey(keyData);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'oct', extractable, usagesSet, 'enc');

      handle = importJwkSecretKey(keyData);

      if (keyData.alg !== undefined && keyData.alg !== 'C20P') {
        throw lazyDOMException(
          'JWK "alg" does not match the requested algorithm',
          'DataError');
      }

      break;
    }
    default:
      return undefined;
  }

  validateKeyLength(handle.getSymmetricKeySize() * 8);

  return new InternalCryptoKey(
    handle,
    { name },
    getUsagesMask(usagesSet),
    extractable);
}

module.exports = {
  c20pCipher,
  c20pGenerateKey,
  c20pImportKey,
};
