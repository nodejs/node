'use strict';

const assert = require('assert');
const binding = process.binding('crypto');
const cryptoUtil = require('internal/crypto/util');
const internalUtil = require('internal/util');
const LazyTransform = require('internal/streams/lazy_transform');
const StringDecoder = require('string_decoder').StringDecoder;
const util = require('util');

const inspect = internalUtil.customInspectSymbol;
const toBuf = cryptoUtil.toBuf;
const kAlgorithm = cryptoUtil.kAlgorithm;

function getDecoder(decoder, encoding) {
  encoding = internalUtil.normalizeEncoding(encoding);
  decoder = decoder || new StringDecoder(encoding);
  assert(decoder.encoding === encoding, 'Cannot change encoding');
  return decoder;
}

class CipherBase extends LazyTransform {
  constructor(handle, algorithm, decoder, options) {
    super(options);
    this._handle = handle;
    this._decoder = decoder;
    this[kAlgorithm] = algorithm;
  }

  _transform(chunk, encoding, callback) {
    this.push(this._handle.update(chunk, encoding));
    callback();
  }

  _flush(callback) {
    try {
      this.push(this._handle.final());
    } catch (e) {
      callback(e);
      return;
    }
    callback();
  }

  update(data, inputEncoding, outputEncoding) {
    inputEncoding = inputEncoding || cryptoUtil.DEFAULT_ENCODING;
    outputEncoding = outputEncoding || cryptoUtil.DEFAULT_ENCODING;

    var ret = this._handle.update(data, inputEncoding);

    if (outputEncoding && outputEncoding !== 'buffer') {
      this._decoder = getDecoder(this._decoder, outputEncoding);
      ret = this._decoder.write(ret);
    }

    return ret;
  }

  final(outputEncoding) {
    outputEncoding = outputEncoding || cryptoUtil.DEFAULT_ENCODING;
    var ret = this._handle.final();

    if (outputEncoding && outputEncoding !== 'buffer') {
      this._decoder = getDecoder(this._decoder, outputEncoding);
      ret = this._decoder.end(ret);
    }

    return ret;
  }

  setAutoPadding(ap) {
    this._handle.setAutoPadding(ap);
    return this;
  }

  getAuthTag() {
    return this._handle.getAuthTag();
  }

  setAuthTag(tagbuf) {
    this._handle.setAuthTag(tagbuf);
    return this;
  }

  setAAD(aadbuf) {
    this._handle.setAAD(aadbuf);
    return this;
  }
}

class Cipher extends CipherBase {
  constructor(cipher, password, options) {
    const handle = new binding.CipherBase(true);
    handle.init(cipher, toBuf(password));
    super(handle, cipher, null, options);
  }

  [inspect](depth, options) {
    var obj = {
      cipher: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `Cipher ${util.inspect(obj, options)}`;
  }
}

class Cipheriv extends CipherBase {
  constructor(cipher, key, iv, options) {
    const handle = new binding.CipherBase(true);
    handle.initiv(cipher, toBuf(key), toBuf(iv));
    super(handle, cipher, null, options);
  }

  [inspect](depth, options) {
    var obj = {
      cipher: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `Cipheriv ${util.inspect(obj, options)}`;
  }
}

class Decipher extends CipherBase {
  constructor(cipher, password, options) {
    const handle = new binding.CipherBase(false);
    handle.init(cipher, toBuf(password));
    super(handle, cipher, null, options);
  }

  [inspect](depth, options) {
    var obj = {
      cipher: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `Decipher ${util.inspect(obj, options)}`;
  }
}
Decipher.prototype.finaltol = Decipher.prototype.final;

class Decipheriv extends CipherBase {
  constructor(cipher, key, iv, options) {
    const handle = new binding.CipherBase(false);
    handle.initiv(cipher, toBuf(key), toBuf(iv));
    super(handle, cipher, null, options);
  }

  [inspect](depth, options) {
    var obj = {
      cipher: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `Decipheriv ${util.inspect(obj, options)}`;
  }
}
Decipheriv.prototype.finaltol = Decipheriv.prototype.final;

module.exports = {
  Cipher,
  Decipher,
  Cipheriv,
  Decipheriv,
  createCipher: internalUtil.createClassWrapper(Cipher),
  createCipheriv: internalUtil.createClassWrapper(Cipheriv),
  createDecipher: internalUtil.createClassWrapper(Decipher),
  createDecipheriv: internalUtil.createClassWrapper(Decipheriv)
};
