// This tests that when the proxy server connection is refused, the client can
// handle it correctly.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { runProxiedRequest } from '../common/proxy-server.js';
import dgram from 'node:dgram';

const server = http.createServer(common.mustNotCall());
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;

// Make it fail on connection refused by connecting to a UDP port with TCP.
const udp = dgram.createSocket('udp4');
udp.bind(0, '127.0.0.1');
await once(udp, 'listening');

const port = udp.address().port;

const { code, signal, stderr, stdout } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTP_PROXY: `http://localhost:${port}`,
});

// The proxy client should get a connection refused error.
assert.match(stderr, /Error.*connect ECONNREFUSED/);
assert.strictEqual(stdout.trim(), '');
assert.strictEqual(code, 0);
assert.strictEqual(signal, null);

server.close();
udp.close();
