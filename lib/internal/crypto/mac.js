'use strict';

const {
  StringPrototypeSubstring,
} = primordials;

const {
  HmacJob,
  KmacJob,
  kCryptoJobWebCrypto,
  kSignJobModeSign,
  kSignJobModeVerify,
  SecretKeyGenJob,
} = internalBinding('crypto');

const {
  getBlockSize,
  getUsagesMask,
  jobPromise,
  normalizeHashName,
  numBitsToBytes,
  truncateToBitLength,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  InternalCryptoKey,
  getCryptoKeyAlgorithm,
  getCryptoKeyHandle,
} = require('internal/crypto/keys');

const {
  importJwkSecretKey,
  importSecretKey,
  validateJwk,
  validateKeyUsages,
  validateUsagesNotEmpty,
} = require('internal/crypto/webcrypto_util');

const kUsages = ['sign', 'verify'];

function normalizeKeyLength(handle, algorithm) {
  let length = handle.getSymmetricKeySize() * 8;
  if (length === 0 && algorithm.name === 'HMAC')
    throw lazyDOMException('Zero-length key is not supported', 'DataError');

  if (algorithm.length !== undefined) {
    const byteLength = numBitsToBytes(algorithm.length);
    if (byteLength !== handle.getSymmetricKeySize())
      throw lazyDOMException('Invalid key length', 'DataError');

    if (algorithm.length % 8 !== 0) {
      handle = importSecretKey(
        truncateToBitLength(algorithm.length, handle.export()));
    }

    length = algorithm.length;
  }

  return { handle, length };
}

function hmacGenerateKey(algorithm, extractable, usages) {
  const {
    hash,
    name,
    length = getBlockSize(hash.name),
  } = algorithm;

  const usageSet = validateUsagesNotEmpty(
    validateKeyUsages(usages, kUsages, name));

  return jobPromise(() => new SecretKeyGenJob(
    kCryptoJobWebCrypto,
    length,
    { name, length, hash },
    getUsagesMask(usageSet),
    extractable));
}

function kmacGenerateKey(algorithm, extractable, usages) {
  const {
    name,
    length = {
      __proto__: null,
      KMAC128: 128,
      KMAC256: 256,
    }[name],
  } = algorithm;

  const usageSet = validateUsagesNotEmpty(
    validateKeyUsages(usages, kUsages, name));

  return jobPromise(() => new SecretKeyGenJob(
    kCryptoJobWebCrypto,
    length,
    { name, length },
    getUsagesMask(usageSet),
    extractable));
}

function macImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  usages,
) {
  const isHmac = algorithm.name === 'HMAC';
  const usagesSet = validateKeyUsages(
    usages, kUsages, algorithm.name);
  let handle;
  let length;
  switch (format) {
    case 'KeyObjectHandle': {
      handle = keyData;
      break;
    }
    case 'raw-secret':
    case 'raw': {
      if (format === 'raw' && !isHmac) {
        return undefined;
      }
      handle = importSecretKey(keyData);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'oct', extractable, usagesSet, 'sig');

      if (keyData.alg !== undefined) {
        const expected = isHmac ?
          normalizeHashName(algorithm.hash.name, normalizeHashName.kContextJwkHmac) :
          `K${StringPrototypeSubstring(algorithm.name, 4)}`;
        if (expected && keyData.alg !== expected)
          throw lazyDOMException(
            'JWK "alg" does not match the requested algorithm',
            'DataError');
      }

      handle = importJwkSecretKey(keyData);
      break;
    }
    default:
      return undefined;
  }

  ({ handle, length } = normalizeKeyLength(handle, algorithm)); // eslint-disable-line prefer-const

  const algorithmObject = {
    name: algorithm.name,
    length,
  };

  if (isHmac) {
    algorithmObject.hash = algorithm.hash;
  }

  return new InternalCryptoKey(
    handle,
    algorithmObject,
    getUsagesMask(usagesSet),
    extractable);
}

function hmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  return jobPromise(() => new HmacJob(
    kCryptoJobWebCrypto,
    mode,
    normalizeHashName(getCryptoKeyAlgorithm(key).hash.name),
    getCryptoKeyHandle(key),
    data,
    signature));
}

function kmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  return jobPromise(() => new KmacJob(
    kCryptoJobWebCrypto,
    mode,
    getCryptoKeyHandle(key),
    algorithm.name,
    algorithm.customization,
    getCryptoKeyAlgorithm(key).length,
    algorithm.outputLength,
    data,
    signature));
}

module.exports = {
  macImportKey,
  hmacGenerateKey,
  hmacSignVerify,
  kmacGenerateKey,
  kmacSignVerify,
};
