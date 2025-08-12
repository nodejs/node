// This tests that when the proxy server returns a 500 response, the https client can
// handle it correctly.

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
}, common.mustNotCall());
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a proxy server that sends 500 response back.
const proxy = http.createServer();
proxy.on('connect', common.mustCall((req, res) => {
  res.write('HTTP/1.1 500 Connection Error\r\n\r\n');
  res.end('Proxy error: test error');
}, 1));
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

// The proxy client should get an error from failure in establishing the tunnel.
assert.match(stderr, /ERR_PROXY_TUNNEL.*Failed to establish tunnel to .* HTTP\/1\.1 500 Connection Error/);
assert.strictEqual(stdout.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

proxy.close();
server.close();
