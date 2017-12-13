'use strict';

const errors = require('internal/errors');
const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');
const { isArrayBufferView } = require('internal/util/types');
const {
  PBKDF2
} = process.binding('crypto');
const {
  INT_MAX
} = process.binding('constants').crypto;

function pbkdf2(password, salt, iterations, keylen, digest, callback) {
  if (typeof digest === 'function') {
    callback = digest;
    digest = undefined;
  }

  if (typeof callback !== 'function')
    throw new errors.TypeError('ERR_INVALID_CALLBACK');

  return _pbkdf2(password, salt, iterations, keylen, digest, callback);
}

function pbkdf2Sync(password, salt, iterations, keylen, digest) {
  return _pbkdf2(password, salt, iterations, keylen, digest);
}

function _pbkdf2(password, salt, iterations, keylen, digest, callback) {

  if (digest !== null && typeof digest !== 'string')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'digest',
                               ['string', 'null']);

  password = toBuf(password);
  salt = toBuf(salt);

  if (!isArrayBufferView(password)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'password',
                               ['string', 'Buffer', 'TypedArray']);
  }

  if (!isArrayBufferView(salt)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'salt',
                               ['string', 'Buffer', 'TypedArray']);
  }

  if (typeof iterations !== 'number')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'iterations', 'number');

  if (iterations < 0)
    throw new errors.RangeError('ERR_OUT_OF_RANGE',
                                'iterations',
                                'a non-negative number',
                                iterations);

  if (typeof keylen !== 'number')
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'keylen', 'number');

  if (keylen < 0 ||
      !Number.isFinite(keylen) ||
      keylen > INT_MAX) {
    throw new errors.RangeError('ERR_OUT_OF_RANGE', 'keylen');
  }

  const encoding = getDefaultEncoding();

  if (encoding === 'buffer') {
    const ret = PBKDF2(password, salt, iterations, keylen, digest, callback);
    if (ret === -1)
      throw new errors.TypeError('ERR_CRYPTO_INVALID_DIGEST', digest);
    return ret;
  }

  // at this point, we need to handle encodings.
  if (callback) {
    function next(er, ret) {
      if (ret)
        ret = ret.toString(encoding);
      callback(er, ret);
    }
    if (PBKDF2(password, salt, iterations, keylen, digest, next) === -1)
      throw new errors.TypeError('ERR_CRYPTO_INVALID_DIGEST', digest);
  } else {
    const ret = PBKDF2(password, salt, iterations, keylen, digest);
    if (ret === -1)
      throw new errors.TypeError('ERR_CRYPTO_INVALID_DIGEST', digest);
    return ret.toString(encoding);
  }
}

module.exports = {
  pbkdf2,
  pbkdf2Sync
};
