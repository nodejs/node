// This tests that a scheme-less proxy value no longer crashes startup.
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

// Start a target server. The built-in http agent still ignores a scheme-less
// proxy value, so the request is sent directly.
const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req.url, '/test');
  res.end('Hello world');
}));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTP_PROXY: 'not-a-valid-url',
});

assert.strictEqual(stderr.trim(), '');
assert.match(stdout, /Status Code: 200/);
assert.match(stdout, /Hello world/);
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

server.close();
