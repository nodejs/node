'use strict';

const { Buffer } = require('buffer');
const errors = require('internal/errors');
const { isArrayBufferView } = require('internal/util/types');
const {
  getCurves,
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');
const {
  DiffieHellman: _DiffieHellman,
  DiffieHellmanGroup: _DiffieHellmanGroup,
  ECDH: _ECDH,
  DH_INVALID_KEY,
  DH_INVALID_KEY_TOO_LARGE,
  DH_INVALID_KEY_TOO_SMALL,
  ECDH_FAILED_TO_ALLOCATE_EC_POINT,
  ECDH_FAILED_TO_COMPUTE_ECDH_KEY,
  ECDH_FAILED_TO_CONVERT_BUFFER_TO_BN,
  ECDH_FAILED_TO_CONVERT_BUFFER_TO_POINT,
  ECDH_FAILED_TO_CONVERT_CN_TO_PRIVATEKEY,
  ECDH_FAILED_TO_GENERATE_PUBLIC_KEY,
  ECDH_FAILED_TO_SET_POINT_AS_PUBLIC_KEY,
  ECDH_FAILED_TO_SET_GENERATED_PUBLIC_KEY,
  ECDH_INVALID_KEY_PAIR,
  ECDH_INVALID_PRIVATE_KEY,
  dhGroupNames
} = process.binding('crypto');
const {
  POINT_CONVERSION_COMPRESSED,
  POINT_CONVERSION_HYBRID,
  POINT_CONVERSION_UNCOMPRESSED
} = process.binding('constants').crypto;

const DH_GENERATOR = 2;

const dhGroupsNamesSet = new Set(dhGroupNames);
const ecdhCurvesSet = new Set(getCurves());

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

  if (generator === undefined)
    generator = DH_GENERATOR;
  else if (typeof generator !== 'number')
    generator = toBuf(generator, genEncoding);

  if (typeof generator !== 'number' &&
      typeof generator !== 'string' &&
      !isArrayBufferView(generator)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'generator',
                               ['number', 'string', 'Buffer', 'TypedArray',
                                'DataView']);
  }

  this._handle = new _DiffieHellman(sizeOrKey, generator);
  if (!this._handle.getInitialized()) {
    const err = new errors.Error('ERR_CRYPTO_DH_INITIALIZATION_FAILED');
    err.opensslErrorStack = this._handle.opensslErrorStack;
    this._handle.opensslErrorStack = undefined;
    throw err;
  }
  Object.defineProperty(this, 'verifyError', {
    enumerable: true,
    value: this._handle.verifyError,
    writable: false
  });
}


function DiffieHellmanGroup(name) {
  if (!(this instanceof DiffieHellmanGroup))
    return new DiffieHellmanGroup(name);

  if (typeof name !== 'string')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'name', 'string');

  if (!dhGroupsNamesSet.has(name.toLowerCase()))
    throw new errors.TypeError('ERR_CRYPTO_UNKNOWN_DH_GROUP', name);

  this._handle = new _DiffieHellmanGroup(name);
  if (!this._handle.getInitialized())
    throw new errors.Error('ERR_CRYPTO_DH_INITIALIZATION_FAILED');

  Object.defineProperty(this, 'verifyError', {
    enumerable: true,
    value: this._handle.verifyError,
    writable: false
  });
}

DiffieHellmanGroup.getNames = function() {
  return dhGroupNames.slice();
};

DiffieHellmanGroup.prototype.generateKeys =
    DiffieHellman.prototype.generateKeys =
    dhGenerateKeys;

function dhGenerateKeys(encoding) {
  const keys = this._handle.generateKeys();
  if (keys === undefined) {
    const err = new errors.Error('ERR_CRYPTO_DH_GENERATE_KEYS_FAILED');
    err.opensslErrorStack = this._handle.opensslErrorStack;
    this._handle.opensslErrorStack = undefined;
    throw err;
  }
  encoding = encoding || getDefaultEncoding();
  return encoding && encoding !== 'buffer' ?
    keys.toString(encoding) : keys;
}


DiffieHellmanGroup.prototype.computeSecret =
    DiffieHellman.prototype.computeSecret =
    dhComputeSecret;

function dhComputeSecret(key, inEnc, outEnc) {
  const encoding = getDefaultEncoding();
  inEnc = inEnc || encoding;
  outEnc = outEnc || encoding;

  key = toBuf(key, inEnc);
  if (!isArrayBufferView(key)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'key',
                               ['string', 'Buffer', 'TypedArray', 'DataView']);
  }

  const ret = this._handle.computeSecret(key);

  switch (ret) {
    case DH_INVALID_KEY:
      throw new errors.Error('ERR_CRYPTO_DH_INVALID_KEY');
    case DH_INVALID_KEY_TOO_LARGE:
      throw new errors.Error('ERR_CRYPTO_DH_INVALID_KEY_TOO_LARGE');
    case DH_INVALID_KEY_TOO_SMALL:
      throw new errors.Error('ERR_CRYPTO_DH_INVALID_KEY_TOO_SMALL');
    case ECDH_INVALID_KEY_PAIR:
      throw new errors.Error('ERR_CRYPTO_ECDH_INVALID_KEY_PAIR');
    case ECDH_FAILED_TO_COMPUTE_ECDH_KEY:
      throw new errors.Error('ERR_CRYPTO_ECDH_FAILED_TO_COMPUTE_ECDH_KEY');
    case ECDH_FAILED_TO_ALLOCATE_EC_POINT:
      throw new errors.Error('ERR_CRYPTO_ECDH_FAILED_TO_ALLOCATE_EC_POINT');
    case ECDH_FAILED_TO_CONVERT_BUFFER_TO_POINT:
      throw new errors.Error(
        'ERR_CRYPTO_ECDH_FAILED_TO_CONVERT_BUFFER_TO_POINT');
    default:
      if (outEnc && outEnc !== 'buffer')
        return ret.toString(outEnc);
      return ret;
  }
}


DiffieHellmanGroup.prototype.getPrime =
    DiffieHellman.prototype.getPrime =
    dhGetPrime;

function dhGetPrime(encoding) {
  const prime = this._handle.getPrime();
  encoding = encoding || getDefaultEncoding();
  if (prime !== undefined && encoding && encoding !== 'buffer')
    return prime.toString(encoding);
  return prime;
}


DiffieHellmanGroup.prototype.getGenerator =
    DiffieHellman.prototype.getGenerator =
    dhGetGenerator;

function dhGetGenerator(encoding) {
  const generator = this._handle.getGenerator();
  encoding = encoding || getDefaultEncoding();
  if (generator !== undefined && encoding && encoding !== 'buffer')
    return generator.toString(encoding);
  return generator;
}


DiffieHellmanGroup.prototype.getPublicKey =
    DiffieHellman.prototype.getPublicKey =
    dhGetPublicKey;

function dhGetPublicKey(encoding) {
  const key = this._handle.getPublicKey();
  encoding = encoding || getDefaultEncoding();
  if (key !== undefined && encoding && encoding !== 'buffer')
    return key.toString(encoding);
  return key;
}


DiffieHellmanGroup.prototype.getPrivateKey =
    DiffieHellman.prototype.getPrivateKey =
    dhGetPrivateKey;

function dhGetPrivateKey(encoding) {
  const key = this._handle.getPrivateKey();
  encoding = encoding || getDefaultEncoding();
  if (key !== undefined && encoding && encoding !== 'buffer')
    return key.toString(encoding);
  return key;
}


DiffieHellman.prototype.setPublicKey = function setPublicKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  key = toBuf(key, encoding);
  if (!isArrayBufferView(key)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'publicKey',
                               ['string', 'Buffer', 'TypedArray', 'DataView']);
  }
  switch (this._handle.setPublicKey(key)) {
    case ECDH_FAILED_TO_CONVERT_BUFFER_TO_POINT:
      throw new errors.Error(
        'ERR_CRYPTO_ECDH_FAILED_TO_CONVERT_BUFFER_TO_POINT');
    case ECDH_FAILED_TO_SET_POINT_AS_PUBLIC_KEY:
      throw new errors.Error(
        'ERR_CRYPTO_ECDH_FAILED_TO_SET_POINT_AS_PUBLIC_KEY');
    case ECDH_FAILED_TO_ALLOCATE_EC_POINT:
      throw new errors.Error('ERR_CRYPTO_ECDH_FAILED_TO_ALLOCATE_EC_POINT');
    default:
      return this;
  }
};


DiffieHellman.prototype.setPrivateKey = function setPrivateKey(key, encoding) {
  encoding = encoding || getDefaultEncoding();
  key = toBuf(key, encoding);
  if (!isArrayBufferView(key)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'publicKey',
                               ['string', 'Buffer', 'TypedArray', 'DataView']);
  }
  switch (this._handle.setPrivateKey(key)) {
    case ECDH_FAILED_TO_CONVERT_BUFFER_TO_BN:
      throw new errors.Error('ERR_CRYPTO_ECDH_FAILED_TO_CONVERT_BUFFER_TO_BN');
    case ECDH_INVALID_PRIVATE_KEY:
      throw new errors.Error('ERR_CRYPTO_ECDH_INVALID_PRIVATE_KEY');
    case ECDH_FAILED_TO_CONVERT_CN_TO_PRIVATEKEY:
      throw new errors.Error(
        'ERR_CRYPTO_ECDH_FAILED_TO_CONVERT_CN_TO_PRIVATEKEY');
    case ECDH_FAILED_TO_SET_GENERATED_PUBLIC_KEY:
      throw new errors.Error(
        'ERR_CRYPTO_ECDH_FAILED_TO_SET_GENERATED_PUBLIC_KEY');
    case ECDH_FAILED_TO_GENERATE_PUBLIC_KEY:
      throw new errors.Error('ERR_CRYPTO_ECDH_FAILED_TO_GENERATE_PUBLIC_KEY');
    default:
      return this;
  }
};


function ECDH(curve) {
  if (!(this instanceof ECDH))
    return new ECDH(curve);

  if (typeof curve !== 'string')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'curve', 'string');

  if (!ecdhCurvesSet.has(curve))
    throw new errors.TypeError('ERR_CRYPTO_ECDH_UNKNOWN_CURVE', curve);

  this._handle = new _ECDH(curve);
}

ECDH.prototype.computeSecret = DiffieHellman.prototype.computeSecret;
ECDH.prototype.setPrivateKey = DiffieHellman.prototype.setPrivateKey;
ECDH.prototype.setPublicKey = DiffieHellman.prototype.setPublicKey;
ECDH.prototype.getPrivateKey = DiffieHellman.prototype.getPrivateKey;

ECDH.prototype.generateKeys = function generateKeys(encoding, format) {
  if (!this._handle.generateKeys())
    throw new errors.Error('ERR_CRYPTO_ECDH_GENERATE_KEYS_FAILED');

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
