'use strict';

const {
  ArrayFrom,
  SafeSet,
} = primordials;

const {
  ChaCha20Poly1305CipherJob,
  KeyObjectHandle,
  kCryptoJobAsync,
  kKeyFormatJWK,
  kKeyTypeSecret,
} = internalBinding('crypto');

const {
  hasAnyNotIn,
  jobPromise,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
  promisify,
} = require('internal/util');

const {
  InternalCryptoKey,
  SecretKeyObject,
  createSecretKey,
} = require('internal/crypto/keys');

const {
  validateJwk,
} = require('internal/crypto/webcrypto_util');

const {
  randomBytes: _randomBytes,
} = require('internal/crypto/random');

const randomBytes = promisify(_randomBytes);

function validateKeyLength(length) {
  if (length !== 256)
    throw lazyDOMException('Invalid key length', 'DataError');
}

function c20pCipher(mode, key, data, algorithm) {
  return jobPromise(() => new ChaCha20Poly1305CipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
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

  let keyData;
  try {
    keyData = await randomBytes(32);
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason' +
      `[${err.message}]`,
      { name: 'OperationError', cause: err });
  }

  return new InternalCryptoKey(
    createSecretKey(keyData),
    { name },
    ArrayFrom(usagesSet),
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

  let keyObject;
  switch (format) {
    case 'KeyObject': {
      keyObject = keyData;
      break;
    }
    case 'raw-secret': {
      keyObject = createSecretKey(keyData);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'oct', extractable, usagesSet, 'enc');

      const handle = new KeyObjectHandle();
      try {
        handle.init(kKeyTypeSecret, keyData, kKeyFormatJWK, null, null);
      } catch (err) {
        throw lazyDOMException(
          'Invalid keyData', { name: 'DataError', cause: err });
      }

      if (keyData.alg !== undefined && keyData.alg !== 'C20P') {
        throw lazyDOMException(
          'JWK "alg" does not match the requested algorithm',
          'DataError');
      }

      keyObject = new SecretKeyObject(handle);
      break;
    }
    default:
      return undefined;
  }

  validateKeyLength(keyObject.symmetricKeySize * 8);

  return new InternalCryptoKey(
    keyObject,
    { name },
    keyUsages,
    extractable);
}

module.exports = {
  c20pCipher,
  c20pGenerateKey,
  c20pImportKey,
};
