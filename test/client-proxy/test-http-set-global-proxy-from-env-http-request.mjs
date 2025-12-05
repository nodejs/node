// Tests that http.setGlobalProxyFromEnv() works with http.request().

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedRequest } from '../common/proxy-server.js';
const { proxyLogs, shutdown, proxyUrl, httpEndpoint: { requestUrl, serverHost } } = await startTestServers({
  httpEndpoint: true,
});

await checkProxiedRequest({
  REQUEST_URL: requestUrl,
  SET_GLOBAL_PROXY: JSON.stringify({ http_proxy: proxyUrl }),
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
