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

