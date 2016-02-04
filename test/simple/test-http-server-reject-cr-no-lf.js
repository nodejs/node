var common = require('../common');
var net = require('net');
var http = require('http');
var assert = require('assert');

var str = 'GET / HTTP/1.1\r\n' +
          'Dummy: Header\r' +
          'Content-Length: 1\r\n' +
          '\r\n';

var server = http.createServer(function(req, res) {
  assert.fail(null, null, 'this should not be called');
});
server.on('clientError', common.mustCall(function(err) {
  assert(/^Parse Error/.test(err.message));
  assert.equal(err.code, 'HPE_LF_EXPECTED');
  server.close();
}));
server.listen(common.PORT, function() {
  var client = net.connect({port:common.PORT}, function() {
    client.on('data', function(chunk) {
      assert.fail(null, null, 'this should not be called');
    });
    client.on('end', common.mustCall(function() {}));
    client.write(str);
    client.end();
  });
});
