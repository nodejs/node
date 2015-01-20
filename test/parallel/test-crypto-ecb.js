var common = require('../common');
var assert = require('assert');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

crypto.DEFAULT_ENCODING = 'buffer';

// Testing whether EVP_CipherInit_ex is functioning correctly.
// Reference: bug#1997

(function() {
  var encrypt = crypto.createCipheriv('BF-ECB', 'SomeRandomBlahz0c5GZVnR', '');
  var hex = encrypt.update('Hello World!', 'ascii', 'hex');
  hex += encrypt.final('hex');
  assert.equal(hex.toUpperCase(), '6D385F424AAB0CFBF0BB86E07FFB7D71');
}());

(function() {
  var decrypt = crypto.createDecipheriv('BF-ECB', 'SomeRandomBlahz0c5GZVnR',
      '');
  var msg = decrypt.update('6D385F424AAB0CFBF0BB86E07FFB7D71', 'hex', 'ascii');
  msg += decrypt.final('ascii');
  assert.equal(msg, 'Hello World!');
}());
