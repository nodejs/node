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
  kCryptoJobAsync,
  kKeyFormatDER,
  kKeyFormatRawPublic,
  kKeyFormatRawSeed,
  kSignJobModeSign,
  kSignJobModeVerify,
  kWebCryptoKeyFormatRaw,
  kWebCryptoKeyFormatPKCS8,
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
  kAlgorithm,
  kKeyType,
} = require('internal/crypto/keys');

const {
  importDerKey,
  importJwkKey,
  importRawKey,
  validateJwk,
} = require('internal/crypto/webcrypto_util');

const generateKeyPair = promisify(_generateKeyPair);

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

  let keyPair;
  try {
    keyPair = await generateKeyPair(name.toLowerCase());
  } catch (err) {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  }

  const publicUsages = getUsagesUnion(usageSet, 'verify');
  const privateUsages = getUsagesUnion(usageSet, 'sign');

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

function mlDsaExportKey(key, format) {
  try {
    switch (format) {
      case kWebCryptoKeyFormatRaw: {
        const handle = key[kKeyObject][kHandle];
        return TypedArrayPrototypeGetBuffer(
          key[kKeyType] === 'private' ? handle.rawSeed() : handle.rawPublicKey());
      }
      case kWebCryptoKeyFormatSPKI: {
        return TypedArrayPrototypeGetBuffer(key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatSPKI));
      }
      case kWebCryptoKeyFormatPKCS8: {
        const seed = key[kKeyObject][kHandle].rawSeed();
        const buffer = new Uint8Array(54);
        const orc = {
          '__proto__': null,
          'ML-DSA-44': 0x11,
          'ML-DSA-65': 0x12,
          'ML-DSA-87': 0x13,
        }[key[kAlgorithm].name];
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
  keyUsages) {

  const { name } = algorithm;
  let keyObject;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableMlDsaKeyUse(name, keyData.type === 'public', usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableMlDsaKeyUse(name, true, usagesSet);
      keyObject = importDerKey(keyData, true);
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

      keyObject = importDerKey(keyData, false);
      break;
    }
    case 'jwk': {
      validateJwk(keyData, 'AKP', extractable, usagesSet, 'sig');

      if (keyData.alg !== name)
        throw lazyDOMException(
          'JWK "alg" Parameter and algorithm name mismatch', 'DataError');

      const isPublic = keyData.priv === undefined;
      verifyAcceptableMlDsaKeyUse(name, isPublic, usagesSet);
      keyObject = importJwkKey(isPublic, keyData);

      if (!isPublic) {
        const publicKey = Buffer.from(keyData.pub, 'base64url');
        if (!Buffer.from(keyObject[kHandle].rawPublicKey()).equals(publicKey))
          throw lazyDOMException('Invalid keyData', 'DataError');
      }
      break;
    }
    case 'raw-public':
    case 'raw-seed': {
      const isPublic = format === 'raw-public';
      verifyAcceptableMlDsaKeyUse(name, isPublic, usagesSet);
      keyObject = importRawKey(isPublic, keyData, isPublic ? kKeyFormatRawPublic : kKeyFormatRawSeed, name);
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

async function mlDsaSignVerify(key, data, algorithm, signature) {
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
    algorithm.context,
    signature));
}

module.exports = {
  mlDsaExportKey,
  mlDsaImportKey,
  mlDsaGenerateKey,
  mlDsaSignVerify,
};
