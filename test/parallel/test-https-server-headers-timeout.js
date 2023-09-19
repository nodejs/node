'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const { createServer } = require('https');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

const server = createServer(options);

// 60000 seconds is the default
assert.strictEqual(server.headersTimeout, 60000);
const headersTimeout = common.platformTimeout(1000);
server.headersTimeout = headersTimeout;
assert.strictEqual(server.headersTimeout, headersTimeout);
