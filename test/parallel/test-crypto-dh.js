'use strict';
var common = require('../common');
var assert = require('assert');
var constants = require('constants');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

// Test Diffie-Hellman with two parties sharing a secret,
// using various encodings as we go along
var dh1 = crypto.createDiffieHellman(256);
var p1 = dh1.getPrime('buffer');
var dh2 = crypto.createDiffieHellman(p1, 'buffer');
var key1 = dh1.generateKeys();
var key2 = dh2.generateKeys('hex');
var secret1 = dh1.computeSecret(key2, 'hex', 'base64');
var secret2 = dh2.computeSecret(key1, 'binary', 'buffer');

assert.equal(secret1, secret2.toString('base64'));
assert.equal(dh1.verifyError, 0);
assert.equal(dh2.verifyError, 0);

assert.throws(function() {
  crypto.createDiffieHellman([0x1, 0x2]);
});

assert.throws(function() {
  crypto.createDiffieHellman(function() { });
});

assert.throws(function() {
  crypto.createDiffieHellman(/abc/);
});

assert.throws(function() {
  crypto.createDiffieHellman({});
});

// Create "another dh1" using generated keys from dh1,
// and compute secret again
var dh3 = crypto.createDiffieHellman(p1, 'buffer');
var privkey1 = dh1.getPrivateKey();
dh3.setPublicKey(key1);
dh3.setPrivateKey(privkey1);

assert.deepEqual(dh1.getPrime(), dh3.getPrime());
assert.deepEqual(dh1.getGenerator(), dh3.getGenerator());
assert.deepEqual(dh1.getPublicKey(), dh3.getPublicKey());
assert.deepEqual(dh1.getPrivateKey(), dh3.getPrivateKey());
assert.equal(dh3.verifyError, 0);

var secret3 = dh3.computeSecret(key2, 'hex', 'base64');

assert.equal(secret1, secret3);

// Run this one twice to make sure that the dh3 clears its error properly
(function() {
  var c = crypto.createDecipher('aes-128-ecb', '');
  assert.throws(function() { c.final('utf8'); }, /wrong final block length/);
})();

assert.throws(function() {
  dh3.computeSecret('');
}, /key is too small/i);

(function() {
  var c = crypto.createDecipher('aes-128-ecb', '');
  assert.throws(function() { c.final('utf8'); }, /wrong final block length/);
})();

// Create a shared using a DH group.
var alice = crypto.createDiffieHellmanGroup('modp5');
var bob = crypto.createDiffieHellmanGroup('modp5');
alice.generateKeys();
bob.generateKeys();
var aSecret = alice.computeSecret(bob.getPublicKey()).toString('hex');
var bSecret = bob.computeSecret(alice.getPublicKey()).toString('hex');
assert.equal(aSecret, bSecret);
assert.equal(alice.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);
assert.equal(bob.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);

// Ensure specific generator (buffer) works as expected.
var modp1 = crypto.createDiffieHellmanGroup('modp1');
var modp1buf = new Buffer([
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc9, 0x0f,
  0xda, 0xa2, 0x21, 0x68, 0xc2, 0x34, 0xc4, 0xc6, 0x62, 0x8b,
  0x80, 0xdc, 0x1c, 0xd1, 0x29, 0x02, 0x4e, 0x08, 0x8a, 0x67,
  0xcc, 0x74, 0x02, 0x0b, 0xbe, 0xa6, 0x3b, 0x13, 0x9b, 0x22,
  0x51, 0x4a, 0x08, 0x79, 0x8e, 0x34, 0x04, 0xdd, 0xef, 0x95,
  0x19, 0xb3, 0xcd, 0x3a, 0x43, 0x1b, 0x30, 0x2b, 0x0a, 0x6d,
  0xf2, 0x5f, 0x14, 0x37, 0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51,
  0xc2, 0x45, 0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6,
  0xf4, 0x4c, 0x42, 0xe9, 0xa6, 0x3a, 0x36, 0x20, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff
]);
var exmodp1 = crypto.createDiffieHellman(modp1buf, new Buffer([2]));
modp1.generateKeys();
exmodp1.generateKeys();
var modp1Secret = modp1.computeSecret(exmodp1.getPublicKey()).toString('hex');
var exmodp1Secret = exmodp1.computeSecret(modp1.getPublicKey()).toString('hex');
assert.equal(modp1Secret, exmodp1Secret);
assert.equal(modp1.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);
assert.equal(exmodp1.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Ensure specific generator (string with encoding) works as expected.
var exmodp1_2 = crypto.createDiffieHellman(modp1buf, '02', 'hex');
exmodp1_2.generateKeys();
modp1Secret = modp1.computeSecret(exmodp1_2.getPublicKey()).toString('hex');
var exmodp1_2Secret = exmodp1_2.computeSecret(modp1.getPublicKey())
                               .toString('hex');
assert.equal(modp1Secret, exmodp1_2Secret);
assert.equal(exmodp1_2.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Ensure specific generator (string without encoding) works as expected.
var exmodp1_3 = crypto.createDiffieHellman(modp1buf, '\x02');
exmodp1_3.generateKeys();
modp1Secret = modp1.computeSecret(exmodp1_3.getPublicKey()).toString('hex');
var exmodp1_3Secret = exmodp1_3.computeSecret(modp1.getPublicKey())
                               .toString('hex');
assert.equal(modp1Secret, exmodp1_3Secret);
assert.equal(exmodp1_3.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Ensure specific generator (numeric) works as expected.
var exmodp1_4 = crypto.createDiffieHellman(modp1buf, 2);
exmodp1_4.generateKeys();
modp1Secret = modp1.computeSecret(exmodp1_4.getPublicKey()).toString('hex');
var exmodp1_4Secret = exmodp1_4.computeSecret(modp1.getPublicKey())
                               .toString('hex');
assert.equal(modp1Secret, exmodp1_4Secret);
assert.equal(exmodp1_4.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


var p = 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74' +
        '020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437' +
        '4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED' +
        'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381FFFFFFFFFFFFFFFF';
var bad_dh = crypto.createDiffieHellman(p, 'hex');
assert.equal(bad_dh.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Test ECDH
var ecdh1 = crypto.createECDH('prime256v1');
var ecdh2 = crypto.createECDH('prime256v1');
var key1 = ecdh1.generateKeys();
var key2 = ecdh2.generateKeys('hex');
var secret1 = ecdh1.computeSecret(key2, 'hex', 'base64');
var secret2 = ecdh2.computeSecret(key1, 'binary', 'buffer');

assert.equal(secret1, secret2.toString('base64'));

// Point formats
assert.equal(ecdh1.getPublicKey('buffer', 'uncompressed')[0], 4);
var firstByte = ecdh1.getPublicKey('buffer', 'compressed')[0];
assert(firstByte === 2 || firstByte === 3);
var firstByte = ecdh1.getPublicKey('buffer', 'hybrid')[0];
assert(firstByte === 6 || firstByte === 7);

// ECDH should check that point is on curve
var ecdh3 = crypto.createECDH('secp256k1');
var key3 = ecdh3.generateKeys();

assert.throws(function() {
  var secret3 = ecdh2.computeSecret(key3, 'binary', 'buffer');
});

// ECDH should allow .setPrivateKey()/.setPublicKey()
var ecdh4 = crypto.createECDH('prime256v1');

ecdh4.setPrivateKey(ecdh1.getPrivateKey());
ecdh4.setPublicKey(ecdh1.getPublicKey());

assert.throws(function() {
  ecdh4.setPublicKey(ecdh3.getPublicKey());
});
