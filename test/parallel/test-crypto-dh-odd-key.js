'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

var odd = new Buffer(39);
odd.fill('A');

var c = crypto.createDiffieHellman(32);
c.setPrivateKey(odd);
c.generateKeys();
