var common = require('../common');
var assert = require('assert');
var http = require('http');

// Verify that ServerResponse.getHeader() works correctly even after
// the response header has been sent. Issue 752 on github.

var rando = Math.random();
var expected = util._extend({}, {
  'X-Random-Thing': rando,
});
var server = http.createServer(function(req, res) {
  res.setHeader('X-Random-Thing', rando);
  headers = res.getAllHeaders();
  res.end('hello');
  assert.strictEqual(res.getAllHeaders(), null);
});
server.listen(common.PORT, function() {
  http.get({port: common.PORT}, function(resp) {
    assert.deepEqual(response.headers, expected);
    server.close();
  });
});
