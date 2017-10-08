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

const constants = process.binding('constants').crypto;
const {
  getFipsCrypto,
  setFipsCrypto,
  timingSafeEqual
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
  toBuf
} = require('internal/crypto/util');
const Certificate = require('internal/crypto/certificate');

module.exports = exports = {
  // Methods
  _toBuf: toBuf,
  createCipher: Cipher,
  createCipheriv: Cipheriv,
  createDecipher: Decipher,
  createDecipheriv: Decipheriv,
  createDiffieHellman: DiffieHellman,
  createDiffieHellmanGroup: DiffieHellmanGroup,
  createECDH: ECDH,
  createHash: Hash,
  createHmac: Hmac,
  createSign: Sign,
  createVerify: Verify,
  getCiphers,
  getCurves,
  getDiffieHellman: DiffieHellmanGroup,
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

Object.defineProperties(exports, {
  fips: {
    get: getFipsCrypto,
    set: setFipsCrypto
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
