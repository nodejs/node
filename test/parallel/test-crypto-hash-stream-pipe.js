'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

var stream = require('stream');
var s = new stream.PassThrough();
var h = crypto.createHash('sha1');
var expect = '15987e60950cf22655b9323bc1e281f9c4aff47e';

s.pipe(h).on('data', common.mustCall(function(c) {
  assert.equal(c, expect);
})).setEncoding('hex');

s.end('aoeu');
