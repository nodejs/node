'use strict';

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeSlice,
  ArrayFrom,
  PromiseReject,
  SafeSet,
  TypedArrayPrototypeSlice,
} = primordials;

const {
  ChaCha20Poly1305CipherJob,
  KeyObjectHandle,
  kCryptoJobAsync,
  kWebCryptoCipherDecrypt,
  kWebCryptoCipherEncrypt,
} = internalBinding('crypto');

const {
  hasAnyNotIn,
  jobPromise,
  validateKeyOps,
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
  randomBytes: _randomBytes,
} = require('internal/crypto/random');

const randomBytes = promisify(_randomBytes);

function validateKeyLength(length) {
  if (length !== 256)
    throw lazyDOMException('Invalid key length', 'DataError');
}

function c20pCipher(mode, key, data, algorithm) {
  let tag;
  switch (mode) {
    case kWebCryptoCipherDecrypt: {
      const slice = ArrayBufferIsView(data) ?
        TypedArrayPrototypeSlice : ArrayBufferPrototypeSlice;

      if (data.byteLength < 16) {
        return PromiseReject(lazyDOMException(
          'The provided data is too small.',
          'OperationError'));
      }

      tag = slice(data, -16);
      data = slice(data, 0, -16);
      break;
    }
    case kWebCryptoCipherEncrypt:
      tag = 16;
      break;
  }

  return jobPromise(() => new ChaCha20Poly1305CipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    algorithm.iv,
    tag,
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

  const keyData = await randomBytes(32).catch((err) => {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason' +
      `[${err.message}]`,
      { name: 'OperationError', cause: err });
  });

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
      if (!keyData.kty)
        throw lazyDOMException('Invalid keyData', 'DataError');

      if (keyData.kty !== 'oct')
        throw lazyDOMException('Invalid JWK "kty" Parameter', 'DataError');

      if (usagesSet.size > 0 &&
          keyData.use !== undefined &&
          keyData.use !== 'enc') {
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

      const handle = new KeyObjectHandle();
      try {
        handle.initJwk(keyData);
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
