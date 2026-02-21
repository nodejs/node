// This tests that NO_PROXY environment variable supports port-specific matches.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 1));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a proxy server that should NOT be used.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

// Test NO_PROXY with port-specific match correctly bypassing the proxy.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://localhost:${server.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: `localhost:${server.address().port}`,
  });

  // The request should succeed and bypass proxy.
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  // Proxy should not have been used, so logs should be empty.
  assert.deepStrictEqual(logs, []);
}

// Test NO_PROXY with port-specific mismatch does not bypass the proxy.
{
  const server2 = http.createServer(common.mustCall((req, res) => {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Hello World\n');
  }, 1));
  server2.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
  server2.listen(0);
  await once(server2, 'listening');

  const serverHost = `localhost:${server2.address().port}`;
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://${serverHost}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: `localhost:${server.address().port}`,
  });
  // The request should succeed and bypass proxy.
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  // Proxy should not have been used, so logs should be empty.
  assert.deepStrictEqual(logs, [{
    method: 'GET',
    url: `http://${serverHost}/test`,
    headers: {
      'host': serverHost,
      'proxy-connection': 'keep-alive',
      'connection': 'keep-alive',
    },
  }]);

  server2.close();
}

proxy.close();
server.close();
