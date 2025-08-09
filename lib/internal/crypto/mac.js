'use strict';

const {
  ArrayFrom,
  SafeSet,
} = primordials;

const {
  HmacJob,
  KeyObjectHandle,
  kCryptoJobAsync,
  kSignJobModeSign,
  kSignJobModeVerify,
} = internalBinding('crypto');

const {
  getBlockSize,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  validateKeyOps,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
  promisify,
} = require('internal/util');

const {
  generateKey: _generateKey,
} = require('internal/crypto/keygen');

const {
  InternalCryptoKey,
  SecretKeyObject,
  createSecretKey,
} = require('internal/crypto/keys');

const generateKey = promisify(_generateKey);

async function hmacGenerateKey(algorithm, extractable, keyUsages) {
  const { hash, name } = algorithm;
  let { length } = algorithm;

  if (length === undefined)
    length = getBlockSize(hash.name);

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for an HMAC key',
      'SyntaxError');
  }

  const key = await generateKey('hmac', { length }).catch((err) => {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  });

  return new InternalCryptoKey(
    key,
    { name, length, hash },
    ArrayFrom(usageSet),
    extractable);
}

function hmacImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages,
) {
  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for an HMAC key',
      'SyntaxError');
  }
  let keyObject;
  switch (format) {
    case 'KeyObject': {
      keyObject = keyData;
      break;
    }
    case 'raw': {
      keyObject = createSecretKey(keyData);
      break;
    }
    case 'jwk': {
      if (!keyData.kty)
        throw lazyDOMException('Invalid keyData', 'DataError');

      if (keyData.kty !== 'oct')
        throw lazyDOMException('Invalid JWK "kty" Parameter', 'DataError');

      if (usagesSet.size > 0 &&
          keyData.use !== undefined &&
          keyData.use !== 'sig') {
        throw lazyDOMException('Invalid JWK "use" Parameter', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException(
          'JWK "ext" Parameter and extractable mismatch',
          'DataError');
      }

      if (keyData.alg !== undefined) {
        const expected =
          normalizeHashName(algorithm.hash.name, normalizeHashName.kContextJwkHmac);
        if (expected && keyData.alg !== expected)
          throw lazyDOMException(
            'JWK "alg" does not match the requested algorithm',
            'DataError');
      }

      const handle = new KeyObjectHandle();
      try {
        handle.initJwk(keyData);
      } catch (err) {
        throw lazyDOMException(
          'Invalid keyData', { name: 'DataError', cause: err });
      }
      keyObject = new SecretKeyObject(handle);
      break;
    }
    default:
      return undefined;
  }

  const { length } = keyObject[kHandle].keyDetail({});

  if (length === 0)
    throw lazyDOMException('Zero-length key is not supported', 'DataError');

  if (algorithm.length !== undefined &&
      algorithm.length !== length) {
    throw lazyDOMException('Invalid key length', 'DataError');
  }

  return new InternalCryptoKey(
    keyObject, {
      name: 'HMAC',
      hash: algorithm.hash,
      length,
    },
    keyUsages,
    extractable);
}

function hmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  return jobPromise(() => new HmacJob(
    kCryptoJobAsync,
    mode,
    normalizeHashName(key.algorithm.hash.name),
    key[kKeyObject][kHandle],
    data,
    signature));
}

module.exports = {
  hmacImportKey,
  hmacGenerateKey,
  hmacSignVerify,
};
