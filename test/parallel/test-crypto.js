'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const tls = require('tls');

crypto.DEFAULT_ENCODING = 'buffer';

// Test Certificates
const caPem = fs.readFileSync(common.fixturesDir + '/test_ca.pem', 'ascii');
const certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
const certPfx = fs.readFileSync(common.fixturesDir + '/test_cert.pfx');
const keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');

// 'this' safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  const options = {key: keyPem, cert: certPem, ca: caPem};
  const credentials = tls.createSecureContext(options);
  const context = credentials.context;
  const notcontext = { setOptions: context.setOptions, setKey: context.setKey };
  tls.createSecureContext({ secureOptions: 1 }, notcontext);
}, /^TypeError: Illegal invocation$/);

// PFX tests
assert.doesNotThrow(function() {
  tls.createSecureContext({pfx: certPfx, passphrase: 'sample'});
});

assert.throws(function() {
  tls.createSecureContext({pfx: certPfx});
}, /^Error: mac verify failure$/);

assert.throws(function() {
  tls.createSecureContext({pfx: certPfx, passphrase: 'test'});
}, /^Error: mac verify failure$/);

assert.throws(function() {
  tls.createSecureContext({pfx: 'sample', passphrase: 'test'});
}, /^Error: not enough data$/);


// update() should only take buffers / strings
assert.throws(function() {
  crypto.createHash('sha1').update({foo: 'bar'});
}, /^TypeError: Data must be a string or a buffer$/);


function assertSorted(list) {
  // Array#sort() modifies the list in place so make a copy.
  const sorted = list.slice().sort();
  assert.deepStrictEqual(list, sorted);
}

// Assume that we have at least AES-128-CBC.
assert.notStrictEqual(0, crypto.getCiphers().length);
assert(crypto.getCiphers().includes('aes-128-cbc'));
assert(!crypto.getCiphers().includes('AES-128-CBC'));
assertSorted(crypto.getCiphers());

// Assume that we have at least AES256-SHA.
assert.notStrictEqual(0, tls.getCiphers().length);
assert(tls.getCiphers().includes('aes256-sha'));
assert(!tls.getCiphers().includes('AES256-SHA'));
assertSorted(tls.getCiphers());

// Assert that we have sha and sha1 but not SHA and SHA1.
assert.notStrictEqual(0, crypto.getHashes().length);
assert(crypto.getHashes().includes('sha1'));
assert(crypto.getHashes().includes('sha'));
assert(!crypto.getHashes().includes('SHA1'));
assert(!crypto.getHashes().includes('SHA'));
assert(crypto.getHashes().includes('RSA-SHA1'));
assert(!crypto.getHashes().includes('rsa-sha1'));
assertSorted(crypto.getHashes());

// Assume that we have at least secp384r1.
assert.notStrictEqual(0, crypto.getCurves().length);
assert(crypto.getCurves().includes('secp384r1'));
assert(!crypto.getCurves().includes('SECP384R1'));
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
}, /^TypeError: Bad input string$/);

assert.throws(function() {
  crypto.createSign('RSA-SHA1').update('0', 'hex');
}, /^TypeError: Bad input string$/);

assert.throws(function() {
  crypto.createVerify('RSA-SHA1').update('0', 'hex');
}, /^TypeError: Bad input string$/);

assert.throws(function() {
  const priv = [
    '-----BEGIN RSA PRIVATE KEY-----',
    'MIGrAgEAAiEA+3z+1QNF2/unumadiwEr+C5vfhezsb3hp4jAnCNRpPcCAwEAAQIgQNriSQK4',
    'EFwczDhMZp2dvbcz7OUUyt36z3S4usFPHSECEQD/41K7SujrstBfoCPzwC1xAhEA+5kt4BJy',
    'eKN7LggbF3Dk5wIQN6SL+fQ5H/+7NgARsVBp0QIRANxYRukavs4QvuyNhMx+vrkCEQCbf6j/',
    'Ig6/HueCK/0Jkmp+',
    '-----END RSA PRIVATE KEY-----',
    ''
  ].join('\n');
  crypto.createSign('RSA-SHA256').update('test').sign(priv);
}, /digest too big for rsa key$/);

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
  const sha1_privateKey = fs.readFileSync(common.fixturesDir +
                                        '/test_bad_rsa_privkey.pem', 'ascii');
  // this would inject errors onto OpenSSL's error stack
  crypto.createSign('sha1').sign(sha1_privateKey);
}, /asn1 encoding routines:ASN1_CHECK_TLEN:wrong tag/);

// Make sure memory isn't released before being returned
console.log(crypto.randomBytes(16));

assert.throws(function() {
  tls.createSecureContext({ crl: 'not a CRL' });
}, /^Error: Failed to parse CRL$/);
