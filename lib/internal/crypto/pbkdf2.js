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

    return pbkdf2(password,
                  salt,
                  iterations,
                  keylen,
                  digest,
                  callback);
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

    if (crypto.DEFAULT_ENCODING === 'buffer')
      return binding.PBKDF2(password,
                            salt,
                            iterations,
                            keylen,
                            digest,
                            callback);

    // at this point, we need to handle encodings.
    var encoding = crypto.DEFAULT_ENCODING;
    if (callback) {
      var next = function(er, ret) {
        if (ret)
          ret = ret.toString(encoding);
        callback(er, ret);
      };
      binding.PBKDF2(password, salt, iterations, keylen, digest, next);
    } else {
      var ret = binding.PBKDF2(password, salt, iterations, keylen, digest);
      return ret.toString(encoding);
    }
  }
};
