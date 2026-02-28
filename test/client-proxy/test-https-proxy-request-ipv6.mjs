// This tests making HTTPS requests through an HTTP proxy using IPv6 addresses.

import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasIPv6) {
  common.skip('missing IPv6 support');
}

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
}, 2));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

{
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

  const { code, signal, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    HTTPS_PROXY: `http://[::1]:${proxy.address().port}`,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
  });
  assert.deepStrictEqual(logs, expectedLogs);
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test with IPv6 address in the request URL.
{
  logs.splice(0, logs.length); // Clear the logs.
  const serverHost = `[::1]:${server.address().port}`;
  const requestUrl = `https://${serverHost}/test`;
  const expectedLogs = [{
    method: 'CONNECT',
    url: serverHost,
    headers: {
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  }];

  const { code, signal, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    HTTPS_PROXY: `http://[::1]:${proxy.address().port}`,
    // Disable certificate verification for this request, for we don't have
    // a certificate for [::1].
    NODE_TLS_REJECT_UNAUTHORIZED: '0',
  });
  assert.deepStrictEqual(logs, expectedLogs);
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

proxy.close();
server.close();
