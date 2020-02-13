'use strict';

const {
  ObjectSetPrototypeOf,
} = primordials;

const {
  ERR_CRYPTO_SIGN_KEY_REQUIRED,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;
const { validateEncoding, validateString } = require('internal/validators');
const {
  Sign: _Sign,
  Verify: _Verify,
  kSigEncDER,
  kSigEncP1363,
  signOneShot: _signOneShot,
  verifyOneShot: _verifyOneShot
} = internalBinding('crypto');
const {
  getDefaultEncoding,
  kHandle,
  getArrayBufferView,
} = require('internal/crypto/util');
const {
  preparePrivateKey,
  preparePublicOrPrivateKey
} = require('internal/crypto/keys');
const { Writable } = require('stream');
const { isArrayBufferView } = require('internal/util/types');

function Sign(algorithm, options) {
  if (!(this instanceof Sign))
    return new Sign(algorithm, options);
  validateString(algorithm, 'algorithm');
  this[kHandle] = new _Sign();
  this[kHandle].init(algorithm);

  Writable.call(this, options);
}

ObjectSetPrototypeOf(Sign.prototype, Writable.prototype);
ObjectSetPrototypeOf(Sign, Writable);

Sign.prototype._write = function _write(chunk, encoding, callback) {
  this.update(chunk, encoding);
  callback();
};

Sign.prototype.update = function update(data, encoding) {
  encoding = encoding || getDefaultEncoding();

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
    else
      throw new ERR_INVALID_OPT_VALUE('dsaEncoding', dsaEncoding);
  }

  return kSigEncDER;
}

function getIntOption(name, options) {
  const value = options[name];
  if (value !== undefined) {
    if (value === value >> 0) {
      return value;
    } else {
      throw new ERR_INVALID_OPT_VALUE(name, value);
    }
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

  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    return ret.toString(encoding);

  return ret;
};

function signOneShot(algorithm, data, key) {
  if (algorithm != null)
    validateString(algorithm, 'algorithm');

  if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data',
      ['Buffer', 'TypedArray', 'DataView'],
      data
    );
  }

  if (!key)
    throw new ERR_CRYPTO_SIGN_KEY_REQUIRED();

  const {
    data: keyData,
    format: keyFormat,
    type: keyType,
    passphrase: keyPassphrase
  } = preparePrivateKey(key);

  // Options specific to RSA
  const rsaPadding = getPadding(key);
  const pssSaltLength = getSaltLength(key);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(key);

  return _signOneShot(keyData, keyFormat, keyType, keyPassphrase, data,
                      algorithm, rsaPadding, pssSaltLength, dsaSigEnc);
}

function Verify(algorithm, options) {
  if (!(this instanceof Verify))
    return new Verify(algorithm, options);
  validateString(algorithm, 'algorithm');
  this[kHandle] = new _Verify();
  this[kHandle].init(algorithm);

  Writable.call(this, options);
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
    passphrase
  } = preparePublicOrPrivateKey(options, true);

  sigEncoding = sigEncoding || getDefaultEncoding();

  // Options specific to RSA
  const rsaPadding = getPadding(options);
  const pssSaltLength = getSaltLength(options);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(options);

  signature = getArrayBufferView(signature, 'signature', sigEncoding);

  return this[kHandle].verify(data, format, type, passphrase, signature,
                              rsaPadding, pssSaltLength, dsaSigEnc);
};

function verifyOneShot(algorithm, data, key, signature) {
  if (algorithm != null)
    validateString(algorithm, 'algorithm');

  if (!isArrayBufferView(data)) {
    throw new ERR_INVALID_ARG_TYPE(
      'data',
      ['Buffer', 'TypedArray', 'DataView'],
      data
    );
  }

  const {
    data: keyData,
    format: keyFormat,
    type: keyType,
    passphrase: keyPassphrase
  } = preparePublicOrPrivateKey(key);

  // Options specific to RSA
  const rsaPadding = getPadding(key);
  const pssSaltLength = getSaltLength(key);

  // Options specific to (EC)DSA
  const dsaSigEnc = getDSASignatureEncoding(key);

  if (!isArrayBufferView(signature)) {
    throw new ERR_INVALID_ARG_TYPE(
      'signature',
      ['Buffer', 'TypedArray', 'DataView'],
      signature
    );
  }

  return _verifyOneShot(keyData, keyFormat, keyType, keyPassphrase, signature,
                        data, algorithm, rsaPadding, pssSaltLength, dsaSigEnc);
}

module.exports = {
  Sign,
  signOneShot,
  Verify,
  verifyOneShot
};
