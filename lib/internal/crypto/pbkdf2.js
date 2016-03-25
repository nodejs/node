'use strict';

const internalUtil = require('internal/util');
const toBuf = require('internal/crypto/util').toBuf;

const pbkdf2DeprecationWarning =
    internalUtil.deprecate(() => {}, 'crypto.pbkdf2 without specifying' +
      ' a digest is deprecated. Please specify a digest');

module.exports = function(binding, crypto) {

  crypto.pbkdf2 = function(password,
                           salt,
                           iterations,
                           keylen,
                           digest,
                           callback) {
    if (typeof digest === 'function') {
      callback = digest;
      digest = undefined;
      pbkdf2DeprecationWarning();
    }

    if (typeof callback !== 'function')
      throw new Error('No callback provided to pbkdf2');

    return pbkdf2(password, salt, iterations, keylen, digest, callback);
  };


  crypto.pbkdf2Sync = function(password,
                               salt,
                               iterations,
                               keylen,
                               digest) {
    if (typeof digest === 'undefined') {
      digest = undefined;
      pbkdf2DeprecationWarning();
    }
    return pbkdf2(password, salt, iterations, keylen, digest);
  };


  function pbkdf2(password,
                  salt,
                  iterations,
                  keylen,
                  digest,
                  callback) {
    password = toBuf(password);
    salt = toBuf(salt);
    const encoding = crypto.DEFAULT_ENCODING;

    if (encoding === 'buffer') {
      return binding.PBKDF2(password,
                            salt,
                            iterations,
                            keylen,
                            digest,
                            callback);
    }

    // at this point, we need to handle encodings.
    const next = callback ?
        (err, ret) => { callback(err, ret ? ret.toString(encoding) : ret); } :
        undefined;
    const ret = binding.PBKDF2(password,
                               salt,
                               iterations,
                               keylen,
                               digest,
                               next);
    return ret ? ret.toString(encoding) : ret;
  }
};
