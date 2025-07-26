// This tests that NO_PROXY environment variable is respected for HTTPS requests.
import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Start a server to process the final request.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 1));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a proxy server that should NOT be used.
const proxy = http.createServer();
proxy.on('connect', common.mustNotCall());
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

// Test NO_PROXY with exact hostname match.
const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  NO_PROXY: 'localhost',
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

// The request should succeed and bypass proxy.
assert.match(stdout, /Status Code: 200/);
assert.match(stdout, /Hello World/);
assert.strictEqual(stderr.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

proxy.close();
server.close();
