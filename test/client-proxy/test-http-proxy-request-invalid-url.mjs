// This tests that invalid proxy URLs are handled correctly.
import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import http from 'node:http';
import { runProxiedRequest } from '../common/proxy-server.js';

// Start a target server that should not be reached.
const server = http.createServer(common.mustNotCall());
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;

// Test invalid proxy URL
const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTP_PROXY: 'not-a-valid-url',
});

// Should get an error about invalid URL
assert.match(stderr, /TypeError.*Invalid URL/);
assert.strictEqual(stdout.trim(), '');
assert.strictEqual(code, 1);
assert.strictEqual(signal, null);

server.close();
