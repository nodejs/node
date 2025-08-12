// This tests that when using a proxy with an agent with maxSockets: 1 and
// keepAlive: true, after the first request finishes, a subsequent request
// reuses the same socket connection.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { once } from 'events';
import { createProxyServer } from '../common/proxy-server.js';
import fixtures from '../common/fixtures.js';

if (!common.hasCrypto)
  common.skip('missing crypto');

// https must be dynamically imported so that builds without crypto support
// can skip it.
const { default: https } = await import('node:https');

// Start a server to handle requests
const server = https.createServer({
  cert: fixtures.readKey('agent8-cert.pem'),
  key: fixtures.readKey('agent8-key.pem'),
}, common.mustCall((req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(`Response for ${req.url}`);
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

// Create an agent with maxSockets: 1, keepAlive: true, and proxy support
const agent = new https.Agent({
  maxSockets: 1,
  keepAlive: true,
  proxyEnv: {
    HTTPS_PROXY: proxyUrl,
  },
  ca: fixtures.readKey('fake-startcom-root-cert.pem'),
});

// Make first request
const firstReq = https.request({
  hostname: 'localhost',
  port: server.address().port,
  path: '/first',
  agent: agent,
}, common.mustCall((res) => {
  let data = '';
  res.on('data', (chunk) => data += chunk);
  res.on('end', common.mustCall(() => {
    assert.strictEqual(data, `Response for /first`);
  }));
}));
firstReq.on('error', common.mustNotCall());
firstReq.end();

agent.once('free', common.mustCall((socket) => {
  // At this point, the first request has completed and the socket is returned
  // to the pool.
  process.nextTick(() => {
    const options = {
      hostname: 'localhost',
      port: server.address().port,
      path: '/second',
      agent: agent,
    };
    // Check that the socket is still in the pool.
    // TODO(joyeecheung): we don't currently have a way to set the root certificate
    // dynamically and have to use the ca: option which affects name computation.
    // So we cannot check the pool here until we have that feature.
    // See https://github.com/nodejs/node/pull/58822
    // Send second request when first request closes (socket returned to pool)
    const secondReq = https.request(options, common.mustCall((res) => {
      let data = '';
      res.on('data', (chunk) => data += chunk);
      res.on('end', common.mustCall(() => {
        assert.strictEqual(data, `Response for /second`);

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
    secondReq.end();
  });
}));
