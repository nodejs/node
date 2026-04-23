// Regression test for https://github.com/nodejs/node/issues/62904.
// This test drives that scenario by writing the CONNECT response one byte at
// a time with a short delay between writes, and asserts that no warning is
// ever emitted while the full HTTPS response is received correctly.

import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import http from 'node:http';
import net from 'node:net';
import { once } from 'events';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Target HTTPS server the client ultimately wants to reach.
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Proxy server that writes the CONNECT response one byte at a time. Using the
// built-in HTTP server avoids having to parse the CONNECT request ourselves
// (which cannot rely on a single TCP chunk containing the whole request line).
const proxy = http.createServer();
proxy.on('connect', common.mustCall(async (req, res, head) => {
  const { hostname, port } = new URL(`https://${req.url}`);
  const normalizedHost = hostname.startsWith('[') && hostname.endsWith(']') ?
    hostname.slice(1, -1) : hostname;

  const upstream = net.connect(Number(port), normalizedHost);
  upstream.on('error', () => res.destroy());
  res.on('error', () => upstream.destroy());
  await once(upstream, 'connect');

  const response = 'HTTP/1.1 200 Connection Established\r\n' +
                  'Proxy-agent: Node.js-Proxy\r\n' +
                  '\r\n';
  // Feed the response byte by byte with a macrotask boundary between writes
  // so the client sees many separate 'readable' events during tunnel setup.
  // This is what triggers the listener leak described in the issue.
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

// No warning should be emitted during the proxied request. The pre-fix code
// emits MaxListenersExceededWarning; any other warning here would also be
// unexpected and should fail the test.
process.on('warning',
           common.mustNotCall('unexpected warning during proxied request'));

const agent = new https.Agent({
  proxyEnv: {
    HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  },
  ca: fixtures.readKey('fake-startcom-root-cert.pem'),
});

const req = https.request({
  hostname: 'localhost',
  port: server.address().port,
  path: '/test',
  agent,
}, common.mustCall((res) => {
  let data = '';
  res.setEncoding('utf8');
  res.on('data', (chunk) => { data += chunk; });
  res.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'Hello world');
    assert.strictEqual(res.statusCode, 200);
    proxy.close();
    server.close();
  }));
}));
req.on('error', common.mustNotCall());
req.end();
