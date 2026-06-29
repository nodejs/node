// This tests that NO_PROXY environment variable supports domain matching.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 3));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0, '127.0.0.1');
await once(server, 'listening');

// Start a proxy server that should NOT be used.
const proxy = http.createServer();
proxy.on('request', common.mustNotCall());
proxy.listen(0);
await once(proxy, 'listening');

{
  // Test NO_PROXY with exact domain match.
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://test.example.com:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    RESOLVE_TO_LOCALHOST: 'test.example.com',
    NO_PROXY: 'test.example.com',
  });

  // The request should succeed and bypass proxy.
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.match(stdout, /Resolving lookup for test\.example\.com/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

{
  // Test NO_PROXY with wildcard match.
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://test.example.com:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    RESOLVE_TO_LOCALHOST: 'test.example.com',
    NO_PROXY: '*.example.com',
  });

  // The request should succeed and bypass proxy.
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.match(stdout, /Resolving lookup for test\.example\.com/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test NO_PROXY with domain suffix match.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://test.example.com:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    RESOLVE_TO_LOCALHOST: 'test.example.com',
    NO_PROXY: '.example.com',
  });

  // The request should succeed and bypass proxy
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.match(stdout, /Resolving lookup for test\.example\.com/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}
proxy.close();
server.close();
