var assert = require('assert');
var common = require('../common');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

function test1() {
  var ciphers = '';
  tls.createSecureContext = function(options) {
    ciphers = options.ciphers
  }
  var s = tls.connect(common.PORT);
  s.destroy();
  assert.equal(ciphers, tls.DEFAULT_CIPHERS);
}
test1();
