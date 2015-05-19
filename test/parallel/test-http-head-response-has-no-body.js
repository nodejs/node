'use strict';
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

server.on('listening', function() {
  var req = http.request({
    port: common.PORT,
    method: 'HEAD',
    path: '/'
  }, function(res) {
    common.error('response');
    res.on('end', function() {
      common.error('response end');
      server.close();
      responseComplete = true;
    });
    res.resume();
  });
  common.error('req');
  req.end();
});

process.on('exit', function() {
  assert.ok(responseComplete);
});
