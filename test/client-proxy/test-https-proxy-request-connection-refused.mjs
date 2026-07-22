// This tests that when the proxy server connection is refused, the HTTPS client can
// handle it correctly.
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
}, common.mustNotCall());
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;

let foundRefused = false;
const port = 10;
console.log(`Trying proxy at port ${port}`);
const { stderr } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: `http://127.0.0.1:${port}`,
  NO_PROXY: '',
  no_proxy: '',
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
  REQUEST_TIMEOUT: 5000,
});

foundRefused = /Error.*connect ECONNREFUSED/.test(stderr);

server.close();
assert(foundRefused, 'Expected ECONNREFUSED error from proxy request');
