'use strict';

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const { Buffer } = require('buffer');
const { scrypt: _scrypt } = internalBinding('crypto');
const { validateInteger, validateUint32 } = require('internal/validators');
const {
  ERR_CRYPTO_SCRYPT_INVALID_PARAMETER,
  ERR_CRYPTO_SCRYPT_NOT_SUPPORTED,
  ERR_INVALID_CALLBACK
} = require('internal/errors').codes;
const {
  getDefaultEncoding,
  getArrayBufferView,
} = require('internal/crypto/util');

const defaults = {
  N: 16384,
  r: 8,
  p: 1,
  maxmem: 32 << 20,  // 32 MB, matches SCRYPT_MAX_MEM.
};

function scrypt(password, salt, keylen, options, callback = defaults) {
  if (callback === defaults) {
    callback = options;
    options = defaults;
  }

  options = check(password, salt, keylen, options);
  const { N, r, p, maxmem } = options;
  ({ password, salt, keylen } = options);

  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  const encoding = getDefaultEncoding();
  const keybuf = Buffer.alloc(keylen);

  const wrap = new AsyncWrap(Providers.SCRYPTREQUEST);
  wrap.ondone = (ex) => {  // Retains keybuf while request is in flight.
    if (ex) return callback.call(wrap, ex);
    if (encoding === 'buffer') return callback.call(wrap, null, keybuf);
    callback.call(wrap, null, keybuf.toString(encoding));
  };

  handleError(_scrypt(keybuf, password, salt, N, r, p, maxmem, wrap));
}

function scryptSync(password, salt, keylen, options = defaults) {
  options = check(password, salt, keylen, options);
  const { N, r, p, maxmem } = options;
  ({ password, salt, keylen } = options);
  const keybuf = Buffer.alloc(keylen);
  handleError(_scrypt(keybuf, password, salt, N, r, p, maxmem));
  const encoding = getDefaultEncoding();
  if (encoding === 'buffer') return keybuf;
  return keybuf.toString(encoding);
}

function handleError(ex) {
  if (ex === undefined)
    return;

  if (ex === null)
    throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();  // Bad N, r, p, or maxmem.

  throw ex;  // Scrypt operation failed, exception object contains details.
}

function check(password, salt, keylen, options) {
  if (_scrypt === undefined)
    throw new ERR_CRYPTO_SCRYPT_NOT_SUPPORTED();

  password = getArrayBufferView(password, 'password');
  salt = getArrayBufferView(salt, 'salt');
  validateUint32(keylen, 'keylen');

  let { N, r, p, maxmem } = defaults;
  if (options && options !== defaults) {
    let has_N, has_r, has_p;
    if (has_N = (options.N !== undefined)) {
      N = options.N;
      validateUint32(N, 'N');
    }
    if (options.cost !== undefined) {
      if (has_N) throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();
      N = options.cost;
      validateUint32(N, 'cost');
    }
    if (has_r = (options.r !== undefined)) {
      r = options.r;
      validateUint32(r, 'r');
    }
    if (options.blockSize !== undefined) {
      if (has_r) throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();
      r = options.blockSize;
      validateUint32(r, 'blockSize');
    }
    if (has_p = (options.p !== undefined)) {
      p = options.p;
      validateUint32(p, 'p');
    }
    if (options.parallelization !== undefined) {
      if (has_p) throw new ERR_CRYPTO_SCRYPT_INVALID_PARAMETER();
      p = options.parallelization;
      validateUint32(p, 'parallelization');
    }
    if (options.maxmem !== undefined) {
      maxmem = options.maxmem;
      validateInteger(maxmem, 'maxmem', 0);
    }
    if (N === 0) N = defaults.N;
    if (r === 0) r = defaults.r;
    if (p === 0) p = defaults.p;
    if (maxmem === 0) maxmem = defaults.maxmem;
  }

  return { password, salt, keylen, N, r, p, maxmem };
}

module.exports = { scrypt, scryptSync };
