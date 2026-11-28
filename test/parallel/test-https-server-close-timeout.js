'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const assert = require('assert');
const { createServer, get } = require('https');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

// Connections that are still active when the timeout expires are closed.
const server = createServer(options, common.mustCall((req, res) => {
  // Never send a response.
  res.on('close', common.mustCall());
  const ret = server.close({ timeout: common.platformTimeout(100) },
                           common.mustSucceed());
  assert.strictEqual(ret, server);
}));

server.listen(0, common.mustCall(() => {
  get({
    port: server.address().port,
    rejectUnauthorized: false,
  }, common.mustNotCall()).on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ECONNRESET');
  }));
}));
