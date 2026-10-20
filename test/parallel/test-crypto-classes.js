'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const crypto = require('crypto');
const { hasOpenSSL, hasFIPS } = require('../common/crypto');

// 'ClassName' : ['args', 'for', 'constructor']
const TEST_CASES = {
  'Hash': ['sha1'],
  'Hmac': ['sha1', 'Node'],
  'Cipheriv': ['des-ede3-cbc', '0123456789abcd0123456789', '12345678'],
  'Decipheriv': ['des-ede3-cbc', '0123456789abcd0123456789', '12345678'],
  'Sign': ['RSA-SHA1'],
  'Verify': ['RSA-SHA1'],
  'DiffieHellman': [1024],
  'DiffieHellmanGroup': ['modp5'],
  'ECDH': ['prime256v1'],
};

if (hasFIPS(3)) {
  TEST_CASES.Hmac = ['sha1', '0123456789abcdef'];
  TEST_CASES.Cipheriv = [
    'aes-128-cbc', '0123456789abcdef', '1234567890abcdef'];
  TEST_CASES.Decipheriv = TEST_CASES.Cipheriv;
  TEST_CASES.Sign = ['RSA-SHA256'];
  TEST_CASES.Verify = ['RSA-SHA256'];
  TEST_CASES.DiffieHellman = [2048];
  TEST_CASES.DiffieHellmanGroup = ['modp14'];
} else if (crypto.getFips() !== 1) {
  TEST_CASES.DiffieHellman = [hasOpenSSL(3) ? 1024 : 256];
}

for (const [clazz, args] of Object.entries(TEST_CASES)) {
  assert(crypto[`create${clazz}`](...args) instanceof crypto[clazz]);
}
