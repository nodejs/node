// Flags: --expose-gc
'use strict';

const common = require('../common');
const { gcUntil } = require('../common/gc');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { once } = require('node:events');
const http = require('node:http');

// Idle keep-alive sockets must release request contexts held by their
// handles and timeout timers.
// Refs: https://github.com/nodejs/node/issues/67181

const storage = new AsyncLocalStorage();
const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 1,
  timeout: 60_000,
});
const server = http.createServer(common.mustCall((req, res) => {
  res.end('ok');
}, 2));
server.keepAliveTimeout = 60_000;

function requestInContext() {
  const store = {};
  const reference = new WeakRef(store);
  return storage.run(store, common.mustCall(() => new Promise((resolve, reject) => {
    // Avoid initializing the default DNS resolver inside ALS: it can retain
    // the request context independently of the Agent.
    http.get({
      agent,
      host: '127.0.0.1',
      port: server.address().port,
    }, common.mustCall((res) => {
      const socket = res.socket;
      res.on('end', common.mustCall(() => resolve({ reference, socket })));
      res.resume();
    })).on('error', reject);
  })));
}

function assertPooled(socket) {
  assert.deepStrictEqual(Object.values(agent.freeSockets).flat(), [socket]);
}

async function main() {
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');

  const created = await requestInContext();
  const reused = await requestInContext();
  assert.strictEqual(reused.socket, created.socket);

  await gcUntil('idle agent socket releases request contexts', () => {
    return created.reference.deref() === undefined &&
      reused.reference.deref() === undefined;
  });
  // The contexts must be collected while the socket is still pooled.
  assertPooled(created.socket);

  agent.destroy();
  server.close();
}

main().then(common.mustCall());
