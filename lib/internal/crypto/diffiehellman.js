'use strict';

const {
  ObjectDefineProperty,
  Set
} = primordials;

const { Buffer } = require('buffer');
const {
  ERR_CRYPTO_ECDH_INVALID_FORMAT,
  ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY,
  ERR_CRYPTO_INCOMPATIBLE_KEY,
  ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const { isArrayBufferView } = require('internal/util/types');
const { KeyObject } = require('internal/crypto/keys');
const {
  getDefaultEncoding,
  kHandle,
  toBuf
} = require('internal/crypto/util');
const {
  DiffieHellman: _DiffieHellman,
  DiffieHellmanGroup: _DiffieHellmanGroup,
  ECDH: _ECDH,
  ECDHConvertKey: _ECDHConvertKey,
  statelessDH
} = internalBinding('crypto');
const {
  POINT_CONVERSION_COMPRESSED,
  POINT_CONVERSION_HYBRID,
  POINT_CONVERSION_UNCOMPRESSED
} = internalBinding('constants').crypto;

const DH_GENERATOR = 2;

function DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding) {
  if (!(this instanceof DiffieHellman))
    return new DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding);

  if (typeof sizeOrKey !== 'number' &&
      typeof sizeOrKey !== 'string' &&
      !isArrayBufferView(sizeOrKey)) {
    throw new ERR_INVALID_ARG_TYPE(
      'sizeOrKey',
      ['number', 'string', 'Buffer', 'TypedArray', 'DataView'],
      sizeOrKey
    );
  }

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

  if (!generator)
    generator = DH_GENERATOR;
  else if (typeof generator !== 'number')
    generator = toBuf(generator, genEncoding);

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
  const ret = this[kHandle].computeSecret(toBuf(key, inEnc));
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
  this[kHandle].setPublicKey(toBuf(key, encoding));
  return this;
};


DiffieHellman.prototype.setPrivateKey = function setPrivateKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  this[kHandle].setPrivateKey(toBuf(key, encoding));
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
  if (typeof key !== 'string' && !isArrayBufferView(key)) {
    throw new ERR_INVALID_ARG_TYPE(
      'key',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      key
    );
  }

  validateString(curve, 'curve');

  const encoding = getDefaultEncoding();
  inEnc = inEnc || encoding;
  outEnc = outEnc || encoding;
  const f = getFormat(format);
  const convertedKey = _ECDHConvertKey(toBuf(key, inEnc), curve, f);
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

const dhEnabledKeyTypes = new Set(['dh', 'ec', 'x448', 'x25519']);

function diffieHellman(options) {
  if (typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'object', options);

  const { privateKey, publicKey } = options;
  if (!(privateKey instanceof KeyObject))
    throw new ERR_INVALID_OPT_VALUE('privateKey', privateKey);

  if (!(publicKey instanceof KeyObject))
    throw new ERR_INVALID_OPT_VALUE('publicKey', publicKey);

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

module.exports = {
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH,
  diffieHellman
};
