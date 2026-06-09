'use strict';

const {
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');

const {
  SignJob,
  kCryptoJobWebCrypto,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyFormatRawSeed,
  kKeyTypePublic,
  kSignJobModeSign,
  kSignJobModeVerify,
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatSPKI,
  NidKeyPairGenJob,
  EVP_PKEY_ML_DSA_44,
  EVP_PKEY_ML_DSA_65,
  EVP_PKEY_ML_DSA_87,
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
  InternalCryptoKey,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  importRawKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

function verifyAcceptableMlDsaKeyUse(name, isPublic, usages) {
  const checkSet = isPublic ? ['verify'] : ['sign'];
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }
}

function mlDsaGenerateKey(algorithm, extractable, usages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(usages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }

  const nid = {
    '__proto__': null,
    'ML-DSA-44': EVP_PKEY_ML_DSA_44,
    'ML-DSA-65': EVP_PKEY_ML_DSA_65,
    'ML-DSA-87': EVP_PKEY_ML_DSA_87,
  }[name];

  const publicUsages = getUsagesUnion(usageSet, 'verify');
  const privateUsages = getUsagesUnion(usageSet, 'sign');

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

function mlDsaExportKey(key, format) {
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
        const buffer = new Uint8Array(54);
        const orc = {
          '__proto__': null,
          'ML-DSA-44': 0x11,
          'ML-DSA-65': 0x12,
          'ML-DSA-87': 0x13,
        }[getCryptoKeyAlgorithm(key).name];
        TypedArrayPrototypeSet(buffer, [
          0x30, 0x34, 0x02, 0x01, 0x00, 0x30, 0x0b, 0x06,
          0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04,
          0x03, orc, 0x04, 0x22, 0x80, 0x20,
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

function mlDsaImportKey(
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
      verifyAcceptableMlDsaKeyUse(
        name, keyData.getKeyType() === kKeyTypePublic, usagesSet);
      handle = keyData;
      break;
    case 'spki': {
      verifyAcceptableMlDsaKeyUse(name, true, usagesSet);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableMlDsaKeyUse(name, false, usagesSet);

      const privOnlyLengths = {
        '__proto__': null,
        'ML-DSA-44': 2588,
        'ML-DSA-65': 4060,
        'ML-DSA-87': 4924,
      };
      if (keyData.byteLength === privOnlyLengths[name]) {
        throw lazyDOMException(
          'Importing an ML-DSA PKCS#8 key without a seed is not supported',
          'NotSupportedError');
      }

      handle = importDerKey(keyData, false);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'AKP', extractable, usagesSet, 'sig');

      if (keyData.alg !== name)
        throw lazyDOMException(
          'JWK "alg" Parameter and algorithm name mismatch', 'DataError');

      const isPublic = keyData.priv === undefined;
      verifyAcceptableMlDsaKeyUse(name, isPublic, usagesSet);
      handle = importJwkKey(isPublic, keyData);

      if (!isPublic) {
        const publicKey = Buffer.from(keyData.pub, 'base64url');
        if (!Buffer.from(handle.rawPublicKey()).equals(publicKey))
          throw lazyDOMException('Invalid keyData', 'DataError');
      }
      break;
    }
    case 'raw-public':
    case 'raw-seed': {
      const isPublic = format === 'raw-public';
      verifyAcceptableMlDsaKeyUse(name, isPublic, usagesSet);
      handle = importRawKey(isPublic, keyData, isPublic ? kKeyFormatRawPublic : kKeyFormatRawSeed, name);
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

function mlDsaSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (getCryptoKeyType(key) !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return jobPromise(() => new SignJob(
    kCryptoJobWebCrypto,
    mode,
    getCryptoKeyHandle(key),
    undefined,
    undefined,
    undefined,
    undefined,
    data,
    undefined,
    undefined,
    undefined,
    undefined,
    algorithm.context,
    signature));
}

module.exports = {
  mlDsaExportKey,
  mlDsaImportKey,
  mlDsaGenerateKey,
  mlDsaSignVerify,
};
