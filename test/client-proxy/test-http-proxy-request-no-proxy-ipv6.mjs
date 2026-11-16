// This tests that NO_PROXY environment variable supports IPv6 ranges.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasIPv6) {
  common.skip('missing IPv6 support');
}

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello IPv6\n');
}, 3));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0, '::1');
await once(server, 'listening');

// Start a proxy server that should be used only when NO_PROXY does not match.
const proxy = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Proxied Hello IPv6\n');
}, 2));
proxy.listen(0, '::1');
await once(proxy, 'listening');

// Test NO_PROXY with a bracketed exact IPv6 address.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::1]:${server.address().port}/test`,
    HTTP_PROXY: `http://[::1]:${proxy.address().port}`,
    NO_PROXY: '[::1]',
  });

  // The request should succeed and bypass proxy
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello IPv6/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test NO_PROXY with an IPv6 host and port.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::1]:${server.address().port}/test`,
    HTTP_PROXY: `http://[::1]:${proxy.address().port}`,
    NO_PROXY: `[::1]:${server.address().port}`,
  });

  // The request should succeed and bypass proxy
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello IPv6/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test NO_PROXY with IPv6 range (::1-::100 includes ::1).
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::1]:${server.address().port}/test`,
    HTTP_PROXY: `http://[::1]:${proxy.address().port}`,
    NO_PROXY: '::1-::100',
  });

  // The request should succeed and bypass proxy
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello IPv6/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test NO_PROXY with an IPv6 address outside the range.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::1]:${server.address().port}/test`,
    HTTP_PROXY: `http://[::1]:${proxy.address().port}`,
    NO_PROXY: '::50-::100',
  });

  // The request should be proxied
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Proxied Hello IPv6/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Test NO_PROXY with an invalid IPv6 range.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::1]:${server.address().port}/test`,
    HTTP_PROXY: `http://[::1]:${proxy.address().port}`,
    NO_PROXY: '::100-::1',
  });

  // The request should be proxied
  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Proxied Hello IPv6/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

proxy.close();
server.close();
