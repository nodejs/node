'use strict';

// This test that the family option of https.get is honored.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

common.skipIfNoIpv6Localhost((ipv6Host) => {
  const assert = require('assert');
  const https = require('https');
  const fixtures = require('../common/fixtures');

  https.createServer({
    cert: fixtures.readKey('agent1-cert.pem'),
    key: fixtures.readKey('agent1-key.pem'),
  }, common.mustCall(function(req, res) {
    this.close();
    res.end();
  })).listen(0, '::1', common.mustCall(function() {
    const options = {
      host: ipv6Host,
      port: this.address().port,
      family: 6,
      rejectUnauthorized: false,
    };
    // Will fail with ECONNREFUSED if the address family is not honored.
    https.get(options, common.mustCall(function() {
      assert.strictEqual('::1', this.socket.remoteAddress);
      this.destroy();
    }));
  }));
});
