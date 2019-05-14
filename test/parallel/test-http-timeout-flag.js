'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const http = require('http');
const https = require('https');
const http2 = require('http2');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Make sure the defaults are correct.
const servers = [
  http.createServer(),
  https.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem')
  }),
  http2.createServer()
];

for (const server of servers) {
  assert.strictEqual(server.timeout, 120000);
  server.close();
}

// Ensure that command line flag overrides the default timeout.
const child1 = spawnSync(process.execPath, ['--http-server-default-timeout=10',
                                            '-p', 'http.createServer().timeout'
]);
assert.strictEqual(+child1.stdout.toString().trim(), 10);

// Ensure that the flag is whitelisted for NODE_OPTIONS.
const env = Object.assign({}, process.env, {
  NODE_OPTIONS: '--http-server-default-timeout=10'
});
const child2 = spawnSync(process.execPath,
                         [ '-p', 'http.createServer().timeout'], { env });
assert.strictEqual(+child2.stdout.toString().trim(), 10);
