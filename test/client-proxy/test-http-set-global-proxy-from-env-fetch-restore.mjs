// Tests that http.setGlobalProxyFromEnv() works with fetch() and https.

import '../common/index.mjs';
import assert from 'node:assert';
import { startTestServers, checkProxiedFetch } from '../common/proxy-server.js';

const { proxyLogs, proxyUrl, shutdown, httpEndpoint: { serverHost, requestUrl } } = await startTestServers({
  httpEndpoint: true,
});

await checkProxiedFetch({
  FETCH_URL: requestUrl,
  SET_GLOBAL_PROXY: JSON.stringify({
    http_proxy: proxyUrl,
  }),
  CLEAR_GLOBAL_PROXY_AND_RETRY: '1',
}, {
  stdout: 'Hello world\nHello world',
});

shutdown();

const expectedLogs = [{
  method: 'GET',
  url: requestUrl,
  headers: {
    'host': serverHost,
    'connection': 'keep-alive',
    'accept': '*/*',
    'accept-language': '*',
    'sec-fetch-mode': 'cors',
    'user-agent': 'node',
    'accept-encoding': 'gzip, deflate',
  },
}];

assert.deepStrictEqual(proxyLogs, expectedLogs);
