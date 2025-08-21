'use strict';

const {
  PromiseWithResolvers,
  SafeSet,
  Uint8Array,
} = primordials;

const {
  kCryptoJobAsync,
  KEMDecapsulateJob,
  KEMEncapsulateJob,
  KeyObjectHandle,
  kKeyFormatDER,
  kKeyTypePrivate,
  kKeyTypePublic,
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
  PrivateKeyObject,
  PublicKeyObject,
  createPrivateKey,
  createPublicKey,
  kAlgorithm,
  kKeyType,
} = require('internal/crypto/keys');

const generateKeyPair = promisify(_generateKeyPair);

async function mlKemGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['encapsulateKey', 'encapsulateBits', 'decapsulateKey', 'decapsulateBits'])) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }

  const keyPair = await generateKeyPair(name.toLowerCase()).catch((err) => {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  });

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
        if (key[kKeyType] === 'private') {
          return key[kKeyObject][kHandle].rawSeed().buffer;
        }

        return key[kKeyObject][kHandle].rawPublicKey().buffer;
      }
      case kWebCryptoKeyFormatSPKI: {
        return key[kKeyObject][kHandle].export(kKeyFormatDER, kWebCryptoKeyFormatSPKI).buffer;
      }
      case kWebCryptoKeyFormatPKCS8: {
        const seed = key[kKeyObject][kHandle].rawSeed();
        const buffer = new Uint8Array(86);
        buffer.set([
          0x30, 0x54, 0x02, 0x01, 0x00, 0x30, 0x0B, 0x06,
          0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04,
          0x04, 0x00, 0x04, 0x42, 0x80, 0x40,
        ], 0);
        switch (key[kAlgorithm].name) {
          case 'ML-KEM-512':
            buffer.set([0x01], 17);
            break;
          case 'ML-KEM-768':
            buffer.set([0x02], 17);
            break;
          case 'ML-KEM-1024':
            buffer.set([0x03], 17);
            break;
        }
        buffer.set(seed, 22);
        return buffer.buffer;
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

function createMlKemRawKey(name, keyData, isPublic) {
  const handle = new KeyObjectHandle();
  const keyType = isPublic ? kKeyTypePublic : kKeyTypePrivate;
  if (!handle.initPqcRaw(name, keyData, keyType)) {
    throw lazyDOMException('Invalid keyData', 'DataError');
  }

  return isPublic ? new PublicKeyObject(handle) : new PrivateKeyObject(handle);
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
      try {
        keyObject = createPublicKey({
          key: keyData,
          format: 'der',
          type: 'spki',
        });
      } catch (err) {
        throw lazyDOMException(
          'Invalid keyData', { name: 'DataError', cause: err });
      }
      break;
    }
    case 'pkcs8': {
      verifyAcceptableMlKemKeyUse(name, false, usagesSet);
      try {
        keyObject = createPrivateKey({
          key: keyData,
          format: 'der',
          type: 'pkcs8',
        });
      } catch (err) {
        throw lazyDOMException(
          'Invalid keyData', { name: 'DataError', cause: err });
      }
      break;
    }
    case 'raw-public':
    case 'raw-seed': {
      const isPublic = format === 'raw-public';
      verifyAcceptableMlKemKeyUse(name, isPublic, usagesSet);

      try {
        keyObject = createMlKemRawKey(name, keyData, isPublic);
      } catch (err) {
        throw lazyDOMException('Invalid keyData', { name: 'DataError', cause: err });
      }
      break;
    }
    default:
      return undefined;
  }

  if (keyObject.asymmetricKeyType !== name.toLowerCase()) {
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
    undefined);

  job.ondone = (error, result) => {
    if (error) {
      reject(lazyDOMException(
        'The operation failed for an operation-specific reason',
        { name: 'OperationError', cause: error }));
    } else {
      const { 0: sharedKey, 1: ciphertext } = result;
      resolve({ sharedKey: sharedKey.buffer, ciphertext: ciphertext.buffer });
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
    ciphertext);

  job.ondone = (error, result) => {
    if (error) {
      reject(lazyDOMException(
        'The operation failed for an operation-specific reason',
        { name: 'OperationError', cause: error }));
    } else {
      resolve(result.buffer);
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
