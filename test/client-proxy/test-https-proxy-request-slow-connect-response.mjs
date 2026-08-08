// Regression test for https://github.com/nodejs/node/issues/62904.
// Drives an HTTPS request through a proxy that writes the CONNECT response
// one byte at a time. The pre-fix code re-attached the `'readable'` listener
// inside the read function on every chunk, doubling the listener count and
// tripping MaxListenersExceededWarning. The request runs in a subprocess so
// the warning lands in its stderr where we can assert against it.

import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import net from 'node:net';
import { runProxiedRequest } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Proxy that writes the CONNECT response byte-by-byte, forcing many separate
// 'readable' events on the client.
const proxy = http.createServer();
proxy.on('connect', common.mustCall(async (req, res, head) => {
  const { hostname, port } = new URL(`https://${req.url}`);
  const normalizedHost = hostname.startsWith('[') && hostname.endsWith(']') ?
    hostname.slice(1, -1) : hostname;

  const upstream = net.connect(Number(port), normalizedHost);
  upstream.once('error', () => res.destroy());
  res.once('error', () => upstream.destroy());
  try {
    await once(upstream, 'connect');
  } catch {
    res.destroy();
    return;
  }

  const response = 'HTTP/1.1 200 Connection Established\r\n' +
                  'Proxy-agent: Node.js-Proxy\r\n' +
                  '\r\n';
  for (let i = 0; i < response.length; i++) {
    if (res.destroyed) {
      upstream.destroy();
      return;
    }
    res.write(response[i]);
    await new Promise((resolve) => setImmediate(resolve));
  }
  if (head?.length) upstream.write(head);
  res.pipe(upstream);
  upstream.pipe(res);
}, 1));
proxy.on('error', common.mustNotCall((err) => { console.error('Proxy error', err); }));
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `https://${serverHost}/test`;
const proxyUrl = `http://localhost:${proxy.address().port}`;

// Pin every proxy env var so an ambient shell setting (lower-case `https_proxy`
// takes precedence over upper-case in lib/internal/http.js, and `no_proxy=*`
// would bypass the proxy entirely) cannot distort the subprocess.
const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTPS_PROXY: proxyUrl,
  https_proxy: proxyUrl,
  NO_PROXY: '',
  no_proxy: '',
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

assert.strictEqual(stderr.trim(), '');
assert.match(stdout, /Hello world/);
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

proxy.close();
server.close();
