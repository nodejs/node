// This tests that proxied HTTP requests succeed end-to-end when the
// absolute-form request-target matches the request authority derived from options.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer } from '../common/proxy-server.js';

const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}, 6));
server.on('error', common.mustNotCall());
server.listen(0);
await once(server, 'listening');

const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const port = server.address().port;
const serverHost = `localhost:${port}`;
const requestUrl = `http://${serverHost}/test`;

const agent = new http.Agent({
  proxyEnv: {
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  },
});

async function roundTrip(options) {
  const req = http.request({ agent, ...options });
  req.end();
  const [res] = await once(req, 'response');
  res.setEncoding('utf8');
  let body = '';
  for await (const chunk of res) body += chunk;
  assert.strictEqual(body, 'Hello world');
}

const baseAbsolute = { host: 'localhost', port, path: requestUrl };

const options = [
  // No user-supplied headers.
  baseAbsolute,
  // Object form with an explicit Host that matches.
  { ...baseAbsolute, headers: { Host: serverHost } },
  // Flat array form.
  { ...baseAbsolute, headers: ['Host', serverHost] },
  // Array-of-pairs form.
  { ...baseAbsolute, headers: [['Host', serverHost]] },
  // Contains defaultPort that matches options.port.
  { host: 'localhost', port, defaultPort: port, path: '/test' },
  // Stringifiable non-string path object.
  { host: 'localhost', port, path: { toString() { return '/test'; } } },
];

for (const opts of options) {
  await roundTrip(opts);
  // Check what the proxy server received.
  const log = logs.pop();
  assert.strictEqual(logs.length, 0);
  assert.strictEqual(log.method, 'GET');
  assert.strictEqual(log.url, requestUrl);
  assert.strictEqual(log.headers.host, serverHost);
}

server.close();
proxy.close();
agent.destroy();
