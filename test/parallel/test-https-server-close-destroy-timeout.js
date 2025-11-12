'use strict';
const common = require('../common');
const assert = require('assert');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { createServer } = require('https');
const { kConnectionsCheckingInterval } = require('_http_server');

const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = createServer(options, function(req, res) {});
server.listen(0, common.mustCall(function() {
  assert.strictEqual(server[kConnectionsCheckingInterval]._destroyed, false);
  server.close(common.mustCall(() => {
    assert(server[kConnectionsCheckingInterval]._destroyed);
  }));
}));
