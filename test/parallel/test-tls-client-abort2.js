if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
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
