'use strict';

const {
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  SignJob,
  kCryptoJobAsync,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyFormatRawSeed,
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
  kHandle,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

const {
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

async function mlDsaGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }

  let nid;
  switch (name) {
    case 'ML-DSA-44':
      nid = EVP_PKEY_ML_DSA_44;
      break;
    case 'ML-DSA-65':
      nid = EVP_PKEY_ML_DSA_65;
      break;
    case 'ML-DSA-87':
      nid = EVP_PKEY_ML_DSA_87;
      break;
  }

  const handles = await jobPromise(() => new NidKeyPairGenJob(kCryptoJobAsync, nid));
  const publicUsagesMask = getUsagesMask(getUsagesUnion(usageSet, 'verify'));
  const privateUsagesMask = getUsagesMask(getUsagesUnion(usageSet, 'sign'));

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
        const pkcs8 = handle.export(
          kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null);
        // Edge case only possible when user creates a seedless KeyObject
        // first and converts it with KeyObject.prototype.toCryptoKey.
        // 54 = 22 bytes of PKCS#8 ASN.1 + 32-byte seed.
        if (TypedArrayPrototypeGetByteLength(pkcs8) !== 54) {
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

function mlDsaImportKey(
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
      verifyAcceptableMlDsaKeyUse(name, keyData.type === 'public', usagesSet);
      handle = keyData[kHandle];
      break;
    }
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

async function mlDsaSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (getCryptoKeyType(key) !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return await jobPromise(() => new SignJob(
    kCryptoJobAsync,
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
