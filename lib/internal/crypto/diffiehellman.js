'use strict';

const { Buffer } = require('buffer');
const errors = require('internal/errors');
const { isArrayBufferView } = require('internal/util/types');
const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');
const {
  DiffieHellman: _DiffieHellman,
  DiffieHellmanGroup: _DiffieHellmanGroup,
  ECDH: _ECDH
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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'sizeOrKey',
                               ['number', 'string', 'Buffer', 'TypedArray',
                                'DataView']);
  }

  if (keyEncoding) {
    if (typeof keyEncoding !== 'string' ||
        (!Buffer.isEncoding(keyEncoding) && keyEncoding !== 'buffer')) {
      genEncoding = generator;
      generator = keyEncoding;
      keyEncoding = false;
    }
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
  var keys = this._handle.generateKeys();
  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    keys = keys.toString(encoding);
  return keys;
}


DiffieHellmanGroup.prototype.computeSecret =
    DiffieHellman.prototype.computeSecret =
    dhComputeSecret;

function dhComputeSecret(key, inEnc, outEnc) {
  const encoding = getDefaultEncoding();
  inEnc = inEnc || encoding;
  outEnc = outEnc || encoding;
  var ret = this._handle.computeSecret(toBuf(key, inEnc));
  if (typeof ret === 'string')
    throw new errors.Error('ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY');
  if (outEnc && outEnc !== 'buffer')
    ret = ret.toString(outEnc);
  return ret;
}


DiffieHellmanGroup.prototype.getPrime =
    DiffieHellman.prototype.getPrime =
    dhGetPrime;

function dhGetPrime(encoding) {
  var prime = this._handle.getPrime();
  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    prime = prime.toString(encoding);
  return prime;
}


DiffieHellmanGroup.prototype.getGenerator =
    DiffieHellman.prototype.getGenerator =
    dhGetGenerator;

function dhGetGenerator(encoding) {
  var generator = this._handle.getGenerator();
  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    generator = generator.toString(encoding);
  return generator;
}


DiffieHellmanGroup.prototype.getPublicKey =
    DiffieHellman.prototype.getPublicKey =
    dhGetPublicKey;

function dhGetPublicKey(encoding) {
  var key = this._handle.getPublicKey();
  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
}


DiffieHellmanGroup.prototype.getPrivateKey =
    DiffieHellman.prototype.getPrivateKey =
    dhGetPrivateKey;

function dhGetPrivateKey(encoding) {
  var key = this._handle.getPrivateKey();
  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'curve', 'string');

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
  var f;
  if (format) {
    if (format === 'compressed')
      f = POINT_CONVERSION_COMPRESSED;
    else if (format === 'hybrid')
      f = POINT_CONVERSION_HYBRID;
    // Default
    else if (format === 'uncompressed')
      f = POINT_CONVERSION_UNCOMPRESSED;
    else
      throw new errors.TypeError('ERR_CRYPTO_ECDH_INVALID_FORMAT', format);
  } else {
    f = POINT_CONVERSION_UNCOMPRESSED;
  }
  var key = this._handle.getPublicKey(f);
  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    key = key.toString(encoding);
  return key;
};

module.exports = {
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH
};
