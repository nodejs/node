'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const tls = require('tls');

if (!common.hasIPv6) {
  common.skip('no IPv6 support');
  return;
}

const ciphers = 'AECDH-NULL-SHA';
tls.createServer({ ciphers }, function() {
  this.close();
}).listen(common.PORT, '::1', function() {
  const options = {
    host: 'localhost',
    port: common.PORT,
    family: 6,
    ciphers: ciphers,
    rejectUnauthorized: false,
  };
  // Will fail with ECONNREFUSED if the address family is not honored.
  tls.connect(options).once('secureConnect', common.mustCall(function() {
    assert.strictEqual('::1', this.remoteAddress);
    this.destroy();
  }));
});
