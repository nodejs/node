'use strict';

// Regression test for HackerOne report #3582376
// HTTP Response Queue Poisoning via TOCTOU Race Condition in http.Agent
//
// When keepAlive is true, there is a window between a socket entering the
// freeSockets pool (parser detached) and being reassigned. If the server
// writes a full HTTP response during this window, it is consumed as the
// response for the *next* request — poisoning the response queue.
//
// The fix installs a read guard on idle sockets so that unsolicited data
// causes the socket to be destroyed without adding public stream listeners.

const common = require('../common');
const assert = require('assert');
const http = require('http');

let serverSocket;

const server = http.createServer(common.mustCall((req, res) => {
  // Capture the raw socket on the first request
  serverSocket ||= req.socket;
  res.end(req.url);
}, 2));  // Expect request1 and request2

server.listen(0, common.mustCall(() => {
  const agent = new http.Agent({ keepAlive: true });
  const options = { host: '127.0.0.1', port: server.address().port, agent };
  const name = agent.getName(options);

  // Step 1: Send request1
  const request1 = http.request({ ...options, path: '/request1' });
  request1.end();

  request1.on('response', common.mustCall((response) => {
    let body = '';
    response.setEncoding('utf8');
    response.on('data', (data) => { body += data; });
    response.on('end', common.mustCall(() => {
      assert.strictEqual(body, '/request1');
    }));
  }));

  request1.on('close', common.mustCall(() => {
    // Use nextTick to ensure socket is in freeSockets
    process.nextTick(common.mustCall(() => {
      // Verify the socket is in the free pool with parser detached
      assert.strictEqual(agent.freeSockets[name]?.length, 1);
      const freeSocket = agent.freeSockets[name][0];
      assert.strictEqual(freeSocket.parser, null);
      // With the fix, no public stream listeners are added.
      assert.strictEqual(freeSocket.listenerCount('data'), 0);
      assert.strictEqual(freeSocket.listenerCount('readable'), 0);

      // Step 2: Server injects a poisoned response while socket is idle
      serverSocket.write(
        'HTTP/1.1 200 OK\r\n' +
        'X-Poisoned: true\r\n' +
        'Connection: keep-alive\r\n' +
        'Content-Length: 0\r\n' +
        '\r\n'
      );

      // Step 3: Allow the event loop to poll I/O so the guard can fire.
      // In a real attack, there is always time between the poison arriving
      // and the next client request. setTimeout(0) runs after the I/O poll
      // phase, giving the guard a chance to receive the poisoned data.
      setTimeout(common.mustCall(() => {
        // The guard should have destroyed the poisoned socket
        assert.strictEqual(freeSocket.destroyed, true);
        assert.strictEqual(agent.freeSockets[name], undefined);

        // Step 4: Send request2 — should get a fresh connection
        const request2 = http.request({ ...options, path: '/request2' });
        request2.end();

        request2.on('response', common.mustCall((response) => {
          let body = '';
          response.setEncoding('utf8');
          response.on('data', (data) => { body += data; });
          response.on('end', common.mustCall(() => {
            assert.strictEqual(response.headers['x-poisoned'], undefined);
            assert.strictEqual(body, '/request2');
            agent.destroy();
            server.close();
          }));
        }));
      }), 50);
    }));
  }));
}));
