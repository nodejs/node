'use strict';

const {
  ArrayPrototypePush,
  SafeSet,
} = primordials;

const {
  AESCipherJob,
  kCryptoJobAsync,
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
  SecretKeyGenJob,
} = internalBinding('crypto');

const {
  getUsagesMask,
  hasAnyNotIn,
  jobPromise,
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
    getCryptoKeyHandle(key),
    data,
    getVariant('AES-CTR', getCryptoKeyAlgorithm(key).length),
    algorithm.counter,
    algorithm.length));
}

function asyncAesCbcCipher(mode, key, data, algorithm) {
  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    getCryptoKeyHandle(key),
    data,
    getVariant('AES-CBC', getCryptoKeyAlgorithm(key).length),
    algorithm.iv));
}

function asyncAesKwCipher(mode, key, data) {
  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    getCryptoKeyHandle(key),
    data,
    getVariant('AES-KW', getCryptoKeyAlgorithm(key).length)));
}

function asyncAesGcmCipher(mode, key, data, algorithm) {
  const { tagLength = 128 } = algorithm;
  const tagByteLength = tagLength / 8;

  return jobPromise(() => new AESCipherJob(
    kCryptoJobAsync,
    mode,
    getCryptoKeyHandle(key),
    data,
    getVariant('AES-GCM', getCryptoKeyAlgorithm(key).length),
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
    getCryptoKeyHandle(key),
    data,
    getVariant('AES-OCB', getCryptoKeyAlgorithm(key).length),
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

  const handle = await jobPromise(() => new SecretKeyGenJob(kCryptoJobAsync, length));

  return new InternalCryptoKey(
    handle,
    { name, length },
    getUsagesMask(usagesSet),
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

  let handle;
  let length;
  switch (format) {
    case 'KeyObject': {
      length = keyData.symmetricKeySize * 8;
      validateKeyLength(length);
      handle = keyData[kHandle];
      break;
    }
    case 'raw-secret':
    case 'raw': {
      if (format === 'raw' && name === 'AES-OCB') {
        return undefined;
      }
      length = keyData.byteLength * 8;
      validateKeyLength(length);
      handle = importSecretKey(keyData);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'oct', extractable, usagesSet, 'enc');
      handle = importJwkSecretKey(keyData);
      length = handle.getSymmetricKeySize() * 8;
      validateKeyLength(length);
      if (keyData.alg !== undefined) {
        if (keyData.alg !== getAlgorithmName(algorithm.name, length))
          throw lazyDOMException(
            'JWK "alg" does not match the requested algorithm',
            'DataError');
      }
      break;
    }
    default:
      return undefined;
  }

  return new InternalCryptoKey(
    handle,
    { name, length },
    getUsagesMask(usagesSet),
    extractable);
}

module.exports = {
  aesCipher,
  aesGenerateKey,
  aesImportKey,
  getAlgorithmName,
};
