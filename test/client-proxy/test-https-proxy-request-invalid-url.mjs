// This tests that a scheme-less proxy value no longer crashes startup for
// HTTPS requests.
import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  assert.strictEqual(req.url, '/test');
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: 'not-a-valid-url',
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

assert.strictEqual(stderr.trim(), '');
assert.match(stdout, /Status Code: 200/);
assert.match(stdout, /Hello world/);
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

server.close();
