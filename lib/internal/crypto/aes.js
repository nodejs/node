'use strict';

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeSlice,
  ArrayFrom,
  ArrayPrototypePush,
  PromiseReject,
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
  kAlgorithm,
} = require('internal/crypto/keys');

const {
  generateKey: _generateKey,
} = require('internal/crypto/keygen');

const generateKey = promisify(_generateKey);

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

function asyncAesCtrCipher(mode, key, data, algorithm) {
  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-CTR', key[kAlgorithm].length),
    algorithm.counter,
    algorithm.length));
}

function asyncAesCbcCipher(mode, key, data, algorithm) {
  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-CBC', key[kAlgorithm].length),
    algorithm.iv));
}

function asyncAesKwCipher(mode, key, data) {
  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-KW', key[kAlgorithm].length)));
}

function asyncAesGcmCipher(mode, key, data, algorithm) {
  const { tagLength = 128 } = algorithm;

  const tagByteLength = tagLength / 8;
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

  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-GCM', key[kAlgorithm].length),
    algorithm.iv,
    tag,
    algorithm.additionalData));
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

  const checkUsages = ['wrapKey', 'unwrapKey'];
  if (name !== 'AES-KW')
    ArrayPrototypePush(checkUsages, 'encrypt', 'decrypt');

  const usagesSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usagesSet, checkUsages)) {
    throw lazyDOMException(
      'Unsupported key usage for an AES key',
      'SyntaxError');
  }

  const key = await generateKey('aes', { length }).catch((err) => {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason' +
      `[${err.message}]`,
      { name: 'OperationError', cause: err });
  });

  return new InternalCryptoKey(
    key,
    { name, length },
    ArrayFrom(usagesSet),
    extractable);
}

function aesImportKey(
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
    case 'KeyObject': {
      validateKeyLength(keyData.symmetricKeySize * 8);
      keyObject = keyData;
      break;
    }
    case 'raw': {
      validateKeyLength(keyData.byteLength * 8);
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

      ({ length } = handle.keyDetail({ }));
      validateKeyLength(length);

      if (keyData.alg !== undefined) {
        if (keyData.alg !== getAlgorithmName(algorithm.name, length))
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
