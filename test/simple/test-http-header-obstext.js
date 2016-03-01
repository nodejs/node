var common = require('../common');
var http = require('http');
var assert = require('assert');

var server = http.createServer(common.mustCall(function(req, res) {
  res.end('ok');
}));
server.listen(common.PORT, function() {
  http.get({
    port: common.PORT,
    headers: {'Connection': 'close', 'Test': 'Düsseldorf'}
  }, common.mustCall(function(res) {
    assert.equal(res.statusCode, 200);
    server.close();
  }));
});
