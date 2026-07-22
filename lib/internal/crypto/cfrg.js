'use strict';

const {
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
} = primordials;

const {
  SignJob,
  kCryptoJobWebCrypto,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyTypePublic,
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
  jobPromise,
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

const kDeriveUsages = createKeyUsages([], ['deriveKey', 'deriveBits']);

const kSignVerifyUsages = createKeyUsages(['verify'], ['sign']);

const kUsages = {
  '__proto__': null,
  'X25519': kDeriveUsages,
  'X448': kDeriveUsages,
  'Ed25519': kSignVerifyUsages,
  'Ed448': kSignVerifyUsages,
};

function cfrgGenerateKey(algorithm, extractable, usages) {
  const { name } = algorithm;
  const allowedUsages = kUsages[name];
  const usagesSet = validateKeyUsages(usages, allowedUsages.keygen, name);
  const nid = {
    '__proto__': null,
    'Ed25519': EVP_PKEY_ED25519,
    'Ed448': EVP_PKEY_ED448,
    'X25519': EVP_PKEY_X25519,
    'X448': EVP_PKEY_X448,
  }[name];

  const keyAlgorithm = { name };
  const keyUsages = getKeyPairUsages(usagesSet, allowedUsages);
  validateUsagesNotEmpty(keyUsages.private);

  return jobPromise(() => new NidKeyPairGenJob(
    kCryptoJobWebCrypto,
    nid,
    keyAlgorithm,
    getUsagesMask(keyUsages.public),
    getUsagesMask(keyUsages.private),
    extractable));
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
  usages) {

  const { name } = algorithm;
  let handle;
  const allowedUsages = kUsages[name];
  const usagesSet = new SafeSet(usages);
  switch (format) {
    case 'KeyObjectHandle':
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        keyData.getKeyType() === kKeyTypePublic ?
          allowedUsages.public :
          allowedUsages.private);
      handle = keyData;
      break;
    case 'spki': {
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.public);
      handle = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.private);
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
      verifyAcceptableKeyUse(
        name,
        usagesSet,
        isPublic ? allowedUsages.public : allowedUsages.private);
      handle = importJwkKey(isPublic, keyData);
      break;
    }
    case 'raw': {
      verifyAcceptableKeyUse(name, usagesSet, allowedUsages.public);
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

function eddsaSignVerify(key, data, algorithm, signature) {
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
    algorithm.name === 'Ed448' ? algorithm.context : undefined,
    signature));
}

module.exports = {
  cfrgExportKey,
  cfrgImportKey,
  cfrgGenerateKey,
  eddsaSignVerify,
};
