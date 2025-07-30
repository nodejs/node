'use strict';

const {
  SafeSet,
} = primordials;

const { Buffer } = require('buffer');

const {
  KeyObjectHandle,
  SignJob,
  kCryptoJobAsync,
  kKeyTypePrivate,
  kKeyTypePublic,
  kSignJobModeSign,
  kSignJobModeVerify,
} = internalBinding('crypto');

const {
  codes: {
    ERR_CRYPTO_INVALID_JWK,
  },
} = require('internal/errors');

const {
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  validateKeyOps,
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
  createPublicKey,
} = require('internal/crypto/keys');

const generateKeyPair = promisify(_generateKeyPair);

function verifyAcceptableMlDsaKeyUse(name, isPublic, usages) {
  const checkSet = isPublic ? ['verify'] : ['sign'];
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for a ${name} key`,
      'SyntaxError');
  }
}

function createMlDsaRawKey(name, keyData, isPublic) {
  const handle = new KeyObjectHandle();
  const keyType = isPublic ? kKeyTypePublic : kKeyTypePrivate;
  if (!handle.initMlDsaRaw(name, keyData, keyType)) {
    throw lazyDOMException('Invalid keyData', 'DataError');
  }

  return isPublic ? new PublicKeyObject(handle) : new PrivateKeyObject(handle);
}

async function mlDsaGenerateKey(algorithm, extractable, keyUsages) {
  const { name } = algorithm;

  const usageSet = new SafeSet(keyUsages);
  if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }

  const keyPair = await generateKeyPair(name.toLowerCase()).catch((err) => {
    throw lazyDOMException(
      'The operation failed for an operation-specific reason',
      { name: 'OperationError', cause: err });
  });

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

function mlDsaExportKey(key) {
  try {
    if (key.type === 'private') {
      const { priv } = key[kKeyObject][kHandle].exportJwk({}, false);
      return Buffer.alloc(32, priv, 'base64url').buffer;
    }

    const { pub } = key[kKeyObject][kHandle].exportJwk({}, false);
    return Buffer.alloc(Buffer.byteLength(pub, 'base64url'), pub, 'base64url').buffer;
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
    case 'jwk': {
      if (!keyData.kty)
        throw lazyDOMException('Invalid keyData', 'DataError');
      if (keyData.kty !== 'AKP')
        throw lazyDOMException('Invalid JWK "kty" Parameter', 'DataError');
      if (keyData.alg !== name)
        throw lazyDOMException(
          'JWK "alg" Parameter and algorithm name mismatch', 'DataError');
      const isPublic = keyData.priv === undefined;

      if (usagesSet.size > 0 && keyData.use !== undefined) {
        if (keyData.use !== 'sig')
          throw lazyDOMException('Invalid JWK "use" Parameter', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException(
          'JWK "ext" Parameter and extractable mismatch',
          'DataError');
      }

      if (!isPublic && typeof keyData.pub !== 'string') {
        throw lazyDOMException('Invalid JWK', 'DataError');
      }

      verifyAcceptableMlDsaKeyUse(
        name,
        isPublic,
        usagesSet);

      try {
        const publicKeyObject = createMlDsaRawKey(
          name,
          Buffer.from(keyData.pub, 'base64url'),
          true);

        if (isPublic) {
          keyObject = publicKeyObject;
        } else {
          keyObject = createMlDsaRawKey(
            name,
            Buffer.from(keyData.priv, 'base64url'),
            false);

          if (!createPublicKey(keyObject).equals(publicKeyObject)) {
            throw new ERR_CRYPTO_INVALID_JWK();
          }
        }
      } catch (err) {
        throw lazyDOMException('Invalid keyData', { name: 'DataError', cause: err });
      }
      break;
    }
    case 'raw-public':
    case 'raw-seed': {
      const isPublic = format === 'raw-public';
      verifyAcceptableMlDsaKeyUse(name, isPublic, usagesSet);

      try {
        keyObject = createMlDsaRawKey(name, keyData, isPublic);
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

function mlDsaSignVerify(key, data, algorithm, signature) {
  const mode = signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
  const type = mode === kSignJobModeSign ? 'private' : 'public';

  if (key.type !== type)
    throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

  return jobPromise(() => new SignJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    undefined,
    undefined,
    undefined,
    data,
    undefined,
    undefined,
    undefined,
    undefined,
    signature));
}

module.exports = {
  mlDsaExportKey,
  mlDsaImportKey,
  mlDsaGenerateKey,
  mlDsaSignVerify,
};
