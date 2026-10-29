// This tests that when the proxy server connection is refused, the client can
// handle it correctly.

import * as common from '../common/index.mjs';
import http from 'node:http';
import assert from 'node:assert';
import { once } from 'events';
import { runProxiedRequest } from '../common/proxy-server.js';

const server = http.createServer(common.mustNotCall());
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

const serverHost = `localhost:${server.address().port}`;
const requestUrl = `http://${serverHost}/test`;

let foundRefused = false;
const port = 10;
console.log(`Trying proxy at port ${port}`);
const { stderr } = await runProxiedRequest({
  NODE_USE_ENV_PROXY: 1,
  REQUEST_URL: requestUrl,
  HTTP_PROXY: `http://127.0.0.1:${port}`,
  NO_PROXY: '',
  no_proxy: '',
  REQUEST_TIMEOUT: 5000,
});

foundRefused = /Error.*connect ECONNREFUSED/.test(stderr);

server.close();
assert(foundRefused, 'Expected ECONNREFUSED error from proxy request');
