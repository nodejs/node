// This tests that when the target server fails during a POST request,
// the proxy client can handle it correctly.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { createProxyServer, runProxiedPOST } from '../common/proxy-server.js';

// Start a server that immediately destroys connections during resource creation.
const server = http.createServer(common.mustCall((req, res) => {
  // Simulate server failure during resource creation
  req.destroy();
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/resources`;
const resourceData = JSON.stringify({ name: 'failing-resource', data: 'will-not-be-created' });

const { code, signal, stderr, stdout } = await runProxiedPOST({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  RESOURCE_DATA: resourceData,
  HTTP_PROXY: `http://localhost:${proxy.address().port}`,
});

// The proxy client should receive a well-formed 500 response.
assert.match(stdout, /Proxy error ECONNRESET: socket hang up/);
assert.match(stdout, /Status Code: 500/);
assert.strictEqual(stderr.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

// The proxy should receive the POST request with correct headers.
assert.deepStrictEqual(logs[0], {
  method: 'POST',
  url: requestUrl,
  headers: {
    'connection': 'keep-alive',
    'proxy-connection': 'keep-alive',
    'host': serverHost,
    'content-type': 'application/json',
    'content-length': Buffer.byteLength(resourceData).toString(),
  },
});

// The proxy should receive a ECONNRESET from the target server.
assert.strictEqual(logs[1].source, 'proxy request');
assert.strictEqual(logs[1].error.code, 'ECONNRESET');

proxy.close();
server.close();
