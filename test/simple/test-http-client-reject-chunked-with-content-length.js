var common = require('../common');
var http = require('http');
var net = require('net');
var assert = require('assert');

var reqstr = 'HTTP/1.1 200 OK\r\n' +
             'Content-Length: 1\r\n' +
             'Transfer-Encoding: chunked\r\n\r\n';

var server = net.createServer(function(socket) {
  socket.write(reqstr);
});

server.listen(common.PORT, function() {
  // The callback should not be called because the server is sending
  // both a Content-Length header and a Transfer-Encoding: chunked
  // header, which is a violation of the HTTP spec.
  var req = http.get({port:common.PORT}, function(res) {
    assert.fail(null, null, 'callback should not be called');
  });
  req.on('error', common.mustCall(function(err) {
    assert(/^Parse Error/.test(err.message));
    assert.equal(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
    server.close();
  }));
});