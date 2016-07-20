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
const https = require('https');

function runTest(hostname, ipv6) {
  const ciphers = 'AECDH-NULL-SHA';
  https.createServer({ ciphers }, common.mustCall(function(req, res) {
    this.close();
    res.end();
  })).listen(common.PORT, ipv6, common.mustCall(function() {
    const options = {
      host: hostname,
      port: common.PORT,
      family: 6,
      ciphers: ciphers,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    https.get(options, common.mustCall(function() {
      assert.strictEqual(ipv6, this.socket.remoteAddress);
      this.destroy();
    }));
  }));
}

common.getLocalIPv6Address((err, address) => {
  if (err) return common.skip(err.message);
  runTest(address.hostname, address.address);
});
