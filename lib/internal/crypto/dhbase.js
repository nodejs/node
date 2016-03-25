'use strict';

const Buffer = require('buffer').Buffer;
const crypto_util = require('internal/crypto/util');
const toBuf = crypto_util.toBuf;
const encode = crypto_util.encode;
const util = require('util');
const constants = require('constants');

const DH_GENERATOR = 2;

module.exports = function(binding, crypto) {

  function DHBase() {}
  DHBase.DEFAULT_DH_GENERATOR = DH_GENERATOR;

  DHBase.prototype.generateKeys = function dhGenerateKeys(encoding) {
    var keys = this._handle.generateKeys();
    return encode(keys, encoding, crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.computeSecret = function dhComputeSecret(key,
                                                            inEnc,
                                                            outEnc) {
    const defaultEncoding = crypto.DEFAULT_ENCODING;
    inEnc = inEnc || defaultEncoding;
    var ret = this._handle.computeSecret(toBuf(key, inEnc));
    return encode(ret, outEnc, defaultEncoding);
  };

  DHBase.prototype.getPrime = function dhGetPrime(encoding) {
    var prime = this._handle.getPrime();
    return encode(prime, encoding, crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getGenerator = function dhGetGenerator(encoding) {
    var generator = this._handle.getGenerator();
    return encode(generator, encoding, crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getPublicKey = function dhGetPublicKey(encoding) {
    var key = this._handle.getPublicKey();
    return encode(key, encoding, crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getPrivateKey = function dhGetPrivateKey(encoding) {
    var key = this._handle.getPrivateKey();
    return encode(key, encoding, crypto.DEFAULT_ENCODING);
  };


  crypto.createDiffieHellman = crypto.DiffieHellman = DiffieHellman;

  function DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding) {
    if (!(this instanceof DiffieHellman))
      return new DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding);
    DHBase.call(this);
    if (!(sizeOrKey instanceof Buffer) &&
        typeof sizeOrKey !== 'number' &&
        typeof sizeOrKey !== 'string')
      throw new TypeError('First argument must be number, string or Buffer');

    if (keyEncoding) {
      if (typeof keyEncoding !== 'string' ||
          (!Buffer.isEncoding(keyEncoding) && keyEncoding !== 'buffer')) {
        genEncoding = generator;
        generator = keyEncoding;
        keyEncoding = false;
      }
    }

    keyEncoding = keyEncoding || crypto.DEFAULT_ENCODING;
    genEncoding = genEncoding || crypto.DEFAULT_ENCODING;

    if (typeof sizeOrKey !== 'number')
      sizeOrKey = toBuf(sizeOrKey, keyEncoding);

    if (!generator)
      generator = DHBase.DEFAULT_DH_GENERATOR;
    else if (typeof generator !== 'number')
      generator = toBuf(generator, genEncoding);

    this._handle = new binding.DiffieHellman(sizeOrKey, generator);
    Object.defineProperty(this, 'verifyError', {
      enumerable: true,
      value: this._handle.verifyError,
      writable: false
    });
  }
  util.inherits(DiffieHellman, DHBase);

  DiffieHellman.prototype.setPublicKey = function(key, encoding) {
    encoding = encoding || crypto.DEFAULT_ENCODING;
    this._handle.setPublicKey(toBuf(key, encoding));
    return this;
  };

  DiffieHellman.prototype.setPrivateKey = function(key, encoding) {
    encoding = encoding || crypto.DEFAULT_ENCODING;
    this._handle.setPrivateKey(toBuf(key, encoding));
    return this;
  };


  crypto.DiffieHellmanGroup =
      crypto.createDiffieHellmanGroup =
      crypto.getDiffieHellman = DiffieHellmanGroup;

  function DiffieHellmanGroup(name) {
    if (!(this instanceof DiffieHellmanGroup))
      return new DiffieHellmanGroup(name);
    DHBase.call(this);
    this._handle = new binding.DiffieHellmanGroup(name);
    Object.defineProperty(this, 'verifyError', {
      enumerable: true,
      value: this._handle.verifyError,
      writable: false
    });
  }
  util.inherits(DiffieHellmanGroup, DHBase);


  crypto.createECDH = ECDH;

  function ECDH(curve) {
    if (!(this instanceof ECDH))
      return new ECDH(curve);
    if (typeof curve !== 'string')
      throw new TypeError('"curve" argument must be a string');
    DHBase.call(this);
    this._handle = new binding.ECDH(curve);
  }
  util.inherits(ECDH, DHBase);

  ECDH.prototype.setPrivateKey = DiffieHellman.prototype.setPrivateKey;
  ECDH.prototype.setPublicKey = DiffieHellman.prototype.setPublicKey;

  ECDH.prototype.generateKeys = function generateKeys(encoding, format) {
    this._handle.generateKeys();

    return this.getPublicKey(encoding, format);
  };

  ECDH.prototype.getPublicKey = function getPublicKey(encoding, format) {
    var f;
    if (format) {
      if (typeof format === 'number')
        f = format;
      if (format === 'compressed')
        f = constants.POINT_CONVERSION_COMPRESSED;
      else if (format === 'hybrid')
        f = constants.POINT_CONVERSION_HYBRID;
      // Default
      else if (format === 'uncompressed')
        f = constants.POINT_CONVERSION_UNCOMPRESSED;
      else
        throw new TypeError('Bad format: ' + format);
    } else {
      f = constants.POINT_CONVERSION_UNCOMPRESSED;
    }
    const key = this._handle.getPublicKey(f);
    return encode(key, encoding, crypto.DEFAULT_ENCODING);
  };
};
