import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { createProxyServer, runProxiedPOST } from '../common/proxy-server.js';

// Start a server that creates resources
const resources = [];
const server = http.createServer(common.mustCall((req, res) => {
  let body = '';
  req.on('data', (chunk) => {
    body += chunk;
  });
  req.on('end', () => {
    const resource = JSON.parse(body);
    resource.id = resources.length + 1;
    resources.push(resource);

    res.writeHead(201, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(resource));
  });
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server.
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/resources`;
const resourceData = JSON.stringify({ name: 'test-resource', value: 'some-value' });

const { code, signal, stderr, stdout } = await runProxiedPOST({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  RESOURCE_DATA: resourceData,
  HTTP_PROXY: `http://localhost:${proxy.address().port}`,
});

assert.strictEqual(code, 0);
assert.strictEqual(signal, null);
assert.strictEqual(stderr.trim(), '');
assert.match(stdout, /Status Code: 201/);

// Verify the resource was created
const jsonMatch = stdout.match(/{"[^}]*}$/);
assert(jsonMatch, 'Should have JSON response');
const response = JSON.parse(jsonMatch[0]);
assert.strictEqual(response.name, 'test-resource');
assert.strictEqual(response.id, 1);

// Verify proxy logged the POST request
assert.strictEqual(logs.length, 1);
assert.strictEqual(logs[0].method, 'POST');
assert.strictEqual(logs[0].url, requestUrl);
assert.strictEqual(logs[0].headers['content-type'], 'application/json');

proxy.close();
server.close();
