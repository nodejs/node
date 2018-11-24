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
const { validateString } = require('internal/validators');

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

const { inherits } = require('util');
const { deprecate, normalizeEncoding } = require('internal/util');

// Lazy loaded for startup performance.
let StringDecoder;

function rsaFunctionFor(method, defaultPadding) {
  return (options, buffer) => {
    const key = options.key || options;
    const padding = options.padding || defaultPadding;
    const passphrase = options.passphrase || null;
    return method(toBuf(key), buffer, padding, passphrase);
  };
}

const publicEncrypt = rsaFunctionFor(_publicEncrypt, RSA_PKCS1_OAEP_PADDING);
const publicDecrypt = rsaFunctionFor(_publicDecrypt, RSA_PKCS1_PADDING);
const privateEncrypt = rsaFunctionFor(_privateEncrypt, RSA_PKCS1_PADDING);
const privateDecrypt = rsaFunctionFor(_privateDecrypt, RSA_PKCS1_OAEP_PADDING);

function getDecoder(decoder, encoding) {
  encoding = normalizeEncoding(encoding);
  if (StringDecoder === undefined)
    StringDecoder = require('string_decoder').StringDecoder;
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

function createCipherBase(cipher, credential, options, decipher, iv) {
  const authTagLength = getUIntOption(options, 'authTagLength');

  this._handle = new CipherBase(decipher);
  if (iv === undefined) {
    this._handle.init(cipher, credential, authTagLength);
  } else {
    this._handle.initiv(cipher, credential, iv, authTagLength);
  }
  this._decoder = null;

  LazyTransform.call(this, options);
}

function invalidArrayBufferView(name, value) {
  return new ERR_INVALID_ARG_TYPE(
    name,
    ['string', 'Buffer', 'TypedArray', 'DataView'],
    value
  );
}

function createCipher(cipher, password, options, decipher) {
  validateString(cipher, 'cipher');
  password = toBuf(password);
  if (!isArrayBufferView(password)) {
    throw invalidArrayBufferView('password', password);
  }

  createCipherBase.call(this, cipher, password, options, decipher);
}

function createCipherWithIV(cipher, key, options, decipher, iv) {
  validateString(cipher, 'cipher');
  key = toBuf(key);
  if (!isArrayBufferView(key)) {
    throw invalidArrayBufferView('key', key);
  }

  iv = toBuf(iv);
  if (iv !== null && !isArrayBufferView(iv)) {
    throw invalidArrayBufferView('iv', iv);
  }
  createCipherBase.call(this, cipher, key, options, decipher, iv);
}

function Cipher(cipher, password, options) {
  if (!(this instanceof Cipher))
    return new Cipher(cipher, password, options);

  createCipher.call(this, cipher, password, options, true);
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
    throw invalidArrayBufferView('data', data);
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
  if (!this._handle.setAutoPadding(!!ap))
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
  if (!this._handle.setAuthTag(tagbuf))
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
  if (!this._handle.setAAD(aadbuf, plaintextLength))
    throw new ERR_CRYPTO_INVALID_STATE('setAAD');
  return this;
};

function Cipheriv(cipher, key, iv, options) {
  if (!(this instanceof Cipheriv))
    return new Cipheriv(cipher, key, iv, options);

  createCipherWithIV.call(this, cipher, key, options, true, iv);
}

function addCipherPrototypeFunctions(constructor) {
  constructor.prototype._transform = Cipher.prototype._transform;
  constructor.prototype._flush = Cipher.prototype._flush;
  constructor.prototype.update = Cipher.prototype.update;
  constructor.prototype.final = Cipher.prototype.final;
  constructor.prototype.setAutoPadding = Cipher.prototype.setAutoPadding;
  constructor.prototype.getAuthTag = Cipher.prototype.getAuthTag;
  constructor.prototype.setAuthTag = Cipher.prototype.setAuthTag;
  constructor.prototype.setAAD = Cipher.prototype.setAAD;
}

inherits(Cipheriv, LazyTransform);
addCipherPrototypeFunctions(Cipheriv);

const finaltol = deprecate(Cipher.prototype.final,
                           'crypto.Decipher.finaltol is deprecated. Use ' +
                           'crypto.Decipher.final instead.', 'DEP0105');

function Decipher(cipher, password, options) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password, options);

  createCipher.call(this, cipher, password, options, false);
}

inherits(Decipher, LazyTransform);
addCipherPrototypeFunctions(Decipher);
Decipher.prototype.finaltol = finaltol;


function Decipheriv(cipher, key, iv, options) {
  if (!(this instanceof Decipheriv))
    return new Decipheriv(cipher, key, iv, options);

  createCipherWithIV.call(this, cipher, key, options, false, iv);
}

inherits(Decipheriv, LazyTransform);
addCipherPrototypeFunctions(Decipheriv);
Decipheriv.prototype.finaltol = finaltol;

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
