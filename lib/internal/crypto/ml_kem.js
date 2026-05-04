'use strict';

const {
  PromiseWithResolvers,
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const {
  kCryptoJobAsync,
  KEMDecapsulateJob,
  KEMEncapsulateJob,
  kKeyFormatDER,
  kKeyFormatRawPrivate,
  kKeyFormatRawPublic,
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
  getCryptoKeyAlgorithm,
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

async function mlKemGenerateKey(algorithm, extractable, keyUsages) {
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

  const handles = await jobPromise(() => new NidKeyPairGenJob(kCryptoJobAsync, nid));
  const publicUsagesMask = getUsagesMask(
    getUsagesUnion(usageSet, 'encapsulateKey', 'encapsulateBits'));
  const privateUsagesMask = getUsagesMask(
    getUsagesUnion(usageSet, 'decapsulateKey', 'decapsulateBits'));

  const keyAlgorithm = { name };

  const publicKey =
    new InternalCryptoKey(
      handles[0],
      keyAlgorithm,
      publicUsagesMask,
      true);

  const privateKey =
    new InternalCryptoKey(
      handles[1],
      keyAlgorithm,
      privateUsagesMask,
      extractable);

  return { __proto__: null, privateKey, publicKey };
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
      handle = importRawKey(isPublic, keyData, isPublic ? kKeyFormatRawPublic : kKeyFormatRawPrivate, name);
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

  const { promise, resolve, reject } = PromiseWithResolvers();

  const job = new KEMEncapsulateJob(
    kCryptoJobAsync,
    getCryptoKeyHandle(encapsulationKey),
    undefined,
    undefined,
    undefined,
    undefined);

  job.ondone = (error, result) => {
    if (error) {
      reject(lazyDOMException(
        'The operation failed for an operation-specific reason',
        { name: 'OperationError', cause: error }));
    } else {
      const { 0: sharedKey, 1: ciphertext } = result;

      resolve({
        sharedKey: TypedArrayPrototypeGetBuffer(sharedKey),
        ciphertext: TypedArrayPrototypeGetBuffer(ciphertext),
      });
    }
  };
  job.run();

  return promise;
}

function mlKemDecapsulate(decapsulationKey, ciphertext) {
  if (getCryptoKeyType(decapsulationKey) !== 'private') {
    throw lazyDOMException(`Key must be a private key`, 'InvalidAccessError');
  }

  const { promise, resolve, reject } = PromiseWithResolvers();

  const job = new KEMDecapsulateJob(
    kCryptoJobAsync,
    getCryptoKeyHandle(decapsulationKey),
    undefined,
    undefined,
    undefined,
    undefined,
    ciphertext);

  job.ondone = (error, result) => {
    if (error) {
      reject(lazyDOMException(
        'The operation failed for an operation-specific reason',
        { name: 'OperationError', cause: error }));
    } else {
      resolve(TypedArrayPrototypeGetBuffer(result));
    }
  };
  job.run();

  return promise;
}

module.exports = {
  mlKemExportKey,
  mlKemImportKey,
  mlKemEncapsulate,
  mlKemDecapsulate,
  mlKemGenerateKey,
};
