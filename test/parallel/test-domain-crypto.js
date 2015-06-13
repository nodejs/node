/* eslint-disable strict */
try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Not compiled with OPENSSL support.');
  process.exit();
}

// the missing var keyword is intentional
domain = require('domain');

// should not throw a 'TypeError: undefined is not a function' exception
crypto.randomBytes(8);
crypto.randomBytes(8, function() {});
crypto.pseudoRandomBytes(8);
crypto.pseudoRandomBytes(8, function() {});
crypto.pbkdf2('password', 'salt', 8, 8, function() {});
