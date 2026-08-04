// Tests that http.setGlobalProxyFromEnv() is a no-op when no proxy is configured.

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedRequest } from '../common/proxy-server.js';
const { proxyLogs, shutdown, httpEndpoint: { requestUrl } } = await startTestServers({
  httpEndpoint: true,
});

await checkProxiedRequest({
  REQUEST_URL: requestUrl,
  SET_GLOBAL_PROXY: JSON.stringify({}),
}, {
  stdout: 'Hello world',
});

shutdown();

// Verify request did NOT go through proxy.
assert.deepStrictEqual(proxyLogs, []);
