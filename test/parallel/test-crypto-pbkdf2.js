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
  var actual = crypto.pbkdf2Sync(password, salt, iterations, keylen, 'sha256');
  assert.equal(actual.toString('hex'), expected);

  crypto.pbkdf2(password, salt, iterations, keylen, 'sha256', (err, actual) => {
    assert.equal(actual.toString('hex'), expected);
  });
}


testPBKDF2('password', 'salt', 1, 20,
           '120fb6cffcf8b32c43e72252' +
           '56c4f837a86548c9');

testPBKDF2('password', 'salt', 2, 20,
           'ae4d0c95af6b46d32d0adff9' +
           '28f06dd02a303f8e');

testPBKDF2('password', 'salt', 4096, 20,
           'c5e478d59288c841aa530db6' +
           '845c4c8d962893a0');

testPBKDF2('passwordPASSWORDpassword',
           'saltSALTsaltSALTsaltSALTsaltSALTsalt',
           4096,
           25,
           '348c89dbcbd32b2f32d814b811' +
           '6e84cf2b17347ebc1800181c');

testPBKDF2('pass\0word', 'sa\0lt', 4096, 16,
           '89b69d0516f829893c69622665' +
           '0a8687');

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
  crypto.pbkdf2('password', 'salt', 1, Infinity, 'sha256', common.fail);
}, /Bad key length/);

// Should not work with negative Infinity key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, -Infinity, 'sha256', common.fail);
}, /Bad key length/);

// Should not work with NaN key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, NaN, 'sha256', common.fail);
}, /Bad key length/);

// Should not work with negative key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, -1, 'sha256', common.fail);
}, /Bad key length/);

// Should not work with key length that does not fit into 32 signed bits
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, 4073741824, 'sha256', common.fail);
}, /Bad key length/);
