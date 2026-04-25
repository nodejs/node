'use strict';

const {
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
} = primordials;

const {
  SignJob,
  kCryptoJobAsync,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kSignJobModeSign,
  kSignJobModeVerify,
  kWebCryptoKeyFormatPKCS8,
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatSPKI,
  NidKeyPairGenJob,
  EVP_PKEY_ED25519,
  EVP_PKEY_ED448,
  EVP_PKEY_X25519,
  EVP_PKEY_X448,
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

function verifyAcceptableCfrgKeyUse(name, isPublic, usages) {
  let checkSet;
  switch (name) {
    case 'X25519':
      // Fall through
    case 'X448':
      checkSet = isPublic ? [] : ['deriveKey', 'deriveBits'];
      break;
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      checkSet = isPublic ? ['verify'] : ['sign'];
      break;
    default:
      throw lazyDOMException(
        'The algorithm is not supported', 'NotSupportedError');
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }
}

async function cfrgGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  switch (name) {
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
        throw lazyDOMException(
          `Unsupported key usage for an ${name} key`,
          'SyntaxError');
      }
      break;
    case 'X25519':
      // Fall through
    case 'X448':
      if (hasAnyNotIn(usageSet, ['deriveKey', 'deriveBits'])) {
        throw lazyDOMException(
          `Unsupported key usage for an ${name} key`,
          'SyntaxError');
      }
      break;
  }
  let nid;
  switch (name) {
    case 'Ed25519':
      nid = EVP_PKEY_ED25519;
      break;
    case 'Ed448':
      nid = EVP_PKEY_ED448;
      break;
    case 'X25519':
      nid = EVP_PKEY_X25519;
      break;
    case 'X448':
      nid = EVP_PKEY_X448;
      break;
  }

  const handles = await jobPromise(() => new NidKeyPairGenJob(kCryptoJobAsync, nid));

  let publicUsages;
  let privateUsages;
  switch (name) {
    case 'Ed25519':
      // Fall through
    case 'Ed448':
      publicUsages = getUsagesUnion(usageSet, 'verify');
      privateUsages = getUsagesUnion(usageSet, 'sign');
      break;
    case 'X25519':
      // Fall through
    case 'X448':
      publicUsages = new SafeSet();
      privateUsages = getUsagesUnion(usageSet, 'deriveKey', 'deriveBits');
      break;
  }

  const keyAlgorithm = { name };

  const publicKey =
    new InternalCryptoKey(
      handles[0],
      keyAlgorithm,
      getUsagesMask(publicUsages),
      true);

  const privateKey =
    new InternalCryptoKey(
      handles[1],
      keyAlgorithm,
      getUsagesMask(privateUsages),
      extractable);

  return { __proto__: null, privateKey, publicKey };
}

function cfrgExportKey(key, format) {
  try {
    switch (format) {
      case kWebCryptoKeyFormatRaw: {
        const handle = getCryptoKeyHandle(key);
        return TypedArrayPrototypeGetBuffer(
          getCryptoKeyType(key) === 'private' ? handle.rawPrivateKey() : handle.rawPublicKey());
      }
      case kWebCryptoKeyFormatSPKI: {
        return TypedArrayPrototypeGetBuffer(
          getCryptoKeyHandle(key).export(kKeyFormatDER, kWebCryptoKeyFormatSPKI));
      }
      case kWebCryptoKeyFormatPKCS8: {
        return TypedArrayPrototypeGetBuffer(
          getCryptoKeyHandle(key).export(kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null));
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

function cfrgImportKey(
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
      verifyAcceptableCfrgKeyUse(name, keyData.type === 'public', usagesSet);
      handle = keyData[kHandle];
      break;
    }
    case 'spki': {
      verifyAcceptableCfrgKeyUse(name, true, usagesSet);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableCfrgKeyUse(name, false, usagesSet);
      handle = importDerKey(keyData, false);
      break;
    }
    case 'jwk': {
      const expectedUse = (name === 'X25519' || name === 'X448') ? 'enc' : 'sig';
      validateJwk(keyData, 'OKP', extractable, usagesSet, expectedUse);

      if (keyData.crv !== name)
        throw lazyDOMException(
          'JWK "crv" Parameter and algorithm name mismatch', 'DataError');

      if (keyData.alg !== undefined && (name === 'Ed25519' || name === 'Ed448')) {
        if (keyData.alg !== name && keyData.alg !== 'EdDSA')
          throw lazyDOMException(
            'JWK "alg" does not match the requested algorithm', 'DataError');
      }

      const isPublic = keyData.d === undefined;
      verifyAcceptableCfrgKeyUse(name, isPublic, usagesSet);
      handle = importJwkKey(isPublic, keyData);
      break;
    }
    case 'raw': {
      verifyAcceptableCfrgKeyUse(name, true, usagesSet);
      handle = importRawKey(true, keyData, kKeyFormatRawPublic, name);
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

async function eddsaSignVerify(key, data, algorithm, signature) {
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
    algorithm.name === 'Ed448' ? algorithm.context : undefined,
    signature));
}

module.exports = {
  cfrgExportKey,
  cfrgImportKey,
  cfrgGenerateKey,
  eddsaSignVerify,
};
