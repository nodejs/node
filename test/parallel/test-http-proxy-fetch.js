'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const http = require('http');
const { createProxyServer, checkProxiedRequest } = require('../common/proxy-server');

(async () => {
  // Start a server to process the final request.
  const server = http.createServer(common.mustCall((req, res) => {
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

  // FIXME(undici:4083): undici currently always tunnels the request over
  // CONNECT, no matter it's HTTP traffic or not, which is different from e.g.
  // how curl behaves.
  const expectedLogs = [{
    method: 'CONNECT',
    url: serverHost,
    headers: {
      // FIXME(undici:4086): this should be keep-alive.
      connection: 'close',
      host: serverHost
    }
  }];

  // Check upper-cased HTTPS_PROXY environment variable.
  await checkProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    FETCH_URL: `http://${serverHost}/test`,
    HTTP_PROXY: `http://localhost:${proxy.address().port}`,
  }, {
    stdout: 'Hello world',
  });
  assert.deepStrictEqual(logs, expectedLogs);

  // Check lower-cased https_proxy environment variable.
  logs.splice(0, logs.length);
  await checkProxiedRequest({
    NODE_USE_ENV_PROXY: 1,
    FETCH_URL: `http://${serverHost}/test`,
    http_proxy: `http://localhost:${proxy.address().port}`,
  }, {
    stdout: 'Hello world',
  });
  assert.deepStrictEqual(logs, expectedLogs);

  proxy.close();
  server.close();
})().then(common.mustCall());
