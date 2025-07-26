// This tests that when the proxy server connection is refused, the HTTPS client can
// handle it correctly.
import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { runProxiedRequest } from '../common/proxy-server.js';
import dgram from 'node:dgram';

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

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

// Make it fail on connection refused by connecting to a UDP port with TCP.
const udp = dgram.createSocket('udp4');
udp.bind(0, '127.0.0.1');
await once(udp, 'listening');
const port = udp.address().port;

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `http://localhost:${port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

// The proxy client should get a connection refused error.
assert.match(stderr, /Error.*connect ECONNREFUSED/);
assert.strictEqual(stdout.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

server.close();
udp.close();
