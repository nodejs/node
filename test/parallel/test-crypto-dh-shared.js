'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const { hasFIPS } = require('../common/crypto');

const group = hasFIPS(3) ? 'modp14' : 'modp5';
const alice = crypto.createDiffieHellmanGroup(group);
const bob = crypto.createDiffieHellmanGroup(group);
alice.generateKeys();
bob.generateKeys();
const aSecret = alice.computeSecret(bob.getPublicKey()).toString('hex');
const bSecret = bob.computeSecret(alice.getPublicKey()).toString('hex');
assert.strictEqual(aSecret, bSecret);
