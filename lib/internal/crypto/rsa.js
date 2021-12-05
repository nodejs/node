'use strict';

const {
  Promise,
  SafeSet,
  Symbol,
  Uint8Array,
} = primordials;

const {
  KeyObjectHandle,
  RSACipherJob,
  RSAKeyExportJob,
  SignJob,
  Sign: _Sign,
  Verify: _Verify,
  kCryptoJobAsync,
  kSignJobModeSign,
  kSignJobModeVerify,
  kKeyVariantRSA_SSA_PKCS1_v1_5,
  kKeyVariantRSA_PSS,
  kKeyVariantRSA_OAEP,
  kKeyTypePrivate,
  kWebCryptoCipherEncrypt,
  RSA_PKCS1_PSS_PADDING,
} = internalBinding('crypto');

const {
  codes: {
    ERR_CRYPTO_HASH_FINALIZED,
    ERR_INVALID_ARG_TYPE,
    ERR_MISSING_OPTION,
  }
} = require('internal/errors');

const {
  Buffer,
} = require('buffer');

const {
  validateInt32,
  validateUint32,
} = require('internal/validators');

const {
  bigIntArrayToUnsignedInt,
  getArrayBufferOrView,
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  normalizeAlgorithm,
  normalizeHashName,
  validateKeyOps,
  validateMaxBufferLength,
  kHandle,
  kKeyObject,
  kState,
} = require('internal/crypto/util');

const {
  createDeferredPromise,
  lazyDOMException,
} = require('internal/util');

const {
  isUint8Array,
  isArrayBufferView,
} = require('internal/util/types');

const kFinalized = Symbol('kFinalized');

const {
  InternalCryptoKey,
  PrivateKeyObject,
  PublicKeyObject,
  createPublicKey,
  createPrivateKey,
  isKeyObject,
  preparePrivateKey,
  preparePublicOrPrivateKey,
} = require('internal/crypto/keys');

const {
  generateKeyPair,
} = require('internal/crypto/keygen');

const kRsaVariants = {
  'RSASSA-PKCS1-v1_5': kKeyVariantRSA_SSA_PKCS1_v1_5,
  'RSA-PSS': kKeyVariantRSA_PSS,
  'RSA-OAEP': kKeyVariantRSA_OAEP,
};

function verifyAcceptableRsaKeyUse(name, type, usages) {
  let checkSet;
  switch (name) {
    case 'RSA-OAEP':
      switch (type) {
        case 'private':
          checkSet = ['decrypt', 'unwrapKey'];
          break;
        case 'public':
          checkSet = ['encrypt', 'wrapKey'];
          break;
      }
      break;
    default:
      switch (type) {
        case 'private':
          checkSet = ['sign'];
          break;
        case 'public':
          checkSet = ['verify'];
          break;
      }
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }
}

function rsaOaepCipher(mode, key, data, { label }) {
  const type = mode === kWebCryptoCipherEncrypt ? 'public' : 'private';
  if (key.type !== type) {
    throw lazyDOMException(
      'The requested operation is not valid for the provided key',
      'InvalidAccessError');
  }
  if (label !== undefined) {
    label = getArrayBufferOrView(label, 'algorithm.label');
    validateMaxBufferLength(label, 'algorithm.label');
  }

  return jobPromise(new RSACipherJob(
    kCryptoJobAsync,
    mode,
    key[kKeyObject][kHandle],
    data,
    kKeyVariantRSA_OAEP,
    normalizeHashName(key.algorithm.hash.name),
    label));
}

async function rsaKeyGenerate(
  algorithm,
  extractable,
  keyUsages) {

  const {
    name,
    modulusLength,
    publicExponent,
    hash
  } = algorithm;

  if (hash === undefined)
    throw new ERR_MISSING_OPTION('algorithm.hash');
  validateUint32(modulusLength, 'algorithm.modulusLength');
  if (!isUint8Array(publicExponent)) {
    throw new ERR_INVALID_ARG_TYPE(
      'algorithm.publicExponent',
      'Uint8Array',
      publicExponent);
  }

  const usageSet = new SafeSet(keyUsages);

  const publicExponentConverted = bigIntArrayToUnsignedInt(publicExponent);
  if (publicExponentConverted === undefined) {
    throw lazyDOMException(
      'The publicExponent must be equivalent to an unsigned 32-bit value',
      'OperationError');
  }

  switch (name) {
    case 'RSA-OAEP':
      if (hasAnyNotIn(usageSet,
                      ['encrypt', 'decrypt', 'wrapKey', 'unwrapKey'])) {
        throw lazyDOMException(
          'Unsupported key usage for a RSA key',
          'SyntaxError');
      }
      break;
    default:
      if (hasAnyNotIn(usageSet, ['sign', 'verify'])) {
        throw lazyDOMException(
          'Unsupported key usage for a RSA key',
          'SyntaxError');
      }
  }

  return new Promise((resolve, reject) => {
    generateKeyPair('rsa', {
      modulusLength,
      publicExponentConverted,
    }, (err, pubKey, privKey) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      const algorithm = {
        name,
        modulusLength,
        publicExponent,
        hash: { name: hash.name }
      };

      let publicUsages;
      let privateUsages;
      switch (name) {
        case 'RSA-OAEP': {
          publicUsages = getUsagesUnion(usageSet, 'encrypt', 'wrapKey');
          privateUsages = getUsagesUnion(usageSet, 'decrypt', 'unwrapKey');
          break;
        }
        default: {
          publicUsages = getUsagesUnion(usageSet, 'verify');
          privateUsages = getUsagesUnion(usageSet, 'sign');
          break;
        }
      }

      const publicKey =
        new InternalCryptoKey(
          pubKey,
          algorithm,
          publicUsages,
          true);

      const privateKey =
        new InternalCryptoKey(
          privKey,
          algorithm,
          privateUsages,
          extractable);

      resolve({ publicKey, privateKey });
    });
  });
}

function rsaExportKey(key, format) {
  return jobPromise(new RSAKeyExportJob(
    kCryptoJobAsync,
    format,
    key[kKeyObject][kHandle],
    kRsaVariants[key.algorithm.name]));
}

async function rsaImportKey(
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
      verifyAcceptableRsaKeyUse(algorithm.name, keyData.type, usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableRsaKeyUse(algorithm.name, 'public', usagesSet);
      keyObject = createPublicKey({
        key: keyData,
        format: 'der',
        type: 'spki'
      });
      break;
    }
    case 'pkcs8': {
      verifyAcceptableRsaKeyUse(algorithm.name, 'private', usagesSet);
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

      verifyAcceptableRsaKeyUse(
        algorithm.name,
        keyData.d !== undefined ? 'private' : 'public',
        usagesSet);

      if (keyData.kty !== 'RSA')
        throw lazyDOMException('Invalid key type', 'DataError');

      if (usagesSet.size > 0 && keyData.use !== undefined) {
        const checkUse = algorithm.name === 'RSA-OAEP' ? 'enc' : 'sig';
        if (keyData.use !== checkUse)
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
        `Unable to import RSA key with format ${format}`,
        'NotSupportedError');
  }

  if (algorithm.name === 'RSA-PSS') {
    if (
      keyObject.asymmetricKeyType !== 'rsa' &&
      keyObject.asymmetricKeyType !== 'rsa-pss'
    ) {
      throw lazyDOMException('Invalid key type', 'DataError');
    }
  } else if (keyObject.asymmetricKeyType !== 'rsa') {
    throw lazyDOMException('Invalid key type', 'DataError');
  }

  const {
    modulusLength,
    publicExponent,
  } = keyObject[kHandle].keyDetail({});

  return new InternalCryptoKey(keyObject, {
    name: algorithm.name,
    modulusLength,
    publicExponent: new Uint8Array(publicExponent),
    hash: algorithm.hash
  }, keyUsages, extractable);
}

function rsaSignVerify(key, data, { saltLength }, signature) {
  let padding;
  if (key.algorithm.name === 'RSA-PSS') {
    padding = RSA_PKCS1_PSS_PADDING;
    // TODO(@jasnell): Validate maximum size of saltLength
    // based on the key size:
    //   Math.ceil((keySizeInBits - 1)/8) - digestSizeInBytes - 2
    validateInt32(saltLength, 'algorithm.saltLength', -2);
  }

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
    saltLength,
    padding,
    undefined,
    signature));
}

class RsaSignSink {
  constructor(algorithm, key, signature) {
    const padding = RSA_PKCS1_PSS_PADDING;
    let saltLength = -2;
    if (key.algorithm.name === 'RSA-PSS') {
      saltLength = algorithm.saltLength;
      // TODO(@jasnell): Validate maximum size of saltLength
      // based on the key size:
      //   Math.ceil((keySizeInBits - 1)/8) - digestSizeInBytes - 2
      validateInt32(saltLength, 'algorithm.saltLength', -2);
    }

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
      padding,
      saltLength,
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
        padding,
        saltLength,
        0);
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
  rsaCipher: rsaOaepCipher,
  rsaExportKey,
  rsaImportKey,
  rsaKeyGenerate,
  rsaSignVerify,
  RsaSignSink,
};
