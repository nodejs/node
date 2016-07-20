'use strict';
const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

if (!common.hasIPv6) {
  common.skip('no IPv6 support');
  return;
}

const assert = require('assert');
const tls = require('tls');

function runTest(hostname, ipv6) {
  const ciphers = 'AECDH-NULL-SHA';
  tls.createServer({ ciphers }, common.mustCall(function() {
    this.close();
  })).listen(common.PORT, ipv6, common.mustCall(function() {
    const options = {
      host: hostname,
      port: common.PORT,
      family: 6,
      ciphers: ciphers,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    tls.connect(options).once('secureConnect', common.mustCall(function() {
      assert.strictEqual(ipv6, this.remoteAddress);
      this.destroy();
    }));
  }));
}

common.getLocalIPv6Address((err, address) => {
  if (err) return common.skip(err.message);
  runTest(address.hostname, address.address);
});
