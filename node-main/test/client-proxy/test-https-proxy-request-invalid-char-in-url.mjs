// This tests that invalid hosts or ports with carriage return or newline characters
// in HTTPS request urls are stripped away before being sent to the server.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import fixtures from '../common/fixtures.js';
import { once } from 'events';
import { inspect } from 'node:util';
import { createProxyServer } from '../common/proxy-server.js';

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');
const requests = new Set();

const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, (req, res) => {
  console.log(`[Upstream server] responding to request for ${inspect(req.url)}`);
  requests.add(`https://localhost:${server.address().port}${req.url}`);
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(`Response for ${inspect(req.url)}`);
});

server.listen(0);
await once(server, 'listening');
server.on('error', common.mustNotCall());
const port = server.address().port.toString();

const testCases = [
  { host: 'local\rhost', port: port, path: '/carriage-return-in-host' },
  { host: 'local\nhost', port: port, path: '/newline-in-host' },
  { host: 'local\r\nhost', port: port, path: '/crlf-in-host' },
  { host: 'localhost', port: port.substring(0, 1) + '\r' + port.substring(1), path: '/carriage-return-in-port' },
  { host: 'localhost', port: port.substring(0, 1) + '\n' + port.substring(1), path: '/newline-in-port' },
  { host: 'localhost', port: port.substring(0, 1) + '\r\n' + port.substring(1), path: '/crlf-in-port' },
];

// Start a minimal proxy server
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

https.globalAgent = new https.Agent({
  ca: fixtures.readKey('fake-startcom-root-cert.pem'),
  proxyEnv: {
    HTTPS_PROXY: `http://localhost:${proxy.address().port}`,
  },
});

const severHost = `localhost:${server.address().port}`;

let counter = 0;
const expectedUrls = new Set();
const expectedProxyLogs = new Set();
for (const testCase of testCases) {
  const url = `https://${testCase.host}:${testCase.port}${testCase.path}`;
  // The invalid characters should all be stripped away before being sent.
  expectedUrls.add(url.replaceAll(/\r|\n/g, ''));
  expectedProxyLogs.add({
    method: 'CONNECT',
    url: severHost,
    headers: { host: severHost },
  });
  https.request(url, common.mustCall((res) => {
    res.on('error', common.mustNotCall());
    res.setEncoding('utf8');
    res.on('data', (data) => {
      console.log(`[Proxy client] Received response from server for ${inspect(url)}: ${data.toString()}`);
    });
    res.on('close', common.mustCall(() => {
      console.log(`[Proxy client] #${++counter} closed request for: ${inspect(url)}`);
      // Finished all test cases.
      if (counter === testCases.length) {
        setImmediate(common.mustCall(() => {
          console.log('All requests completed, shutting down.');
          proxy.close();
          server.close();
          assert.deepStrictEqual(requests, expectedUrls);
          const logSet = new Set(logs);
          for (const log of logSet) {
            if (log.source === 'proxy connect' && log.error?.code === 'EPIPE') {
              // There can be a race from eagerly shutting down the servers and severing
              // two pipes at the same time but for the purpose of this test, we only
              // care about whether the requests are initiated from the client as expected,
              // not how the upstream/proxy servers behave. Ignore EPIPE errors from them..
              // Refs: https://github.com/nodejs/node/issues/59741
              console.log('Ignoring EPIPE error from proxy connect', log.error);
              logSet.delete(log);
            }
          }
          assert.deepStrictEqual(logSet, expectedProxyLogs);
        }));
      }
    }));
  })).on('error', common.mustNotCall()).end();
}
