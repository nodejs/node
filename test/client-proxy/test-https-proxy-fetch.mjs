import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import https from 'node:https';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer, checkProxiedFetch } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// Start a server to process the final request.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.end('Hello world');
}, 3));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;

const expectedLogs = [{
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'connection': 'close',
    'host': serverHost,
    'proxy-connection': 'keep-alive',
  },
}];

// Check upper-cased HTTPS_PROXY environment variable.
await checkProxiedFetch({
  NODE_USE_ENV_PROXY: 1,
  FETCH_URL: `https://${serverHost}/test`,
  HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
}, {
  stdout: 'Hello world',
});
assert.deepStrictEqual(logs, expectedLogs);

// Check lower-cased https_proxy environment variable.
logs.splice(0, logs.length);
await checkProxiedFetch({
  NODE_USE_ENV_PROXY: 1,
  FETCH_URL: `https://${serverHost}/test`,
  https_proxy: `http://localhost:${proxy.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
}, {
  stdout: 'Hello world',
});
assert.deepStrictEqual(logs, expectedLogs);

const proxy2 = http.createServer();
proxy2.on('connect', common.mustNotCall());
proxy2.listen(0);
await once(proxy2, 'listening');

// Check lower-cased https_proxy environment variable takes precedence.
logs.splice(0, logs.length);
await checkProxiedFetch({
  NODE_USE_ENV_PROXY: 1,
  FETCH_URL: `https://${serverHost}/test`,
  https_proxy: `http://localhost:${proxy.address().port}`,
  HTTPS_PROXY: `http://localhost:${proxy2.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
}, {
  stdout: 'Hello world',
});
assert.deepStrictEqual(logs, expectedLogs);

proxy.close();
proxy2.close();
server.close();
