'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var path = require('path');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

// Test hashing
var a1 = crypto.createHash('sha1').update('Test123').digest('hex');
var a2 = crypto.createHash('sha256').update('Test123').digest('base64');
var a3 = crypto.createHash('sha512').update('Test123').digest(); // buffer
var a4 = crypto.createHash('sha1').update('Test123').digest('buffer');

// stream interface
var a5 = crypto.createHash('sha512');
a5.end('Test123');
a5 = a5.read();

var a6 = crypto.createHash('sha512');
a6.write('Te');
a6.write('st');
a6.write('123');
a6.end();
a6 = a6.read();

var a7 = crypto.createHash('sha512');
a7.end();
a7 = a7.read();

var a8 = crypto.createHash('sha512');
a8.write('');
a8.end();
a8 = a8.read();

assert.equal(a1, '8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'Test SHA1');
assert.equal(a2, '2bX1jws4GYKTlxhloUB09Z66PoJZW+y+hq5R8dnx9l4=',
             'Test SHA256 as base64');
assert.deepEqual(
  a3,
  new Buffer('c12834f1031f6497214f27d4432f2651' +
             '7ad494156cb88d512bdb1dc4b57db2d6' +
             '92a3dfa269a19b0a0a2a0fd7d6a2a885' +
             'e33c839c93c206da30a187392847ed27', 'hex'),
  'Test SHA512 as assumed buffer');
assert.deepEqual(a4,
                 new Buffer('8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'hex'),
                 'Test SHA1');

// stream interface should produce the same result.
assert.deepEqual(a5, a3, 'stream interface is consistent');
assert.deepEqual(a6, a3, 'stream interface is consistent');
assert.notEqual(a7, undefined, 'no data should return data');
assert.notEqual(a8, undefined, 'empty string should generate data');

// Test multiple updates to same hash
var h1 = crypto.createHash('sha1').update('Test123').digest('hex');
var h2 = crypto.createHash('sha1').update('Test').update('123').digest('hex');
assert.equal(h1, h2, 'multipled updates');

// Test hashing for binary files
var fn = path.join(common.fixturesDir, 'sample.png');
var sha1Hash = crypto.createHash('sha1');
var fileStream = fs.createReadStream(fn);
fileStream.on('data', function(data) {
  sha1Hash.update(data);
});
fileStream.on('close', function() {
  assert.equal(sha1Hash.digest('hex'),
               '22723e553129a336ad96e10f6aecdf0f45e4149e',
               'Test SHA1 of sample.png');
});

// Issue #2227: unknown digest method should throw an error.
assert.throws(function() {
  crypto.createHash('xyzzy');
});
