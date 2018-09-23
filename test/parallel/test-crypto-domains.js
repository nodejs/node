'use strict';
var common = require('../common');
var domain = require('domain');
var assert = require('assert');
var d = domain.create();
var expect = ['pbkdf2', 'randomBytes', 'pseudoRandomBytes'];
var errors = 0;

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var crypto = require('crypto');

process.on('exit', function() {
  assert.equal(errors, 3);
});

d.on('error', function(e) {
  assert.equal(e.message, expect.shift());
  errors += 1;
});

d.run(function() {
  one();

  function one() {
    crypto.pbkdf2('a', 'b', 1, 8, function() {
      two();
      throw new Error('pbkdf2');
    });
  }

  function two() {
    crypto.randomBytes(4, function() {
      three();
      throw new Error('randomBytes');
    });
  }

  function three() {
    crypto.pseudoRandomBytes(4, function() {
      throw new Error('pseudoRandomBytes');
    });
  }
});
