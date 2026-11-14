// This tests that NO_PROXY matches alternate loopback host spellings.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasIPv6) {
  common.skip('missing IPv6 support');
}

const server4 = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 2));
server4.listen(0, '127.0.0.1');
await once(server4, 'listening');

const server6 = http.createServer(common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('Hello World\n');
}, 1));
server6.listen(0, '::1');
await once(server6, 'listening');

// Start a proxy server that should NOT be used.
const proxy = http.createServer();
proxy.on('request', common.mustNotCall());
proxy.on('connect', common.mustNotCall());
proxy.listen(0);
await once(proxy, 'listening');

// A trailing dot in a DNS name should match the dotless NO_PROXY entry.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://localhost.:${server4.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: 'localhost',
    RESOLVE_TO_LOCALHOST: 'localhost.',
  });

  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.match(stdout, /Resolving lookup for localhost\./);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// Bracketed IPv6 URL hosts should match unbracketed NO_PROXY entries.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::1]:${server6.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: '::1',
  });

  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

// IPv4-mapped IPv6 loopback should match its IPv4 NO_PROXY entry.
{
  const { code, signal, stderr, stdout } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: `http://[::ffff:7f00:1]:${server4.address().port}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
    NO_PROXY: 'localhost,127.0.0.1,::1',
  });

  assert.match(stdout, /Status Code: 200/);
  assert.match(stdout, /Hello World/);
  assert.strictEqual(stderr.trim(), '');
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}

proxy.close();
server6.close();
server4.close();
