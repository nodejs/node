var common = require('../common');
var http = require('http');
var net = require('net');
var assert = require('assert');

var reqstr = 'HTTP/1.1 200 OK\r\n' +
             'Foo: Bar\r' +
             'Content-Length: 1\r\n\r\n';

var server = net.createServer(function(socket) {
  socket.write(reqstr);
});

server.listen(common.PORT, function() {
  // The callback should not be called because the server is sending a
  // header field that ends only in \r with no following \n
  var req = http.get({port:common.PORT}, function(res) {
    assert.fail(null, null, 'callback should not be called');
  });
  req.on('error', common.mustCall(function(err) {
    assert(/^Parse Error/.test(err.message));
    assert.equal(err.code, 'HPE_LF_EXPECTED');
    server.close();
  }));
});