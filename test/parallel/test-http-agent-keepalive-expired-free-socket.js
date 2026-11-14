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

// When the socket that the scheduling picks first has expired and the other
// one has not, the expired one is removed and the request reuses the other.
function testMixedPool(scheduling, next) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 3));
  // No Keep-Alive hint from the server, so both sockets enter the pool with
  // the agent's `timeout` as their idle timeout.
  server.keepAliveTimeout = 0;

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({
      keepAlive: true,
      timeout: 60_000,
      scheduling,
    });
    const { port } = server.address();

    fillPool(agent, port, 2, common.mustCall(() => {
      const [first, last] = Object.values(agent.freeSockets)[0];
      // The socket that the scheduling picks first gets an idle timeout that
      // passes during the block.
      const [expired, kept] = scheduling === 'fifo' ?
        [first, last] :
        [last, first];
      expired.setTimeout(100);
      common.sleepSync(300);
      const req = http.get({ port, agent }, common.mustCall((res) => {
        assert.strictEqual(req.socket, kept);
        res.resume();
        res.on('end', common.mustCall(() => {
          agent.destroy();
          server.close(next);
        }));
      }));
      assert.strictEqual(req.reusedSocket, true);
      assert.strictEqual(expired.destroyed, true);
      assert.strictEqual(kept.destroyed, false);
      assert.strictEqual(agent.totalSocketCount, 1);
      assert.strictEqual(count(agent.freeSockets), 0);
      assert.strictEqual(count(agent.sockets), 1);
    }));
  }));
}

// A pooled socket whose timeout is set again is expired by its restarted timer,
// not by the time it entered the pool.
function testRestartedTimeout(next) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 2));
  // No Keep-Alive hint from the server, so the agent's `timeout` alone sets
  // the socket's idle timeout.
  server.keepAliveTimeout = 0;

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, timeout: 60_000 });
    const { port } = server.address();

    fillPool(agent, port, 1, common.mustCall(() => {
      const [socket] = Object.values(agent.freeSockets)[0];
      common.sleepSync(300);
      // Shorter than the block: only a timer restarted now has not passed it.
      socket.setTimeout(200);
      const req = http.get({ port, agent }, common.mustCall((res) => {
        assert.strictEqual(res.socket, socket);
        res.resume();
        res.on('end', common.mustCall(() => {
          agent.destroy();
          server.close(next);
        }));
      }));
      assert.strictEqual(req.reusedSocket, true);
    }));
  }));
}

// A request timeout that fired while the response waited unread is cleared when
// the socket is kept alive. Its timer then still holds the fired timeout, but
// the socket has no idle timeout, so it is reused.
function testFiredTimeoutCleared(next) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 2));
  // No Keep-Alive hint from the server, and the agent has no `timeout`, so the
  // pooled socket has no idle timeout.
  server.keepAliveTimeout = 0;

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true });
    const { port } = server.address();

    function reuse(socket) {
      assert.strictEqual(socket.timeout, 0);
      const req = http.get({ port, agent }, common.mustCall((res) => {
        assert.strictEqual(res.socket, socket);
        res.resume();
        res.on('end', common.mustCall(() => {
          agent.destroy();
          server.close(next);
        }));
      }));
      assert.strictEqual(req.reusedSocket, true);
    }

    const first = http.get({ port, agent }, common.mustCall((res) => {
      const { socket } = res;
      // The response is read only after the request timeout has fired.
      first.setTimeout(50, common.mustCall(() => {
        setImmediate(common.mustCall(() => {
          agent.once('free', common.mustCall(() => {
            setImmediate(common.mustCall(() => reuse(socket)));
          }));
          res.resume();
        }));
      }));
    }));
  }));
}

// A destroyed socket can stay in the free pool until its 'close' event. A
// request made before that event must not get it.
function testDestroyedSocket(next) {
  const server = http.createServer(common.mustCall((req, res) => {
    res.end('ok');
  }, 3));
  // No Keep-Alive hint from the server, so the pooled sockets have no idle
  // timeout and cannot expire.
  server.keepAliveTimeout = 0;

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true });
    const { port } = server.address();

    fillPool(agent, port, 2, common.mustCall(() => {
      const [first, last] = Object.values(agent.freeSockets)[0];
      last.destroy();
      const req = http.get({ port, agent }, common.mustCall((res) => {
        assert.strictEqual(req.socket, first);
        res.resume();
        res.on('end', common.mustCall(() => {
          agent.destroy();
          server.close(next);
        }));
      }));
      assert.strictEqual(req.reusedSocket, true);
      assert.strictEqual(agent.totalSocketCount, 1);
      assert.strictEqual(count(agent.freeSockets), 0);
      assert.strictEqual(count(agent.sockets), 1);
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
  // The idle timeout of one of two pooled sockets passed during the block.
  (next) => testMixedPool('lifo', next),
  (next) => testMixedPool('fifo', next),
  testRestartedTimeout,
  testFiredTimeoutCleared,
  testDestroyedSocket,
  testOptimizedAddRequest,
];

(function run(i) {
  if (i < cases.length) cases[i](common.mustCall(() => run(i + 1)));
})(0);
