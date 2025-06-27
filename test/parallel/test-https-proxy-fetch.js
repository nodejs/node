'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const https = require('https');
const { once } = require('events');
const { createProxyServer, checkProxiedRequest } = require('../common/proxy-server');

(async () => {
  // Start a server to process the final request.
  const server = https.createServer({
    cert: fixtures.readKey('agent8-cert.pem'),
    key: fixtures.readKey('agent8-key.pem'),
  }, common.mustCall((req, res) => {
    res.end('Hello world');
  }, 2));
  server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
  server.listen(0);
  await once(server, 'listening');

  // Start a minimal proxy server.
  const { proxy, logs } = createProxyServer();
  proxy.listen(0);
  await once(proxy, 'listening');

  const serverHost = `localhost:${server.address().port}`;

  const expectedLogs = [{
    method: 'CONNECT',
    url: serverHost,
    headers: {
      connection: 'close',
      host: serverHost,
      'proxy-connection': 'keep-alive'
    }
  }];

  // Check upper-cased HTTPS_PROXY environment variable.
  await checkProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    FETCH_URL: `https://${serverHost}/test`,
    HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
  }, {
    stdout: 'Hello world',
  });
  assert.deepStrictEqual(logs, expectedLogs);

  // Check lower-cased https_proxy environment variable.
  logs.splice(0, logs.length);
  await checkProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    FETCH_URL: `https://${serverHost}/test`,
    https_proxy: `http://localhost:${proxy.address().port}`,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'fake-startcom-root-cert.pem'),
  }, {
    stdout: 'Hello world',
  });
  assert.deepStrictEqual(logs, expectedLogs);

  proxy.close();
  server.close();
})().then(common.mustCall());
