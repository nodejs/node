// This tests that invalid TLS options do not result in an uncaught exception
// after an HTTPS proxy tunnel has been established.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import fixtures from '../common/fixtures.js';
import { createProxyServer } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustNotCall());
server.on('error', common.mustNotCall());
server.listen(0);
await once(server, 'listening');

const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;

// tls.connect() throws for these options while building the secure context,
// before any handshake happens.
const testCases = [
  { options: { minVersion: 'definitely-invalid' }, code: 'ERR_TLS_INVALID_PROTOCOL_VERSION' },
  { options: { ciphers: 123 }, code: 'ERR_INVALID_ARG_TYPE' },
  { options: { secureProtocol: 'definitely-invalid' }, code: 'ERR_TLS_INVALID_PROTOCOL_METHOD' },
];

for (const { options, code } of testCases) {
  const agent = new https.Agent({
    ca: fixtures.readKey('fake-startcom-root-cert.pem'),
    proxyEnv: {
      HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
    },
  });
  const req = https.get({
    host: 'localhost',
    port: server.address().port,
    path: '/test',
    agent,
    ...options,
  }, common.mustNotCall());
  const [err] = await once(req, 'error');
  assert.strictEqual(err.code, code);
  agent.destroy();
}

// Verify that the requests went through the proxy and the tunnel was established.
const requests = logs.filter((log) => !('error' in log));
// The client resets the tunnel as soon as tls.connect() throws, which the
// proxy may observe as an ECONNRESET while still relaying it.
const errors = logs.filter((log) =>
  'error' in log && log.error.code !== 'ECONNRESET');

assert.deepStrictEqual(requests, testCases.map(() => ({
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'host': serverHost,
  },
})));
assert.deepStrictEqual(errors, []);

proxy.close();
server.close();
