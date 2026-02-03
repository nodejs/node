'use strict';

const {
  ArrayFrom,
  SafeSet,
  StringPrototypeSubstring,
} = primordials;

const {
  HmacJob,
  KeyObjectHandle,
  KmacJob,
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
  randomBytes: _randomBytes,
} = require('internal/crypto/random');

const randomBytes = promisify(_randomBytes);

const {
  InternalCryptoKey,
  SecretKeyObject,
  createSecretKey,
  kAlgorithm,
} = require('internal/crypto/keys');

const generateKey = promisify(_generateKey);

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

  let key;
  try {
    key = await generateKey('hmac', { length });
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }

  return new InternalCryptoKey(
    key,
    { name, length, hash },
    ArrayFrom(usageSet),
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

  let keyData;
  try {
    keyData = await randomBytes(length / 8);
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason' +
      `[${err.message}]`,
      { name: 'OperationError', cause: err });
  }

  return new InternalCryptoKey(
    createSecretKey(keyData),
    { name, length },
    ArrayFrom(usageSet),
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
  let keyObject;
  switch (format) {
    case 'KeyObject': {
      keyObject = keyData;
      break;
    }
    case 'raw-secret':
    case 'raw': {
      if (format === 'raw' && !isHmac) {
        return undefined;
      }
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
        const expected = isHmac ?
          normalizeHashName(algorithm.hash.name, normalizeHashName.kContextJwkHmac) :
          `K${StringPrototypeSubstring(algorithm.name, 4)}`;
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

  const algorithmObject = {
    name: algorithm.name,
    length,
  };

  if (isHmac) {
    algorithmObject.hash = algorithm.hash;
  }

  return new InternalCryptoKey(
    keyObject,
    algorithmObject,
    keyUsages,
    extractable);
}

function hmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  return jobPromise(() => new HmacJob(
    kCryptoJobAsync,
    mode,
    normalizeHashName(key[kAlgorithm].hash.name),
    key[kKeyObject][kHandle],
    data,
    signature));
}

function kmacSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  return jobPromise(() => new KmacJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    algorithm.name,
    algorithm.customization,
    algorithm.length / 8,
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
