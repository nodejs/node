// Tests that http.setGlobalProxyFromEnv() works with no_proxy configuration.

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedRequest } from '../common/proxy-server.js';
const { proxyLogs, shutdown, proxyUrl, httpEndpoint: { requestUrl } } = await startTestServers({
  httpEndpoint: true,
});

await checkProxiedRequest({
  REQUEST_URL: requestUrl,
  SET_GLOBAL_PROXY: JSON.stringify({
    http_proxy: proxyUrl,
    no_proxy: 'localhost',
  }),
}, {
  stdout: 'Hello world',
});

shutdown();

// Verify request did NOT go through proxy.
assert.deepStrictEqual(proxyLogs, []);
