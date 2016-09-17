'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const domain = require('domain');

function test(fn) {
  const ex = new Error('BAM');
  const d = domain.create();
  d.on('error', common.mustCall(function(err) {
    assert.strictEqual(err, ex);
  }));
  const cb = common.mustCall(function() {
    throw ex;
  });
  d.run(cb);
}

test(function(cb) {
  crypto.pbkdf2('password', 'salt', 1, 8, cb);
});

test(function(cb) {
  crypto.randomBytes(32, cb);
});
