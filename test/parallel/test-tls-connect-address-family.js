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
    if (err.code !== 'ENOTFOUND')
      throw err;
    index = index + 1;
    if (index < common.localIPv6Hosts.length)
      return checkForLocalhost(common.localIPv6Hosts[index]);
    common.fail('Could not find an IPv6 localhost');
  });
}

const runTest = common.mustCall(function(host) {
  const ciphers = 'AECDH-NULL-SHA';
  tls.createServer({ ciphers }, common.mustCall(function() {
    this.close();
  })).listen(common.PORT, '::1', common.mustCall(function() {
    const options = {
      host,
      port: common.PORT,
      family: 6,
      ciphers,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    tls.connect(options).once('secureConnect', common.mustCall(function() {
      assert.strictEqual('::1', this.remoteAddress);
      this.destroy();
    }));
  }));
});
