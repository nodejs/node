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
