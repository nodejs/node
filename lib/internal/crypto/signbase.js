'use strict';

const stream = require('stream');
const util = require('util');
const crypto_util = require('internal/crypto/util');
const toBuf = crypto_util.toBuf;
const encode = crypto_util.encode;

module.exports = function(binding, crypto) {

  function SignBase(handle, options) {
    this._handle = handle;
    stream.Writable.call(this, options);
  }
  util.inherits(SignBase, stream.Writable);

  SignBase.prototype._write = function(chunk, encoding, callback) {
    this._handle.update(chunk, encoding);
    callback();
  };

  SignBase.prototype.update = function(data, encoding) {
    this._handle.update(data, encoding || crypto.DEFAULT_ENCODING);
    return this;
  };

  crypto.createSign = crypto.Sign = Sign;
  function Sign(algorithm, options) {
    if (!(this instanceof Sign))
      return new Sign(algorithm, options);
    const _handle = new binding.Sign();
    _handle.init(algorithm);
    SignBase.call(this, _handle, options);
  }
  util.inherits(Sign, SignBase);

  Sign.prototype.sign = function(options, encoding) {
    if (!options)
      throw new Error('Not enough information provided to sign');
    return encode(this._handle.sign(toBuf(options.key || options),
                                    null,
                                    options.passphrase || null),
                  encoding,
                  exports.DEFAULT_ENCODING);
  };

  crypto.createVerify = crypto.Verify = Verify;
  function Verify(algorithm, options) {
    if (!(this instanceof Verify))
      return new Verify(algorithm, options);
    const _handle = new binding.Verify();
    _handle.init(algorithm);
    SignBase.call(this, _handle, options);
  }
  util.inherits(Verify, SignBase);

  Verify.prototype.verify = function(object, signature, sigEncoding) {
    sigEncoding = sigEncoding || exports.DEFAULT_ENCODING;
    return this._handle.verify(toBuf(object), toBuf(signature, sigEncoding));
  };
};
