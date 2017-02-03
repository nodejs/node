'use strict';

const binding = process.binding('crypto');
const cryptoUtil = require('internal/crypto/util');
const internalUtil = require('internal/util');
const Writable = require('stream').Writable;
const util = require('util');

const inspect = internalUtil.customInspectSymbol;
const toBuf = cryptoUtil.toBuf;
const kAlgorithm = cryptoUtil.kAlgorithm;

class SignBase extends Writable {
  constructor(handle, options) {
    super(options);
    this._handle = handle;
  }

  _write(chunk, encoding, callback) {
    this._handle.update(chunk, encoding);
    callback();
  }

  update(data, encoding) {
    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    this._handle.update(data, encoding);
    return this;
  }
}

class Sign extends SignBase {
  constructor(algorithm, options) {
    const handle = new binding.Sign();
    handle.init(algorithm);
    super(handle, options);
    this[kAlgorithm] = algorithm;
  }

  sign(options, encoding) {
    if (!options)
      throw new Error('No key provided to sign');

    var key = options.key || options;
    var passphrase = options.passphrase || null;
    var ret = this._handle.sign(toBuf(key), null, passphrase);

    encoding = encoding || cryptoUtil.DEFAULT_ENCODING;
    if (encoding && encoding !== 'buffer')
      ret = ret.toString(encoding);

    return ret;
  }

  [inspect](depth, options) {
    var obj = {
      algorithm: this[kAlgorithm],
      _writableState: this._writableState,
      writable: this.writable,
      domain: this.domain,
      _events: this._events,
      _eventsCount: this._eventsCount,
      _maxListeners: this._maxListeners
    };
    if (this._options)
      obj.options = this._options;
    return `Sign ${util.inspect(obj, options)}`;
  }
}

class Verify extends SignBase {
  constructor(algorithm, options) {
    const handle = new binding.Verify();
    handle.init(algorithm);
    super(handle, options);
    this[kAlgorithm] = algorithm;
  }

  verify(object, signature, sigEncoding) {
    sigEncoding = sigEncoding || cryptoUtil.DEFAULT_ENCODING;
    return this._handle.verify(toBuf(object), toBuf(signature, sigEncoding));
  }

  [inspect](depth, options) {
    var obj = {
      algorithm: this[kAlgorithm],
      _writableState: this._writableState,
      writable: this.writable,
      domain: this.domain,
      _events: this._events,
      _eventsCount: this._eventsCount,
      _maxListeners: this._maxListeners
    };
    if (this._options)
      obj.options = this._options;
    return `Verify ${util.inspect(obj, options)}`;
  }
}

module.exports = {
  Sign,
  Verify,
  createSign: internalUtil.createClassWrapper(Sign),
  createVerify: internalUtil.createClassWrapper(Verify)
};
