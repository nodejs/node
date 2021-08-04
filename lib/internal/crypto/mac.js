'use strict';

const {
  ArrayFrom,
  Promise,
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
  getHashLength,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  validateBitLength,
  validateKeyOps,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  codes: {
    ERR_MISSING_OPTION,
    ERR_INVALID_ARG_TYPE,
  }
} = require('internal/errors');

const {
  generateKey,
} = require('internal/crypto/keygen');

const {
  InternalCryptoKey,
  SecretKeyObject,
  createSecretKey,
  isKeyObject,
} = require('internal/crypto/keys');

async function hmacGenerateKey(algorithm, extractable, keyUsages) {
  const { hash, name } = algorithm;
  let { length } = algorithm;
  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');

  if (length === undefined)
    length = getHashLength(hash.name);

  validateBitLength(length, 'algorithm.length', true);

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for an HMAC key',
      'SyntaxError');
  }
  return new Promise((resolve, reject) => {
    generateKey('hmac', { length }, (err, key) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      resolve(new InternalCryptoKey(
        key,
        { name, length, hash: { name: hash.name } },
        ArrayFrom(usageSet),
        extractable));
    });
  });
}

async function hmacImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {
  const { hash } = algorithm;
  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');

  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for an HMAC key',
      'SyntaxError');
  }
  let keyObject;
  switch (format) {
    case 'node.keyObject': {
      if (!isKeyObject(keyData))
        throw new ERR_INVALID_ARG_TYPE('keyData', 'KeyObject', keyData);

      if (keyData.type !== 'secret') {
        throw lazyDOMException(
          `Unable to import HMAC key with format ${format}`);
      }

      keyObject = keyData;
      break;
    }
    case 'raw': {
      const checkLength = keyData.byteLength * 8;

      if (checkLength === 0 || algorithm.length === 0)
        throw lazyDOMException('Zero-length key is not supported', 'DataError');

      // The Web Crypto spec allows for key lengths that are not multiples of
      // 8. We don't. Our check here is stricter than that defined by the spec
      // in that we require that algorithm.length match keyData.length * 8 if
      // algorithm.length is specified.
      if (algorithm.length !== undefined &&
          algorithm.length !== checkLength) {
        throw lazyDOMException('Invalid key length', 'DataError');
      }

      keyObject = createSecretKey(keyData);
      break;
    }
    case 'jwk': {
      if (keyData == null || typeof keyData !== 'object')
        throw lazyDOMException('Invalid JWK keyData', 'DataError');

      if (keyData.kty !== 'oct')
        throw lazyDOMException('Invalid key type', 'DataError');

      if (usagesSet.size > 0 &&
          keyData.use !== undefined &&
          keyData.use !== 'sig') {
        throw lazyDOMException('Invalid use type', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException('JWK is not extractable', 'DataError');
      }

      if (keyData.alg !== undefined) {
        if (typeof keyData.alg !== 'string')
          throw lazyDOMException('Invalid alg', 'DataError');
        switch (keyData.alg) {
          case 'HS1':
            if (algorithm.hash.name !== 'SHA-1')
              throw lazyDOMException('Digest algorithm mismatch', 'DataError');
            break;
          case 'HS256':
            if (algorithm.hash.name !== 'SHA-256')
              throw lazyDOMException('Digest algorithm mismatch', 'DataError');
            break;
          case 'HS384':
            if (algorithm.hash.name !== 'SHA-384')
              throw lazyDOMException('Digest algorithm mismatch', 'DataError');
            break;
          case 'HS512':
            if (algorithm.hash.name !== 'SHA-512')
              throw lazyDOMException('Digest algorithm mismatch', 'DataError');
            break;
          default:
            throw lazyDOMException('Unsupported digest algorithm', 'DataError');
        }
      }

      const handle = new KeyObjectHandle();
      handle.initJwk(keyData);
      keyObject = new SecretKeyObject(handle);
      break;
    }
    default:
      throw lazyDOMException(`Unable to import HMAC key with format ${format}`);
  }

  const { length } = keyObject[kHandle].keyDetail({});

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
  return jobPromise(new HmacJob(
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
