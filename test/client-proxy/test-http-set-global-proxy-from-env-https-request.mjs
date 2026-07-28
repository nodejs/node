// Tests that http.setGlobalProxyFromEnv() works with https.request().

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
import { startTestServers, checkProxiedRequest } from '../common/proxy-server.js';

if (!common.hasCrypto) {
  common.skip('Needs crypto');
}

const { proxyLogs, httpsEndpoint: { requestUrl, serverHost }, proxyUrl, shutdown } = await startTestServers({
  httpsEndpoint: true,
});

await checkProxiedRequest({
  REQUEST_URL: requestUrl,
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
    'proxy-connection': 'keep-alive',
    'host': serverHost,
  },
}];

assert.deepStrictEqual(proxyLogs, expectedLogs);
