'use strict';

// Regression test: when a pooled socket's writableHighWaterMark differs from
// the new request's highWaterMark, the agent must sync the socket's HWM so
// that backpressure semantics match what the caller requested.
//
// See: https://github.com/nodejs/node/issues/64680

const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  req.on('end', () => res.end('ok'));
}, 2));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const agent = new http.Agent({ keepAlive: true });

  // Request A: creates socket with HWM=1MB.
  http.request({
    host: 'localhost', port, method: 'POST', agent,
    highWaterMark: 1024 * 1024,
  }, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustCall(() => {
      // Wait for socket to return to pool.
      setTimeout(common.mustCall(requestB), 100);
    }));
  })).end('x');

  function requestB() {
    const freeCount = Object.values(agent.freeSockets).flat().length;
    assert.strictEqual(freeCount, 1);

    // Request B: HWM=10KB — agent must sync the reused socket's HWM.
    const reqB = http.request({
      host: 'localhost', port, method: 'POST', agent,
      highWaterMark: 10 * 1024,
    }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        server.close();
      }));
    }));

    reqB.on('socket', common.mustCall((socket) => {
      // Socket HWM must be synced to the request's value.
      assert.strictEqual(socket.writableHighWaterMark, 10 * 1024);
    }));

    reqB.end('y');
  }
}));
