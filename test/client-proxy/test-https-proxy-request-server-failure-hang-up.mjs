// This tests that when the target server hangs up,
// the client can receive a correct error.

import * as common from '../common/index.mjs';
if (!common.hasCrypto)
  common.skip('missing crypto');

import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Start a server to process the final request.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => { req.destroy(); }, 1));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

// The proxy client should get hung up because the tunnel gets hung up.
// Note: this is different from the http proxy test because when a tunnel
// is used for HTTPS requests, the proxy server doesn't get to interject
// and send a 500 response back.
assert.match(stderr, /Error: socket hang up/);
assert.strictEqual(stdout.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

// The proxy should receive a CONNECT request.
assert.deepStrictEqual(logs, [{
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'proxy-connection': 'keep-alive',
    'host': serverHost,
  },
}]);

proxy.close();
server.close();
