'use strict';
var common = require('../common');
var assert = require('assert');
var util = require('util');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

var fs = require('fs');

// Test Certificates
var caPem = fs.readFileSync(common.fixturesDir + '/test_ca.pem', 'ascii');
var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
var certPfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');
var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');
var tls = require('tls');

// 'this' safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  var options = {key: keyPem, cert: certPem, ca: caPem};
  var credentials = crypto.createCredentials(options);
  var context = credentials.context;
  var notcontext = { setOptions: context.setOptions, setKey: context.setKey };
  crypto.createCredentials({ secureOptions: 1 }, notcontext);
}, TypeError);

// PFX tests
assert.doesNotThrow(function() {
  tls.createSecureContext({pfx: certPfx, passphrase: 'sample'});
});

assert.throws(function() {
  tls.createSecureContext({pfx: certPfx});
}, 'mac verify failure');

assert.throws(function() {
  tls.createSecureContext({pfx: certPfx, passphrase: 'test'});
}, 'mac verify failure');

assert.throws(function() {
  tls.createSecureContext({pfx: 'sample', passphrase: 'test'});
}, 'not enough data');


// update() should only take buffers / strings
assert.throws(function() {
  crypto.createHash('sha1').update({foo: 'bar'});
}, /buffer/);


function assertSorted(list) {
  // Array#sort() modifies the list in place so make a copy.
  var sorted = util._extend([], list).sort();
  assert.deepStrictEqual(list, sorted);
}

// Assume that we have at least AES-128-CBC.
assert.notEqual(0, crypto.getCiphers().length);
assert.notEqual(-1, crypto.getCiphers().indexOf('aes-128-cbc'));
assert.equal(-1, crypto.getCiphers().indexOf('AES-128-CBC'));
assertSorted(crypto.getCiphers());

// Assume that we have at least AES256-SHA.
assert.notEqual(0, tls.getCiphers().length);
assert.notEqual(-1, tls.getCiphers().indexOf('aes256-sha'));
assert.equal(-1, tls.getCiphers().indexOf('AES256-SHA'));
assertSorted(tls.getCiphers());

// Assert that we have sha and sha1 but not SHA and SHA1.
assert.notEqual(0, crypto.getHashes().length);
assert.notEqual(-1, crypto.getHashes().indexOf('sha1'));
assert.notEqual(-1, crypto.getHashes().indexOf('sha'));
assert.equal(-1, crypto.getHashes().indexOf('SHA1'));
assert.equal(-1, crypto.getHashes().indexOf('SHA'));
assert.notEqual(-1, crypto.getHashes().indexOf('RSA-SHA1'));
assert.equal(-1, crypto.getHashes().indexOf('rsa-sha1'));
assertSorted(crypto.getHashes());

// Assume that we have at least secp384r1.
assert.notEqual(0, crypto.getCurves().length);
assert.notEqual(-1, crypto.getCurves().indexOf('secp384r1'));
assert.equal(-1, crypto.getCurves().indexOf('SECP384R1'));
assertSorted(crypto.getCurves());

// Regression tests for #5725: hex input that's not a power of two should
// throw, not assert in C++ land.
assert.throws(function() {
  crypto.createCipher('aes192', 'test').update('0', 'hex');
}, common.hasFipsCrypto ? /not supported in FIPS mode/ : /Bad input string/);

assert.throws(function() {
  crypto.createDecipher('aes192', 'test').update('0', 'hex');
}, common.hasFipsCrypto ? /not supported in FIPS mode/ : /Bad input string/);

assert.throws(function() {
  crypto.createHash('sha1').update('0', 'hex');
}, /Bad input string/);

assert.throws(function() {
  crypto.createSign('RSA-SHA1').update('0', 'hex');
}, /Bad input string/);

assert.throws(function() {
  crypto.createVerify('RSA-SHA1').update('0', 'hex');
}, /Bad input string/);

assert.throws(function() {
  var priv = [
    '-----BEGIN RSA PRIVATE KEY-----',
    'MIGrAgEAAiEA+3z+1QNF2/unumadiwEr+C5vfhezsb3hp4jAnCNRpPcCAwEAAQIgQNriSQK4',
    'EFwczDhMZp2dvbcz7OUUyt36z3S4usFPHSECEQD/41K7SujrstBfoCPzwC1xAhEA+5kt4BJy',
    'eKN7LggbF3Dk5wIQN6SL+fQ5H/+7NgARsVBp0QIRANxYRukavs4QvuyNhMx+vrkCEQCbf6j/',
    'Ig6/HueCK/0Jkmp+',
    '-----END RSA PRIVATE KEY-----',
    ''
  ].join('\n');
  crypto.createSign('RSA-SHA256').update('test').sign(priv);
}, /digest too big for rsa key/);

assert.throws(function() {
  // The correct header inside `test_bad_rsa_privkey.pem` should have been
  // -----BEGIN PRIVATE KEY----- and -----END PRIVATE KEY-----
  // instead of
  // -----BEGIN RSA PRIVATE KEY----- and -----END RSA PRIVATE KEY-----
  // It is generated in this way:
  //   $ openssl genrsa -out mykey.pem 512;
  //   $ openssl pkcs8 -topk8 -inform PEM -outform PEM -in mykey.pem \
  //     -out private_key.pem -nocrypt;
  //   Then open private_key.pem and change its header and footer.
  var sha1_privateKey = fs.readFileSync(common.fixturesDir +
                                        '/test_bad_rsa_privkey.pem', 'ascii');
  // this would inject errors onto OpenSSL's error stack
  crypto.createSign('sha1').sign(sha1_privateKey);
}, /asn1 encoding routines:ASN1_CHECK_TLEN:wrong tag/);

// Make sure memory isn't released before being returned
console.log(crypto.randomBytes(16));
