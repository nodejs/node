// This tests that --use-env-proxy works the same as NODE_USE_ENV_PROXY=1
// for HTTP requests using the built-in http module and fetch API.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer, runProxiedRequest, checkProxiedFetch } from '../common/proxy-server.js';

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

// Tests --use-env-proxy works with http builtins.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, ['--use-env-proxy']);
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.deepStrictEqual(logs, [{
    method: 'GET',
    url: requestUrl,
    headers: {
      'connection': 'keep-alive',
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  }]);
}

// Tests --use-env-proxy works with fetch and http.
{
  logs.splice(0, logs.length);
  await checkProxiedFetch({
    FETCH_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, {
    stdout: 'Hello world',
  }, ['--use-env-proxy']);

  // FIXME(undici:4083): undici currently always tunnels the request over
  // CONNECT if proxyTunnel is not explicitly set to false, but what we
  // need is for it to be automatically false for HTTP requests to be
  // consistent with curl.
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
