'use strict';

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeSlice,
  ArrayFrom,
  ArrayPrototypeIncludes,
  ArrayPrototypePush,
  MathFloor,
  Promise,
  SafeSet,
  TypedArrayPrototypeSlice,
} = primordials;

const {
  AESCipherJob,
  KeyObjectHandle,
  kCryptoJobAsync,
  kKeyVariantAES_CTR_128,
  kKeyVariantAES_CBC_128,
  kKeyVariantAES_GCM_128,
  kKeyVariantAES_KW_128,
  kKeyVariantAES_CTR_192,
  kKeyVariantAES_CBC_192,
  kKeyVariantAES_GCM_192,
  kKeyVariantAES_KW_192,
  kKeyVariantAES_CTR_256,
  kKeyVariantAES_CBC_256,
  kKeyVariantAES_GCM_256,
  kKeyVariantAES_KW_256,
  kWebCryptoCipherDecrypt,
  kWebCryptoCipherEncrypt,
} = internalBinding('crypto');

const {
  getArrayBufferOrView,
  hasAnyNotIn,
  jobPromise,
  validateByteLength,
  validateKeyOps,
  validateMaxBufferLength,
  kAesKeyLengths,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const { PromiseReject } = primordials;

const {
  InternalCryptoKey,
  SecretKeyObject,
  createSecretKey,
} = require('internal/crypto/keys');

const {
  generateKey,
} = require('internal/crypto/keygen');

const {
  validateInteger,
  validateOneOf,
} = require('internal/validators');

const kMaxCounterLength = 128;
const kTagLengths = [32, 64, 96, 104, 112, 120, 128];

function getAlgorithmName(name, length) {
  switch (name) {
    case 'AES-CBC': return `A${length}CBC`;
    case 'AES-CTR': return `A${length}CTR`;
    case 'AES-GCM': return `A${length}GCM`;
    case 'AES-KW': return `A${length}KW`;
  }
}

function validateKeyLength(length) {
  if (length !== 128 && length !== 192 && length !== 256)
    throw lazyDOMException('Invalid key length', 'DataError');
}

function getVariant(name, length) {
  switch (name) {
    case 'AES-CBC':
      switch (length) {
        case 128: return kKeyVariantAES_CBC_128;
        case 192: return kKeyVariantAES_CBC_192;
        case 256: return kKeyVariantAES_CBC_256;
      }
      break;
    case 'AES-CTR':
      switch (length) {
        case 128: return kKeyVariantAES_CTR_128;
        case 192: return kKeyVariantAES_CTR_192;
        case 256: return kKeyVariantAES_CTR_256;
      }
      break;
    case 'AES-GCM':
      switch (length) {
        case 128: return kKeyVariantAES_GCM_128;
        case 192: return kKeyVariantAES_GCM_192;
        case 256: return kKeyVariantAES_GCM_256;
      }
      break;
    case 'AES-KW':
      switch (length) {
        case 128: return kKeyVariantAES_KW_128;
        case 192: return kKeyVariantAES_KW_192;
        case 256: return kKeyVariantAES_KW_256;
      }
      break;
  }
}

function asyncAesCtrCipher(mode, key, data, { counter, length }) {
  counter = getArrayBufferOrView(counter, 'algorithm.counter');
  validateByteLength(counter, 'algorithm.counter', 16);
  // The length must specify an integer between 1 and 128. While
  // there is no default, this should typically be 64.
  if (typeof length !== 'number' ||
      length <= 0 ||
      length > kMaxCounterLength) {
    throw lazyDOMException(
      'AES-CTR algorithm.length must be between 1 and 128',
      'OperationError');
  }

  return jobPromise(new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-CTR', key.algorithm.length),
    counter,
    length));
}

function asyncAesCbcCipher(mode, key, data, { iv }) {
  iv = getArrayBufferOrView(iv, 'algorithm.iv');
  validateByteLength(iv, 'algorithm.iv', 16);
  return jobPromise(new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-CBC', key.algorithm.length),
    iv));
}

function asyncAesKwCipher(mode, key, data) {
  return jobPromise(new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-KW', key.algorithm.length)));
}

function asyncAesGcmCipher(
  mode,
  key,
  data,
  { iv, additionalData, tagLength = 128 }) {
  if (!ArrayPrototypeIncludes(kTagLengths, tagLength)) {
    return PromiseReject(lazyDOMException(
      `${tagLength} is not a valid AES-GCM tag length`,
      'OperationError'));
  }

  iv = getArrayBufferOrView(iv, 'algorithm.iv');
  validateMaxBufferLength(iv, 'algorithm.iv');

  if (additionalData !== undefined) {
    additionalData =
      getArrayBufferOrView(additionalData, 'algorithm.additionalData');
    validateMaxBufferLength(additionalData, 'algorithm.additionalData');
  }

  const tagByteLength = MathFloor(tagLength / 8);
  let tag;
  switch (mode) {
    case kWebCryptoCipherDecrypt: {
      const slice = ArrayBufferIsView(data) ?
        TypedArrayPrototypeSlice : ArrayBufferPrototypeSlice;
      tag = slice(data, -tagByteLength);

      // Refs: https://www.w3.org/TR/WebCryptoAPI/#aes-gcm-operations
      //
      // > If *plaintext* has a length less than *tagLength* bits, then `throw`
      // > an `OperationError`.
      if (tagByteLength > tag.byteLength) {
        return PromiseReject(lazyDOMException(
          'The provided data is too small.',
          'OperationError'));
      }

      data = slice(data, 0, -tagByteLength);
      break;
    }
    case kWebCryptoCipherEncrypt:
      tag = tagByteLength;
      break;
  }

  return jobPromise(new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-GCM', key.algorithm.length),
    iv,
    tag,
    additionalData));
}

function aesCipher(mode, key, data, algorithm) {
  switch (algorithm.name) {
    case 'AES-CTR': return asyncAesCtrCipher(mode, key, data, algorithm);
    case 'AES-CBC': return asyncAesCbcCipher(mode, key, data, algorithm);
    case 'AES-GCM': return asyncAesGcmCipher(mode, key, data, algorithm);
    case 'AES-KW': return asyncAesKwCipher(mode, key, data);
  }
}

async function aesGenerateKey(algorithm, extractable, keyUsages) {
  const { name, length } = algorithm;
  validateInteger(length, 'algorithm.length');
  validateOneOf(length, 'algorithm.length', kAesKeyLengths);

  const usageSet = new SafeSet(keyUsages);

  if (hasAnyNotIn(usageSet, ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'])) {
    throw lazyDOMException(
      'Unsupported key usage for an AES key',
      'SyntaxError');
  }
  return new Promise((resolve, reject) => {
    generateKey('aes', { length }, (err, key) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason ' +
          `[${err.message}]`,
          'OperationError'));
      }

      resolve(new InternalCryptoKey(
        key,
        { name, length },
        ArrayFrom(usageSet),
        extractable));
    });
  });
}

async function aesImportKey(
  algorithm,
  format,
  keyData,
  extractable,
  keyUsages) {
  const { name } = algorithm;
  const checkUsages = ['wrapKey', 'unwrapKey'];
  if (name !== 'AES-KW')
    ArrayPrototypePush(checkUsages, 'encrypt', 'decrypt');

  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, checkUsages)) {
    throw lazyDOMException(
      'Unsupported key usage for an AES key',
      'SyntaxError');
  }

  let keyObject;
  let length;
  switch (format) {
    case 'raw': {
      validateKeyLength(keyData.byteLength * 8);
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
          keyData.use !== 'enc') {
        throw lazyDOMException('Invalid use type', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException('JWK is not extractable', 'DataError');
      }

      const handle = new KeyObjectHandle();
      handle.initJwk(keyData);

      ({ length } = handle.keyDetail({ }));
      validateKeyLength(length);

      if (keyData.alg !== undefined) {
        if (typeof keyData.alg !== 'string')
          throw lazyDOMException('Invalid alg', 'DataError');
        if (keyData.alg !== getAlgorithmName(algorithm.name, length))
          throw lazyDOMException('Algorithm mismatch', 'DataError');
      }

      keyObject = new SecretKeyObject(handle);
      break;
    }
    default:
      throw lazyDOMException(
        `Unable to import AES key with format ${format}`,
        'NotSupportedError');
  }

  if (length === undefined) {
    ({ length } = keyObject[kHandle].keyDetail({ }));
    validateKeyLength(length);
  }

  return new InternalCryptoKey(
    keyObject,
    { name, length },
    keyUsages,
    extractable);
}

module.exports = {
  aesCipher,
  aesGenerateKey,
  aesImportKey,
  getAlgorithmName,
};
