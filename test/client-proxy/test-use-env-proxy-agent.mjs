// This tests that when Node.js is configured to use the environment proxy
// (via --use-env-proxy or NODE_USE_ENV_PROXY), a custom agent created without
// a `proxyEnv` option falls back to process.env and goes through the proxy,
// while an agent created with an explicit `proxyEnv: null` bypasses it.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import fixtures from '../common/fixtures.js';
import { createProxyServer } from '../common/proxy-server.js';

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
const script = fixtures.path('agent-request-and-log.js');

function runAgentRequest(env, cliArgs = []) {
  return common.spawnPromisified(process.execPath, [...cliArgs, script], {
    env: {
      ...process.env,
      REQUEST_URL: requestUrl,
      HTTP_PROXY: `http://localhost:${proxy.address().port}`,
      ...env,
    },
  });
}

// A plain agent without a `proxyEnv` option should use the environment proxy
// when Node.js is started with --use-env-proxy.
{
  const { code, signal, stderr, stdout } = await runAgentRequest({}, ['--use-env-proxy']);
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  // The request should go through the proxy.
  assert.strictEqual(logs.length, 1);
  assert.strictEqual(logs[0].method, 'GET');
  assert.strictEqual(logs[0].url, requestUrl);
}

// An agent created with an explicit `proxyEnv: null` should bypass the proxy
// even when Node.js is configured to use the environment proxy. The same must
// hold whether the proxy is enabled via NODE_USE_ENV_PROXY or --use-env-proxy.
{
  logs.splice(0, logs.length);
  const { code, signal, stderr, stdout } = await runAgentRequest({
    NODE_USE_ENV_PROXY: '1',
    PROXY_ENV_NULL: '1',
  });
  assert.strictEqual(stderr.trim(), '');
  assert.match(stdout, /Hello world/);
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  // The request should reach the server directly, so the proxy sees nothing.
  assert.strictEqual(logs.length, 0);
}

server.close();
proxy.close();
