'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var path = require('path');

if (!common.hasCrypto) {
  common.skip('missing crypto');
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

if (!common.hasFipsCrypto) {
  var a0 = crypto.createHash('md5').update('Test123').digest('latin1');
  assert.strictEqual(
    a0,
    'h\u00ea\u00cb\u0097\u00d8o\fF!\u00fa+\u000e\u0017\u00ca\u00bd\u008c',
    'Test MD5 as latin1'
  );
}
assert.strictEqual(a1, '8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'Test SHA1');
assert.strictEqual(a2, '2bX1jws4GYKTlxhloUB09Z66PoJZW+y+hq5R8dnx9l4=',
                   'Test SHA256 as base64');
assert.deepStrictEqual(
  a3,
  Buffer.from(
    '\u00c1(4\u00f1\u0003\u001fd\u0097!O\'\u00d4C/&Qz\u00d4' +
    '\u0094\u0015l\u00b8\u008dQ+\u00db\u001d\u00c4\u00b5}\u00b2' +
    '\u00d6\u0092\u00a3\u00df\u00a2i\u00a1\u009b\n\n*\u000f' +
    '\u00d7\u00d6\u00a2\u00a8\u0085\u00e3<\u0083\u009c\u0093' +
    '\u00c2\u0006\u00da0\u00a1\u00879(G\u00ed\'',
    'latin1'),
  'Test SHA512 as assumed buffer');
assert.deepStrictEqual(
  a4,
  Buffer.from('8308651804facb7b9af8ffc53a33a22d6a1c8ac2', 'hex'),
  'Test SHA1'
);

// stream interface should produce the same result.
assert.deepStrictEqual(a5, a3, 'stream interface is consistent');
assert.deepStrictEqual(a6, a3, 'stream interface is consistent');
assert.notEqual(a7, undefined, 'no data should return data');
assert.notEqual(a8, undefined, 'empty string should generate data');

// Test multiple updates to same hash
var h1 = crypto.createHash('sha1').update('Test123').digest('hex');
var h2 = crypto.createHash('sha1').update('Test').update('123').digest('hex');
assert.strictEqual(h1, h2, 'multipled updates');

// Test hashing for binary files
var fn = path.join(common.fixturesDir, 'sample.png');
var sha1Hash = crypto.createHash('sha1');
var fileStream = fs.createReadStream(fn);
fileStream.on('data', function(data) {
  sha1Hash.update(data);
});
fileStream.on('close', function() {
  assert.strictEqual(sha1Hash.digest('hex'),
                     '22723e553129a336ad96e10f6aecdf0f45e4149e',
                     'Test SHA1 of sample.png');
});

// Issue #2227: unknown digest method should throw an error.
assert.throws(function() {
  crypto.createHash('xyzzy');
}, /Digest method not supported/);

// Default UTF-8 encoding
var hutf8 = crypto.createHash('sha512').update('УТФ-8 text').digest('hex');
assert.strictEqual(
    hutf8,
    '4b21bbd1a68e690a730ddcb5a8bc94ead9879ffe82580767ad7ec6fa8ba2dea6' +
        '43a821af66afa9a45b6a78c712fecf0e56dc7f43aef4bcfc8eb5b4d8dca6ea5b');

assert.notEqual(
    hutf8,
    crypto.createHash('sha512').update('УТФ-8 text', 'latin1').digest('hex'));

var h3 = crypto.createHash('sha256');
h3.digest();
assert.throws(function() {
  h3.digest();
},
  /Digest already called/);

assert.throws(function() {
  h3.update('foo');
},
  /Digest already called/);
