'use strict';

const {
  Hash: _Hash,
  Hmac: _Hmac
} = process.binding('crypto');

const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');

const { Buffer } = require('buffer');

const {
  ERR_CRYPTO_HASH_DIGEST_NO_UTF16,
  ERR_CRYPTO_HASH_FINALIZED,
  ERR_CRYPTO_HASH_UPDATE_FAILED,
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;
const { inherits } = require('util');
const { normalizeEncoding } = require('internal/util');
const { isArrayBufferView } = require('internal/util/types');
const LazyTransform = require('internal/streams/lazy_transform');
const kState = Symbol('state');
const kFinalized = Symbol('finalized');

function Hash(algorithm, options) {
  if (!(this instanceof Hash))
    return new Hash(algorithm, options);
  if (typeof algorithm !== 'string')
    throw new ERR_INVALID_ARG_TYPE('algorithm', 'string', algorithm);
  this._handle = new _Hash(algorithm);
  this[kState] = {
    [kFinalized]: false
  };
  LazyTransform.call(this, options);
}

inherits(Hash, LazyTransform);

Hash.prototype._transform = function _transform(chunk, encoding, callback) {
  this._handle.update(chunk, encoding);
  callback();
};

Hash.prototype._flush = function _flush(callback) {
  this.push(this._handle.digest());
  callback();
};

Hash.prototype.update = function update(data, encoding) {
  const state = this[kState];
  if (state[kFinalized])
    throw new ERR_CRYPTO_HASH_FINALIZED();

  if (typeof data !== 'string' && !isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE('data',
                                   ['string', 'TypedArray', 'DataView'], data);
  }

  if (!this._handle.update(data, encoding || getDefaultEncoding()))
    throw new ERR_CRYPTO_HASH_UPDATE_FAILED();
  return this;
};


Hash.prototype.digest = function digest(outputEncoding) {
  const state = this[kState];
  if (state[kFinalized])
    throw new ERR_CRYPTO_HASH_FINALIZED();
  outputEncoding = outputEncoding || getDefaultEncoding();
  if (normalizeEncoding(outputEncoding) === 'utf16le')
    throw new ERR_CRYPTO_HASH_DIGEST_NO_UTF16();

  // Explicit conversion for backward compatibility.
  const ret = this._handle.digest(`${outputEncoding}`);
  state[kFinalized] = true;
  return ret;
};


function Hmac(hmac, key, options) {
  if (!(this instanceof Hmac))
    return new Hmac(hmac, key, options);
  if (typeof hmac !== 'string')
    throw new ERR_INVALID_ARG_TYPE('hmac', 'string', hmac);
  if (typeof key !== 'string' && !isArrayBufferView(key)) {
    throw new ERR_INVALID_ARG_TYPE('key',
                                   ['string', 'TypedArray', 'DataView'], key);
  }
  this._handle = new _Hmac();
  this._handle.init(hmac, toBuf(key));
  this[kState] = {
    [kFinalized]: false
  };
  LazyTransform.call(this, options);
}

inherits(Hmac, LazyTransform);

Hmac.prototype.update = Hash.prototype.update;

Hmac.prototype.digest = function digest(outputEncoding) {
  const state = this[kState];
  outputEncoding = outputEncoding || getDefaultEncoding();
  if (normalizeEncoding(outputEncoding) === 'utf16le')
    throw new ERR_CRYPTO_HASH_DIGEST_NO_UTF16();

  if (state[kFinalized]) {
    const buf = Buffer.from('');
    return outputEncoding === 'buffer' ? buf : buf.toString(outputEncoding);
  }

  // Explicit conversion for backward compatibility.
  const ret = this._handle.digest(`${outputEncoding}`);
  state[kFinalized] = true;
  return ret;
};

Hmac.prototype._flush = Hash.prototype._flush;
Hmac.prototype._transform = Hash.prototype._transform;

module.exports = {
  Hash,
  Hmac
};
