'use strict';

const binding = process.binding('crypto');
const Buffer = require('buffer').Buffer;
const constants = process.binding('constants').crypto;
const cryptoUtil = require('internal/crypto/util');
const internalUtil = require('internal/util');
const util = require('util');

const inspect = internalUtil.customInspectSymbol;
const toBuf = cryptoUtil.toBuf;
const kAlgorithm = cryptoUtil.kAlgorithm;

const kGenerator = 2;

class DiffieHellmanBase {
  constructor(handle) {
    this._handle = handle;
    Object.defineProperty(this, 'verifyError', {
      enumerable: true,
      value: this._handle.verifyError,
      writable: false
    });
  }

  generateKeys(encoding) {
    var keys = this._handle.generateKeys();
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      keys = keys.toString(encoding);
    return keys;
  }

  computeSecret(key, inEnc, outEnc) {
    inEnc = inEnc || cryptoUtil.DEFAULT_ENCODING;
    outEnc = outEnc || cryptoUtil.DEFAULT_ENCODING;
    var ret = this._handle.computeSecret(toBuf(key, inEnc));
    if (outEnc && outEnc !== 'buffer')
      ret = ret.toString(outEnc);
    return ret;
  }

  getPrime(encoding) {
    var prime = this._handle.getPrime();
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      prime = prime.toString(encoding);
    return prime;
  }

  getGenerator(encoding) {
    var generator = this._handle.getGenerator();
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      generator = generator.toString(encoding);
    return generator;
  }

  getPublicKey(encoding) {
    var key = this._handle.getPublicKey();
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      key = key.toString(encoding);
    return key;
  }

  getPrivateKey(encoding) {
    var key = this._handle.getPrivateKey();
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      key = key.toString(encoding);
    return key;
  }
}

class DiffieHellman extends DiffieHellmanBase {
  constructor(sizeOrKey, keyEncoding, generator, genEncoding) {
    if (!(sizeOrKey instanceof Buffer) &&
        typeof sizeOrKey !== 'number' &&
        typeof sizeOrKey !== 'string')
      throw new TypeError('First argument should be number, string or Buffer');

    if (keyEncoding) {
      if (typeof keyEncoding !== 'string' ||
          (!Buffer.isEncoding(keyEncoding) && keyEncoding !== 'buffer')) {
        genEncoding = generator;
        generator = keyEncoding;
        keyEncoding = false;
      }
    }

    keyEncoding = keyEncoding || cryptoUtil.DEFAULT_ENCODING;
    genEncoding = genEncoding || cryptoUtil.DEFAULT_ENCODING;

    if (typeof sizeOrKey !== 'number')
      sizeOrKey = toBuf(sizeOrKey, keyEncoding);

    if (!generator)
      generator = kGenerator;
    else if (typeof generator !== 'number')
      generator = toBuf(generator, genEncoding);

    super(new binding.DiffieHellman(sizeOrKey, generator));
  }

  setPublicKey(key, encoding) {
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    this._handle.setPublicKey(toBuf(key, encoding));
    return this;
  }

  setPrivateKey(key, encoding) {
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    this._handle.setPrivateKey(toBuf(key, encoding));
    return this;
  }

  [inspect](depth, options) {
    var obj = {
      generator: this.getGenerator('hex'),
      prime: this.getPrime('hex')
    };
    if (this._options)
      obj.options = this._options;
    return `DiffieHellman ${util.inspect(obj, options)}`;
  }
}

class DiffieHellmanGroup extends DiffieHellmanBase {
  constructor(name) {
    super(new binding.DiffieHellmanGroup(name));
    this[kAlgorithm] = name;
  }

  [inspect](depth, options) {
    var obj = {
      name: this[kAlgorithm],
      generator: this.getGenerator('hex'),
      prime: this.getPrime('hex')
    };
    if (this._options)
      obj.options = this._options;
    return `DiffieHellmanGroup ${util.inspect(obj, options)}`;
  }
}

class ECDH {
  constructor(curve) {
    this._handle = new binding.ECDH(curve);
    this[kAlgorithm] = curve;
  }

  generateKeys(encoding, format) {
    this._handle.generateKeys();
    return this.getPublicKey(encoding, format);
  }

  getPublicKey(encoding, format) {
    var f = constants.POINT_CONVERSION_UNCOMPRESSED;
    if (format !== undefined) {
      switch (format) {
        case 'compressed':
          f = constants.POINT_CONVERSION_COMPRESSED;
          break;
        case 'hybrid':
          f = constants.POINT_CONVERSION_HYBRID;
          break;
        case 'uncompressed':
          f = constants.POINT_CONVERSION_UNCOMPRESSED;
          break;
        default:
          throw new TypeError(`Bad format: ${format}`);
      }
    }
    var key = this._handle.getPublicKey(f);
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      key = key.toString(encoding);
    return key;
  }

  [inspect](depth, options) {
    var obj = {
      curve: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `ECDH ${util.inspect(obj, options)}`;
  }
}
ECDH.prototype.computeSecret = DiffieHellman.prototype.computeSecret;
ECDH.prototype.setPrivateKey = DiffieHellman.prototype.setPrivateKey;
ECDH.prototype.setPublicKey = DiffieHellman.prototype.setPublicKey;
ECDH.prototype.getPrivateKey = DiffieHellman.prototype.getPrivateKey;

function createECDH(curve) {
  return new ECDH(curve);
}

module.exports = {
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH,
  createDiffieHellman: internalUtil.createClassWrapper(DiffieHellman),
  createDiffieHellmanGroup: internalUtil.createClassWrapper(DiffieHellmanGroup),
  createECDH
};
