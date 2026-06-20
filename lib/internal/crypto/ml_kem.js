'use strict';

const {
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const {
  kCryptoJobWebCrypto,
  KEMDecapsulateJob,
  KEMEncapsulateJob,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyFormatRawSeed,
  kKeyTypePublic,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatSPKI,
  NidKeyPairGenJob,
  EVP_PKEY_ML_KEM_512,
  EVP_PKEY_ML_KEM_768,
  EVP_PKEY_ML_KEM_1024,
} = internalBinding('crypto');

const {
  getUsagesMask,
  jobPromise,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  getCryptoKeyAlgorithm,
  getCryptoKeyHandle,
  getCryptoKeyType,
  InternalCryptoKey,
} = require('internal/crypto/keys');

const {
  createKeyUsages,
  getKeyPairUsages,
  importDerKey,
  importJwkKey,
  importRawKey,
  validateJwk,
  validateKeyUsages,
  validateUsagesNotEmpty,
  verifyAcceptableKeyUse,
} = require('internal/crypto/webcrypto_util');

const kUsages = createKeyUsages(
  ['encapsulateKey', 'encapsulateBits'],
  ['decapsulateKey', 'decapsulateBits']);

function mlKemGenerateKey(algorithm, extractable, usages) {
  const { name } = algorithm;
  const usagesSet = validateKeyUsages(usages, kUsages.keygen, name);

  const nid = {
    '__proto__': null,
    'ML-KEM-512': EVP_PKEY_ML_KEM_512,
    'ML-KEM-768': EVP_PKEY_ML_KEM_768,
    'ML-KEM-1024': EVP_PKEY_ML_KEM_1024,
  }[name];

  const keyAlgorithm = { name };
  const keyUsages = getKeyPairUsages(usagesSet, kUsages);
  validateUsagesNotEmpty(keyUsages.private);

  return jobPromise(() => new NidKeyPairGenJob(
    kCryptoJobWebCrypto,
    nid,
    keyAlgorithm,
    getUsagesMask(keyUsages.public),
    getUsagesMask(keyUsages.private),
    extractable));
}

function mlKemExportKey(key, format) {
  try {
    const handle = getCryptoKeyHandle(key);
    switch (format) {
      case kWebCryptoKeyFormatRaw: {
        return TypedArrayPrototypeGetBuffer(
          getCryptoKeyType(key) === 'private' ? handle.rawSeed() : handle.rawPublicKey());
      }
      case kWebCryptoKeyFormatSPKI: {
        return TypedArrayPrototypeGetBuffer(
          handle.export(kKeyFormatDER, kWebCryptoKeyFormatSPKI));
      }
      case kWebCryptoKeyFormatPKCS8: {
        const seed = handle.rawSeed();
        const buffer = new Uint8Array(86);
        const orc = {
          '__proto__': null,
          'ML-KEM-512': 0x01,
          'ML-KEM-768': 0x02,
          'ML-KEM-1024': 0x03,
        }[getCryptoKeyAlgorithm(key).name];
        TypedArrayPrototypeSet(buffer, [
          0x30, 0x54, 0x02, 0x01, 0x00, 0x30, 0x0b, 0x06,
          0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04,
          0x04, orc, 0x04, 0x42, 0x80, 0x40,
        ], 0);
        TypedArrayPrototypeSet(buffer, seed, 22);
        return TypedArrayPrototypeGetBuffer(buffer);
      }
      default:
        return undefined;
    }
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }
}

function mlKemImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  usages) {

  const { name } = algorithm;
  let handle;
  const usagesSet = new SafeSet(usages);
  switch (format) {
    case 'KeyObjectHandle':
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        keyData.getKeyType() === kKeyTypePublic ?
          kUsages.public :
          kUsages.private);
      handle = keyData;
      break;
    case 'spki': {
      verifyAcceptableKeyUse(name, usagesSet, kUsages.public);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableKeyUse(name, usagesSet, kUsages.private);

      const privOnlyLengths = {
        '__proto__': null,
        'ML-KEM-512': 1660,
        'ML-KEM-768': 2428,
        'ML-KEM-1024': 3196,
      };
      if (keyData.byteLength === privOnlyLengths[name]) {
        throw lazyDOMException(
          'Importing an ML-KEM PKCS#8 key without a seed is not supported',
          'NotSupportedError');
      }

      handle = importDerKey(keyData, false);
      break;
    }
    case 'raw-public':
    case 'raw-seed': {
      const isPublic = format === 'raw-public';
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        isPublic ? kUsages.public : kUsages.private);
      handle = importRawKey(isPublic, keyData, isPublic ? kKeyFormatRawPublic : kKeyFormatRawSeed, name);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'AKP', extractable, usagesSet, 'enc');

      if (keyData.alg !== name)
        throw lazyDOMException(
          'JWK "alg" Parameter and algorithm name mismatch', 'DataError');

      const isPublic = keyData.priv === undefined;
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        isPublic ? kUsages.public : kUsages.private);
      handle = importJwkKey(isPublic, keyData);
      break;
    }
    default:
      return undefined;
  }

  if (handle.getAsymmetricKeyType() !== StringPrototypeToLowerCase(name)) {
    throw lazyDOMException('Invalid key type', 'DataError');
  }

  return new InternalCryptoKey(
    handle,
    { name },
    getUsagesMask(usagesSet),
    extractable);
}

function mlKemEncapsulate(encapsulationKey) {
  if (getCryptoKeyType(encapsulationKey) !== 'public') {
    throw lazyDOMException(`Key must be a public key`, 'InvalidAccessError');
  }

  return jobPromise(() => new KEMEncapsulateJob(
    kCryptoJobWebCrypto,
    getCryptoKeyHandle(encapsulationKey),
    undefined,
    undefined,
    undefined,
    undefined));
}

function mlKemDecapsulate(decapsulationKey, ciphertext) {
  if (getCryptoKeyType(decapsulationKey) !== 'private') {
    throw lazyDOMException(`Key must be a private key`, 'InvalidAccessError');
  }

  return jobPromise(() => new KEMDecapsulateJob(
    kCryptoJobWebCrypto,
    getCryptoKeyHandle(decapsulationKey),
    undefined,
    undefined,
    undefined,
    undefined,
    ciphertext));
}

module.exports = {
  mlKemExportKey,
  mlKemImportKey,
  mlKemEncapsulate,
  mlKemDecapsulate,
  mlKemGenerateKey,
};
