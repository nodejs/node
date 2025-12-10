// Tests that http.setGlobalProxyFromEnv() overrides configuration from NODE_USE_ENV_PROXY.

import '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'node:events';
import { startTestServers, checkProxiedFetch, createProxyServer } from '../common/proxy-server.js';
const { proxyLogs, shutdown, proxyUrl, httpEndpoint: { serverHost, requestUrl } } = await startTestServers({
  httpEndpoint: true,
});
const { proxy: proxy2, logs: proxyLogs2 } = createProxyServer();
proxy2.listen(0);
await once(proxy2, 'listening');

await checkProxiedFetch({
  FETCH_URL: requestUrl,
  NODE_USE_ENV_PROXY: '1',
  http_proxy: proxyUrl,
  SET_GLOBAL_PROXY: JSON.stringify({
    http_proxy: `http://localhost:${proxy2.address().port}`,
  }),
}, {
  stdout: 'Hello world',
});

shutdown();
proxy2.close();

// Verify request did NOT go through original proxy, but the overriden one.
assert.deepStrictEqual(proxyLogs, []);

// FIXME(undici:4083): undici currently always tunnels the request over
// CONNECT if proxyTunnel is not explicitly set to false, but what we
// need is for it to be automatically false for HTTP requests to be
// consistent with curl.
const expectedLogs = [{
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'connection': 'close',
    'host': serverHost,
    'proxy-connection': 'keep-alive',
  },
}];
assert.deepStrictEqual(proxyLogs2, expectedLogs);
