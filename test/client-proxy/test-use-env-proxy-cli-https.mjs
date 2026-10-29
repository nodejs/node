// This tests that --use-env-proxy works the same as NODE_USE_ENV_PROXY=1
// for HTTPS requests using the built-in https module and fetch API.

import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer, runProxiedRequest, checkProxiedFetch } from '../common/proxy-server.js';

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

delete process.env.NODE_USE_ENV_PROXY; // Ensure the environment variable is not set.
// Start a HTTPS server to process the final request.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.end('Hello world');
}, 2));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

// Tests --use-env-proxy works with https builtins.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    REQUEST_URL: requestUrl,
    HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
  }, ['--use-env-proxy']);
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.deepStrictEqual(logs, [{
    method: 'CONNECT',
    url: serverHost,
    headers: {
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  }]);
}

// Tests --use-env-proxy works with fetch and https.
{
  logs.splice(0, logs.length);
  await checkProxiedFetch({
    FETCH_URL: `https://${serverHost}/test`,
    HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
  }, {
    stdout: 'Hello world',
  }, ['--use-env-proxy']);
  assert.deepStrictEqual(logs, [{
    method: 'CONNECT',
    url: serverHost,
    headers: {
      'connection': 'close',
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  }]);
}

server.close();
proxy.close();
