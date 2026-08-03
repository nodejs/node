import * as common from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer, runProxiedPOST } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Start a HTTPS server that creates resources
const resources = [];
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  let body = '';
  req.on('data', (chunk) => {
    body += chunk;
  });
  req.on('end', () => {
    const resource = JSON.parse(body);
    resource.id = resources.length + 1;
    resource.secure = true;
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
const requestUrl = `https://${serverHost}/secure-resources`;
const resourceData = JSON.stringify({ name: 'secure-resource', confidential: true });

const { code, signal, stderr, stdout } = await runProxiedPOST({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  RESOURCE_DATA: resourceData,
  HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
});

assert.strictEqual(code, 0);
assert.strictEqual(signal, null);
assert.strictEqual(stderr.trim(), '');
assert.match(stdout, /Status Code: 201/);

// Verify the resource was created securely
const jsonMatch = stdout.match(/{[^}]*}$/);
assert(jsonMatch, 'Should have JSON response');
const response = JSON.parse(jsonMatch[0]);
assert.strictEqual(response.name, 'secure-resource');
assert.strictEqual(response.secure, true);
assert.strictEqual(response.id, 1);

// Verify proxy logged the CONNECT request (for HTTPS tunneling)
assert.strictEqual(logs.length, 1);
assert.strictEqual(logs[0].method, 'CONNECT');
assert.strictEqual(logs[0].url, serverHost);

proxy.close();
server.close();
