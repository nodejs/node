// This tests that when using a proxy with an agent with maxSockets: 1,
// subsequent requests are queued when the first request is still alive,
// and processed after the first request completes, and both are sending
// the request through the proxy.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import fixtures from '../common/fixtures.js';
import { createProxyServer } from '../common/proxy-server.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

let resolve;
const p = new Promise((r) => { resolve = r; });

// Start a server that delays responses to test queuing behavior
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  console.log('headers received for', req.url, req.headers);
  if (req.url === '/first') {
    // Simulate a long response for the first request
    p.then(() => {
      console.log('Responding to /first');
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('Response for /first');
    });
  } else if (req.url === '/second') {
    // Respond immediately for the second request
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('Response for /second');
  } else {
    assert.fail(`Unexpected request to ${req.url}`);
  }
}, 2));
server.on('error', common.mustNotCall((err) => { console.error('Server error', err); }));
server.listen(0);
await once(server, 'listening');

// Start a minimal proxy server
const { proxy, logs } = createProxyServer();
proxy.listen(0);
await once(proxy, 'listening');

const serverHost = `localhost:${server.address().port}`;
const proxyUrl = `http://localhost:${proxy.address().port}`;

// Create an agent with maxSockets: 1 and proxy support
const agent = new https.Agent({
  maxSockets: 1,
  proxyEnv: {
    HTTPS_PROXY: proxyUrl,
  },
  ca: fixtures.readKey('fake-startcom-root-cert.pem'),
});

const requestTimes = [];

// Make first request that takes longer
const firstReq = https.request({
  hostname: 'localhost',
  port: server.address().port,
  path: '/first',
  agent: agent,
}, common.mustCall((res) => {
  console.log('req1 response received');
  let data = '';
  res.on('data', (chunk) => {
    data += chunk;
  });
  res.on('end', common.mustCall(() => {
    console.log('req1 end');
    requestTimes[0] = { path: '/first', data, endTime: Date.now() };
    assert.strictEqual(data, 'Response for /first');
  }));
}));

firstReq.on('socket', common.mustCall((socket) => {
  console.log('req1 socket acquired');
  // Start second request when first request gets its socket
  // so that it will be queued.
  const secondReq = https.request({
    hostname: 'localhost',
    port: server.address().port,
    path: '/second',
    agent: agent,
  }, common.mustCall((res) => {
    let data = '';
    res.on('data', (chunk) => {
      data += chunk;
    });
    res.on('end', common.mustCall(() => {
      requestTimes[1] = { path: '/second', data, endTime: Date.now() };
      assert.strictEqual(data, 'Response for /second');

      // The two shares the same proxy connection.
      assert.deepStrictEqual(logs, [{
        method: 'CONNECT',
        url: serverHost,
        headers: { 'proxy-connection': 'keep-alive', 'host': serverHost },
      }]);
      proxy.close();
      server.close();
    }));
  }));

  secondReq.on('error', common.mustNotCall());
  firstReq.end();
  secondReq.end();
  resolve();  // Tell the server to respond to the first request
}));

firstReq.on('error', common.mustNotCall());
