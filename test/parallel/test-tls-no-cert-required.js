'use strict';
const assert = require('assert');
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

// Omitting the cert or pfx option to tls.createServer() should not throw.
// AECDH-NULL-SHA is a no-authentication/no-encryption cipher and hence
// doesn't need a certificate.
tls.createServer({ ciphers: 'AECDH-NULL-SHA' })
  .listen(0, common.mustCall(close));

tls.createServer(assert.fail)
  .listen(0, common.mustCall(close));

tls.createServer({})
  .listen(0, common.mustCall(close));

assert.throws(() => tls.createServer('this is not valid'), TypeError);

tls.createServer()
  .listen(0, common.mustCall(close));

function close() {
  this.close();
}
