var common = require('../common');
var http = require('http');
var net = require('net');
var assert = require('assert');

var reqstr = 'POST / HTTP/1.1\r\n' +
             'Content-Length: 1\r\n' +
             'Transfer-Encoding: chunked\r\n\r\n';

var server = http.createServer(function(req, res) {
  assert.fail(null, null, 'callback should not be invoked');
});
server.on('clientError', common.mustCall(function(err) {
  assert(/^Parse Error/.test(err.message));
  assert.equal(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
  server.close();
}));
server.listen(common.PORT, function() {
  var client = net.connect({port: common.PORT}, function() {
    client.write(reqstr);
    client.end();
  });
  client.on('data', function(data) {
    // Should not get to this point because the server should simply
    // close the connection without returning any data.
    assert.fail(null, null, 'no data should be returned by the server');
  });
  client.on('end', common.mustCall(function() {}));
});