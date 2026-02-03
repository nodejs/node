// This tests the precedence and interaction between --use-env-proxy CLI flag
// and NODE_USE_ENV_PROXY environment variable when both are set.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer, runProxiedRequest } from '../common/proxy-server.js';

// Start a proxy server for testing
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

// Start a HTTP server to process the final request
const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}, 4));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;

delete process.env.NODE_USE_ENV_PROXY; // Ensure the environment variable is not set.
// NODE_USE_ENV_PROXY=1 and --use-env-proxy can be used at the same time.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: '1',
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, ['--use-env-proxy']);

  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);

  // Should use the proxy
  assert.strictEqual(logs.length, 1);
  assert.deepStrictEqual(logs[0], {
    method: 'GET',
    url: requestUrl,
    headers: {
      'connection': 'keep-alive',
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  });

  logs.splice(0, logs.length);
}

// NODE_USE_ENV_PROXY=0 and --no-use-env-proxy can be used at the same time.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: '0',
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, ['--no-use-env-proxy']);

  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);

  // Should NOT use the proxy
  assert.strictEqual(logs.length, 0);
}

// --use-env-proxy CLI flag takes precedence over NODE_USE_ENV_PROXY=0.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: '0',
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, ['--use-env-proxy']);

  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);

  // Should use the proxy because CLI flag takes precedence
  assert.strictEqual(logs.length, 1);
  assert.deepStrictEqual(logs[0], {
    method: 'GET',
    url: requestUrl,
    headers: {
      'connection': 'keep-alive',
      'proxy-connection': 'keep-alive',
      'host': serverHost,
    },
  });

  logs.splice(0, logs.length);
}

// --no-use-env-proxy CLI flag disables the proxy even if NODE_USE_ENV_PROXY=1.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: '1',
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, ['--no-use-env-proxy']);

  // Should NOT use the proxy because CLI flag takes precedence.
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(logs.length, 0);
}

proxy.close();
server.close();
