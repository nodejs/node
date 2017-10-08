'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

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
  'Credentials': []
};

if (!common.hasFipsCrypto) {
  TEST_CASES.Cipher = ['aes192', 'secret'];
  TEST_CASES.Decipher = ['aes192', 'secret'];
  TEST_CASES.DiffieHellman = [256];
}

for (const [clazz, args] of Object.entries(TEST_CASES)) {
  assert(crypto[`create${clazz}`](...args) instanceof crypto[clazz]);
}
