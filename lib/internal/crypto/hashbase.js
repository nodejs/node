'use strict';

const LazyTransform = require('internal/streams/lazy_transform');
const util = require('util');
const toBuf = require('internal/crypto/util').toBuf;

module.exports = function(binding, crypto) {

  function HashBase(handle, options) {
    this._handle = handle;
    LazyTransform.call(this, options);
  }
  util.inherits(HashBase, LazyTransform);

  HashBase.prototype._transform = function(chunk, encoding, callback) {
    this._handle.update(chunk, encoding);
    callback();
  };

  HashBase.prototype._flush = function(callback) {
    this.push(this._handle.digest());
    callback();
  };

  HashBase.prototype.update = function(data, encoding) {
    this._handle.update(data, encoding || crypto.DEFAULT_ENCODING);
    return this;
  };

  HashBase.prototype.digest = function(outputEncoding) {
    return this._handle.digest(outputEncoding || crypto.DEFAULT_ENCODING);
  };

  crypto.createHash = crypto.Hash = Hash;
  function Hash(algorithm, options) {
    if (!(this instanceof Hash))
      return new Hash(algorithm, options);
    const _handle = new binding.Hash(algorithm);
    HashBase.call(this, _handle, options);
  }
  util.inherits(Hash, HashBase);


  crypto.createHmac = crypto.Hmac = Hmac;
  function Hmac(hmac, key, options) {
    if (!(this instanceof Hmac))
      return new Hmac(hmac, key, options);
    const _handle = new binding.Hmac();
    _handle.init(hmac, toBuf(key));
    HashBase.call(this, _handle, options);
  }
  util.inherits(Hmac, HashBase);
};
