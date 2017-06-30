'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

//
// Test PBKDF2 with RFC 6070 test vectors (except #4)
//
function testPBKDF2(password, salt, iterations, keylen, expected) {
  const actual =
    crypto.pbkdf2Sync(password, salt, iterations, keylen, 'sha256');
  assert.strictEqual(actual.toString('latin1'), expected);

  crypto.pbkdf2(password, salt, iterations, keylen, 'sha256', (err, actual) => {
    assert.strictEqual(actual.toString('latin1'), expected);
  });
}


testPBKDF2('password', 'salt', 1, 20,
           '\x12\x0f\xb6\xcf\xfc\xf8\xb3\x2c\x43\xe7\x22\x52' +
           '\x56\xc4\xf8\x37\xa8\x65\x48\xc9');

testPBKDF2('password', 'salt', 2, 20,
           '\xae\x4d\x0c\x95\xaf\x6b\x46\xd3\x2d\x0a\xdf\xf9' +
           '\x28\xf0\x6d\xd0\x2a\x30\x3f\x8e');

testPBKDF2('password', 'salt', 4096, 20,
           '\xc5\xe4\x78\xd5\x92\x88\xc8\x41\xaa\x53\x0d\xb6' +
           '\x84\x5c\x4c\x8d\x96\x28\x93\xa0');

testPBKDF2('passwordPASSWORDpassword',
           'saltSALTsaltSALTsaltSALTsaltSALTsalt',
           4096,
           25,
           '\x34\x8c\x89\xdb\xcb\xd3\x2b\x2f\x32\xd8\x14\xb8\x11' +
           '\x6e\x84\xcf\x2b\x17\x34\x7e\xbc\x18\x00\x18\x1c');

testPBKDF2('pass\0word', 'sa\0lt', 4096, 16,
           '\x89\xb6\x9d\x05\x16\xf8\x29\x89\x3c\x69\x62\x26\x65' +
           '\x0a\x86\x87');

const expected =
    '64c486c55d30d4c5a079b8823b7d7cb37ff0556f537da8410233bcec330ed956';
const key = crypto.pbkdf2Sync('password', 'salt', 32, 32, 'sha256');
assert.strictEqual(key.toString('hex'), expected);

crypto.pbkdf2('password', 'salt', 32, 32, 'sha256', common.mustCall(ondone));
function ondone(err, key) {
  assert.ifError(err);
  assert.strictEqual(key.toString('hex'), expected);
}

// Error path should not leak memory (check with valgrind).
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, 20, null);
}, /^Error: No callback provided to pbkdf2$/);

// Should not work with Infinity key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, Infinity, 'sha256',
                common.mustNotCall());
}, /^TypeError: Bad key length$/);

// Should not work with negative Infinity key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, -Infinity, 'sha256',
                common.mustNotCall());
}, /^TypeError: Bad key length$/);

// Should not work with NaN key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, NaN, 'sha256', common.mustNotCall());
}, /^TypeError: Bad key length$/);

// Should not work with negative key length
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, -1, 'sha256', common.mustNotCall());
}, /^TypeError: Bad key length$/);

// Should not work with key length that does not fit into 32 signed bits
assert.throws(function() {
  crypto.pbkdf2('password', 'salt', 1, 4073741824, 'sha256',
                common.mustNotCall());
}, /^TypeError: Bad key length$/);

// Should not get FATAL ERROR with empty password and salt
// https://github.com/nodejs/node/issues/8571
assert.doesNotThrow(() => {
  crypto.pbkdf2('', '', 1, 32, 'sha256', common.mustCall((e) => {
    assert.ifError(e);
  }));
});

assert.throws(() => {
  crypto.pbkdf2('password', 'salt', 8, 8, common.mustNotCall());
}, /^TypeError: The "digest" argument is required and must not be undefined$/);

assert.throws(() => {
  crypto.pbkdf2Sync('password', 'salt', 8, 8);
}, /^TypeError: The "digest" argument is required and must not be undefined$/);
