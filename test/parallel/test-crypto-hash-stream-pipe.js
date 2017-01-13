'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const crypto = require('crypto');

const stream = require('stream');
const s = new stream.PassThrough();
const h = crypto.createHash('sha1');
const expect = '15987e60950cf22655b9323bc1e281f9c4aff47e';

s.pipe(h).on('data', common.mustCall(function(c) {
  assert.strictEqual(c, expect);
})).setEncoding('hex');

s.end('aoeu');
