'use strict';

const {
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  kCryptoJobWebCrypto,
  KEMDecapsulateJob,
  KEMEncapsulateJob,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyFormatRawSeed,
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
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
  getCryptoKeyHandle,
  getCryptoKeyType,
  getKeyObjectHandle,
  getKeyObjectType,
  InternalCryptoKey,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  importRawKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

function mlKemGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits'])) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }

  const nid = {
    '__proto__': null,
    'ML-KEM-512': EVP_PKEY_ML_KEM_512,
    'ML-KEM-768': EVP_PKEY_ML_KEM_768,
    'ML-KEM-1024': EVP_PKEY_ML_KEM_1024,
  }[name];

  const publicUsages =
    getUsagesUnion(usageSet, 'encapsulateKey', 'encapsulateBits');
  const privateUsages =
    getUsagesUnion(usageSet, 'decapsulateKey', 'decapsulateBits');

  const keyAlgorithm = { name };

  if (privateUsages.size === 0) {
    throw lazyDOMException(
      'Usages cannot be empty when creating a key.',
      'SyntaxError');
  }

  return jobPromise(() => new NidKeyPairGenJob(
    kCryptoJobWebCrypto,
    nid,
    keyAlgorithm,
    getUsagesMask(publicUsages),
    getUsagesMask(privateUsages),
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
        const pkcs8 = handle.export(
          kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null);
        // Edge case only possible when user creates a seedless KeyObject
        // first and converts it with KeyObject.prototype.toCryptoKey.
        // 86 = 22 bytes of PKCS#8 ASN.1 + 64-byte seed.
        if (TypedArrayPrototypeGetByteLength(pkcs8) !== 86) {
          throw lazyDOMException(
            'The operation failed for an operation-specific reason',
            { name: 'OperationError' });
        }
        return TypedArrayPrototypeGetBuffer(pkcs8);
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

function verifyAcceptableMlKemKeyUse(name, isPublic, usages) {
  const checkSet = isPublic ? ['encapsulateKey', 'encapsulateBits'] : ['decapsulateKey', 'decapsulateBits'];
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }
}

function mlKemImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {

  const { name } = algorithm;
  let handle;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableMlKemKeyUse(
        name, getKeyObjectType(keyData) === 'public', usagesSet);
      handle = getKeyObjectHandle(keyData);
      break;
    }
    case 'spki': {
      verifyAcceptableMlKemKeyUse(name, true, usagesSet);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableMlKemKeyUse(name, false, usagesSet);

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
      verifyAcceptableMlKemKeyUse(name, isPublic, usagesSet);
      handle = importRawKey(isPublic, keyData, isPublic ? kKeyFormatRawPublic : kKeyFormatRawSeed, name);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'AKP', extractable, usagesSet, 'enc');

      if (keyData.alg !== name)
        throw lazyDOMException(
          'JWK "alg" Parameter and algorithm name mismatch', 'DataError');

      const isPublic = keyData.priv === undefined;
      verifyAcceptableMlKemKeyUse(name, isPublic, usagesSet);
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
