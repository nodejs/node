// Tests that http.setGlobalProxyFromEnv() works with fetch().

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
import { startTestServers, checkProxiedFetch } from '../common/proxy-server.js';

if (!common.hasCrypto) {
  common.skip('Needs crypto');
}

const { proxyLogs, proxyUrl, shutdown, httpsEndpoint: { serverHost, requestUrl } } = await startTestServers({
  httpsEndpoint: true,
});

await checkProxiedFetch({
  FETCH_URL: requestUrl,
  SET_GLOBAL_PROXY: JSON.stringify({
    https_proxy: proxyUrl,
  }),
  NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
}, {
  stdout: 'Hello world',
});

shutdown();

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
