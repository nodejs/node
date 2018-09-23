'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

//
// Test PBKDF2 with RFC 6070 test vectors (except #4)
//
function testPBKDF2(password, salt, iterations, keylen, expected) {
  var actual = crypto.pbkdf2Sync(password, salt, iterations, keylen);
  assert.equal(actual.toString('binary'), expected);

  crypto.pbkdf2(password, salt, iterations, keylen, function(err, actual) {
    assert.equal(actual.toString('binary'), expected);
  });
}


testPBKDF2('password', 'salt', 1, 20,
           '\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9\xb5\x24' +
           '\xaf\x60\x12\x06\x2f\xe0\x37\xa6');

testPBKDF2('password', 'salt', 2, 20,
           '\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e\xd9\x2a' +
           '\xce\x1d\x41\xf0\xd8\xde\x89\x57');

testPBKDF2('password', 'salt', 4096, 20,
           '\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad\x49\xd9\x26' +
           '\xf7\x21\xd0\x65\xa4\x29\xc1');

testPBKDF2('passwordPASSWORDpassword',
           'saltSALTsaltSALTsaltSALTsaltSALTsalt',
           4096,
           25,
           '\x3d\x2e\xec\x4f\xe4\x1c\x84\x9b\x80\xc8\xd8\x36\x62' +
           '\xc0\xe4\x4a\x8b\x29\x1a\x96\x4c\xf2\xf0\x70\x38');

testPBKDF2('pass\0word', 'sa\0lt', 4096, 16,
           '\x56\xfa\x6a\xa7\x55\x48\x09\x9d\xcc\x37\xd7\xf0\x34' +
           '\x25\xe0\xc3');

var expected =
    '64c486c55d30d4c5a079b8823b7d7cb37ff0556f537da8410233bcec330ed956';
var key = crypto.pbkdf2Sync('password', 'salt', 32, 32, 'sha256');
assert.equal(key.toString('hex'), expected);

crypto.pbkdf2('password', 'salt', 32, 32, 'sha256', common.mustCall(ondone));
function ondone(err, key) {
  if (err) throw err;
  assert.equal(key.toString('hex'), expected);
}

// Error path should not leak memory (check with valgrind).
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, 20, null);
});

// Should not work with Infinity key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, Infinity, common.fail);
}, function(err) {
  return err instanceof Error && err.message === 'Bad key length';
});

// Should not work with negative Infinity key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, -Infinity, common.fail);
}, function(err) {
  return err instanceof Error && err.message === 'Bad key length';
});

// Should not work with NaN key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, NaN, common.fail);
}, function(err) {
  return err instanceof Error && err.message === 'Bad key length';
});

// Should not work with negative key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, -1, common.fail);
}, function(err) {
  return err instanceof Error && err.message === 'Bad key length';
});
