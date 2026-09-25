// This tests that when the TLS handshake with the endpoint fails,
// the proxy client will get a connection error.
import * as common from '../common/index.mjs';

import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { runProxiedRequest, createProxyServer } from '../common/proxy-server.js';

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

// Start a proxy server that sends incomplete headers.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
});

// The proxy client should get a UNABLE_TO_VERIFY_LEAF_SIGNATURE during TLS handshake.
assert.match(stderr, /UNABLE_TO_VERIFY_LEAF_SIGNATURE/);
assert.strictEqual(stdout, '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

// Verify that it goes through the proxy.
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
