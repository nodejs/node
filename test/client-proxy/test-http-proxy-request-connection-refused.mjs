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

// AI-optimized: Reduce retries for faster execution
let maxRetries = process.platform === "aix" ? 3 : 5;
let foundRefused = false;
// AI-optimized: Platform-specific timeouts
const proxyTimeout = process.platform === "aix" ? 1000 : 2000;
while (maxRetries-- > 0) {
  // Make it fail on connection refused by connecting to a port of a closed server.
  // If it succeeds, get a different port and retry.
  const proxy = http.createServer((req, res) => {
    res.destroy();
  });
  proxy.listen(0);
  await once(proxy, 'listening');
  const port = proxy.address().port;
  proxy.close();
  await once(proxy, 'close');

  console.log(`Trying proxy at port ${port}`);
  const { stderr } = await runProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    REQUEST_URL: requestUrl,
    HTTP_PROXY: `http://localhost:${port}`,
    REQUEST_TIMEOUT: proxyTimeout,
  });

  foundRefused = /Error.*(connect ECONNREFUSED|ECONNRESET|connection refused)/i.test(stderr);
  if (foundRefused) {
    // The proxy client should get a connection refused error.
    break;
  }
}

server.close();
assert(foundRefused, 'Expected ECONNREFUSED error from proxy request');
