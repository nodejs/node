'use strict';
// Test that queued (pipelined) outgoing responses are destroyed when the
// socket closes before the first response has finished.  Previously,
// socketOnClose only aborted state.incoming (pending requests) but left
// state.outgoing responses with socket=null alive forever.

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

let requestCount = 0;

const server = http.createServer(common.mustCall((req, res) => {
  requestCount++;

  if (requestCount === 1) {
    // Keep the first response open so the second response is queued in
    // state.outgoing with socket === null.
    res.writeHead(200);
    res.write('start');
    // Intentionally do not call res.end().
  } else {
    // The second response should be queued — no socket assigned yet.
    assert.strictEqual(res.socket, null);
    assert.strictEqual(res.destroyed, false);
    assert.strictEqual(res.closed, false);

    res.on('close', common.mustCall(() => {
      assert.strictEqual(res.destroyed, true);
      assert.strictEqual(res.closed, true);
      server.close();
    }));

    // Simulate client dying while first response is still in flight.
    req.socket.destroy();
  }
}, 2));

server.listen(0, common.mustCall(function() {
  const port = this.address().port;
  const client = net.connect(port);

  // Send two pipelined HTTP/1.1 requests at once.
  client.write(
    `GET /1 HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n` +
    `GET /2 HTTP/1.1\r\nHost: localhost:${port}\r\n\r\n`,
  );
  client.resume();
}));
