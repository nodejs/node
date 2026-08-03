// This tests that NO_PROXY environment variable supports IP matches.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 2));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0, '127.0.0.1');
await once(server, 'listening');

// Start a proxy server that should NOT be used.
const proxy = http.createServer();
proxy.on('request', common.mustNotCall());
proxy.listen(0);
await once(proxy, 'listening');

// Test NO_PROXY with exact IP.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://127.0.0.1:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: '127.0.0.1',
  });

  // The request should succeed and bypass proxy
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test NO_PROXY with IP range (127.0.0.1-127.0.0.255 includes 127.0.0.1)
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://127.0.0.1:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: '127.0.0.1-127.0.0.255',
  });

  // The request should succeed and bypass proxy
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

proxy.close();
server.close();
