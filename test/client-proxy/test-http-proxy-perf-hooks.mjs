// This tests that when a request path is rewritten to absolute-form for
// proxying, the perf_hooks HTTP entries report it as the URL as-is, instead
// of appending it to the protocol and authority again.
// Refs: https://github.com/nodejs/node/issues/59625
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { PerformanceObserver } from 'node:perf_hooks';
import { createProxyServer } from '../common/proxy-server.js';

const entries = [];
const obs = new PerformanceObserver(common.mustCallAtLeast((items) => {
  entries.push(...items.getEntries());
}));
obs.observe({ type: 'http' });

// Start a server to process the final request.
const server = http.createServer(common.mustCall((req, res) => {
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const requestUrl = `http://localhost:${server.address().port}/test`;
const agent = new http.Agent({
  proxyEnv: {
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  },
});

const res = await new Promise((resolve, reject) => {
  http.request(requestUrl, { agent }, resolve).on('error', reject).end();
});
res.resume();
await once(res, 'end');

// Verify that the request went through the proxy.
assert.strictEqual(logs.length, 1);
assert.strictEqual(logs[0].url, requestUrl);

proxy.close();
server.close();

process.on('exit', () => {
  // Two HttpClient entries are expected: one for the proxied request, one for
  // the request the proxy makes to forward it. Both should report the full
  // URL, including the port.
  const clientUrls = entries.filter((entry) => entry.name === 'HttpClient')
    .map((entry) => entry.detail.req.url);
  assert.deepStrictEqual(clientUrls, [requestUrl, requestUrl]);
  // Two HttpRequest entries are expected: the proxy server receives the
  // request target in absolute-form, the final server in origin-form.
  const requestUrls = entries.filter((entry) => entry.name === 'HttpRequest')
    .map((entry) => entry.detail.req.url).sort();
  assert.deepStrictEqual(requestUrls, ['/test', requestUrl].sort());
});
