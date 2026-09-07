// This tests that the proxy server cannot send unbounded incomplete headers
// while establishing a CONNECT tunnel.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

const proxy = http.createServer();
proxy.on('connect', common.mustCall((req, socket) => {
  socket.write('HTTP/1.1 200 Connection Established\r\n');

  const interval = setInterval(() => {
    if (socket.destroyed) {
      clearInterval(interval);
      return;
    }
    socket.write('x'.repeat(100));
  }, 10);
  socket.on('close', () => clearInterval(interval));
  socket.on('error', () => clearInterval(interval));
}, 1));
proxy.listen(0);
await once(proxy, 'listening');

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: 'https://localhost:1/test',
  HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
}, ['--max-http-header-size=1024']);

assert.match(
  stderr,
  /ERR_PROXY_TUNNEL.*Proxy response headers exceeded 1024 bytes/,
);
assert.doesNotMatch(stderr, /MaxListenersExceededWarning/);
assert.strictEqual(stdout.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

proxy.close();
