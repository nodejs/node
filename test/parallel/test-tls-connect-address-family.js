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

function runTest() {
  const ciphers = 'AECDH-NULL-SHA';
  tls.createServer({ ciphers }, common.mustCall(function() {
    this.close();
  })).listen(0, '::1', common.mustCall(function() {
    const options = {
      host: 'localhost',
      port: this.address().port,
      family: 6,
      ciphers: ciphers,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    tls.connect(options).once('secureConnect', common.mustCall(function() {
      assert.strictEqual('::1', this.remoteAddress);
      this.destroy();
    }));
  }));
}

dns.lookup('localhost', {family: 6, all: true}, (err, addresses) => {
  if (err) {
    if (err.code === 'ENOTFOUND') {
      common.skip('localhost does not resolve to ::1');
      return;
    }
    throw err;
  }

  if (addresses.some((val) => val.address === '::1'))
    runTest();
  else
    common.skip('localhost does not resolve to ::1');
});
