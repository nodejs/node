'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('missing crypto');
if (common.hasFipsCrypto)
  common.skip('crypto.createCipher() is not supported in FIPS mode');

const crypto = require('crypto');
const key = '0123456789';

{
  common.expectWarning({
    DeprecationWarning: [
      ['crypto.createCipher is deprecated.', 'DEP0106'],
    ],
    Warning: [
      ['Use Cipheriv for counter mode of aes-256-gcm'],
    ]
  });

  // Emits regular warning expected by expectWarning()
  crypto.createCipher('aes-256-gcm', key);
}

const realEmitWarning = process.emitWarning;

{
  // It's a good idea to make this overridable from userland.
  process.emitWarning = () => { throw new Error('foo'); };
  assert.throws(() => {
    crypto.createCipher('aes-256-gcm', key);
  }, /^Error: foo$/);
}

{
  Object.defineProperty(process, 'emitWarning', {
    get() { throw new Error('bar'); },
    configurable: true
  });
  assert.throws(() => {
    crypto.createCipher('aes-256-gcm', key);
  }, /^Error: bar$/);
}

// Reset back to default after the test.
Object.defineProperty(process, 'emitWarning', {
  value: realEmitWarning,
  configurable: true,
  writable: true
});
