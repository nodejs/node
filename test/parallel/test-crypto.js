// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

common.expectWarning({
  DeprecationWarning: [
    ['crypto.createCipher is deprecated.', 'DEP0106']
  ]
});

const assert = require('assert');
const crypto = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');

// Test Certificates
const certPfx = fixtures.readSync('test_cert.pfx');

// 'this' safety
// https://github.com/joyent/node/issues/6690
assert.throws(function() {
  const credentials = tls.createSecureContext();
  const context = credentials.context;
  const notcontext = { setOptions: context.setOptions };

  // Methods of native objects should not segfault when reassigned to a new
  // object and called illegally. This core dumped in 0.10 and was fixed in
  // 0.11.
  notcontext.setOptions();
}, (err) => {
  // Throws TypeError, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      /^TypeError: Illegal invocation$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

// PFX tests
tls.createSecureContext({ pfx: certPfx, passphrase: 'sample' });

assert.throws(function() {
  tls.createSecureContext({ pfx: certPfx });
}, (err) => {
  // Throws general Error, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      /^Error: mac verify failure$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

assert.throws(function() {
  tls.createSecureContext({ pfx: certPfx, passphrase: 'test' });
}, (err) => {
  // Throws general Error, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      /^Error: mac verify failure$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

assert.throws(function() {
  tls.createSecureContext({ pfx: 'sample', passphrase: 'test' });
}, (err) => {
  // Throws general Error, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      /^Error: not enough data$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});


// update() should only take buffers / strings
common.expectsError(
  () => crypto.createHash('sha1').update({ foo: 'bar' }),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  });


function validateList(list) {
  // The list must not be empty
  assert(list.length > 0);

  // The list should be sorted.
  // Array#sort() modifies the list in place so make a copy.
  const sorted = [...list].sort();
  assert.deepStrictEqual(list, sorted);

  // Each element should be unique.
  assert.strictEqual([...new Set(list)].length, list.length);

  // Each element should be a string.
  assert(list.every((value) => typeof value === 'string'));
}

// Assume that we have at least AES-128-CBC.
const cryptoCiphers = crypto.getCiphers();
assert(crypto.getCiphers().includes('aes-128-cbc'));
validateList(cryptoCiphers);

// Assume that we have at least AES256-SHA.
const tlsCiphers = tls.getCiphers();
assert(tls.getCiphers().includes('aes256-sha'));
// There should be no capital letters in any element.
const noCapitals = /^[^A-Z]+$/;
assert(tlsCiphers.every((value) => noCapitals.test(value)));
validateList(tlsCiphers);

// Assert that we have sha1 and sha256 but not SHA1 and SHA256.
assert.notStrictEqual(crypto.getHashes().length, 0);
assert(crypto.getHashes().includes('sha1'));
assert(crypto.getHashes().includes('sha256'));
assert(!crypto.getHashes().includes('SHA1'));
assert(!crypto.getHashes().includes('SHA256'));
assert(crypto.getHashes().includes('RSA-SHA1'));
assert(!crypto.getHashes().includes('rsa-sha1'));
validateList(crypto.getHashes());

// Assume that we have at least secp384r1.
assert.notStrictEqual(crypto.getCurves().length, 0);
assert(crypto.getCurves().includes('secp384r1'));
assert(!crypto.getCurves().includes('SECP384R1'));
validateList(crypto.getCurves());

// Modifying return value from get* functions should not mutate subsequent
// return values.
function testImmutability(fn) {
  const list = fn();
  const copy = [...list];
  list.push('some-arbitrary-value');
  assert.deepStrictEqual(fn(), copy);
}

testImmutability(crypto.getCiphers);
testImmutability(tls.getCiphers);
testImmutability(crypto.getHashes);
testImmutability(crypto.getCurves);

// Regression tests for https://github.com/nodejs/node-v0.x-archive/pull/5725:
// hex input that's not a power of two should throw, not assert in C++ land.
assert.throws(function() {
  crypto.createCipher('aes192', 'test').update('0', 'hex');
}, (err) => {
  const errorMessage =
    common.hasFipsCrypto ? /not supported in FIPS mode/ : /Bad input string/;
  // Throws general Error, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      errorMessage.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

assert.throws(function() {
  crypto.createDecipher('aes192', 'test').update('0', 'hex');
}, (err) => {
  const errorMessage =
    common.hasFipsCrypto ? /not supported in FIPS mode/ : /Bad input string/;
  // Throws general Error, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      errorMessage.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

assert.throws(function() {
  crypto.createHash('sha1').update('0', 'hex');
}, (err) => {
  // Throws TypeError, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      /^TypeError: Bad input string$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

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
  crypto.createSign('SHA256').update('test').sign(priv);
}, (err) => {
  if ((err instanceof Error) &&
      /digest too big for rsa key$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

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
  const sha1_privateKey = fixtures.readSync('test_bad_rsa_privkey.pem',
                                            'ascii');
  // This would inject errors onto OpenSSL's error stack
  crypto.createSign('sha1').sign(sha1_privateKey);
}, (err) => {
  // Throws crypto error, so there is an opensslErrorStack property.
  // The openSSL stack should have content.
  if ((err instanceof Error) &&
      /asn1 encoding routines:[^:]*:wrong tag/.test(err) &&
      err.opensslErrorStack !== undefined &&
      Array.isArray(err.opensslErrorStack) &&
      err.opensslErrorStack.length > 0) {
    return true;
  }
});

// Make sure memory isn't released before being returned
console.log(crypto.randomBytes(16));

assert.throws(function() {
  tls.createSecureContext({ crl: 'not a CRL' });
}, (err) => {
  // Throws general error, so there is no opensslErrorStack property.
  if ((err instanceof Error) &&
      /^Error: Failed to parse CRL$/.test(err) &&
      err.opensslErrorStack === undefined) {
    return true;
  }
});

/**
 * Check if the stream function uses utf8 as a default encoding.
 */

function testEncoding(options, assertionHash) {
  const hash = crypto.createHash('sha256', options);
  let hashValue = '';

  hash.on('data', (data) => {
    hashValue += data.toString('hex');
  });

  hash.on('end', common.mustCall(() => {
    assert.strictEqual(hashValue, assertionHash);
  }));

  hash.write('öäü');
  hash.end();
}

// Hash of "öäü" in utf8 format
const assertionHashUtf8 =
  '4f53d15bee524f082380e6d7247cc541e7cb0d10c64efdcc935ceeb1e7ea345c';

// Hash of "öäü" in latin1 format
const assertionHashLatin1 =
  'cd37bccd5786e2e76d9b18c871e919e6eb11cc12d868f5ae41c40ccff8e44830';

testEncoding(undefined, assertionHashUtf8);
testEncoding({}, assertionHashUtf8);

testEncoding({
  defaultEncoding: 'utf8'
}, assertionHashUtf8);

testEncoding({
  defaultEncoding: 'latin1'
}, assertionHashLatin1);
