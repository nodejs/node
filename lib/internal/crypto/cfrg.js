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
} = internalBinding('crypto');

const {
  getUsagesUnion,
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
  generateKeyPair: _generateKeyPair,
} = require('internal/crypto/keygen');

const {
  InternalCryptoKey,
  kKeyType,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  importRawKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

const generateKeyPair = promisify(_generateKeyPair);

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
  let genKeyType;
  switch (name) {
    case 'Ed25519':
      genKeyType = 'ed25519';
      break;
    case 'Ed448':
      genKeyType = 'ed448';
      break;
    case 'X25519':
      genKeyType = 'x25519';
      break;
    case 'X448':
      genKeyType = 'x448';
      break;
  }

  let keyPair;
  try {
    keyPair = await generateKeyPair(genKeyType);
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }

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
      publicUsages = [];
      privateUsages = getUsagesUnion(usageSet, 'deriveKey', 'deriveBits');
      break;
  }

  const keyAlgorithm = { name };

  const publicKey =
    new InternalCryptoKey(
      keyPair.publicKey,
      keyAlgorithm,
      publicUsages,
      true);

  const privateKey =
    new InternalCryptoKey(
      keyPair.privateKey,
      keyAlgorithm,
      privateUsages,
      extractable);

  return { __proto__: null, privateKey, publicKey };
}

function cfrgExportKey(key, format) {
  try {
    switch (format) {
      case kWebCryptoKeyFormatRaw: {
        const handle = key[kKeyObject][kHandle];
        return TypedArrayPrototypeGetBuffer(
          key[kKeyType] === 'private' ? handle.rawPrivateKey() : handle.rawPublicKey());
      }
      case kWebCryptoKeyFormatSPKI: {
        return TypedArrayPrototypeGetBuffer(
          key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatSPKI));
      }
      case kWebCryptoKeyFormatPKCS8: {
        return TypedArrayPrototypeGetBuffer(
          key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null));
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
  let keyObject;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableCfrgKeyUse(name, keyData.type === 'public', usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableCfrgKeyUse(name, true, usagesSet);
      keyObject = importDerKey(keyData, true);
      break;
    }
    case 'pkcs8': {
      verifyAcceptableCfrgKeyUse(name, false, usagesSet);
      keyObject = importDerKey(keyData, false);
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
      keyObject = importJwkKey(isPublic, keyData);
      break;
    }
    case 'raw': {
      verifyAcceptableCfrgKeyUse(name, true, usagesSet);
      keyObject = importRawKey(true, keyData, kKeyFormatRawPublic, name);
      break;
    }
    default:
      return undefined;
  }

  if (keyObject.asymmetricKeyType !== StringPrototypeToLowerCase(name)) {
    throw lazyDOMException('Invalid key type', 'DataError');
  }

  return new InternalCryptoKey(
    keyObject,
    { name },
    keyUsages,
    extractable);
}

async function eddsaSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key[kKeyType] !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return await jobPromise(() => new SignJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
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
