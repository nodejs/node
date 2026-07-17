// Tests that http.setGlobalProxyFromEnv() works with fetch() and https.

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedRequest } from '../common/proxy-server.js';

const { proxyLogs, proxyUrl, shutdown, httpEndpoint: { serverHost, requestUrl } } = await startTestServers({
  httpEndpoint: true,
});

await checkProxiedRequest({
  REQUEST_URL: requestUrl,
  SET_GLOBAL_PROXY: JSON.stringify({
    http_proxy: proxyUrl,
  }),
  CLEAR_GLOBAL_PROXY_AND_RETRY: '1',
}, {
  stdout: 'Hello worldHello world',
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
