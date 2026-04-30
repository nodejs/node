// Tests that http.setGlobalProxyFromEnv() without arguments uses process.env.

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedRequest } from '../common/proxy-server.js';

const { proxyLogs, proxyUrl, shutdown, httpEndpoint: { serverHost, requestUrl } } = await startTestServers({
  httpEndpoint: true,
});

// Test that calling setGlobalProxyFromEnv() without arguments uses process.env
await checkProxiedRequest({
  REQUEST_URL: requestUrl,
  // Set the proxy in the environment instead of passing it to SET_GLOBAL_PROXY
  http_proxy: proxyUrl,
  SET_GLOBAL_PROXY_DEFAULT: '1',  // Signal to call without arguments
}, {
  stdout: 'Hello world',
});

shutdown();

const expectedLogs = [{
  method: 'GET',
  url: requestUrl,
  headers: {
    'connection': 'keep-alive',
    'proxy-connection': 'keep-alive',
    'host': serverHost,
  },
}];

assert.deepStrictEqual(proxyLogs, expectedLogs);
