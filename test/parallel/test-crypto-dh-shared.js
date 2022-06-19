'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const alice = crypto.createDiffieHellmanGroup('modp5');
const bob = crypto.createDiffieHellmanGroup('modp5');
alice.generateKeys();
bob.generateKeys();
const aSecret = alice.computeSecret(bob.getPublicKey()).toString('hex');
const bSecret = bob.computeSecret(alice.getPublicKey()).toString('hex');
assert.strictEqual(aSecret, bSecret);
