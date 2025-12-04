// This tests making HTTP requests through an HTTP proxy using IPv6 addresses.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasIPv6) {
  common.skip('missing IPv6 support');
}

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

{
  const serverHost = `[::1]:${server.address().port}`;
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

  const { code, signal, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://[::1]:${proxy.address().port}`,
  });
  assert.deepStrictEqual(logs, expectedLogs);
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

proxy.close();
server.close();
