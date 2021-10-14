'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

assert.throws(
  function() {
    crypto.getDiffieHellman('modp1').setPrivateKey('');
  },
  new RegExp('^TypeError: crypto\\.getDiffieHellman\\(\\.\\.\\.\\)\\.' +
  'setPrivateKey is not a function$'),
  'crypto.getDiffieHellman(\'modp1\').setPrivateKey(\'\') ' +
  'failed to throw the expected error.'
);
assert.throws(
  function() {
    crypto.getDiffieHellman('modp1').setPublicKey('');
  },
  new RegExp('^TypeError: crypto\\.getDiffieHellman\\(\\.\\.\\.\\)\\.' +
  'setPublicKey is not a function$'),
  'crypto.getDiffieHellman(\'modp1\').setPublicKey(\'\') ' +
  'failed to throw the expected error.'
);
