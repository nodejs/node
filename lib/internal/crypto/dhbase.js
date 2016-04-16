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

  DHBase.prototype.generateKeys = function dhGenerateKeys(encoding) {
    return encode(this._handle.generateKeys(),
                  encoding,
                  crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.computeSecret = function dhComputeSecret(key,
                                                            inEnc,
                                                            outEnc) {
    inEnc = inEnc || crypto.DEFAULT_ENCODING;
    return encode(this._handle.computeSecret(toBuf(key, inEnc)),
                  outEnc,
                  crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getPrime = function dhGetPrime(encoding) {
    return encode(this._handle.getPrime(),
                  encoding,
                  crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getGenerator = function dhGetGenerator(encoding) {
    return encode(this._handle.getGenerator(),
                  encoding,
                  crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getPublicKey = function dhGetPublicKey(encoding) {
    return encode(this._handle.getPublicKey(),
                  encoding,
                  crypto.DEFAULT_ENCODING);
  };

  DHBase.prototype.getPrivateKey = function dhGetPrivateKey(encoding) {
    return encode(this._handle.getPrivateKey(),
                  encoding,
                  crypto.DEFAULT_ENCODING);
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

    if (keyEncoding &&
        (typeof keyEncoding !== 'string' ||
        (!Buffer.isEncoding(keyEncoding) && keyEncoding !== 'buffer'))) {
      genEncoding = generator;
      generator = keyEncoding;
      keyEncoding = false;
    }

    sizeOrKey = typeof sizeOrKey !== 'number' ?
        toBuf(sizeOrKey, keyEncoding || crypto.DEFAULT_ENCODING) :
        sizeOrKey;

    generator = generator ?
        typeof generator !== 'number' ?
            toBuf(generator, genEncoding || crypto.DEFAULT_ENCODING) :
            generator :
        DH_GENERATOR;

    this._handle = new binding.DiffieHellman(sizeOrKey, generator);
    Object.defineProperty(this, 'verifyError', {
      enumerable: true,
      value: this._handle.verifyError,
      writable: false
    });
  }
  util.inherits(DiffieHellman, DHBase);

  DiffieHellman.prototype.setPublicKey = function(key, encoding) {
    this._handle.setPublicKey(toBuf(key, encoding || crypto.DEFAULT_ENCODING));
    return this;
  };

  DiffieHellman.prototype.setPrivateKey = function(key, encoding) {
    this._handle.setPrivateKey(toBuf(key, encoding || crypto.DEFAULT_ENCODING));
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

  const formatsMap = {
    compressed: constants.POINT_CONVERSION_COMPRESSED,
    hybrid: constants.POINT_CONVERSION_HYBRID,
    uncompressed: constants.POINT_CONVERSION_UNCOMPRESSED
  };
  ECDH.prototype.getPublicKey = function getPublicKey(encoding, format) {
    const f = format ?
        (typeof format === 'number') ?
            format :
            formatsMap[format] :
        constants.POINT_CONVERSION_UNCOMPRESSED;
    if (!f)
      throw new TypeError(`Invalid ECDH public key format: ${format}`);
    return encode(this._handle.getPublicKey(f),
                  encoding,
                  crypto.DEFAULT_ENCODING);
  };
};
