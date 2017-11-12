'use strict';

// Test that the family option of https.get is honored.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const https = require('https');

{
  // Test that `https` machinery passes host name, and receives IP.
  const hostAddrIPv6 = '::1';
  const HOSTNAME = 'dummy';
  https.createServer({
    cert: fixtures.readKey('agent1-cert.pem'),
    key: fixtures.readKey('agent1-key.pem'),
  }, common.mustCall(function(req, res) {
    this.close();
    res.end();
  })).listen(0, hostAddrIPv6, common.mustCall(function() {
    const options = {
      host: HOSTNAME,
      port: this.address().port,
      family: 6,
      rejectUnauthorized: false,
      lookup: common.mustCall((addr, opt, cb) => {
        assert.strictEqual(addr, HOSTNAME);
        assert.strictEqual(opt.family, 6);
        cb(null, hostAddrIPv6, opt.family);
      })
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    https.get(options, common.mustCall(function() {
      assert.strictEqual('::1', this.socket.remoteAddress);
      this.destroy();
    }));
  }));
}
