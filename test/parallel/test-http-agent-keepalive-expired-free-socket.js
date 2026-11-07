// Flags: --expose-internals --no-warnings --allow-natives-syntax
'use strict';

const common = require('../common');

// A free socket expires by its idle timeout timer. When the event loop is
// blocked longer than that timeout, the timer cannot run, and the next
// request must not reuse the socket, because the server may already have
// closed it.
// Refs: https://github.com/nodejs/node/issues/54293

const assert = require('node:assert');
const http = require('node:http');
const { internalBinding } = require('internal/test/binding');

function count(lists) {
  return Object.values(lists).reduce((n, list) => n + list.length, 0);
}

// Makes `sockets` concurrent requests, then calls `callback` once all their
// sockets are in the agent's free pool.
function fillPool(agent, port, sockets, callback) {
  let free = 0;
  agent.on('free', function onFree() {
    if (++free === sockets) {
      agent.removeListener('free', onFree);
      setImmediate(callback);
    }
  });
  for (let i = 0; i < sockets; i++) {
    http.get({ port, agent }, common.mustCall((res) => res.resume()));
  }
}

function testReuse(agentOptions, blockMs, expectReuse, next) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 2));
  // No Keep-Alive hint from the server, so the agent's `timeout` alone sets
  // the socket's idle timeout.
  server.keepAliveTimeout = 0;

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, ...agentOptions });
    const { port } = server.address();

    fillPool(agent, port, 1, common.mustCall(() => {
      const [socket] = Object.values(agent.freeSockets)[0];
      assert.strictEqual(socket.timeout, agentOptions.timeout ?? 0);
      common.sleepSync(blockMs);
      const req = http.get({ port, agent }, common.mustCall((res) => {
        res.resume();
        res.on('end', common.mustCall(() => {
          agent.destroy();
          server.close(next);
        }));
      }));
      assert.strictEqual(req.reusedSocket, expectReuse);
      assert.strictEqual(socket.destroyed, !expectReuse);
    }));
  }));
}

// The expired sockets release their slots at once, so a burst of requests
// after the block opens no more than maxSockets connections.
function testMaxSockets(next) {
  const timeout = common.platformTimeout(500);
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 6));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, timeout, maxSockets: 2 });
    const { port } = server.address();

    fillPool(agent, port, 2, common.mustCall(() => {
      const pooled = [...Object.values(agent.freeSockets)[0]];
      common.sleepSync(timeout * 2);
      let done = 0;
      for (let i = 0; i < 4; i++) {
        http.get({ port, agent }, common.mustCall((res) => {
          res.resume();
          res.on('end', () => {
            if (++done === 4) {
              agent.destroy();
              server.close(next);
            }
          });
        }));
      }
      assert(pooled.every((socket) => socket.destroyed));
      assert.strictEqual(agent.totalSocketCount, 2);
      assert.strictEqual(count(agent.sockets), 2);
      assert.strictEqual(count(agent.requests), 2);
    }));
  }));
}

// addRequest() reads the timers' clock through a fast API call when it takes a
// socket from the pool; the optimized function must be able to make it.
function testOptimizedAddRequest(next) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 3));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, timeout: 60_000 });
    const { port } = server.address();

    function get(callback) {
      http.get({ port, agent }, common.mustCall((res) => {
        res.resume();
        res.on('end', common.mustCall(() => setImmediate(callback)));
      }));
    }

    eval('%PrepareFunctionForOptimization(http.Agent.prototype.addRequest)');
    get(common.mustCall(() => get(common.mustCall(() => {
      eval('%OptimizeFunctionOnNextCall(http.Agent.prototype.addRequest)');
      const fastCalls = getFastCallCount();
      get(common.mustCall(() => {
        agent.destroy();
        server.close(next);
      }));
      if (common.isDebug) {
        assert.strictEqual(getFastCallCount() - fastCalls, 1);
      }
    }))));
  }));
}

// The fast API call counter exists in debug builds only.
function getFastCallCount() {
  if (!common.isDebug) return 0;
  const { getV8FastApiCallCount } = internalBinding('debug');
  return getV8FastApiCallCount('timers.getLibuvNow');
}

// The cases run one after another, so that one case's block does not delay
// the timers of another.
const cases = [
  // The idle timeout passed during the block.
  (next) => testReuse({ timeout: 100 }, 300, false, next),
  (next) => testReuse({ timeout: 100, scheduling: 'fifo' }, 300, false, next),
  // The idle timeout did not pass during the block.
  (next) => testReuse({ timeout: 60_000 }, 300, true, next),
  // A socket without an idle timeout is not expired by the clock.
  (next) => testReuse({}, 300, true, next),
  testMaxSockets,
  testOptimizedAddRequest,
];

(function run(i) {
  if (i < cases.length) cases[i](common.mustCall(() => run(i + 1)));
})(0);
