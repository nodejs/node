'use strict';

const { AsyncWrap, Providers } = process.binding('async_wrap');
const { Buffer } = require('buffer');
const { INT_MAX, pbkdf2: _pbkdf2 } = process.binding('crypto');
const { validateInt32 } = require('internal/validators');
const {
  ERR_CRYPTO_INVALID_DIGEST,
  ERR_CRYPTO_PBKDF2_ERROR,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
} = require('internal/errors').codes;
const {
  getDefaultEncoding,
  validateArrayBufferView,
} = require('internal/crypto/util');

function pbkdf2(password, salt, iterations, keylen, digest, callback) {
  if (typeof digest === 'function') {
    callback = digest;
    digest = undefined;
  }

  ({ password, salt, iterations, keylen, digest } =
      check(password, salt, iterations, keylen, digest, callback));

  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK();

  const encoding = getDefaultEncoding();
  const keybuf = Buffer.alloc(keylen);

  const wrap = new AsyncWrap(Providers.PBKDF2REQUEST);
  wrap.ondone = (ok) => {  // Retains keybuf while request is in flight.
    if (!ok) return callback.call(wrap, new ERR_CRYPTO_PBKDF2_ERROR());
    if (encoding === 'buffer') return callback.call(wrap, null, keybuf);
    callback.call(wrap, null, keybuf.toString(encoding));
  };

  handleError(keybuf, password, salt, iterations, digest, wrap);
}

function pbkdf2Sync(password, salt, iterations, keylen, digest) {
  ({ password, salt, iterations, keylen, digest } =
      check(password, salt, iterations, keylen, digest, pbkdf2Sync));
  const keybuf = Buffer.alloc(keylen);
  handleError(keybuf, password, salt, iterations, digest);
  const encoding = getDefaultEncoding();
  if (encoding === 'buffer') return keybuf;
  return keybuf.toString(encoding);
}

function check(password, salt, iterations, keylen, digest, callback) {
  if (typeof digest !== 'string') {
    if (digest !== null)
      throw new ERR_INVALID_ARG_TYPE('digest', ['string', 'null'], digest);
    digest = 'sha1';
  }

  password = validateArrayBufferView(password, 'password');
  salt = validateArrayBufferView(salt, 'salt');
  iterations = validateInt32(iterations, 'iterations', 0, INT_MAX);
  keylen = validateInt32(keylen, 'keylen', 0, INT_MAX);

  return { password, salt, iterations, keylen, digest };
}

function handleError(keybuf, password, salt, iterations, digest, wrap) {
  const rc = _pbkdf2(keybuf, password, salt, iterations, digest, wrap);

  if (rc === -1)
    throw new ERR_CRYPTO_INVALID_DIGEST(digest);

  if (rc === false)
    throw new ERR_CRYPTO_PBKDF2_ERROR();
}

module.exports = {
  pbkdf2,
  pbkdf2Sync
};
