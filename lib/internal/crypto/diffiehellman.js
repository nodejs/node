'use strict';

const { Buffer } = require('buffer');
const {
  ERR_CRYPTO_ECDH_INVALID_FORMAT,
  ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY,
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;
const { isArrayBufferView } = require('internal/util/types');
const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');
const {
  DiffieHellman: _DiffieHellman,
  DiffieHellmanGroup: _DiffieHellmanGroup,
  ECDH: _ECDH,
  ECDHConvertKey: _ECDHConvertKey
} = process.binding('crypto');
const {
  POINT_CONVERSION_COMPRESSED,
  POINT_CONVERSION_HYBRID,
  POINT_CONVERSION_UNCOMPRESSED
} = process.binding('constants').crypto;

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

  this._handle = new _DiffieHellman(sizeOrKey, generator);
  Object.defineProperty(this, 'verifyError', {
    enumerable: true,
    value: this._handle.verifyError,
    writable: false
  });
}


function DiffieHellmanGroup(name) {
  if (!(this instanceof DiffieHellmanGroup))
    return new DiffieHellmanGroup(name);
  this._handle = new _DiffieHellmanGroup(name);
  Object.defineProperty(this, 'verifyError', {
    enumerable: true,
    value: this._handle.verifyError,
    writable: false
  });
}


DiffieHellmanGroup.prototype.generateKeys =
    DiffieHellman.prototype.generateKeys =
    dhGenerateKeys;

function dhGenerateKeys(encoding) {
  const keys = this._handle.generateKeys();
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
  const ret = this._handle.computeSecret(toBuf(key, inEnc));
  if (typeof ret === 'string')
    throw new ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY();
  return encode(ret, outEnc);
}


DiffieHellmanGroup.prototype.getPrime =
    DiffieHellman.prototype.getPrime =
    dhGetPrime;

function dhGetPrime(encoding) {
  const prime = this._handle.getPrime();
  encoding = encoding || getDefaultEncoding();
  return encode(prime, encoding);
}


DiffieHellmanGroup.prototype.getGenerator =
    DiffieHellman.prototype.getGenerator =
    dhGetGenerator;

function dhGetGenerator(encoding) {
  const generator = this._handle.getGenerator();
  encoding = encoding || getDefaultEncoding();
  return encode(generator, encoding);
}


DiffieHellmanGroup.prototype.getPublicKey =
    DiffieHellman.prototype.getPublicKey =
    dhGetPublicKey;

function dhGetPublicKey(encoding) {
  const key = this._handle.getPublicKey();
  encoding = encoding || getDefaultEncoding();
  return encode(key, encoding);
}


DiffieHellmanGroup.prototype.getPrivateKey =
    DiffieHellman.prototype.getPrivateKey =
    dhGetPrivateKey;

function dhGetPrivateKey(encoding) {
  const key = this._handle.getPrivateKey();
  encoding = encoding || getDefaultEncoding();
  return encode(key, encoding);
}


DiffieHellman.prototype.setPublicKey = function setPublicKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  this._handle.setPublicKey(toBuf(key, encoding));
  return this;
};


DiffieHellman.prototype.setPrivateKey = function setPrivateKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  this._handle.setPrivateKey(toBuf(key, encoding));
  return this;
};


function ECDH(curve) {
  if (!(this instanceof ECDH))
    return new ECDH(curve);

  if (typeof curve !== 'string')
    throw new ERR_INVALID_ARG_TYPE('curve', 'string', curve);

  this._handle = new _ECDH(curve);
}

ECDH.prototype.computeSecret = DiffieHellman.prototype.computeSecret;
ECDH.prototype.setPrivateKey = DiffieHellman.prototype.setPrivateKey;
ECDH.prototype.setPublicKey = DiffieHellman.prototype.setPublicKey;
ECDH.prototype.getPrivateKey = DiffieHellman.prototype.getPrivateKey;

ECDH.prototype.generateKeys = function generateKeys(encoding, format) {
  this._handle.generateKeys();

  return this.getPublicKey(encoding, format);
};

ECDH.prototype.getPublicKey = function getPublicKey(encoding, format) {
  const f = getFormat(format);
  const key = this._handle.getPublicKey(f);
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

  if (typeof curve !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('curve', 'string', curve);
  }

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

module.exports = {
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH
};
