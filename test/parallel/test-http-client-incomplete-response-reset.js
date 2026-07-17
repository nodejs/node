'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// A response that is cut short by a connection reset must still surface the
// socket error on the request. The response exists, but it is not complete, so
// swallowing the error would leave the application with a silently truncated
// body it believes is intact.

const LENGTH = 1024;

let serverSocket;

const server = http.createServer(common.mustCall((request, response) => {
  serverSocket = request.socket;
  // Promise more body than is ever sent.
  response.writeHead(200, { 'content-length': LENGTH });
  response.write('hello');
}));

server.on('clientError', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const req = http.request({ port: server.address().port }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);

    let received = 0;
    let reset = false;

    res.on('data', common.mustCallAtLeast((chunk) => {
      received += chunk.length;
      if (reset) return;
      reset = true;
      // The headers and part of the body have arrived. Reset from the server
      // side so the client sees a genuine inbound RST mid-body.
      serverSocket.resetAndDestroy();
    }, 1));

    res.on('end', common.mustNotCall());
    res.on('close', common.mustCall(() => {
      assert.strictEqual(res.complete, false);
      assert.strictEqual(res.errored.code, 'ECONNRESET');
      assert.strictEqual(res.errored.message, 'aborted');
      assert.ok(received > 0 && received < LENGTH,
                `expected a truncated body, got ${received} of ${LENGTH}`);
      server.close();
    }));
  }));

  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ECONNRESET');
  }));

  req.end();
}));
