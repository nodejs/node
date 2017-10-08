'use strict';

const errors = require('internal/errors');
const {
  getDefaultEncoding,
  toBuf
} = require('internal/crypto/util');
const {
  PBKDF2
} = process.binding('crypto');

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

  const encoding = getDefaultEncoding();

  if (encoding === 'buffer')
    return PBKDF2(password, salt, iterations, keylen, digest, callback);

  // at this point, we need to handle encodings.
  if (callback) {
    function next(er, ret) {
      if (ret)
        ret = ret.toString(encoding);
      callback(er, ret);
    }
    PBKDF2(password, salt, iterations, keylen, digest, next);
  } else {
    var ret = PBKDF2(password, salt, iterations, keylen, digest);
    return ret.toString(encoding);
  }
}

module.exports = {
  pbkdf2,
  pbkdf2Sync
};
