'use strict';
var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('1..0 # Skipped: node compiled without OpenSSL.');
  return;
}

var prime = 'c51f7bf8f0e1cf899243cdf408b1bc7c09c010e33ef7f3fbe5bd5feaf906113b';
var apub = '6fe9f37037d8d017f908378c1ee04fe60e1cd3668bfc5075fac55c2f7153dd84';
var bpub = '31d83e167fdf956c9dae6b980140577a9f8868acbfcbdc19113e58bfb9223abc';
var apriv = '4fbfd4661f9181bbf574537b1a78adf473e8e771eef13c605e963c0f3094b697';
var secret = '25616eed33f1af7975bbd0a8071d98a014f538b243bef90d76c08e81a0b3c500';

var p = crypto.createDiffieHellman(prime, 'hex');

p.setPublicKey(apub, 'hex');
p.setPrivateKey(apriv, 'hex');

assert.equal(
  p.computeSecret(bpub, 'hex', 'hex'),
  '0025616eed33f1af7975bbd0a8071d98a014f538b243bef90d76c08e81a0b3c5'
);
