'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

var stream = require('stream');
var s = new stream.PassThrough();
var h = crypto.createHash('sha1');
var expect = '15987e60950cf22655b9323bc1e281f9c4aff47e';
var gotData = false;

process.on('exit', function() {
  assert(gotData);
  console.log('ok');
});

s.pipe(h).on('data', function(c) {
  assert.equal(c, expect);
  gotData = true;
}).setEncoding('hex');

s.end('aoeu');
