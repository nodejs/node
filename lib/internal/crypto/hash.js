'use strict';

const binding = process.binding('crypto');
const cryptoUtil = require('internal/crypto/util');
const internalUtil = require('internal/util');
const LazyTransform = require('internal/streams/lazy_transform');
const util = require('util');

const inspect = internalUtil.customInspectSymbol;
const toBuf = cryptoUtil.toBuf;
const kAlgorithm = cryptoUtil.kAlgorithm;

class HashBase extends LazyTransform {
  constructor(handle, algorithm, options) {
    super(options);
    this._handle = handle;
    this[kAlgorithm] = algorithm;
  }

  _transform(chunk, encoding, callback) {
    this._handle.update(chunk, encoding);
    callback();
  }

  _flush(callback) {
    this.push(this._handle.digest());
    callback();
  }

  update(data, encoding) {
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    this._handle.update(data, encoding);
    return this;
  }

  digest(outputEncoding) {
    outputEncoding = outputEncoding || cryptoUtil.DEFAULT_ENCODING;
    return this._handle.digest(outputEncoding);
  }
}

class Hash extends HashBase {
  constructor(algorithm, options) {
    super(new binding.Hash(algorithm), algorithm, options);
  }

  [inspect](depth, options) {
    var obj = {
      algorithm: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `Hash ${util.inspect(obj, options)}`;
  }
}

class Hmac extends HashBase {
  constructor(hmac, key, options) {
    const handle = new binding.Hmac();
    handle.init(hmac, toBuf(key));
    super(handle, hmac, options);
  }

  [inspect](depth, options) {
    var obj = {
      hmac: this[kAlgorithm]
    };
    if (this._options)
      obj.options = this._options;
    return `Hmac ${util.inspect(obj, options)}`;
  }
}

module.exports = {
  Hash,
  Hmac,
  createHash: internalUtil.createClassWrapper(Hash),
  createHmac: internalUtil.createClassWrapper(Hmac)
};
