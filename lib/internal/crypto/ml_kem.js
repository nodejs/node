'use strict';

const {
  PromiseWithResolvers,
  SafeSet,
  StringPrototypeToLowerCase,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
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
} = internalBinding('crypto');

const {
  getUsagesUnion,
  hasAnyNotIn,
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
  importRawKey,
} = require('internal/crypto/webcrypto_util');

const generateKeyPair = promisify(_generateKeyPair);

async function mlKemGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits'])) {
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

  const publicUsages = getUsagesUnion(usageSet, 'encapsulateBits', 'encapsulateKey');
  const privateUsages = getUsagesUnion(usageSet, 'decapsulateBits', 'decapsulateKey');

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

function mlKemExportKey(key, format) {
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
        const pkcs8 = key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatPKCS8, null, null);
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
  let keyObject;
  const usagesSet = new SafeSet(keyUsages);
  switch (format) {
    case 'KeyObject': {
      verifyAcceptableMlKemKeyUse(name, keyData.type === 'public', usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableMlKemKeyUse(name, true, usagesSet);
      keyObject = importDerKey(keyData, true);
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

      keyObject = importDerKey(keyData, false);
      break;
    }
    case 'raw-public':
    case 'raw-seed': {
      const isPublic = format === 'raw-public';
      verifyAcceptableMlKemKeyUse(name, isPublic, usagesSet);
      keyObject = importRawKey(isPublic, keyData, isPublic ? kKeyFormatRawPublic : kKeyFormatRawPrivate, name);
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

function mlKemEncapsulate(encapsulationKey) {
  if (encapsulationKey[kKeyType] !== 'public') {
    throw lazyDOMException(`Key must be a public key`, 'InvalidAccessError');
  }

  const { promise, resolve, reject } = PromiseWithResolvers();

  const job = new KEMEncapsulateJob(
    kCryptoJobAsync,
    encapsulationKey[kKeyObject][kHandle],
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
  if (decapsulationKey[kKeyType] !== 'private') {
    throw lazyDOMException(`Key must be a private key`, 'InvalidAccessError');
  }

  const { promise, resolve, reject } = PromiseWithResolvers();

  const job = new KEMDecapsulateJob(
    kCryptoJobAsync,
    decapsulationKey[kKeyObject][kHandle],
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
