'use strict';

const {
  RSA_PKCS1_OAEP_PADDING,
  RSA_PKCS1_PADDING
} = process.binding('constants').crypto;

const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');

const {
  CipherBase,
  privateDecrypt: _privateDecrypt,
  privateEncrypt: _privateEncrypt,
  publicDecrypt: _publicDecrypt,
  publicEncrypt: _publicEncrypt
} = process.binding('crypto');

const assert = require('assert');
const LazyTransform = require('internal/streams/lazy_transform');
const { StringDecoder } = require('string_decoder');

const { inherits } = require('util');
const { normalizeEncoding } = require('internal/util');

function rsaPublic(method, defaultPadding) {
  return function(options, buffer) {
    const key = options.key || options;
    const padding = options.padding || defaultPadding;
    const passphrase = options.passphrase || null;
    return method(toBuf(key), buffer, padding, passphrase);
  };
}

function rsaPrivate(method, defaultPadding) {
  return function(options, buffer) {
    const key = options.key || options;
    const passphrase = options.passphrase || null;
    const padding = options.padding || defaultPadding;
    return method(toBuf(key), buffer, padding, passphrase);
  };
}

const publicEncrypt = rsaPublic(_publicEncrypt, RSA_PKCS1_OAEP_PADDING);
const publicDecrypt = rsaPublic(_publicDecrypt, RSA_PKCS1_PADDING);
const privateEncrypt = rsaPrivate(_privateEncrypt, RSA_PKCS1_PADDING);
const privateDecrypt = rsaPrivate(_privateDecrypt, RSA_PKCS1_OAEP_PADDING);

function getDecoder(decoder, encoding) {
  encoding = normalizeEncoding(encoding);
  decoder = decoder || new StringDecoder(encoding);
  assert(decoder.encoding === encoding, 'Cannot change encoding');
  return decoder;
}

function Cipher(cipher, password, options) {
  if (!(this instanceof Cipher))
    return new Cipher(cipher, password, options);
  this._handle = new CipherBase(true);

  this._handle.init(cipher, toBuf(password));
  this._decoder = null;

  LazyTransform.call(this, options);
}

inherits(Cipher, LazyTransform);

Cipher.prototype._transform = function _transform(chunk, encoding, callback) {
  this.push(this._handle.update(chunk, encoding));
  callback();
};

Cipher.prototype._flush = function _flush(callback) {
  try {
    this.push(this._handle.final());
  } catch (e) {
    callback(e);
    return;
  }
  callback();
};

Cipher.prototype.update = function update(data, inputEncoding, outputEncoding) {
  const encoding = getDefaultEncoding();
  inputEncoding = inputEncoding || encoding;
  outputEncoding = outputEncoding || encoding;

  var ret = this._handle.update(data, inputEncoding);

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.write(ret);
  }

  return ret;
};


Cipher.prototype.final = function final(outputEncoding) {
  outputEncoding = outputEncoding || getDefaultEncoding();
  var ret = this._handle.final();

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    ret = this._decoder.end(ret);
  }

  return ret;
};


Cipher.prototype.setAutoPadding = function setAutoPadding(ap) {
  this._handle.setAutoPadding(ap);
  return this;
};

Cipher.prototype.getAuthTag = function getAuthTag() {
  return this._handle.getAuthTag();
};


Cipher.prototype.setAuthTag = function setAuthTag(tagbuf) {
  this._handle.setAuthTag(tagbuf);
  return this;
};

Cipher.prototype.setAAD = function setAAD(aadbuf) {
  this._handle.setAAD(aadbuf);
  return this;
};

function Cipheriv(cipher, key, iv, options) {
  if (!(this instanceof Cipheriv))
    return new Cipheriv(cipher, key, iv, options);
  this._handle = new CipherBase(true);
  this._handle.initiv(cipher, toBuf(key), toBuf(iv));
  this._decoder = null;

  LazyTransform.call(this, options);
}

inherits(Cipheriv, LazyTransform);

Cipheriv.prototype._transform = Cipher.prototype._transform;
Cipheriv.prototype._flush = Cipher.prototype._flush;
Cipheriv.prototype.update = Cipher.prototype.update;
Cipheriv.prototype.final = Cipher.prototype.final;
Cipheriv.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;
Cipheriv.prototype.getAuthTag = Cipher.prototype.getAuthTag;
Cipheriv.prototype.setAuthTag = Cipher.prototype.setAuthTag;
Cipheriv.prototype.setAAD = Cipher.prototype.setAAD;


function Decipher(cipher, password, options) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password, options);

  this._handle = new CipherBase(false);
  this._handle.init(cipher, toBuf(password));
  this._decoder = null;

  LazyTransform.call(this, options);
}

inherits(Decipher, LazyTransform);

Decipher.prototype._transform = Cipher.prototype._transform;
Decipher.prototype._flush = Cipher.prototype._flush;
Decipher.prototype.update = Cipher.prototype.update;
Decipher.prototype.final = Cipher.prototype.final;
Decipher.prototype.finaltol = Cipher.prototype.final;
Decipher.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;
Decipher.prototype.getAuthTag = Cipher.prototype.getAuthTag;
Decipher.prototype.setAuthTag = Cipher.prototype.setAuthTag;
Decipher.prototype.setAAD = Cipher.prototype.setAAD;


function Decipheriv(cipher, key, iv, options) {
  if (!(this instanceof Decipheriv))
    return new Decipheriv(cipher, key, iv, options);

  this._handle = new CipherBase(false);
  this._handle.initiv(cipher, toBuf(key), toBuf(iv));
  this._decoder = null;

  LazyTransform.call(this, options);
}

inherits(Decipheriv, LazyTransform);

Decipheriv.prototype._transform = Cipher.prototype._transform;
Decipheriv.prototype._flush = Cipher.prototype._flush;
Decipheriv.prototype.update = Cipher.prototype.update;
Decipheriv.prototype.final = Cipher.prototype.final;
Decipheriv.prototype.finaltol = Cipher.prototype.final;
Decipheriv.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;
Decipheriv.prototype.getAuthTag = Cipher.prototype.getAuthTag;
Decipheriv.prototype.setAuthTag = Cipher.prototype.setAuthTag;
Decipheriv.prototype.setAAD = Cipher.prototype.setAAD;


module.exports = {
  Cipher,
  Cipheriv,
  Decipher,
  Decipheriv,
  privateDecrypt,
  privateEncrypt,
  publicDecrypt,
  publicEncrypt,
};
