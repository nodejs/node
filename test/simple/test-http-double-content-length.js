var common = require('../common');
var http = require('http');
var assert = require('assert');

// The callback should never be invoked because the server
// should respond with a 400 Client Error when a double
// Content-Length header is received.
var server = http.createServer(function(req, res) {
  assert(false, 'callback should not have been invoked');
  res.end();
});
server.on('clientError', common.mustCall(function(err, socket) {
  assert(/^Parse Error/.test(err.message));
  assert.equal(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
  socket.destroy();
}));

server.listen(common.PORT, function() {
  var req = http.get({
    port: common.PORT,
    // Send two content-length header values.
    headers: {'Content-Length': [1, 2]}},
    function(res) {
      assert.fail(null, null, 'an error should have occurred');
      server.close();
    }
  );
  req.on('error', common.mustCall(function() {
    server.close();
  }));
});