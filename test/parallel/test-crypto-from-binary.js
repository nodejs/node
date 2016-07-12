'use strict';
// This is the same as test/simple/test-crypto, but from before the shift
// to use buffers by default.


var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

var EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
var ucs2_control = 'a\u0000';

// grow the strings to proper length
while (ucs2_control.length <= EXTERN_APEX) {
  ucs2_control = ucs2_control.repeat(2);
}


// check resultant buffer and output string
var b = Buffer.from(ucs2_control + ucs2_control, 'ucs2');

//
// Test updating from birant data
//
{
  const datum1 = b.slice(700000);
  const hash1_converted = crypto.createHash('sha1')
    .update(datum1.toString('base64'), 'base64')
    .digest('hex');
  const hash1_direct = crypto.createHash('sha1').update(datum1).digest('hex');
  assert.strictEqual(hash1_direct, hash1_converted, 'should hash the same.');

  const datum2 = b;
  const hash2_converted = crypto.createHash('sha1')
    .update(datum2.toString('base64'), 'base64')
    .digest('hex');
  const hash2_direct = crypto.createHash('sha1').update(datum2).digest('hex');
  assert.strictEqual(hash2_direct, hash2_converted, 'should hash the same.');
}
