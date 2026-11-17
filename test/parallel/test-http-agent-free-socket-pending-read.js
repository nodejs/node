'use strict';

// The free socket data guard in http.Agent only runs when the event loop
// polls for I/O. If data reaches an idle socket's OS buffer and a request
// takes that socket before the loop has polled, the guard never sees the data
// and it would be parsed as the response to that request. The Agent must
// check for pending data before handing a kept-alive socket to a request.

const common = require('../common');
const assert = require('assert');
const http = require('http');

const poison =
  'HTTP/1.1 200 OK\r\n' +
  'X-Poisoned: true\r\n' +
  'Connection: keep-alive\r\n' +
  'Content-Length: 0\r\n' +
  '\r\n';

// Keep the event loop from polling until the bytes are in the client's
// socket buffer, so libuv cannot deliver them to the data guard first.
function waitForPendingRead(socket) {
  const deadline = Date.now() + common.platformTimeout(5000);
  while (!socket._handle.hasPendingRead()) {
    assert.ok(Date.now() < deadline, 'timed out waiting for pending data');
  }
}

function checkResponse(req, expectedBody, cb) {
  req.on('response', common.mustCall((res) => {
    let body = '';
    res.setEncoding('utf8');
    res.on('data', (chunk) => { body += chunk; });
    res.on('end', common.mustCall(() => {
      assert.strictEqual(res.headers['x-poisoned'], undefined);
      assert.strictEqual(body, expectedBody);
      cb?.(res);
    }));
  }));
}

// A socket taken from the free pool.
{
  let serverSocket;
  const server = http.createServer(common.mustCall((req, res) => {
    serverSocket ||= req.socket;
    res.end(req.url);
  }, 2));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true });
    const options = { host: '127.0.0.1', port: server.address().port, agent };
    const name = agent.getName(options);

    const request1 = http.request({ ...options, path: '/request1' });
    request1.end();
    checkResponse(request1, '/request1');

    request1.on('close', common.mustCall(() => {
      process.nextTick(common.mustCall(() => {
        assert.strictEqual(agent.freeSockets[name]?.length, 1);
        const freeSocket = agent.freeSockets[name][0];

        serverSocket.write(poison);
        waitForPendingRead(freeSocket);

        // Same tick: the loop has not polled since the data arrived.
        const request2 = http.request({ ...options, path: '/request2' });
        request2.end();

        assert.strictEqual(freeSocket.destroyed, true);
        assert.strictEqual(request2.reusedSocket, false);

        checkResponse(request2, '/request2', common.mustCall(() => {
          agent.destroy();
          server.close();
        }));
      }));
    }));
  }));
}

// A released socket handed directly to a queued request.
{
  let serverSocket;
  const server = http.createServer(common.mustCall((req, res) => {
    serverSocket ||= req.socket;
    res.end(req.url);
  }, 2));

  server.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });
    const options = { host: '127.0.0.1', port: server.address().port, agent };
    const name = agent.getName(options);

    const request1 = http.request({ ...options, path: '/request1' });
    request1.end();
    const request2 = http.request({ ...options, path: '/request2' });
    request2.end();
    assert.strictEqual(agent.requests[name]?.length, 1);

    let socket1;
    request1.on('socket', common.mustCall((socket) => {
      socket1 = socket;
    }));

    // The socket is released to the queued request on the next tick after
    // 'end', so the loop does not poll in between.
    checkResponse(request1, '/request1', common.mustCall(() => {
      serverSocket.write(poison);
      waitForPendingRead(socket1);
    }));

    checkResponse(request2, '/request2', common.mustCall(() => {
      assert.strictEqual(socket1.destroyed, true);
      assert.strictEqual(request2.reusedSocket, false);
      agent.destroy();
      server.close();
    }));
  }));
}
