'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');
const assert = require('assert');

{
  https.createServer({
    cert: fixtures.readKey('agent1-cert.pem'),
    key: fixtures.readKey('agent1-key.pem'),
  }, common.mustCall(function(req, res) {
    this.close();
    res.end();
  })).listen(0, common.localhostIPv4, common.mustCall(function() {
    const port = this.address().port;
    const req = https.get({
      host: common.localhostIPv4,
      pathname: '/',
      port,
      family: 4,
      localPort: 34567,
      rejectUnauthorized: false
    }, common.mustCall(() => {
      assert.strictEqual(req.socket.localPort, 34567);
      assert.strictEqual(req.socket.remotePort, port);
    }));
  }));
}
