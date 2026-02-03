// This tests that when the proxy server rejects authentication for CONNECT,
// the client can handle it correctly.

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

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustNotCall());
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a proxy server that rejects authentication.
const proxy = http.createServer();
proxy.on('connect', common.mustCall((req, res) => {
  const authHeader = req.headers['proxy-authorization'];
  assert(authHeader);
  assert.match(authHeader, /^Basic /);
  const credentials = Buffer.from(authHeader.slice(6), 'base64').toString();
  assert.strictEqual(credentials, 'baduser:badpass');

  res.write('HTTP/1.1 407 Proxy Authentication Required\r\n');
  res.write('Proxy-Authenticate: Basic realm="proxy"\r\n');
  res.write('\r\n');
  res.end();
}, 1));
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

const { code, signal, stderr } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `http://baduser:badpass@localhost:${proxy.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

// The proxy client should get an error from proxy authentication failure.
// Since the process exits cleanly but with an error, check for any error output
assert.match(stderr, /407 Proxy Authentication Required/);
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

proxy.close();
server.close();
