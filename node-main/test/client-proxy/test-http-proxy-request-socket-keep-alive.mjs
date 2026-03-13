// This tests that when using a proxy with an agent with maxSockets: 1 and
// keepAlive: true, after the first request finishes, a subsequent request
// reuses the same socket connection.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import http from 'node:http';
import { once } from 'events';
import { createProxyServer } from '../common/proxy-server.js';

// Start a server to handle requests
const server = http.createServer(common.mustCall((req, res) => {
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
const agent = new http.Agent({
  maxSockets: 1,
  keepAlive: true,
  proxyEnv: {
    HTTP_PROXY: proxyUrl,
  },
});

// Make first request
const firstReq = http.request({
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
    assert.strictEqual(agent.freeSockets[agent.getName(options)].length, 1);
    // Send second request when first request closes (socket returned to pool)
    const secondReq = http.request(options, common.mustCall((res) => {
      let data = '';
      res.on('data', (chunk) => data += chunk);
      res.on('end', common.mustCall(() => {
        assert.strictEqual(data, `Response for /second`);

        // Verify both requests went through the proxy
        assert.deepStrictEqual(logs, [
          {
            method: 'GET',
            url: `http://${serverHost}/first`,
            headers: {
              'host': serverHost,
              'proxy-connection': 'keep-alive',
              'connection': 'keep-alive',
            },
          },
          {
            method: 'GET',
            url: `http://${serverHost}/second`,
            headers: {
              'host': serverHost,
              'proxy-connection': 'keep-alive',
              'connection': 'keep-alive',
            },
          },
        ]);

        proxy.close();
        server.close();
      }));
    }));

    secondReq.on('error', common.mustNotCall());
    secondReq.end();
  });
}));
