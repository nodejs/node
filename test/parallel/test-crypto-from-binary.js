// This is the same as test/simple/test-crypto, but from before the shift
// to use buffers by default.


var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

var EXTERN_APEX = 0xFBEE9;

// manually controlled string for checking binary output
var ucs2_control = 'a\u0000';

// grow the strings to proper length
while (ucs2_control.length <= EXTERN_APEX) {
  ucs2_control += ucs2_control;
}


// check resultant buffer and output string
var b = new Buffer(ucs2_control + ucs2_control, 'ucs2');

//
// Test updating from birant data
//
(function() {
  var datum1 = b.slice(700000);
  var hash1_converted = crypto.createHash('sha1')
    .update(datum1.toString('base64'), 'base64')
    .digest('hex');
  var hash1_direct = crypto.createHash('sha1').update(datum1).digest('hex');
  assert.equal(hash1_direct, hash1_converted, 'should hash the same.');

  var datum2 = b;
  var hash2_converted = crypto.createHash('sha1')
    .update(datum2.toString('base64'), 'base64')
    .digest('hex');
  var hash2_direct = crypto.createHash('sha1').update(datum2).digest('hex');
  assert.equal(hash2_direct, hash2_converted, 'should hash the same.');
})();
