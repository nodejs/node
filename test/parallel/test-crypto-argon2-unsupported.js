'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL } = require('../common/crypto');

if (hasOpenSSL(3, 2))
  common.skip('requires OpenSSL < 3.2');

const assert = require('node:assert');
const crypto = require('node:crypto');

assert.throws(() => crypto.argon2(), { code: 'ERR_CRYPTO_ARGON2_NOT_SUPPORTED' });
