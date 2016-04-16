'use strict';

const LazyTransform = require('internal/streams/lazy_transform');
const StringDecoder = require('string_decoder').StringDecoder;
const util = require('util');
const assert = require('assert');
const toBuf = require('internal/crypto/util').toBuf;

module.exports = function(binding, crypto) {

  function getDecoder(decoder, encoding) {
    if (encoding === 'utf-8') encoding = 'utf8';  // Normalize encoding.
    decoder = decoder || new StringDecoder(encoding);
    assert(decoder.encoding === encoding, 'Cannot change encoding');
    return decoder;
  }

  function CipherBase(handle, options) {
    this._handle = handle;
    this._decoder = null;
    LazyTransform.call(this, options);
  }
  util.inherits(CipherBase, LazyTransform);

  CipherBase.prototype._transform = function(chunk, encoding, callback) {
    this.push(this._handle.update(chunk, encoding));
    callback();
  };

  CipherBase.prototype._flush = function(callback) {
    try {
      this.push(this._handle.final());
    } catch (e) {
      callback(e);
      return;
    }
    callback();
  };

  CipherBase.prototype.update = function(data, inputEncoding, outputEncoding) {
    const defaultEncoding = crypto.DEFAULT_ENCODING;
    inputEncoding = inputEncoding || defaultEncoding;
    outputEncoding = outputEncoding || defaultEncoding;

    var ret = this._handle.update(data, inputEncoding);

    if (outputEncoding && outputEncoding !== 'buffer') {
      this._decoder = getDecoder(this._decoder, outputEncoding);
      ret = this._decoder.write(ret);
    }

    return ret;
  };


  CipherBase.prototype.final = function(outputEncoding) {
    outputEncoding = outputEncoding || crypto.DEFAULT_ENCODING;
    var ret = this._handle.final();

    if (outputEncoding && outputEncoding !== 'buffer') {
      this._decoder = getDecoder(this._decoder, outputEncoding);
      ret = this._decoder.end(ret);
    }

    return ret;
  };


  CipherBase.prototype.setAutoPadding = function(ap) {
    this._handle.setAutoPadding(ap);
    return this;
  };

  CipherBase.prototype.getAuthTag = function() {
    return this._handle.getAuthTag();
  };


  CipherBase.prototype.setAuthTag = function(tagbuf) {
    this._handle.setAuthTag(tagbuf);
  };

  CipherBase.prototype.setAAD = function(aadbuf) {
    this._handle.setAAD(aadbuf);
  };


  crypto.createCipher = crypto.Cipher = Cipher;
  function Cipher(cipher, password, options) {
    if (!(this instanceof Cipher))
      return new Cipher(cipher, password, options);
    const _handle = new binding.CipherBase(true);
    _handle.init(cipher, toBuf(password));
    CipherBase.call(this, _handle, options);
  }
  util.inherits(Cipher, CipherBase);


  crypto.createCipheriv = crypto.Cipheriv = Cipheriv;
  function Cipheriv(cipher, key, iv, options) {
    if (!(this instanceof Cipheriv))
      return new Cipheriv(cipher, key, iv, options);
    const _handle = new binding.CipherBase(true);
    _handle.initiv(cipher, toBuf(key), toBuf(iv));
    CipherBase.call(this, _handle, options);
  }
  util.inherits(Cipheriv, CipherBase);


  crypto.createDecipher = crypto.Decipher = Decipher;
  function Decipher(cipher, password, options) {
    if (!(this instanceof Decipher))
      return new Decipher(cipher, password, options);
    const _handle = new binding.CipherBase(false);
    _handle.init(cipher, toBuf(password));
    CipherBase.call(this, _handle, options);
  }
  util.inherits(Decipher, CipherBase);
  Decipher.prototype.finaltol = Decipher.prototype.final;


  crypto.createDecipheriv = crypto.Decipheriv = Decipheriv;
  function Decipheriv(cipher, key, iv, options) {
    if (!(this instanceof Decipheriv))
      return new Decipheriv(cipher, key, iv, options);
    const _handle = new binding.CipherBase(false);
    _handle.initiv(cipher, toBuf(key), toBuf(iv));
    CipherBase.call(this, _handle, options);
  }
  util.inherits(Decipheriv, CipherBase);
  Decipheriv.prototype.finaltol = Decipheriv.prototype.final;

};
