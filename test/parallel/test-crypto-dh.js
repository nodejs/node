'use strict';
var common = require('../common');
var assert = require('assert');
var constants = require('constants');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

// Test Diffie-Hellman with two parties sharing a secret,
// using various encodings as we go along
var dh1 = crypto.createDiffieHellman(common.hasFipsCrypto ? 1024 : 256);
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

/* Ensure specific generator (buffer) works as expected.
 * The values below (modp2/modp2buf) are for a 1024 bits long prime from
 * RFC 2412 E.2, see https://tools.ietf.org/html/rfc2412. */
var modp2 = crypto.createDiffieHellmanGroup('modp2');
var modp2buf = new Buffer([
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc9, 0x0f,
  0xda, 0xa2, 0x21, 0x68, 0xc2, 0x34, 0xc4, 0xc6, 0x62, 0x8b,
  0x80, 0xdc, 0x1c, 0xd1, 0x29, 0x02, 0x4e, 0x08, 0x8a, 0x67,
  0xcc, 0x74, 0x02, 0x0b, 0xbe, 0xa6, 0x3b, 0x13, 0x9b, 0x22,
  0x51, 0x4a, 0x08, 0x79, 0x8e, 0x34, 0x04, 0xdd, 0xef, 0x95,
  0x19, 0xb3, 0xcd, 0x3a, 0x43, 0x1b, 0x30, 0x2b, 0x0a, 0x6d,
  0xf2, 0x5f, 0x14, 0x37, 0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51,
  0xc2, 0x45, 0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6,
  0xf4, 0x4c, 0x42, 0xe9, 0xa6, 0x37, 0xed, 0x6b, 0x0b, 0xff,
  0x5c, 0xb6, 0xf4, 0x06, 0xb7, 0xed, 0xee, 0x38, 0x6b, 0xfb,
  0x5a, 0x89, 0x9f, 0xa5, 0xae, 0x9f, 0x24, 0x11, 0x7c, 0x4b,
  0x1f, 0xe6, 0x49, 0x28, 0x66, 0x51, 0xec, 0xe6, 0x53, 0x81,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
]);
var exmodp2 = crypto.createDiffieHellman(modp2buf, new Buffer([2]));
modp2.generateKeys();
exmodp2.generateKeys();
var modp2Secret = modp2.computeSecret(exmodp2.getPublicKey()).toString('hex');
var exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey()).toString('hex');
assert.equal(modp2Secret, exmodp2Secret);
assert.equal(modp2.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);
assert.equal(exmodp2.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Ensure specific generator (string with encoding) works as expected.
var exmodp2_2 = crypto.createDiffieHellman(modp2buf, '02', 'hex');
exmodp2_2.generateKeys();
modp2Secret = modp2.computeSecret(exmodp2_2.getPublicKey()).toString('hex');
var exmodp2_2Secret = exmodp2_2.computeSecret(modp2.getPublicKey())
                               .toString('hex');
assert.equal(modp2Secret, exmodp2_2Secret);
assert.equal(exmodp2_2.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Ensure specific generator (string without encoding) works as expected.
var exmodp2_3 = crypto.createDiffieHellman(modp2buf, '\x02');
exmodp2_3.generateKeys();
modp2Secret = modp2.computeSecret(exmodp2_3.getPublicKey()).toString('hex');
var exmodp2_3Secret = exmodp2_3.computeSecret(modp2.getPublicKey())
                               .toString('hex');
assert.equal(modp2Secret, exmodp2_3Secret);
assert.equal(exmodp2_3.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


// Ensure specific generator (numeric) works as expected.
var exmodp2_4 = crypto.createDiffieHellman(modp2buf, 2);
exmodp2_4.generateKeys();
modp2Secret = modp2.computeSecret(exmodp2_4.getPublicKey()).toString('hex');
var exmodp2_4Secret = exmodp2_4.computeSecret(modp2.getPublicKey())
                               .toString('hex');
assert.equal(modp2Secret, exmodp2_4Secret);
assert.equal(exmodp2_4.verifyError, constants.DH_NOT_SUITABLE_GENERATOR);


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
