'use strict';

const {
  ArrayPrototypePush,
  SafeSet,
} = primordials;

const {
  AESCipherJob,
  KeyObjectHandle,
  kCryptoJobAsync,
  kKeyFormatJWK,
  kKeyTypeSecret,
  kKeyVariantAES_CTR_128,
  kKeyVariantAES_CBC_128,
  kKeyVariantAES_GCM_128,
  kKeyVariantAES_KW_128,
  kKeyVariantAES_OCB_128,
  kKeyVariantAES_CTR_192,
  kKeyVariantAES_CBC_192,
  kKeyVariantAES_GCM_192,
  kKeyVariantAES_KW_192,
  kKeyVariantAES_OCB_192,
  kKeyVariantAES_CTR_256,
  kKeyVariantAES_CBC_256,
  kKeyVariantAES_GCM_256,
  kKeyVariantAES_KW_256,
  kKeyVariantAES_OCB_256,
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
  kAlgorithm,
} = require('internal/crypto/keys');

const {
  validateJwk,
} = require('internal/crypto/webcrypto_util');

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
    case 'AES-OCB': return `A${length}OCB`;
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
    case 'AES-OCB':
      switch (length) {
        case 128: return kKeyVariantAES_OCB_128;
        case 192: return kKeyVariantAES_OCB_192;
        case 256: return kKeyVariantAES_OCB_256;
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

  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-GCM', key[kAlgorithm].length),
    algorithm.iv,
    tagByteLength,
    algorithm.additionalData));
}

function asyncAesOcbCipher(mode, key, data, algorithm) {
  const { tagLength = 128 } = algorithm;
  const tagByteLength = tagLength / 8;

  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    getVariant('AES-OCB', key.algorithm.length),
    algorithm.iv,
    tagByteLength,
    algorithm.additionalData));
}

function aesCipher(mode, key, data, algorithm) {
  switch (algorithm.name) {
    case 'AES-CTR': return asyncAesCtrCipher(mode, key, data, algorithm);
    case 'AES-CBC': return asyncAesCbcCipher(mode, key, data, algorithm);
    case 'AES-GCM': return asyncAesGcmCipher(mode, key, data, algorithm);
    case 'AES-OCB': return asyncAesOcbCipher(mode, key, data, algorithm);
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

  let key;
  try {
    key = await generateKey('aes', { length });
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason' +
      `[${err.message}]`,
      { name: 'OperationError', cause: err });
  }

  return new InternalCryptoKey(
    key,
    { name, length },
    usagesSet,
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
    case 'raw-secret':
    case 'raw': {
      if (format === 'raw' && name === 'AES-OCB') {
        return undefined;
      }
      validateKeyLength(keyData.byteLength * 8);
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
    usagesSet,
    extractable);
}

module.exports = {
  aesCipher,
  aesGenerateKey,
  aesImportKey,
  getAlgorithmName,
};
