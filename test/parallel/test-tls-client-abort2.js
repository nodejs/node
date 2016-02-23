'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var errors = 0;

var conn = tls.connect(common.PORT, function() {
  assert(false); // callback should never be executed
});
conn.on('error', function() {
  ++errors;
  assert.doesNotThrow(function() {
    conn.destroy();
  });
});

process.on('exit', function() {
  assert.equal(errors, 1);
});
