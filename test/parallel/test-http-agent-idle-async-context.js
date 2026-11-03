'use strict';

const common = require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { once } = require('node:events');
const http = require('node:http');
const { setImmediate } = require('node:timers/promises');

// This test ensures that a keep-alive socket runs its callbacks in the context
// of the request currently using it, and in no request context while it is
// idle in the agent pool. Other work started by a request must keep its
// context after the socket is released.
// Refs: https://github.com/nodejs/node/issues/67181

const storage = new AsyncLocalStorage();
const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 1,
  timeout: common.platformTimeout(1000),
});
const server = http.createServer(common.mustCall((req, res) => {
  if (req.url !== '/stall') res.end('ok');
}, 3));
server.keepAliveTimeout = 60_000;

function request(path = '/') {
  return http.get({ agent, port: server.address().port, path });
}

function completedRequest(expectedStore) {
  return new Promise((resolve, reject) => {
    request().on('response', common.mustCall((res) => {
      assert.strictEqual(storage.getStore(), expectedStore);
      const socket = res.socket;
      const asyncId = socket._handle.getAsyncId();
      res.on('end', common.mustCall(() => resolve({ socket, asyncId })));
      res.resume();
    })).on('error', reject);
  });
}

function assertPooled({ socket, asyncId }) {
  assert.deepStrictEqual(Object.values(agent.freeSockets).flat(), [socket]);
  // Entering the pool must not reset the handle's async identity.
  assert.strictEqual(socket._handle.getAsyncId(), asyncId);
}

async function main() {
  server.listen(0);
  await once(server, 'listening');

  // The socket and independent work must capture the same context frame.
  const firstStore = { requestId: 'first' };
  let releaseBackgroundWork;
  let backgroundWork;
  const created = await storage.run(firstStore, common.mustCall(() => {
    backgroundWork = new Promise((resolve) => {
      releaseBackgroundWork = resolve;
    }).then(common.mustCall(() => {
      assert.strictEqual(storage.getStore(), firstStore);
    }));
    return completedRequest(firstStore);
  }));
  const { socket } = created;
  await setImmediate();
  assertPooled(created);

  // A request that reuses the socket must observe its own context in the
  // socket timeout.
  const secondStore = { requestId: 'second' };
  await storage.run(secondStore, common.mustCall(() => new Promise((resolve) => {
    const req = request('/stall');
    req.on('socket', common.mustCall((reused) => {
      assert.strictEqual(reused, socket);
    }));
    req.on('timeout', common.mustCall(() => {
      assert.strictEqual(storage.getStore(), secondStore);
      req.destroy();
    }));
    req.on('error', common.expectsError({ code: 'ECONNRESET' }));
    req.on('close', common.mustCall(resolve));
  })));

  // Releasing the socket must not alter the context of other work.
  releaseBackgroundWork();
  await backgroundWork;

  // An idle socket times out and is closed by the agent outside of the
  // context of the request that created it.
  const thirdStore = { requestId: 'third' };
  const idle = await storage.run(thirdStore,
                                 common.mustCall(() => completedRequest(thirdStore)));
  const idleSocket = idle.socket;
  await setImmediate();
  assertPooled(idle);
  idleSocket.on('timeout', common.mustCall(() => {
    assert.strictEqual(storage.getStore(), undefined);
  }));
  idleSocket.on('close', common.mustCall(() => {
    assert.strictEqual(storage.getStore(), undefined);
  }));
  await once(idleSocket, 'close');

  server.close();
}

main().then(common.mustCall());
