'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

// The callback should never be invoked because the server
// should respond with a 400 Client Error when a double
// Content-Length header is received.
const server = http.createServer((req, res) => {
  assert(false, 'callback should not have been invoked');
  res.end();
});
server.on('clientError', common.mustCall((err, socket) => {
  assert(/^Parse Error/.test(err.message));
  assert.equal(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
  socket.destroy();
}));

server.listen(0, () => {
  const req = http.get({
    port: server.address().port,
    // Send two content-length header values.
    headers: {'Content-Length': [1, 2]}},
    (res) => {
      common.fail('an error should have occurred');
    }
  );
  req.on('error', common.mustCall(() => {
    server.close();
  }));
});
