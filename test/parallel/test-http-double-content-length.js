'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

// The callback should never be invoked because the server
// should respond with a 400 Client Error when a double
// Content-Length header is received.
const server = http.createServer(common.mustNotCall());
server.on('clientError', common.mustCall((err, socket) => {
  assert.match(err.message, /^Parse Error/);
  assert.strictEqual(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
  socket.destroy();
}));

server.listen(0, () => {
  const req = http.get({
    port: server.address().port,
    // Send two content-length header values.
    headers: { 'Content-Length': [1, 2] }
  }, common.mustNotCall('an error should have occurred'));
  req.on('error', common.mustCall(() => {
    server.close();
  }));
});
