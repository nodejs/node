// Tests that NODE_PROXY_TUNNEL=false disables CONNECT tunneling
// and uses direct HTTP forwarding instead.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer, checkProxiedFetch } from '../common/proxy-server.js';

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

delete process.env.NODE_USE_ENV_PROXY; // Ensure the environment variable is not set.

// Start a HTTP server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}, 2));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;

// Test: with NODE_PROXY_TUNNEL=false, fetch should NOT use CONNECT tunneling
// but instead use direct HTTP forwarding.
{
  await checkProxiedFetch({
    FETCH_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NODE_USE_ENV_PROXY: '1',
    NODE_PROXY_TUNNEL: 'false',
  }, {
    stdout: 'Hello world',
  });

  // With proxyTunnel: false, undici uses Http1ProxyWrapper which rewrites
  // the request path to include the origin (e.g. "localhost:PORT/test")
  // and sends it as a normal GET request instead of CONNECT.
  assert.strictEqual(logs[0].method, 'GET');
  assert.strictEqual(logs[0].url, requestUrl);
  assert.strictEqual(logs[0].headers.host, serverHost);
}

// Test: without NODE_PROXY_TUNNEL (default), fetch still uses CONNECT tunneling.
{
  logs.splice(0, logs.length);
  await checkProxiedFetch({
    FETCH_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NODE_USE_ENV_PROXY: '1',
  }, {
    stdout: 'Hello world',
  });

  // Without NODE_PROXY_TUNNEL set, CONNECT tunneling is used by default.
  assert.deepStrictEqual(logs, [{
    method: 'CONNECT',
    url: serverHost,
    headers: {
      'connection': 'close',
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  }]);
}

server.close();
proxy.close();
