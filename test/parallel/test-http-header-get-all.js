var common = require('../common');
var assert = require('assert');
var http = require('http');

var s = http.createServer(function(req, res) {
  var contentType = 'content-type';
  var plain = 'text/plain';
  res.setHeader(contentType, plain);
  assert.ok(!res.headersSent);
  res.writeHead(200);
  assert.ok(res.headersSent);
  res.end('hello world\n');
  // This checks that after the headers have been sent, getHeader works
  // and does not throw an exception (joyent/node Issue 752)
  assert.doesNotThrow(
      function() {
        assert.deepStrictEqual({ 'content-type': 'text/plain' }, res.getAllHeaders());
      }
  );
});

s.listen(common.PORT, runTest);

function runTest() {
  http.get({ port: common.PORT }, function(response) {
    response.on('end', function() {
      s.close();
    });
    response.resume();
  });
}
