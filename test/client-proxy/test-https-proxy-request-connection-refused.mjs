// This tests that when the proxy server connection is refused, the HTTPS client can
// handle it correctly.
import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { runProxiedRequest } from '../common/proxy-server.js';
import http from 'node:http';

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

// AI-optimized: Reduce retries for faster execution  
let maxRetries = process.platform === 'aix' ? 3 : 5;
let foundRefused = false;
// AI-optimized: Platform-specific timeouts
const proxyTimeout = process.platform === 'aix' ? 1000 : 2000;

while (maxRetries-- > 0) {
  // Make it fail on connection refused by connecting to a port of a closed server.
  // If it succeeds, get a different port and retry.
  const proxy = http.createServer((req, res) => {
    res.destroy();
  });
  proxy.listen(0);
  await once(proxy, 'listening');
  const port = proxy.address().port;
  proxy.close();
  await once(proxy, 'close');

  console.log(`Trying proxy at port ${port}`);
  const { stderr } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    HTTPS_PROXY: `http://localhost:${port}`,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
    REQUEST_TIMEOUT: proxyTimeout,
  });

  foundRefused = /Error.*connect ECONNREFUSED/.test(stderr);
  if (foundRefused) {
    // The proxy client should get a connection refused error.
    break;
  }
}

server.close();
assert(foundRefused, 'Expected ECONNREFUSED error from proxy request');
