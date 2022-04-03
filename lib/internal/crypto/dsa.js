'use strict';

const {
  Promise,
  SafeSet,
} = primordials;

const {
  DSAKeyExportJob,
  SignJob,
  kCryptoJobAsync,
  kSigEncDER,
  kSignJobModeSign,
  kSignJobModeVerify,
} = internalBinding('crypto');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_MISSING_OPTION,
  }
} = require('internal/errors');

const {
  validateUint32,
} = require('internal/validators');

const {
  InternalCryptoKey,
  createPrivateKey,
  createPublicKey,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  generateKeyPair,
} = require('internal/crypto/keygen');

const {
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  normalizeHashName,
  kKeyObject,
  kHandle,
} = require('internal/crypto/util');

const {
  lazyDOMException,
} = require('internal/util');

function verifyAcceptableDsaKeyUse(name, type, usages) {
  let checkSet;
  switch (type) {
    case 'private':
      checkSet = ['sign'];
      break;
    case 'public':
      checkSet = ['verify'];
      break;
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }
}

async function dsaGenerateKey(
  algorithm,
  extractable,
  keyUsages) {
  const {
    name,
    modulusLength,
    divisorLength,
    hash
  } = algorithm;

  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');
  validateUint32(modulusLength, 'algorithm.modulusLength');

  const usageSet = new SafeSet(keyUsages);

  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      'Unsupported key usage for a DSA key',
      'SyntaxError');
  }

  return new Promise((resolve, reject) => {
    generateKeyPair('dsa', {
      modulusLength,
      divisorLength,
    }, (err, pubKey, privKey) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      const algorithm = {
        name,
        modulusLength,
        divisorLength,
        hash: { name: hash.name }
      };

      const publicKey =
        new InternalCryptoKey(
          pubKey,
          algorithm,
          getUsagesUnion(usageSet, 'verify'),
          true);

      const privateKey =
        new InternalCryptoKey(
          privKey,
          algorithm,
          getUsagesUnion(usageSet, 'sign'),
          extractable);

      resolve({ publicKey, privateKey });
    });
  });
}

function dsaExportKey(key, format) {
  return jobPromise(new DSAKeyExportJob(
    kCryptoJobAsync,
    format,
    key[kKeyObject][kHandle]));
}

async function dsaImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {
  const { hash } = algorithm;
  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');

  const usagesSet = new SafeSet(keyUsages);
  let keyObject;
  switch (format) {
    case 'node.keyObject': {
      if (!isKeyObject(keyData))
        throw new ERR_INVALID_ARG_TYPE('keyData', 'KeyObject', keyData);
      if (keyData.type === 'secret')
        throw lazyDOMException('Invalid key type', 'InvalidAccessException');
      verifyAcceptableDsaKeyUse(algorithm.name, keyData.type, usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableDsaKeyUse(algorithm.name, 'public', usagesSet);
      keyObject = createPublicKey({
        key: keyData,
        format: 'der',
        type: 'spki'
      });
      break;
    }
    case 'pkcs8': {
      verifyAcceptableDsaKeyUse(algorithm.name, 'private', usagesSet);
      keyObject = createPrivateKey({
        key: keyData,
        format: 'der',
        type: 'pkcs8'
      });
      break;
    }
    default:
      throw lazyDOMException(
        `Unable to import DSA key with format ${format}`,
        'NotSupportedError');
  }

  if (keyObject.asymmetricKeyType !== 'dsa')
    throw lazyDOMException('Invalid key type', 'DataError');

  const {
    modulusLength,
    divisorLength,
  } = keyObject[kHandle].keyDetail({});

  return new InternalCryptoKey(keyObject, {
    name: algorithm.name,
    modulusLength,
    divisorLength,
    hash: algorithm.hash
  }, keyUsages, extractable);
}

function dsaSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key.type !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return jobPromise(new SignJob(
    kCryptoJobAsync,
    signature === undefined ? kSignJobModeSign : kSignJobModeVerify,
    key[kKeyObject][kHandle],
    undefined,
    undefined,
    undefined,
    data,
    normalizeHashName(key.algorithm.hash.name),
    undefined,  // Salt-length is not used in DSA
    undefined,  // Padding is not used in DSA
    kSigEncDER,
    signature));
}

module.exports = {
  dsaExportKey,
  dsaGenerateKey,
  dsaImportKey,
  dsaSignVerify,
};
