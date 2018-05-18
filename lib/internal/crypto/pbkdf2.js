'use strict';

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_CRYPTO_INVALID_DIGEST,
} = require('internal/errors').codes;
const {
  checkIsArrayBufferView,
  checkIsUint,
  getDefaultEncoding,
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
    throw new ERR_INVALID_CALLBACK();

  return _pbkdf2(password, salt, iterations, keylen, digest, callback);
}

function pbkdf2Sync(password, salt, iterations, keylen, digest) {
  return _pbkdf2(password, salt, iterations, keylen, digest);
}

function _pbkdf2(password, salt, iterations, keylen, digest, callback) {

  if (digest !== null && typeof digest !== 'string')
    throw new ERR_INVALID_ARG_TYPE('digest', ['string', 'null'], digest);

  password = checkIsArrayBufferView('password', password);
  salt = checkIsArrayBufferView('salt', salt);
  // FIXME(bnoordhuis) The error message is in fact wrong since |iterations|
  // cannot be > INT_MAX.  Adjust in the next major release.
  iterations = checkIsUint('iterations', iterations, 'a non-negative number');
  keylen = checkIsUint('keylen', keylen);

  const encoding = getDefaultEncoding();

  if (encoding === 'buffer') {
    const ret = PBKDF2(password, salt, iterations, keylen, digest, callback);
    if (ret === -1)
      throw new ERR_CRYPTO_INVALID_DIGEST(digest);
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
      throw new ERR_CRYPTO_INVALID_DIGEST(digest);
  } else {
    const ret = PBKDF2(password, salt, iterations, keylen, digest);
    if (ret === -1)
      throw new ERR_CRYPTO_INVALID_DIGEST(digest);
    return ret.toString(encoding);
  }
}

module.exports = {
  pbkdf2,
  pbkdf2Sync
};
