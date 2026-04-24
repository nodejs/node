'use strict';

const {
  SafeSet,
  StringPrototypeSubstring,
} = primordials;

const {
  HmacJob,
  KmacJob,
  kCryptoJobAsync,
  kSignJobModeSign,
  kSignJobModeVerify,
  SecretKeyGenJob,
} = internalBinding('crypto');

const {
  getBlockSize,
  getUsagesMask,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  kHandle,
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
} = require('internal/crypto/webcrypto_util');

async function hmacGenerateKey(algorithm, extractable, keyUsages) {
  const {
    hash,
    name,
    length = getBlockSize(hash.name),
  } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for an HMAC key',
      'SyntaxError');
  }

  const handle = await jobPromise(() => new SecretKeyGenJob(kCryptoJobAsync, length));

  return new InternalCryptoKey(
    handle,
    { name, length, hash },
    getUsagesMask(usageSet),
    extractable);
}

async function kmacGenerateKey(algorithm, extractable, keyUsages) {
  const {
    name,
    length = {
      __proto__: null,
      KMAC128: 128,
      KMAC256: 256,
    }[name],
  } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for ${name} key`,
      'SyntaxError');
  }

  const handle = await jobPromise(() => new SecretKeyGenJob(kCryptoJobAsync, length));

  return new InternalCryptoKey(
    handle,
    { name, length },
    getUsagesMask(usageSet),
    extractable);
}

function macImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages,
) {
  const isHmac = algorithm.name === 'HMAC';
  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for ${algorithm.name} key`,
      'SyntaxError');
  }
  let handle;
  let length;
  switch (format) {
    case 'KeyObject': {
      length = keyData.symmetricKeySize * 8;
      handle = keyData[kHandle];
      break;
    }
    case 'raw-secret':
    case 'raw': {
      if (format === 'raw' && !isHmac) {
        return undefined;
      }
      length = keyData.byteLength * 8;
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
      length = handle.getSymmetricKeySize() * 8;
      break;
    }
    default:
      return undefined;
  }

  if (length === 0)
    throw lazyDOMException('Zero-length key is not supported', 'DataError');

  if (algorithm.length !== undefined &&
      algorithm.length !== length) {
    throw lazyDOMException('Invalid key length', 'DataError');
  }

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
    kCryptoJobAsync,
    mode,
    normalizeHashName(getCryptoKeyAlgorithm(key).hash.name),
    getCryptoKeyHandle(key),
    data,
    signature));
}

function kmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  return jobPromise(() => new KmacJob(
    kCryptoJobAsync,
    mode,
    getCryptoKeyHandle(key),
    algorithm.name,
    algorithm.customization,
    algorithm.outputLength / 8,
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
