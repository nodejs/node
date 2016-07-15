'use strict';
var common = require('../common');
var domain = require('domain');
var assert = require('assert');
var d = domain.create();
var expect = ['pbkdf2', 'randomBytes', 'pseudoRandomBytes'];

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var crypto = require('crypto');

d.on('error', common.mustCall(function(e) {
  assert.equal(e.message, expect.shift());
}, 3));

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
