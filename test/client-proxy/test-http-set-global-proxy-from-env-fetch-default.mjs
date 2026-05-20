// Tests that http.setGlobalProxyFromEnv() without arguments uses process.env.

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedFetch } from '../common/proxy-server.js';

const { proxyLogs, proxyUrl, shutdown, httpEndpoint: { serverHost, requestUrl } } = await startTestServers({
  httpEndpoint: true,
});

// Test that calling setGlobalProxyFromEnv() without arguments uses process.env
await checkProxiedFetch({
  FETCH_URL: requestUrl,
  // Set the proxy in the environment instead of passing it to SET_GLOBAL_PROXY
  http_proxy: proxyUrl,
  SET_GLOBAL_PROXY_DEFAULT: '1',  // Signal to call without arguments
}, {
  stdout: 'Hello world',
});

shutdown();

// FIXME(undici:4083): undici currently always tunnels the request over
// CONNECT if proxyTunnel is not explicitly set to false.
const expectedLogs = [{
  method: 'CONNECT',
  url: serverHost,
  headers: {
    'connection': 'close',
    'host': serverHost,
    'proxy-connection': 'keep-alive',
  },
}];

assert.deepStrictEqual(proxyLogs, expectedLogs);
