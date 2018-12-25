'use strict';

const {
  ERR_CRYPTO_SIGN_KEY_REQUIRED,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const { Sign: _Sign, Verify: _Verify } = internalBinding('crypto');
const {
  RSA_PSS_SALTLEN_AUTO,
  RSA_PKCS1_PADDING
} = internalBinding('constants').crypto;
const {
  getDefaultEncoding,
  kHandle,
  legacyNativeHandle,
  toBuf,
  validateArrayBufferView,
} = require('internal/crypto/util');
const {
  preparePrivateKey,
  preparePublicOrPrivateKey
} = require('internal/crypto/keys');
const { Writable } = require('stream');

function Sign(algorithm, options) {
  if (!(this instanceof Sign))
    return new Sign(algorithm, options);
  validateString(algorithm, 'algorithm');
  this[kHandle] = new _Sign();
  this[kHandle].init(algorithm);

  Writable.call(this, options);
}

Object.setPrototypeOf(Sign.prototype, Writable.prototype);
Object.setPrototypeOf(Sign, Writable);

Sign.prototype._write = function _write(chunk, encoding, callback) {
  this.update(chunk, encoding);
  callback();
};

Sign.prototype.update = function update(data, encoding) {
  encoding = encoding || getDefaultEncoding();
  data = validateArrayBufferView(toBuf(data, encoding),
                                 'data');
  this[kHandle].update(data);
  return this;
};

legacyNativeHandle(Sign);

function getPadding(options) {
  return getIntOption('padding', RSA_PKCS1_PADDING, options);
}

function getSaltLength(options) {
  return getIntOption('saltLength', RSA_PSS_SALTLEN_AUTO, options);
}

function getIntOption(name, defaultValue, options) {
  if (options.hasOwnProperty(name)) {
    const value = options[name];
    if (value === value >> 0) {
      return value;
    } else {
      throw new ERR_INVALID_OPT_VALUE(name, value);
    }
  }
  return defaultValue;
}

Sign.prototype.sign = function sign(options, encoding) {
  if (!options)
    throw new ERR_CRYPTO_SIGN_KEY_REQUIRED();

  const { data, format, type, passphrase } = preparePrivateKey(options, true);

  // Options specific to RSA
  const rsaPadding = getPadding(options);
  const pssSaltLength = getSaltLength(options);

  const ret = this[kHandle].sign(data, format, type, passphrase, rsaPadding,
                                 pssSaltLength);

  encoding = encoding || getDefaultEncoding();
  if (encoding && encoding !== 'buffer')
    return ret.toString(encoding);

  return ret;
};


function Verify(algorithm, options) {
  if (!(this instanceof Verify))
    return new Verify(algorithm, options);
  validateString(algorithm, 'algorithm');
  this[kHandle] = new _Verify();
  this[kHandle].init(algorithm);

  Writable.call(this, options);
}

Object.setPrototypeOf(Verify.prototype, Writable.prototype);
Object.setPrototypeOf(Verify, Writable);

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
  var rsaPadding = getPadding(options);

  var pssSaltLength = getSaltLength(options);

  signature = validateArrayBufferView(toBuf(signature, sigEncoding),
                                      'signature');

  return this[kHandle].verify(data, format, type, passphrase, signature,
                              rsaPadding, pssSaltLength);
};

legacyNativeHandle(Verify);

module.exports = {
  Sign,
  Verify
};
