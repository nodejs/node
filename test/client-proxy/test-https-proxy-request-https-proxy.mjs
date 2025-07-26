// This tests that when the `NODE_USE_ENV_PROXY` environment variable is set to 1, Node.js
// correctly uses the `HTTPS_PROXY` or `https_proxy` environment variable to proxy HTTPS requests
// via a tunnel when the proxy server itself uses HTTPS.

import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Start a server to process the final request.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start an HTTPS proxy server.
const { proxy, logs } = createProxyServer({ https: true });
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;
const expectedLogs = [{
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'proxy-connection': 'keep-alive',
    'host': serverHost,
  },
}];

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `https://localhost:${proxy.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});
assert.deepStrictEqual(logs, expectedLogs);
assert.strictEqual(stderr.trim(), '');
assert.match(stdout, /Hello world/);
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

proxy.close();
server.close();
