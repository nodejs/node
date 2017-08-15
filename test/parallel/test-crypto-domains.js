'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const domain = require('domain');
const assert = require('assert');
const crypto = require('crypto');

const d = domain.create();
const expect = ['pbkdf2', 'randomBytes', 'pseudoRandomBytes'];

d.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.message, expect.shift());
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
