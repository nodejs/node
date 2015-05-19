'use strict';
var common = require('../common');
var assert = require('assert');
var util = require('util');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

crypto.DEFAULT_ENCODING = 'buffer';

var fs = require('fs');

// Test Certificates
var caPem = fs.readFileSync(common.fixturesDir + '/test_ca.pem', 'ascii');
var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
var certPfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');
var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');


// TODO(indunty): move to a separate test eventually
try {
  var tls = require('tls');
  var context = tls.createSecureContext({
    key: keyPem,
    cert: certPem,
    ca: caPem
  });
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

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
  tls.createSecureContext({pfx:certPfx, passphrase:'sample'});
});

assert.throws(function() {
  tls.createSecureContext({pfx:certPfx});
}, 'mac verify failure');

assert.throws(function() {
  tls.createSecureContext({pfx:certPfx, passphrase:'test'});
}, 'mac verify failure');

assert.throws(function() {
  tls.createSecureContext({pfx:'sample', passphrase:'test'});
}, 'not enough data');


// update() should only take buffers / strings
assert.throws(function() {
  crypto.createHash('sha1').update({foo: 'bar'});
}, /buffer/);


function assertSorted(list) {
  // Array#sort() modifies the list in place so make a copy.
  var sorted = util._extend([], list).sort();
  assert.deepEqual(list, sorted);
}

// Assume that we have at least AES-128-CBC.
assert.notEqual(0, crypto.getCiphers().length);
assert.notEqual(-1, crypto.getCiphers().indexOf('aes-128-cbc'));
assert.equal(-1, crypto.getCiphers().indexOf('AES-128-CBC'));
assertSorted(crypto.getCiphers());

// Assume that we have at least AES256-SHA.
var tls = require('tls');
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

// Regression tests for #5725: hex input that's not a power of two should
// throw, not assert in C++ land.
assert.throws(function() {
  crypto.createCipher('aes192', 'test').update('0', 'hex');
}, /Bad input string/);

assert.throws(function() {
  crypto.createDecipher('aes192', 'test').update('0', 'hex');
}, /Bad input string/);

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
}, /RSA_sign:digest too big for rsa key/);

// Make sure memory isn't released before being returned
console.log(crypto.randomBytes(16));
