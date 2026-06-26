'use strict';

const {
  ChaCha20Poly1305CipherJob,
  SecretKeyGenJob,
  kCryptoJobWebCrypto,
} = internalBinding('crypto');

const {
  getUsagesMask,
  jobPromise,
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
  validateKeyUsages,
  validateUsagesNotEmpty,
} = require('internal/crypto/webcrypto_util');

const kUsages = ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'];

function validateKeyLength(length) {
  if (length !== 256)
    throw lazyDOMException('Invalid key length', 'DataError');
}

function c20pCipher(mode, key, data, algorithm) {
  return jobPromise(() => new ChaCha20Poly1305CipherJob(
    kCryptoJobWebCrypto,
    mode,
    getCryptoKeyHandle(key),
    data,
    algorithm.iv,
    algorithm.additionalData));
}

function c20pGenerateKey(algorithm, extractable, usages) {
  const { name } = algorithm;

  const usagesSet = validateUsagesNotEmpty(
    validateKeyUsages(usages, kUsages, name));

  return jobPromise(() => new SecretKeyGenJob(
    kCryptoJobWebCrypto,
    256,
    { name },
    getUsagesMask(usagesSet),
    extractable));
}

function c20pImportKey(
  algorithm,
  format,
  keyData,
  extractable,
  usages) {
  const { name } = algorithm;

  const usagesSet = validateKeyUsages(
    usages, kUsages, name);

  let handle;
  switch (format) {
    case 'KeyObjectHandle': {
      handle = keyData;
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
