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

var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

var fs = require('fs');
var path = require('path');

// Test Certificates
var caPem = fs.readFileSync(common.fixturesDir + '/test_ca.pem', 'ascii');
var certPem = fs.readFileSync(common.fixturesDir + '/test_cert.pem', 'ascii');
var keyPem = fs.readFileSync(common.fixturesDir + '/test_key.pem', 'ascii');
var rsaPubPem = fs.readFileSync(common.fixturesDir + '/test_rsa_pubkey.pem', 'ascii');
var rsaKeyPem = fs.readFileSync(common.fixturesDir + '/test_rsa_privkey.pem', 'ascii');

try {
  var credentials = crypto.createCredentials(
                                             {key: keyPem,
                                               cert: certPem,
                                               ca: caPem});
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

// Test HMAC

var h1 = crypto.createHmac('sha1', 'Node')
               .update('some data')
               .update('to hmac')
               .digest('hex');
assert.equal(h1, '19fd6e1ba73d9ed2224dd5094a71babe85d9a892', 'test HMAC');

// Test hashing
var a0 = crypto.createHash('sha1').update('Test123').digest('hex');
var a1 = crypto.createHash('md5').update('Test123').digest('binary');
var a2 = crypto.createHash('sha256').update('Test123').digest('base64');
var a3 = crypto.createHash('sha512').update('Test123').digest(); // binary

assert.equal(a0, '8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'Test SHA1');
assert.equal(a1, 'h\u00ea\u00cb\u0097\u00d8o\fF!\u00fa+\u000e\u0017\u00ca' +
             '\u00bd\u008c', 'Test MD5 as binary');
assert.equal(a2, '2bX1jws4GYKTlxhloUB09Z66PoJZW+y+hq5R8dnx9l4=',
             'Test SHA256 as base64');
assert.equal(a3, '\u00c1(4\u00f1\u0003\u001fd\u0097!O\'\u00d4C/&Qz\u00d4' +
                 '\u0094\u0015l\u00b8\u008dQ+\u00db\u001d\u00c4\u00b5}\u00b2' +
                 '\u00d6\u0092\u00a3\u00df\u00a2i\u00a1\u009b\n\n*\u000f' +
                 '\u00d7\u00d6\u00a2\u00a8\u0085\u00e3<\u0083\u009c\u0093' +
                 '\u00c2\u0006\u00da0\u00a1\u00879(G\u00ed\'',
             'Test SHA512 as assumed binary');

// Test multiple updates to same hash
var h1 = crypto.createHash('sha1').update('Test123').digest('hex');
var h2 = crypto.createHash('sha1').update('Test').update('123').digest('hex');
assert.equal(h1, h2, 'multipled updates');

// Test hashing for binary files
var fn = path.join(common.fixturesDir, 'sample.png');
var sha1Hash = crypto.createHash('sha1');
var fileStream = fs.createReadStream(fn);
fileStream.addListener('data', function(data) {
  sha1Hash.update(data);
});
fileStream.addListener('close', function() {
  assert.equal(sha1Hash.digest('hex'),
               '22723e553129a336ad96e10f6aecdf0f45e4149e',
               'Test SHA1 of sample.png');
});

// Test signing and verifying
var s1 = crypto.createSign('RSA-SHA1')
               .update('Test123')
               .sign(keyPem, 'base64');
var verified = crypto.createVerify('RSA-SHA1')
                     .update('Test')
                     .update('123')
                     .verify(certPem, s1, 'base64');
assert.ok(verified, 'sign and verify (base 64)');

var s2 = crypto.createSign('RSA-SHA256')
               .update('Test123')
               .sign(keyPem); // binary
var verified = crypto.createVerify('RSA-SHA256')
                     .update('Test')
                     .update('123')
                     .verify(certPem, s2); // binary
assert.ok(verified, 'sign and verify (binary)');

// Test encryption and decryption
var plaintext = 'Keep this a secret? No! Tell everyone about node.js!';
var cipher = crypto.createCipher('aes192', 'MySecretKey123');

// encrypt plaintext which is in utf8 format
// to a ciphertext which will be in hex
var ciph = cipher.update(plaintext, 'utf8', 'hex');
// Only use binary or hex, not base64.
ciph += cipher.final('hex');

var decipher = crypto.createDecipher('aes192', 'MySecretKey123');
var txt = decipher.update(ciph, 'hex', 'utf8');
txt += decipher.final('utf8');

assert.equal(txt, plaintext, 'encryption and decryption');

// encryption and decryption with Base64
// reported in https://github.com/joyent/node/issues/738
var plaintext =
  '32|RmVZZkFUVmpRRkp0TmJaUm56ZU9qcnJkaXNNWVNpTTU*|iXmckfRWZBGWWELw' +
  'eCBsThSsfUHLeRe0KCsK8ooHgxie0zOINpXxfZi/oNG7uq9JWFVCk70gfzQH8ZUJjAfaFg**';
var cipher = crypto.createCipher('aes256', '0123456789abcdef');

// encrypt plaintext which is in utf8 format
// to a ciphertext which will be in Base64
var ciph = cipher.update(plaintext, 'utf8', 'base64');
ciph += cipher.final('base64');

var decipher = crypto.createDecipher('aes256', '0123456789abcdef');
var txt = decipher.update(ciph, 'base64', 'utf8');
txt += decipher.final('utf8');

assert.equal(txt, plaintext, 'encryption and decryption with Base64');


// Test encyrption and decryption with explicit key and iv
var encryption_key = '0123456789abcd0123456789';
var iv = '12345678';

var cipher = crypto.createCipheriv('des-ede3-cbc', encryption_key, iv);
var ciph = cipher.update(plaintext, 'utf8', 'hex');
ciph += cipher.final('hex');

var decipher = crypto.createDecipheriv('des-ede3-cbc', encryption_key, iv);
var txt = decipher.update(ciph, 'hex', 'utf8');
txt += decipher.final('utf8');

assert.equal(txt, plaintext, 'encryption and decryption with key and iv');

// update() should only take buffers / strings
assert.throws(function() {
  crypto.createHash('sha1').update({foo: 'bar'});
}, /string or buffer/);


// Test RSA key signing/verification
var rsaSign = crypto.createSign('RSA-SHA1');
var rsaVerify = crypto.createVerify('RSA-SHA1');
assert.ok(rsaSign);
assert.ok(rsaVerify);

rsaSign.update(rsaPubPem);
var rsaSignature = rsaSign.sign(rsaKeyPem, 'hex');
assert.equal(rsaSignature, '5c50e3145c4e2497aadb0eabc83b342d0b0021ece0d4c4a064b7c8f020d7e2688b122bfb54c724ac9ee169f83f66d2fe90abeb95e8e1290e7e177152a4de3d944cf7d4883114a20ed0f78e70e25ef0f60f06b858e6af42a2f276ede95bbc6bc9a9bbdda15bd663186a6f40819a7af19e577bb2efa5e579a1f5ce8a0d4ca8b8f6');

rsaVerify.update(rsaPubPem);
assert.equal(rsaVerify.verify(rsaPubPem, rsaSignature, 'hex'), 1);
