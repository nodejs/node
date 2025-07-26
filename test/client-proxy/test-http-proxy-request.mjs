// This tests that when the `NODE_USE_ENV_PROXY` environment variable is set to 1, Node.js
// correctly uses the `HTTP_PROXY` or `http_proxy` environment variable to proxy HTTP requests
// via request rewriting.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}, common.isWindows ? 2 : 3));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;
const expectedLogs = [{
  method: 'GET',
  url: requestUrl,
  headers: {
    'connection': 'keep-alive',
    'proxy-connection': 'keep-alive',
    'host': serverHost,
  },
}];

// Check upper-cased HTTP_PROXY environment variable.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  });
  assert.deepStrictEqual(logs, expectedLogs);
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Check lower-cased http_proxy environment variable.
{
  logs.splice(0, logs.length);
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    http_proxy: `http://localhost:${proxy.address().port}`,
  });
  assert.deepStrictEqual(logs, expectedLogs);
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Check that the lower-cased http_proxy environment variable takes precedence over the
// upper-cased HTTP_PROXY.
// On Windows, environment variables are case-insensitive, so this test is not applicable.
if (!common.isWindows) {
  const proxy2 = http.createServer(common.mustNotCall());
  proxy2.on('connect', common.mustNotCall());
  proxy2.listen(0);
  await once(proxy2, 'listening');

  // Check lower-cased http_proxy environment variable takes precedence.
  logs.splice(0, logs.length);
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    http_proxy: `http://localhost:${proxy.address().port}`,
    HTTP_PROXY: `http://localhost:${proxy2.address().port}`,
  });
  assert.deepStrictEqual(logs, expectedLogs);
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  proxy2.close();
}

proxy.close();
server.close();
