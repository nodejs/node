var common = require('../common');
var assert = require('assert');

var http = require('http');

// This test is to make sure that when the HTTP server
// responds to a HEAD request, it does not send any body.
// In this case it was sending '0\r\n\r\n'

var server = http.createServer(function(req, res) {
  res.writeHead(200); // broken: defaults to TE chunked
  res.end();
});
server.listen(common.PORT);

var responseComplete = false;

server.addListener('listening', function() {
  var req = http.createClient(common.PORT).request('HEAD', '/');
  common.error('req');
  req.end();
  req.addListener('response', function(res) {
    common.error('response');
    res.addListener('end', function() {
      common.error('response end');
      server.close();
      responseComplete = true;
    });
  });
});

process.addListener('exit', function() {
  assert.ok(responseComplete);
});
