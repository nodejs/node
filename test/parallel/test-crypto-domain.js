'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const crypto = require('crypto');

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
