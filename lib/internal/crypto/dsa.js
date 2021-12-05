'use strict';

const {
  Promise,
  SafeSet,
  Symbol,
} = primordials;

const {
  DSAKeyExportJob,
  KeyObjectHandle,
  SignJob,
  Sign: _Sign,
  Verify: _Verify,
  kCryptoJobAsync,
  kSigEncDER,
  kKeyTypePrivate,
  kSignJobModeSign,
  kSignJobModeVerify,
} = internalBinding('crypto');

const {
  codes: {
    ERR_CRYPTO_HASH_FINALIZED,
    ERR_INVALID_ARG_TYPE,
    ERR_MISSING_OPTION,
  }
} = require('internal/errors');

const {
  validateUint32,
} = require('internal/validators');

const {
  InternalCryptoKey,
  PrivateKeyObject,
  PublicKeyObject,
  createPrivateKey,
  createPublicKey,
  isKeyObject,
  preparePrivateKey,
  preparePublicOrPrivateKey,
} = require('internal/crypto/keys');

const {
  Buffer,
} = require('buffer');

const {
  generateKeyPair,
} = require('internal/crypto/keygen');

const {
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  normalizeAlgorithm,
  normalizeHashName,
  validateKeyOps,
  kKeyObject,
  kHandle,
  kState,
} = require('internal/crypto/util');

const {
  createDeferredPromise,
  lazyDOMException,
} = require('internal/util');

const {
  isArrayBufferView,
} = require('internal/util/types');

const kFinalized = Symbol('kFinalized');

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
    case 'jwk': {
      if (keyData == null || typeof keyData !== 'object')
        throw lazyDOMException('Invalid JWK keyData', 'DataError');

      verifyAcceptableDsaKeyUse(
        algorithm.name,
        keyData.x !== undefined ? 'private' : 'public',
        usagesSet);

      if (keyData.kty !== 'DSA')
        throw lazyDOMException('Invalid key type', 'DataError');

      if (usagesSet.size > 0 &&
          keyData.use !== undefined &&
          keyData.use !== 'sig') {
        throw lazyDOMException('Invalid use type', 'DataError');
      }

      validateKeyOps(keyData.key_ops, usagesSet);

      if (keyData.ext !== undefined &&
          keyData.ext === false &&
          extractable === true) {
        throw lazyDOMException('JWK is not extractable', 'DataError');
      }

      if (keyData.alg !== undefined) {
        if (typeof keyData.alg !== 'string')
          throw lazyDOMException('Invalid alg', 'DataError');
        const hash =
          normalizeHashName(keyData.alg, normalizeHashName.kContextWebCrypto);
        if (hash !== algorithm.hash.name)
          throw lazyDOMException('Hash mismatch', 'DataError');
      }

      const handle = new KeyObjectHandle();
      const type = handle.initJwk(keyData);
      if (type === undefined)
        throw lazyDOMException('Invalid JWK keyData', 'DataError');

      keyObject = type === kKeyTypePrivate ?
        new PrivateKeyObject(handle) :
        new PublicKeyObject(handle);

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

class DsaSignSink {
  constructor(algorithm, key, signature) {
    const mode =
        signature === undefined ? kSignJobModeSign : kSignJobModeVerify;
    const type = mode === kSignJobModeSign ? 'private' : 'public';

    if (key.type !== type)
      throw lazyDOMException(`Key must be a ${type} key`, 'InvalidAccessError');

    const hash = normalizeAlgorithm(key.algorithm.hash);

    if (signature === undefined) {
      this[kHandle] = new _Sign();
    } else {
      this[kHandle] = new _Verify();
    }
    this[kHandle].init(normalizeHashName(hash.name));
    this[kState] = {
      ...createDeferredPromise(),
      key,
      mode,
      signature,
    };
  }

  async write(chunk) {
    if (this[kFinalized])
      throw new ERR_CRYPTO_HASH_FINALIZED();

    if (typeof chunk === 'string') {
      chunk = Buffer.from(chunk, 'utf8');
    } else if (!isArrayBufferView(chunk)) {
      throw new ERR_INVALID_ARG_TYPE(
        'chunk', ['string', 'Buffer', 'TypedArray', 'DataView'], chunk);
    }

    this[kHandle].update(chunk);
  }

  async close() {
    if (this[kFinalized])
      throw new ERR_CRYPTO_HASH_FINALIZED();
    this[kFinalized] = true;

    const {
      key,
      padding,
      saltLength,
      signature,
    } = this[kState];

    if (signature === undefined) {
      const {
        data,
      } = preparePrivateKey(key);

      const ret = this[kHandle].sign(
        data,
        undefined,
        undefined,
        undefined,
        padding,
        saltLength,
        0);
      this[kState].resolve(ret.buffer);
    } else {
      const {
        data,
      } = preparePublicOrPrivateKey(key);
      const sig = signature.buffer || signature;
      const ret = this[kHandle].verify(
        data,
        undefined,
        undefined,
        undefined,
        sig,
        undefined,
        undefined,
        kSigEncDER);
      this[kState].resolve(ret);
    }
  }

  async abort(reason) {
    if (this[kFinalized])
      return;
    this[kFinalized] = true;

    this[kState].reject(reason);
  }

  get signature() {
    return this[kState].promise;
  }
}

module.exports = {
  dsaExportKey,
  dsaGenerateKey,
  dsaImportKey,
  dsaSignVerify,
  DsaSignSink,
};
