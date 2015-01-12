var common = require('../common');
var assert = require('assert');
var domain = require('domain');

try {
  var crypto = require('crypto');
} catch (e) {
  console.log('Skipping test, compiled without crypto support.');
  return;
}

function test(fn) {
  var ex = new Error('BAM');
  var d = domain.create();
  d.on('error', common.mustCall(function(err) {
    assert.equal(err, ex);
  }));
  var cb = common.mustCall(function() {
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
