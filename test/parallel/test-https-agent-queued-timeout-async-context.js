'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { once } = require('node:events');
const https = require('node:https');

// A socket handed directly to a queued request must use that request's
// context for timeout, error, and close, including the parent TCP under TLS.
async function main() {
  const storage = new AsyncLocalStorage();
  const timeout = common.platformTimeout(1000);
  const agent = new https.Agent({
    keepAlive: true,
    maxSockets: 1,
    timeout,
    ca: fixtures.readKey('ca1-cert.pem'),
    servername: 'agent1',
  });
  let heldResponse;
  let firstReceived;
  const received = new Promise((resolve) => { firstReceived = resolve; });
  const respond = common.mustCall((req, res) => {
    if (req.url === '/hold') {
      heldResponse = res;
      firstReceived();
    } else {
      assert.strictEqual(req.url, '/stall');
    }
  }, 2);
  const server = https.createServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  }, respond);
  const options = { agent, host: '127.0.0.1' };

  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  options.port = server.address().port;

  try {
    let socket;
    const firstStore = { requestId: 'first' };
    const first = storage.run(firstStore, common.mustCall(() =>
      new Promise((resolve, reject) => {
        const onResponse = common.mustCall((res) => {
          assert.strictEqual(storage.getStore(), firstStore);
          res.on('error', reject);
          res.on('end', common.mustCall(resolve));
          res.resume();
        });
        const req = https.get({ ...options, path: '/hold' }, onResponse);
        req.on('socket', common.mustCall((assigned) => { socket = assigned; }));
        req.on('error', reject);
      })));
    await received;

    const secondStore = { requestId: 'queued' };
    const error = new Error('test timeout');
    const second = storage.run(secondStore, common.mustCall(() =>
      new Promise((resolve) => {
        const req = https.get({ ...options, path: '/stall' },
                              common.mustNotCall('The response must stall'));
        req.on('socket', common.mustCall((assigned) => {
          assert.strictEqual(assigned, socket);
          assert.strictEqual(storage.getStore(), secondStore);
        }));
        const onTimeout = common.mustCall(() => {
          assert.strictEqual(storage.getStore(), secondStore);
          req.destroy(error);
        });
        req.setTimeout(timeout, onTimeout);
        req.on('error', common.mustCall((received) => {
          assert.strictEqual(received, error);
          assert.strictEqual(storage.getStore(), secondStore);
        }));
        req.on('close', common.mustCall(() => {
          assert.strictEqual(storage.getStore(), secondStore);
          resolve();
        }));
        assert.strictEqual(req.socket, null);
      })));
    assert.strictEqual(Object.values(agent.requests).flat().length, 1);
    heldResponse.end('ok');
    await Promise.all([first, second]);
  } finally {
    agent.destroy();
    server.closeAllConnections();
    await new Promise((resolve) => server.close(resolve));
  }
}

main().then(common.mustCall());
