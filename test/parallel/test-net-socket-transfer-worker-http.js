'use strict';

// This test verifies HTTP load balancing on top of transferred TCP sockets:
// the main thread accepts connections and distributes each one round-robin to a
// pool of worker threads, where an http.Server handles the request. It exercises
// the intended use case for transferring net.Socket handles across threads.

const common = require('../common');

if (common.isWindows) {
  common.skip('transferring TCP handles between threads is not supported on ' +
              'Windows yet');
}

const assert = require('assert');
const net = require('net');
const http = require('http');
const {
  Worker,
  isMainThread,
  parentPort,
  threadId,
} = require('worker_threads');

const POOL = 4;
const REQ_PER_WORKER = 5;
const TOTAL = POOL * REQ_PER_WORKER;

if (!isMainThread) {
  // Each worker serves HTTP over connections transferred from the main thread.
  const server = http.createServer((req, res) => {
    res.end(`thread:${threadId}`);
  });
  parentPort.on('message', common.mustCall(({ socket }) => {
    assert.ok(socket instanceof net.Socket);
    server.emit('connection', socket);
  }, REQ_PER_WORKER));
  return;
}

const workers = [];
for (let i = 0; i < POOL; i++)
  workers.push(new Worker(__filename));

let next = 0;
const front = net.createServer((socket) => {
  // Distribute each freshly accepted connection round-robin to a worker.
  workers[next++ % POOL].postMessage({ socket }, [socket]);
});

front.listen(0, common.mustCall(() => {
  const { port } = front.address();
  const counts = { __proto__: null };
  let done = 0;

  for (let i = 0; i < TOTAL; i++) {
    // agent: false forces a fresh connection per request, so each request maps
    // to exactly one round-robin hand-off.
    http.get({ port, agent: false }, common.mustCall((res) => {
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => { body += chunk; });
      res.on('end', common.mustCall(() => {
        assert.match(body, /^thread:\d+$/);
        counts[body] = (counts[body] || 0) + 1;

        if (++done === TOTAL) {
          // Every worker handled the same number of requests.
          assert.strictEqual(Object.keys(counts).length, POOL);
          for (const key of Object.keys(counts))
            assert.strictEqual(counts[key], REQ_PER_WORKER);

          front.close();
          for (const worker of workers)
            worker.terminate();
        }
      }));
    }));
  }
}));
