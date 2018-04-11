'use strict';

const {
  RSA_PKCS1_OAEP_PADDING,
  RSA_PKCS1_PADDING
} = process.binding('constants').crypto;

const {
  ERR_CRYPTO_INVALID_STATE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;

const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');

const { isArrayBufferView } = require('internal/util/types');

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
const { deprecate, normalizeEncoding } = require('internal/util');

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

function getUIntOption(options, key) {
  let value;
  if (options && (value = options[key]) != null) {
    if (value >>> 0 !== value)
      throw new ERR_INVALID_OPT_VALUE(key, value);
    return value;
  }
  return -1;
}

function Cipher(cipher, password, options) {
  if (!(this instanceof Cipher))
    return new Cipher(cipher, password, options);

  if (typeof cipher !== 'string')
    throw new ERR_INVALID_ARG_TYPE('cipher', 'string', cipher);

  password = toBuf(password);
  if (!isArrayBufferView(password)) {
    throw new ERR_INVALID_ARG_TYPE(
      'password',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      password
    );
  }

  const authTagLength = getUIntOption(options, 'authTagLength');

  this._handle = new CipherBase(true);

  this._handle.init(cipher, password, authTagLength);
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

  if (typeof data !== 'string' && !isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      data
    );
  }

  const ret = this._handle.update(data, inputEncoding);

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    return this._decoder.write(ret);
  }

  return ret;
};


Cipher.prototype.final = function final(outputEncoding) {
  outputEncoding = outputEncoding || getDefaultEncoding();
  const ret = this._handle.final();

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    return this._decoder.end(ret);
  }

  return ret;
};


Cipher.prototype.setAutoPadding = function setAutoPadding(ap) {
  if (this._handle.setAutoPadding(ap) === false)
    throw new ERR_CRYPTO_INVALID_STATE('setAutoPadding');
  return this;
};

Cipher.prototype.getAuthTag = function getAuthTag() {
  const ret = this._handle.getAuthTag();
  if (ret === undefined)
    throw new ERR_CRYPTO_INVALID_STATE('getAuthTag');
  return ret;
};


Cipher.prototype.setAuthTag = function setAuthTag(tagbuf) {
  if (!isArrayBufferView(tagbuf)) {
    throw new ERR_INVALID_ARG_TYPE('buffer',
                                   ['Buffer', 'TypedArray', 'DataView'],
                                   tagbuf);
  }
  // Do not do a normal falsy check because the method returns
  // undefined if it succeeds. Returns false specifically if it
  // errored
  if (this._handle.setAuthTag(tagbuf) === false)
    throw new ERR_CRYPTO_INVALID_STATE('setAuthTag');
  return this;
};

Cipher.prototype.setAAD = function setAAD(aadbuf, options) {
  if (!isArrayBufferView(aadbuf)) {
    throw new ERR_INVALID_ARG_TYPE('buffer',
                                   ['Buffer', 'TypedArray', 'DataView'],
                                   aadbuf);
  }

  const plaintextLength = getUIntOption(options, 'plaintextLength');
  if (this._handle.setAAD(aadbuf, plaintextLength) === false)
    throw new ERR_CRYPTO_INVALID_STATE('setAAD');
  return this;
};

function Cipheriv(cipher, key, iv, options) {
  if (!(this instanceof Cipheriv))
    return new Cipheriv(cipher, key, iv, options);

  if (typeof cipher !== 'string')
    throw new ERR_INVALID_ARG_TYPE('cipher', 'string', cipher);

  key = toBuf(key);
  if (!isArrayBufferView(key)) {
    throw new ERR_INVALID_ARG_TYPE(
      'key',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      key
    );
  }

  iv = toBuf(iv);
  if (iv !== null && !isArrayBufferView(iv)) {
    throw new ERR_INVALID_ARG_TYPE(
      'iv',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      iv
    );
  }

  const authTagLength = getUIntOption(options, 'authTagLength');

  this._handle = new CipherBase(true);
  this._handle.initiv(cipher, key, iv, authTagLength);
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


const finaltol = deprecate(Cipher.prototype.final,
                           'crypto.Decipher.finaltol is deprecated. Use ' +
                           'crypto.Decipher.final instead.', 'DEP0105');

function Decipher(cipher, password, options) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password, options);

  if (typeof cipher !== 'string')
    throw new ERR_INVALID_ARG_TYPE('cipher', 'string', cipher);

  password = toBuf(password);
  if (!isArrayBufferView(password)) {
    throw new ERR_INVALID_ARG_TYPE(
      'password',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      password
    );
  }

  const authTagLength = getUIntOption(options, 'authTagLength');

  this._handle = new CipherBase(false);
  this._handle.init(cipher, password, authTagLength);
  this._decoder = null;

  LazyTransform.call(this, options);
}

inherits(Decipher, LazyTransform);

Decipher.prototype._transform = Cipher.prototype._transform;
Decipher.prototype._flush = Cipher.prototype._flush;
Decipher.prototype.update = Cipher.prototype.update;
Decipher.prototype.final = Cipher.prototype.final;
Decipher.prototype.finaltol = finaltol;
Decipher.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;
Decipher.prototype.getAuthTag = Cipher.prototype.getAuthTag;
Decipher.prototype.setAuthTag = Cipher.prototype.setAuthTag;
Decipher.prototype.setAAD = Cipher.prototype.setAAD;


function Decipheriv(cipher, key, iv, options) {
  if (!(this instanceof Decipheriv))
    return new Decipheriv(cipher, key, iv, options);

  if (typeof cipher !== 'string')
    throw new ERR_INVALID_ARG_TYPE('cipher', 'string', cipher);

  key = toBuf(key);
  if (!isArrayBufferView(key)) {
    throw new ERR_INVALID_ARG_TYPE(
      'key',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      key
    );
  }

  iv = toBuf(iv);
  if (iv !== null && !isArrayBufferView(iv)) {
    throw new ERR_INVALID_ARG_TYPE(
      'iv',
      ['string', 'Buffer', 'TypedArray', 'DataView'],
      iv
    );
  }

  const authTagLength = getUIntOption(options, 'authTagLength');

  this._handle = new CipherBase(false);
  this._handle.initiv(cipher, key, iv, authTagLength);
  this._decoder = null;

  LazyTransform.call(this, options);
}

inherits(Decipheriv, LazyTransform);

Decipheriv.prototype._transform = Cipher.prototype._transform;
Decipheriv.prototype._flush = Cipher.prototype._flush;
Decipheriv.prototype.update = Cipher.prototype.update;
Decipheriv.prototype.final = Cipher.prototype.final;
Decipheriv.prototype.finaltol = finaltol;
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
