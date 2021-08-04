'use strict';

const {
  ArrayBufferPrototypeSlice,
  FunctionPrototypeCall,
  MathFloor,
  ObjectDefineProperty,
  Promise,
  SafeSet,
} = primordials;

const { Buffer } = require('buffer');

const {
  DHBitsJob,
  DHKeyExportJob,
  DiffieHellman: _DiffieHellman,
  DiffieHellmanGroup: _DiffieHellmanGroup,
  ECDH: _ECDH,
  ECDHBitsJob,
  ECDHConvertKey: _ECDHConvertKey,
  statelessDH,
  kCryptoJobAsync,
} = internalBinding('crypto');

const {
  codes: {
    ERR_CRYPTO_ECDH_INVALID_FORMAT,
    ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY,
    ERR_CRYPTO_INCOMPATIBLE_KEY,
    ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  }
} = require('internal/errors');

const {
  validateCallback,
  validateInt32,
  validateObject,
  validateString,
  validateUint32,
} = require('internal/validators');

const {
  isArrayBufferView,
  isAnyArrayBuffer,
} = require('internal/util/types');

const {
  lazyDOMException,
} = require('internal/util');

const {
  KeyObject,
  InternalCryptoKey,
  createPrivateKey,
  createPublicKey,
  isCryptoKey,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  generateKeyPair,
} = require('internal/crypto/keygen');

const {
  getArrayBufferOrView,
  getDefaultEncoding,
  getUsagesUnion,
  hasAnyNotIn,
  jobPromise,
  toBuf,
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  crypto: {
    POINT_CONVERSION_COMPRESSED,
    POINT_CONVERSION_HYBRID,
    POINT_CONVERSION_UNCOMPRESSED,
  }
} = internalBinding('constants');

const DH_GENERATOR = 2;

function DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding) {
  if (!(this instanceof DiffieHellman))
    return new DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding);

  if (typeof sizeOrKey !== 'number' &&
      typeof sizeOrKey !== 'string' &&
      !isArrayBufferView(sizeOrKey) &&
      !isAnyArrayBuffer(sizeOrKey)) {
    throw new ERR_INVALID_ARG_TYPE(
      'sizeOrKey',
      ['number', 'string', 'ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
      sizeOrKey
    );
  }

  // Sizes < 0 don't make sense but they _are_ accepted (and subsequently
  // rejected with ERR_OSSL_BN_BITS_TOO_SMALL) by OpenSSL. The glue code
  // in node_crypto.cc accepts values that are IsInt32() for that reason
  // and that's why we do that here too.
  if (typeof sizeOrKey === 'number')
    validateInt32(sizeOrKey, 'sizeOrKey');

  if (keyEncoding && !Buffer.isEncoding(keyEncoding) &&
      keyEncoding !== 'buffer') {
    genEncoding = generator;
    generator = keyEncoding;
    keyEncoding = false;
  }

  const encoding = getDefaultEncoding();
  keyEncoding = keyEncoding || encoding;
  genEncoding = genEncoding || encoding;

  if (typeof sizeOrKey !== 'number')
    sizeOrKey = toBuf(sizeOrKey, keyEncoding);

  if (!generator) {
    generator = DH_GENERATOR;
  } else if (typeof generator === 'number') {
    validateInt32(generator, 'generator');
  } else if (typeof generator === 'string') {
    generator = toBuf(generator, genEncoding);
  } else if (!isArrayBufferView(generator) && !isAnyArrayBuffer(generator)) {
    throw new ERR_INVALID_ARG_TYPE(
      'generator',
      ['number', 'string', 'ArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
      generator
    );
  }


  this[kHandle] = new _DiffieHellman(sizeOrKey, generator);
  ObjectDefineProperty(this, 'verifyError', {
    enumerable: true,
    value: this[kHandle].verifyError,
    writable: false
  });
}


function DiffieHellmanGroup(name) {
  if (!(this instanceof DiffieHellmanGroup))
    return new DiffieHellmanGroup(name);
  this[kHandle] = new _DiffieHellmanGroup(name);
  ObjectDefineProperty(this, 'verifyError', {
    enumerable: true,
    value: this[kHandle].verifyError,
    writable: false
  });
}


DiffieHellmanGroup.prototype.generateKeys =
    DiffieHellman.prototype.generateKeys =
    dhGenerateKeys;

function dhGenerateKeys(encoding) {
  const keys = this[kHandle].generateKeys();
  encoding = encoding || getDefaultEncoding();
  return encode(keys, encoding);
}


DiffieHellmanGroup.prototype.computeSecret =
    DiffieHellman.prototype.computeSecret =
    dhComputeSecret;

function dhComputeSecret(key, inEnc, outEnc) {
  const encoding = getDefaultEncoding();
  inEnc = inEnc || encoding;
  outEnc = outEnc || encoding;
  key = getArrayBufferOrView(key, 'key', inEnc);
  const ret = this[kHandle].computeSecret(key);
  if (typeof ret === 'string')
    throw new ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY();
  return encode(ret, outEnc);
}


DiffieHellmanGroup.prototype.getPrime =
    DiffieHellman.prototype.getPrime =
    dhGetPrime;

function dhGetPrime(encoding) {
  const prime = this[kHandle].getPrime();
  encoding = encoding || getDefaultEncoding();
  return encode(prime, encoding);
}


DiffieHellmanGroup.prototype.getGenerator =
    DiffieHellman.prototype.getGenerator =
    dhGetGenerator;

function dhGetGenerator(encoding) {
  const generator = this[kHandle].getGenerator();
  encoding = encoding || getDefaultEncoding();
  return encode(generator, encoding);
}


DiffieHellmanGroup.prototype.getPublicKey =
    DiffieHellman.prototype.getPublicKey =
    dhGetPublicKey;

function dhGetPublicKey(encoding) {
  const key = this[kHandle].getPublicKey();
  encoding = encoding || getDefaultEncoding();
  return encode(key, encoding);
}


DiffieHellmanGroup.prototype.getPrivateKey =
    DiffieHellman.prototype.getPrivateKey =
    dhGetPrivateKey;

function dhGetPrivateKey(encoding) {
  const key = this[kHandle].getPrivateKey();
  encoding = encoding || getDefaultEncoding();
  return encode(key, encoding);
}


DiffieHellman.prototype.setPublicKey = function setPublicKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  key = getArrayBufferOrView(key, 'key', encoding);
  this[kHandle].setPublicKey(key);
  return this;
};


DiffieHellman.prototype.setPrivateKey = function setPrivateKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  key = getArrayBufferOrView(key, 'key', encoding);
  this[kHandle].setPrivateKey(key);
  return this;
};


function ECDH(curve) {
  if (!(this instanceof ECDH))
    return new ECDH(curve);

  validateString(curve, 'curve');
  this[kHandle] = new _ECDH(curve);
}

ECDH.prototype.computeSecret = DiffieHellman.prototype.computeSecret;
ECDH.prototype.setPrivateKey = DiffieHellman.prototype.setPrivateKey;
ECDH.prototype.setPublicKey = DiffieHellman.prototype.setPublicKey;
ECDH.prototype.getPrivateKey = DiffieHellman.prototype.getPrivateKey;

ECDH.prototype.generateKeys = function generateKeys(encoding, format) {
  this[kHandle].generateKeys();

  return this.getPublicKey(encoding, format);
};

ECDH.prototype.getPublicKey = function getPublicKey(encoding, format) {
  const f = getFormat(format);
  const key = this[kHandle].getPublicKey(f);
  encoding = encoding || getDefaultEncoding();
  return encode(key, encoding);
};

ECDH.convertKey = function convertKey(key, curve, inEnc, outEnc, format) {
  validateString(curve, 'curve');
  const encoding = inEnc || getDefaultEncoding();
  key = getArrayBufferOrView(key, 'key', encoding);
  outEnc = outEnc || encoding;
  const f = getFormat(format);
  const convertedKey = _ECDHConvertKey(key, curve, f);
  return encode(convertedKey, outEnc);
};

function encode(buffer, encoding) {
  if (encoding && encoding !== 'buffer')
    buffer = buffer.toString(encoding);
  return buffer;
}

function getFormat(format) {
  if (format) {
    if (format === 'compressed')
      return POINT_CONVERSION_COMPRESSED;
    if (format === 'hybrid')
      return POINT_CONVERSION_HYBRID;
    if (format !== 'uncompressed')
      throw new ERR_CRYPTO_ECDH_INVALID_FORMAT(format);
  }
  return POINT_CONVERSION_UNCOMPRESSED;
}

const dhEnabledKeyTypes = new SafeSet(['dh', 'ec', 'x448', 'x25519']);

function diffieHellman(options) {
  validateObject(options, 'options');

  const { privateKey, publicKey } = options;
  if (!(privateKey instanceof KeyObject))
    throw new ERR_INVALID_ARG_VALUE('options.privateKey', privateKey);

  if (!(publicKey instanceof KeyObject))
    throw new ERR_INVALID_ARG_VALUE('options.publicKey', publicKey);

  if (privateKey.type !== 'private')
    throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(privateKey.type, 'private');

  if (publicKey.type !== 'public' && publicKey.type !== 'private') {
    throw new ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE(publicKey.type,
                                                 'private or public');
  }

  const privateType = privateKey.asymmetricKeyType;
  const publicType = publicKey.asymmetricKeyType;
  if (privateType !== publicType || !dhEnabledKeyTypes.has(privateType)) {
    throw new ERR_CRYPTO_INCOMPATIBLE_KEY('key types for Diffie-Hellman',
                                          `${privateType} and ${publicType}`);
  }

  return statelessDH(privateKey[kHandle], publicKey[kHandle]);
}

// The deriveBitsECDH function is part of the Web Crypto API and serves both
// deriveKeys and deriveBits functions.
function deriveBitsECDH(name, publicKey, privateKey, callback) {
  validateString(name, 'name');
  validateObject(publicKey, 'publicKey');
  validateObject(privateKey, 'privateKey');
  validateCallback(callback);
  const job = new ECDHBitsJob(kCryptoJobAsync, name, publicKey, privateKey);
  job.ondone = (error, bits) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, bits);
  };
  job.run();
}

// The deriveBitsDH function is part of the Web Crypto API and serves both
// deriveKeys and deriveBits functions.
function deriveBitsDH(publicKey, privateKey, callback) {
  validateObject(publicKey, 'publicKey');
  validateObject(privateKey, 'privateKey');
  validateCallback(callback);
  const job = new DHBitsJob(kCryptoJobAsync, publicKey, privateKey);
  job.ondone = (error, bits) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, bits);
  };
  job.run();
}

function verifyAcceptableDhKeyUse(name, type, usages) {
  let checkSet;
  switch (type) {
    case 'private':
      checkSet = ['deriveBits', 'deriveKey'];
      break;
    case 'public':
      checkSet = [];
      break;
  }
  if (hasAnyNotIn(usages, checkSet)) {
    throw lazyDOMException(
      `Unsupported key usage for an ${name} key`,
      'SyntaxError');
  }
}

async function dhGenerateKey(
  algorithm,
  extractable,
  keyUsages) {
  const usageSet = new SafeSet(keyUsages);

  if (hasAnyNotIn(usageSet, ['deriveKey', 'deriveBits'])) {
    throw lazyDOMException(
      'Unsupported key usage for a DH key',
      'SyntaxError');
  }

  const {
    name,
    primeLength,
    generator,
    group
  } = algorithm;
  let { prime } = algorithm;

  if (prime !== undefined)
    prime = getArrayBufferOrView(prime);

  return new Promise((resolve, reject) => {
    generateKeyPair('dh', {
      prime,
      primeLength,
      generator,
      group,
    }, (err, pubKey, privKey) => {
      if (err) {
        return reject(lazyDOMException(
          'The operation failed for an operation-specific reason',
          'OperationError'));
      }

      const algorithm = { name, prime, primeLength, generator, group };

      const publicKey = new InternalCryptoKey(pubKey, algorithm, [], true);

      const privateKey =
        new InternalCryptoKey(
          privKey,
          algorithm,
          getUsagesUnion(usageSet, 'deriveBits', 'deriveKey'),
          extractable);

      resolve({ publicKey, privateKey });
    });
  });
}

async function asyncDeriveBitsECDH(algorithm, baseKey, length) {
  const { 'public': key } = algorithm;

  // Null means that we're not asking for a specific number of bits, just
  // give us everything that is generated.
  if (length !== null)
    validateUint32(length, 'length');
  if (!isCryptoKey(key))
    throw new ERR_INVALID_ARG_TYPE('algorithm.public', 'CryptoKey', key);

  if (key.type !== 'public') {
    throw lazyDOMException(
      'algorithm.public must be a public key', 'InvalidAccessError');
  }
  if (baseKey.type !== 'private') {
    throw lazyDOMException(
      'baseKey must be a private key', 'InvalidAccessError');
  }

  if (key.algorithm.name !== 'ECDH') {
    throw lazyDOMException('Keys must be ECDH keys', 'InvalidAccessError');
  }

  if (key.algorithm.name !== baseKey.algorithm.name) {
    throw lazyDOMException(
      'The public and private keys must be of the same type',
      'InvalidAccessError');
  }

  if (key.algorithm.namedCurve !== baseKey.algorithm.namedCurve)
    throw lazyDOMException('Named curve mismatch', 'InvalidAccessError');

  const bits = await new Promise((resolve, reject) => {
    deriveBitsECDH(
      baseKey.algorithm.namedCurve,
      key[kKeyObject][kHandle],
      baseKey[kKeyObject][kHandle], (err, bits) => {
        if (err) return reject(err);
        resolve(bits);
      });
  });

  // If a length is not specified, return the full derived secret
  if (length === null)
    return bits;

  // If the length is not a multiple of 8, it will be truncated
  // down to the nearest multiple of 8.
  length = MathFloor(length / 8);
  const { byteLength } = bits;

  // If the length is larger than the derived secret, throw.
  // Otherwise, we either return the secret or a truncated
  // slice.
  if (byteLength < length)
    throw lazyDOMException('derived bit length is too small', 'OperationError');

  return length === byteLength ?
    bits :
    ArrayBufferPrototypeSlice(bits, 0, length);
}

async function asyncDeriveBitsDH(algorithm, baseKey, length) {
  const { 'public': key } = algorithm;
  // Null has a specific meaning for DH
  if (length !== null)
    validateUint32(length, 'length');
  if (!isCryptoKey(key))
    throw new ERR_INVALID_ARG_TYPE('algorithm.public', 'CryptoKey', key);

  if (key.type !== 'public') {
    throw lazyDOMException(
      'algorithm.public must be a public key', 'InvalidAccessError');
  }
  if (baseKey.type !== 'private') {
    throw lazyDOMException(
      'baseKey must be a private key', 'InvalidAccessError');
  }

  if (key.algorithm.name !== 'NODE-DH')
    throw lazyDOMException('Keys must be DH keys', 'InvalidAccessError');

  if (key.algorithm.name !== baseKey.algorithm.name) {
    throw lazyDOMException(
      'The public and private keys must be of the same type',
      'InvalidAccessError');
  }

  const bits = await new Promise((resolve, reject) => {
    deriveBitsDH(
      key[kKeyObject][kHandle],
      baseKey[kKeyObject][kHandle], (err, bits) => {
        if (err) return reject(err);
        resolve(bits);
      });
  });

  // If a length is not specified, return the full derived secret
  if (length === null)
    return bits;

  // If the length is not a multiple of 8, it will be truncated
  // down to the nearest multiple of 8.
  length = MathFloor(length / 8);
  const { byteLength } = bits;

  // If the length is larger than the derived secret, throw.
  // Otherwise, we either return the secret or a truncated
  // slice.
  if (byteLength < length)
    throw lazyDOMException('derived bit length is too small', 'OperationError');

  return length === byteLength ?
    bits :
    ArrayBufferPrototypeSlice(bits, 0, length);
}

function dhExportKey(key, format) {
  return jobPromise(new DHKeyExportJob(
    kCryptoJobAsync,
    format,
    key[kKeyObject][kHandle]));
}

async function dhImportKey(
  format,
  keyData,
  algorithm,
  extractable,
  keyUsages) {
  const usagesSet = new SafeSet(keyUsages);
  let keyObject;
  switch (format) {
    case 'node.keyObject': {
      if (!isKeyObject(keyData))
        throw new ERR_INVALID_ARG_TYPE('keyData', 'KeyObject', keyData);
      if (keyData.type === 'secret')
        throw lazyDOMException('Invalid key type', 'InvalidAccessException');
      verifyAcceptableDhKeyUse(algorithm.name, keyData.type, usagesSet);
      keyObject = keyData;
      break;
    }
    case 'spki': {
      verifyAcceptableDhKeyUse(algorithm.name, 'public', usagesSet);
      keyObject = createPublicKey({
        key: keyData,
        format: 'der',
        type: 'spki'
      });
      break;
    }
    case 'pkcs8': {
      verifyAcceptableDhKeyUse(algorithm.name, 'private', usagesSet);
      keyObject = createPrivateKey({
        key: keyData,
        format: 'der',
        type: 'pkcs8'
      });
      break;
    }
    default:
      throw lazyDOMException(
        `Unable to import DH key with format ${format}`,
        'NotSupportedError');
  }

  const {
    prime,
    primeLength,
    generator,
    group,
  } = keyObject[kHandle].keyDetail({});

  return new InternalCryptoKey(keyObject, {
    name: algorithm.name,
    prime,
    primeLength,
    generator,
    group,
  }, keyUsages, extractable);
}

module.exports = {
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH,
  diffieHellman,
  deriveBitsECDH,
  deriveBitsDH,
  dhGenerateKey,
  asyncDeriveBitsECDH,
  asyncDeriveBitsDH,
  dhExportKey,
  dhImportKey,
};
