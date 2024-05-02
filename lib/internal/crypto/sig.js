'use strict';

const {
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  ReflectApply,
} = primordials;

const {
  codes: {
    ERR_CRYPTO_SIGN_KEY_REQUIRED,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');

const {
  validateFunction,
  validateEncoding,
  validateString,
} = require('internal/validators');

const {
  Sign: _Sign,
  SignJob,
  Verify: _Verify,
  kCryptoJobAsync,
  kCryptoJobSync,
  kSigEncDER,
  kSigEncP1363,
  kSignJobModeSign,
  kSignJobModeVerify,
} = internalBinding('crypto');

const {
  getArrayBufferOrView,
  kHandle,
} = require('internal/crypto/util');

const {
  preparePrivateKey,
  preparePublicOrPrivateKey,
} = require('internal/crypto/keys');

const { Writable } = require('stream');

const { Buffer } = require('buffer');

const {
  isArrayBufferView,
} = require('internal/util/types');

function Sign(algorithm, options) {
  if (!(this instanceof Sign))
    return new Sign(algorithm, options);
  validateString(algorithm, 'algorithm');
  this[kHandle] = new _Sign();
  this[kHandle].init(algorithm);

  ReflectApply(Writable, this, [options]);
}

ObjectSetPrototypeOf(Sign.prototype, Writable.prototype);
ObjectSetPrototypeOf(Sign, Writable);

Sign.prototype._write = function _write(chunk, encoding, callback) {
  this.update(chunk, encoding);
  callback();
};

Sign.prototype.update = function update(data, encoding) {
  if (typeof data === 'string') {
    validateEncoding(data, encoding);
  } else if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data', ['string', 'Buffer', 'TypedArray', 'DataView'], data);
  }

  this[kHandle].update(data, encoding);
  return this;
};

function getPadding(options) {
  return getIntOption('padding', options);
}

function getSaltLength(options) {
  return getIntOption('saltLength', options);
}

function getDSASignatureEncoding(options) {
  if (typeof options === 'object') {
    const { dsaEncoding = 'der' } = options;
    if (dsaEncoding === 'der')
      return kSigEncDER;
    else if (dsaEncoding === 'ieee-p1363')
      return kSigEncP1363;
    throw new ERR_INVALID_ARG_VALUE('options.dsaEncoding', dsaEncoding);
  }

  return kSigEncDER;
}

function getIntOption(name, options) {
  const value = options[name];
  if (value !== undefined) {
    if (value === value >> 0) {
      return value;
    }
    throw new ERR_INVALID_ARG_VALUE(`options.${name}`, value);
  }
  return undefined;
}

Sign.prototype.sign = function sign(options, encoding) {
  if (!options)
    throw new ERR_CRYPTO_SIGN_KEY_REQUIRED();

  const { data, format, type, passphrase } = preparePrivateKey(options, true);

  // Options specific to RSA
  const rsaPadding = getPadding(options);
  const pssSaltLength = getSaltLength(options);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(options);

  const ret = this[kHandle].sign(data, format, type, passphrase, rsaPadding,
                                 pssSaltLength, dsaSigEnc);

  if (encoding && encoding !== 'buffer')
    return ret.toString(encoding);

  return ret;
};

function signOneShot(algorithm, data, key, callback) {
  if (algorithm != null)
    validateString(algorithm, 'algorithm');

  if (callback !== undefined)
    validateFunction(callback, 'callback');

  data = getArrayBufferOrView(data, 'data');

  if (!key)
    throw new ERR_CRYPTO_SIGN_KEY_REQUIRED();

  // Options specific to RSA
  const rsaPadding = getPadding(key);
  const pssSaltLength = getSaltLength(key);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(key);

  const {
    data: keyData,
    format: keyFormat,
    type: keyType,
    passphrase: keyPassphrase,
  } = preparePrivateKey(key);

  const job = new SignJob(
    callback ? kCryptoJobAsync : kCryptoJobSync,
    kSignJobModeSign,
    keyData,
    keyFormat,
    keyType,
    keyPassphrase,
    data,
    algorithm,
    pssSaltLength,
    rsaPadding,
    dsaSigEnc);

  if (!callback) {
    const { 0: err, 1: signature } = job.run();
    if (err !== undefined)
      throw err;

    return Buffer.from(signature);
  }

  job.ondone = (error, signature) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, Buffer.from(signature));
  };
  job.run();
}

function Verify(algorithm, options) {
  if (!(this instanceof Verify))
    return new Verify(algorithm, options);
  validateString(algorithm, 'algorithm');
  this[kHandle] = new _Verify();
  this[kHandle].init(algorithm);

  ReflectApply(Writable, this, [options]);
}

ObjectSetPrototypeOf(Verify.prototype, Writable.prototype);
ObjectSetPrototypeOf(Verify, Writable);

Verify.prototype._write = Sign.prototype._write;
Verify.prototype.update = Sign.prototype.update;

Verify.prototype.verify = function verify(options, signature, sigEncoding) {
  const {
    data,
    format,
    type,
    passphrase,
  } = preparePublicOrPrivateKey(options, true);

  // Options specific to RSA
  const rsaPadding = getPadding(options);
  const pssSaltLength = getSaltLength(options);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(options);

  signature = getArrayBufferOrView(signature, 'signature', sigEncoding);

  return this[kHandle].verify(data, format, type, passphrase, signature,
                              rsaPadding, pssSaltLength, dsaSigEnc);
};

function verifyOneShot(algorithm, data, key, signature, callback) {
  if (algorithm != null)
    validateString(algorithm, 'algorithm');

  if (callback !== undefined)
    validateFunction(callback, 'callback');

  data = getArrayBufferOrView(data, 'data');

  if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data',
      ['Buffer', 'TypedArray', 'DataView'],
      data,
    );
  }

  // Options specific to RSA
  const rsaPadding = getPadding(key);
  const pssSaltLength = getSaltLength(key);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(key);

  if (!isArrayBufferView(signature)) {
    throw new ERR_INVALID_ARG_TYPE(
      'signature',
      ['Buffer', 'TypedArray', 'DataView'],
      signature,
    );
  }

  const {
    data: keyData,
    format: keyFormat,
    type: keyType,
    passphrase: keyPassphrase,
  } = preparePublicOrPrivateKey(key);

  const job = new SignJob(
    callback ? kCryptoJobAsync : kCryptoJobSync,
    kSignJobModeVerify,
    keyData,
    keyFormat,
    keyType,
    keyPassphrase,
    data,
    algorithm,
    pssSaltLength,
    rsaPadding,
    dsaSigEnc,
    signature);

  if (!callback) {
    const { 0: err, 1: result } = job.run();
    if (err !== undefined)
      throw err;

    return result;
  }

  job.ondone = (error, result) => {
    if (error) return FunctionPrototypeCall(callback, job, error);
    FunctionPrototypeCall(callback, job, null, result);
  };
  job.run();
}

module.exports = {
  Sign,
  signOneShot,
  Verify,
  verifyOneShot,
};
