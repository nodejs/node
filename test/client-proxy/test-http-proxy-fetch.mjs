import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { createProxyServer, checkProxiedFetch } from '../common/proxy-server.js';

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

// FIXME(undici:4083): undici currently always tunnels the request over
// CONNECT if proxyTunnel is not explicitly set to false, but what we
// need is for it to be automatically false for HTTP requests to be
// consistent with curl.
const expectedLogs = [{
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'connection': 'close',
    'host': serverHost,
    'proxy-connection': 'keep-alive',
  },
}];

// Check upper-cased HTTPS_PROXY environment variable.
await checkProxiedFetch({
  NODE_USE_ENV_PROXY: 1,
  FETCH_URL: `http://${serverHost}/test`,
  HTTP_PROXY: `http://localhost:${proxy.address().port}`,
}, {
  stdout: 'Hello world',
});
assert.deepStrictEqual(logs, expectedLogs);

// Check lower-cased https_proxy environment variable.
logs.splice(0, logs.length);
await checkProxiedFetch({
  NODE_USE_ENV_PROXY: 1,
  FETCH_URL: `http://${serverHost}/test`,
  http_proxy: `http://localhost:${proxy.address().port}`,
}, {
  stdout: 'Hello world',
});
assert.deepStrictEqual(logs, expectedLogs);

// Check lower-cased http_proxy environment variable takes precedence.
// On Windows, environment variables are case-insensitive, so this test
// is not applicable.
if (!common.isWindows) {
  const proxy2 = http.createServer();
  proxy2.on('connect', common.mustNotCall());
  proxy2.listen(0);
  await once(proxy2, 'listening');

  logs.splice(0, logs.length);
  await checkProxiedFetch({
    NODE_USE_ENV_PROXY: 1,
    FETCH_URL: `http://${serverHost}/test`,
    http_proxy: `http://localhost:${proxy.address().port}`,
    HTTP_PROXY: `http://localhost:${proxy2.address().port}`,
  }, {
    stdout: 'Hello world',
  });
  assert.deepStrictEqual(logs, expectedLogs);
  proxy2.close();
}

proxy.close();
server.close();
