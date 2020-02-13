'use strict';

const {
  ObjectSetPrototypeOf,
} = primordials;

const {
  RSA_PKCS1_OAEP_PADDING,
  RSA_PKCS1_PADDING
} = internalBinding('constants').crypto;

const {
  ERR_CRYPTO_INVALID_STATE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;
const { validateEncoding, validateString } = require('internal/validators');

const {
  preparePrivateKey,
  preparePublicOrPrivateKey,
  prepareSecretKey
} = require('internal/crypto/keys');
const {
  getDefaultEncoding,
  kHandle,
  getArrayBufferView
} = require('internal/crypto/util');

const { isArrayBufferView } = require('internal/util/types');

const {
  CipherBase,
  privateDecrypt: _privateDecrypt,
  privateEncrypt: _privateEncrypt,
  publicDecrypt: _publicDecrypt,
  publicEncrypt: _publicEncrypt
} = internalBinding('crypto');

const assert = require('internal/assert');
const LazyTransform = require('internal/streams/lazy_transform');

const { normalizeEncoding } = require('internal/util');

// Lazy loaded for startup performance.
let StringDecoder;

function rsaFunctionFor(method, defaultPadding, keyType) {
  return (options, buffer) => {
    const { format, type, data, passphrase } =
      keyType === 'private' ?
        preparePrivateKey(options) :
        preparePublicOrPrivateKey(options);
    const padding = options.padding || defaultPadding;
    const { oaepHash, oaepLabel } = options;
    if (oaepHash !== undefined && typeof oaepHash !== 'string')
      throw new ERR_INVALID_ARG_TYPE('options.oaepHash', 'string', oaepHash);
    if (oaepLabel !== undefined && !isArrayBufferView(oaepLabel)) {
      throw new ERR_INVALID_ARG_TYPE('options.oaepLabel',
                                     ['Buffer', 'TypedArray', 'DataView'],
                                     oaepLabel);
    }
    return method(data, format, type, passphrase, buffer, padding, oaepHash,
                  oaepLabel);
  };
}

const publicEncrypt = rsaFunctionFor(_publicEncrypt, RSA_PKCS1_OAEP_PADDING,
                                     'public');
const publicDecrypt = rsaFunctionFor(_publicDecrypt, RSA_PKCS1_PADDING,
                                     'public');
const privateEncrypt = rsaFunctionFor(_privateEncrypt, RSA_PKCS1_PADDING,
                                      'private');
const privateDecrypt = rsaFunctionFor(_privateDecrypt, RSA_PKCS1_OAEP_PADDING,
                                      'private');

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

  this[kHandle] = new CipherBase(decipher);
  if (iv === undefined) {
    this[kHandle].init(cipher, credential, authTagLength);
  } else {
    this[kHandle].initiv(cipher, credential, iv, authTagLength);
  }
  this._decoder = null;

  LazyTransform.call(this, options);
}

function createCipher(cipher, password, options, decipher) {
  validateString(cipher, 'cipher');
  password = getArrayBufferView(password, 'password');

  createCipherBase.call(this, cipher, password, options, decipher);
}

function createCipherWithIV(cipher, key, options, decipher, iv) {
  validateString(cipher, 'cipher');
  key = prepareSecretKey(key);
  iv = iv === null ? null : getArrayBufferView(iv, 'iv');
  createCipherBase.call(this, cipher, key, options, decipher, iv);
}

function Cipher(cipher, password, options) {
  if (!(this instanceof Cipher))
    return new Cipher(cipher, password, options);

  createCipher.call(this, cipher, password, options, true);
}

ObjectSetPrototypeOf(Cipher.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Cipher, LazyTransform);

Cipher.prototype._transform = function _transform(chunk, encoding, callback) {
  this.push(this[kHandle].update(chunk, encoding));
  callback();
};

Cipher.prototype._flush = function _flush(callback) {
  try {
    this.push(this[kHandle].final());
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

  if (typeof data === 'string') {
    validateEncoding(data, inputEncoding);
  } else if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data', ['string', 'Buffer', 'TypedArray', 'DataView'], data);
  }

  const ret = this[kHandle].update(data, inputEncoding);

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    return this._decoder.write(ret);
  }

  return ret;
};


Cipher.prototype.final = function final(outputEncoding) {
  outputEncoding = outputEncoding || getDefaultEncoding();
  const ret = this[kHandle].final();

  if (outputEncoding && outputEncoding !== 'buffer') {
    this._decoder = getDecoder(this._decoder, outputEncoding);
    return this._decoder.end(ret);
  }

  return ret;
};


Cipher.prototype.setAutoPadding = function setAutoPadding(ap) {
  if (!this[kHandle].setAutoPadding(!!ap))
    throw new ERR_CRYPTO_INVALID_STATE('setAutoPadding');
  return this;
};

Cipher.prototype.getAuthTag = function getAuthTag() {
  const ret = this[kHandle].getAuthTag();
  if (ret === undefined)
    throw new ERR_CRYPTO_INVALID_STATE('getAuthTag');
  return ret;
};


function setAuthTag(tagbuf) {
  if (!isArrayBufferView(tagbuf)) {
    throw new ERR_INVALID_ARG_TYPE('buffer',
                                   ['Buffer', 'TypedArray', 'DataView'],
                                   tagbuf);
  }
  if (!this[kHandle].setAuthTag(tagbuf))
    throw new ERR_CRYPTO_INVALID_STATE('setAuthTag');
  return this;
}

Cipher.prototype.setAAD = function setAAD(aadbuf, options) {
  if (!isArrayBufferView(aadbuf)) {
    throw new ERR_INVALID_ARG_TYPE('buffer',
                                   ['Buffer', 'TypedArray', 'DataView'],
                                   aadbuf);
  }

  const plaintextLength = getUIntOption(options, 'plaintextLength');
  if (!this[kHandle].setAAD(aadbuf, plaintextLength))
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
  if (constructor === Cipheriv) {
    constructor.prototype.getAuthTag = Cipher.prototype.getAuthTag;
  } else {
    constructor.prototype.setAuthTag = setAuthTag;
  }
  constructor.prototype.setAAD = Cipher.prototype.setAAD;
}

ObjectSetPrototypeOf(Cipheriv.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Cipheriv, LazyTransform);
addCipherPrototypeFunctions(Cipheriv);

function Decipher(cipher, password, options) {
  if (!(this instanceof Decipher))
    return new Decipher(cipher, password, options);

  createCipher.call(this, cipher, password, options, false);
}

ObjectSetPrototypeOf(Decipher.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Decipher, LazyTransform);
addCipherPrototypeFunctions(Decipher);


function Decipheriv(cipher, key, iv, options) {
  if (!(this instanceof Decipheriv))
    return new Decipheriv(cipher, key, iv, options);

  createCipherWithIV.call(this, cipher, key, options, false, iv);
}

ObjectSetPrototypeOf(Decipheriv.prototype, LazyTransform.prototype);
ObjectSetPrototypeOf(Decipheriv, LazyTransform);
addCipherPrototypeFunctions(Decipheriv);

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
