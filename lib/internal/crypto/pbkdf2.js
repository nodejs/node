'use strict';

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const { Buffer } = require('buffer');
const { pbkdf2: _pbkdf2 } = internalBinding('crypto');
const { validateUint32 } = require('internal/validators');
const {
  ERR_CRYPTO_INVALID_DIGEST,
  ERR_CRYPTO_PBKDF2_ERROR,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
} = require('internal/errors').codes;
const {
  getDefaultEncoding,
  getArrayBufferView,
} = require('internal/crypto/util');

function pbkdf2(password, salt, iterations, keylen, digest, callback) {
  if (typeof digest === 'function') {
    callback = digest;
    digest = undefined;
  }

  ({ password, salt, iterations, keylen, digest } =
    check(password, salt, iterations, keylen, digest));

  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  const encoding = getDefaultEncoding();
  const keybuf = Buffer.alloc(keylen);

  const wrap = new AsyncWrap(Providers.PBKDF2REQUEST);
  wrap.ondone = (ok) => {  // Retains keybuf while request is in flight.
    if (!ok) return callback.call(wrap, new ERR_CRYPTO_PBKDF2_ERROR());
    if (encoding === 'buffer') return callback.call(wrap, null, keybuf);
    callback.call(wrap, null, keybuf.toString(encoding));
  };

  handleError(_pbkdf2(keybuf, password, salt, iterations, digest, wrap),
              digest);
}

function pbkdf2Sync(password, salt, iterations, keylen, digest) {
  ({ password, salt, iterations, keylen, digest } =
    check(password, salt, iterations, keylen, digest));
  const keybuf = Buffer.alloc(keylen);
  handleError(_pbkdf2(keybuf, password, salt, iterations, digest), digest);
  const encoding = getDefaultEncoding();
  if (encoding === 'buffer') return keybuf;
  return keybuf.toString(encoding);
}

function check(password, salt, iterations, keylen, digest) {
  if (typeof digest !== 'string')
    throw new ERR_INVALID_ARG_TYPE('digest', 'string', digest);

  password = getArrayBufferView(password, 'password');
  salt = getArrayBufferView(salt, 'salt');
  validateUint32(iterations, 'iterations', true);
  validateUint32(keylen, 'keylen');

  return { password, salt, iterations, keylen, digest };
}

function handleError(rc, digest) {
  if (rc === -1)
    throw new ERR_CRYPTO_INVALID_DIGEST(digest);

  if (rc === false)
    throw new ERR_CRYPTO_PBKDF2_ERROR();
}

module.exports = {
  pbkdf2,
  pbkdf2Sync
};
