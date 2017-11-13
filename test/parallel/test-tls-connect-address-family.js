'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const tls = require('tls');
const dns = require('dns');

function runTest() {
  tls.createServer({
    cert: fixtures.readKey('agent1-cert.pem'),
    key: fixtures.readKey('agent1-key.pem'),
  }, common.mustCall(function() {
    this.close();
  })).listen(0, '::1', common.mustCall(function() {
    const options = {
      host: 'localhost',
      port: this.address().port,
      family: 6,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    tls.connect(options).once('secureConnect', common.mustCall(function() {
      assert.strictEqual('::1', this.remoteAddress);
      this.destroy();
    }));
  }));
}

dns.lookup('localhost', { family: 6, all: true }, (err, addresses) => {
  if (err) {
    if (err.code === 'ENOTFOUND' || err.code === 'EAI_AGAIN')
      common.skip('localhost does not resolve to ::1');

    throw err;
  }

  if (addresses.some((val) => val.address === '::1'))
    runTest();
  else
    common.skip('localhost does not resolve to ::1');
});
