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
const dns = require('dns');

// In a more perfect world, we could just use `localhost` for the host.
// Alas, we live in a world where some distributions do not map localhost
// to ::1 by default. See https://github.com/nodejs/node/issues/7288

var index = 0;
checkForLocalhost(common.localIPv6Hosts[index]);

function checkForLocalhost(host) {
  dns.lookup(host, 6, (err, address) => {
    if (!err)
      return runTest(host);
    index = index + 1;
    if (index < common.localIPv6Hosts.length)
      return checkForLocalhost(common.localIPv6Hosts[index]);
    common.fail('Could not find an IPv6 host for ::1');
  });
}

const runTest = common.mustCall(function runTest(host) {
  const ciphers = 'AECDH-NULL-SHA';
  https.createServer({ ciphers }, common.mustCall(function(req, res) {
    this.close();
    res.end();
  })).listen(common.PORT, '::1', common.mustCall(function() {
    const options = {
      host,
      port: common.PORT,
      family: 6,
      ciphers,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    https.get(options, common.mustCall(common.mustCall(function() {
      assert.strictEqual('::1', this.socket.remoteAddress);
      this.destroy();
    })));
  }));
});
