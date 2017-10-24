// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Note: In 0.8 and before, crypto functions all defaulted to using
// binary-encoded strings rather than buffers.

'use strict';

const {
  assertCrypto,
  deprecate
} = require('internal/util');
assertCrypto();

const errors = require('internal/errors');
const constants = process.binding('constants').crypto;
const {
  fipsMode,
  fipsForced
} = process.binding('config');
const {
  getFipsCrypto,
  setFipsCrypto,
} = process.binding('crypto');
const {
  randomBytes,
  randomFill,
  randomFillSync
} = require('internal/crypto/random');
const {
  pbkdf2,
  pbkdf2Sync
} = require('internal/crypto/pbkdf2');
const {
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH
} = require('internal/crypto/diffiehellman');
const {
  Cipher,
  Cipheriv,
  Decipher,
  Decipheriv,
  privateDecrypt,
  privateEncrypt,
  publicDecrypt,
  publicEncrypt
} = require('internal/crypto/cipher');
const {
  Sign,
  Verify
} = require('internal/crypto/sig');
const {
  Hash,
  Hmac
} = require('internal/crypto/hash');
const {
  getCiphers,
  getCurves,
  getDefaultEncoding,
  getHashes,
  setDefaultEncoding,
  setEngine,
  timingSafeEqual,
  toBuf
} = require('internal/crypto/util');
const Certificate = require('internal/crypto/certificate');

// These helper functions are needed because the constructors can
// use new, in which case V8 cannot inline the recursive constructor call
function createHash(algorithm, options) {
  return new Hash(algorithm, options);
}

function createCipher(cipher, password, options) {
  return new Cipher(cipher, password, options);
}

function createCipheriv(cipher, key, iv, options) {
  return new Cipheriv(cipher, key, iv, options);
}

function createDecipher(cipher, password, options) {
  return new Decipher(cipher, password, options);
}

function createDecipheriv(cipher, key, iv, options) {
  return new Decipheriv(cipher, key, iv, options);
}

function createDiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding) {
  return new DiffieHellman(sizeOrKey, keyEncoding, generator, genEncoding);
}

function createDiffieHellmanGroup(name) {
  return new DiffieHellmanGroup(name);
}

function createECDH(curve) {
  return new ECDH(curve);
}

function createHmac(hmac, key, options) {
  return new Hmac(hmac, key, options);
}

function createSign(algorithm, options) {
  return new Sign(algorithm, options);
}

function createVerify(algorithm, options) {
  return new Verify(algorithm, options);
}

module.exports = exports = {
  // Methods
  _toBuf: toBuf,
  createCipher,
  createCipheriv,
  createDecipher,
  createDecipheriv,
  createDiffieHellman,
  createDiffieHellmanGroup,
  createECDH,
  createHash,
  createHmac,
  createSign,
  createVerify,
  getCiphers,
  getCurves,
  getDiffieHellman: createDiffieHellmanGroup,
  getHashes,
  pbkdf2,
  pbkdf2Sync,
  privateDecrypt,
  privateEncrypt,
  prng: randomBytes,
  pseudoRandomBytes: randomBytes,
  publicDecrypt,
  publicEncrypt,
  randomBytes,
  randomFill,
  randomFillSync,
  rng: randomBytes,
  setEngine,
  timingSafeEqual,

  // Classes
  Certificate,
  Cipher,
  Cipheriv,
  Decipher,
  Decipheriv,
  DiffieHellman,
  DiffieHellmanGroup,
  ECDH,
  Hash,
  Hmac,
  Sign,
  Verify
};

function setFipsDisabled() {
  throw new errors.Error('ERR_CRYPTO_FIPS_UNAVAILABLE');
}

function setFipsForced(val) {
  if (val) return;
  throw new errors.Error('ERR_CRYPTO_FIPS_FORCED');
}

function getFipsDisabled() {
  return 0;
}

function getFipsForced() {
  return 1;
}

Object.defineProperties(exports, {
  fips: {
    get: !fipsMode ? getFipsDisabled :
      fipsForced ? getFipsForced : getFipsCrypto,
    set: !fipsMode ? setFipsDisabled :
      fipsForced ? setFipsForced : setFipsCrypto
  },
  DEFAULT_ENCODING: {
    enumerable: true,
    configurable: true,
    get: getDefaultEncoding,
    set: setDefaultEncoding
  },
  constants: {
    configurable: false,
    enumerable: true,
    value: constants
  },

  // Legacy API
  createCredentials: {
    configurable: true,
    enumerable: true,
    get: deprecate(() => {
      return require('tls').createSecureContext;
    }, 'crypto.createCredentials is deprecated. ' +
      'Use tls.createSecureContext instead.', 'DEP0010')
  },
  Credentials: {
    configurable: true,
    enumerable: true,
    get: deprecate(function() {
      return require('tls').SecureContext;
    }, 'crypto.Credentials is deprecated. ' +
       'Use tls.SecureContext instead.', 'DEP0011')
  }
});
