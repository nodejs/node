'use strict';

const {
  SafeSet,
  StringPrototypeSubstring,
} = primordials;

const {
  HmacJob,
  KmacJob,
  kSignJobModeSign,
  kSignJobModeVerify,
  SecretKeyGenJob,
} = internalBinding('crypto');

const {
  getBlockSize,
  getUsagesMask,
  getWebCryptoJobMode,
  getWebCryptoJobModeForInputLength,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  InternalCryptoKey,
  getCryptoKeyAlgorithm,
  getCryptoKeyHandle,
  getKeyObjectHandle,
  getKeyObjectSymmetricKeySize,
} = require('internal/crypto/keys');

const {
  importJwkSecretKey,
  importSecretKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

function hmacGenerateKey(algorithm, extractable, usages) {
  const {
    hash,
    name,
    length = getBlockSize(hash.name),
  } = algorithm;

  const usageSet = new SafeSet(usages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for an HMAC key',
      'SyntaxError');
  }
  if (usageSet.size === 0) {
    throw lazyDOMException(
      'Usages cannot be empty when creating a key.',
      'SyntaxError');
  }

  const jobMode = getWebCryptoJobMode();
  return jobPromise(() => new SecretKeyGenJob(
    jobMode,
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

  const usageSet = new SafeSet(usages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for ${name} key`,
      'SyntaxError');
  }
  if (usageSet.size === 0) {
    throw lazyDOMException(
      'Usages cannot be empty when creating a key.',
      'SyntaxError');
  }

  const jobMode = getWebCryptoJobMode();
  return jobPromise(() => new SecretKeyGenJob(
    jobMode,
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
  const usagesSet = new SafeSet(usages);
  if (hasAnyNotIn(usagesSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for ${algorithm.name} key`,
      'SyntaxError');
  }
  let handle;
  let length;
  switch (format) {
    case 'KeyObject': {
      length = getKeyObjectSymmetricKeySize(keyData) * 8;
      handle = getKeyObjectHandle(keyData);
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
  const signatureLength = signature?.byteLength ?? 0;
  const jobMode = getWebCryptoJobModeForInputLength(
    data.byteLength + signatureLength);
  return jobPromise(() => new HmacJob(
    jobMode,
    mode,
    normalizeHashName(getCryptoKeyAlgorithm(key).hash.name),
    getCryptoKeyHandle(key),
    data,
    signature));
}

function kmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const signatureLength = signature?.byteLength ?? 0;
  const outputByteLength = algorithm.outputLength / 8;
  const jobMode = getWebCryptoJobModeForInputLength(
    data.byteLength + signatureLength + outputByteLength);
  return jobPromise(() => new KmacJob(
    jobMode,
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
