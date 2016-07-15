'use strict';
const common = require('../common');
var http = require('http');

// This test is to make sure that when the HTTP server
// responds to a HEAD request with data to res.end,
// it does not send any body.

var server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end('FAIL'); // broken: sends FAIL from hot path.
});
server.listen(0);

server.on('listening', common.mustCall(function() {
  var req = http.request({
    port: this.address().port,
    method: 'HEAD',
    path: '/'
  }, common.mustCall(function(res) {
    res.on('end', common.mustCall(function() {
      server.close();
    }));
    res.resume();
  }));
  req.end();
}));
