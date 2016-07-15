'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const domain = require('domain');

var d;

common.refreshTmpDir();

// first fire up a simple HTTP server
var server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end();
  server.close();
});
server.listen(common.PIPE, function() {
  // create a domain
  d = domain.create();
  d.run(common.mustCall(test));
});

function test() {

  d.on('error', common.mustCall(function(err) {
    assert.equal('should be caught by domain', err.message);
  }));

  var req = http.get({
    socketPath: common.PIPE,
    headers: {'Content-Length': '1'},
    method: 'POST',
    path: '/'
  });
  req.on('response', function(res) {
    res.on('end', function() {
      res.emit('error', new Error('should be caught by domain'));
    });
    res.resume();
  });
  req.end();
}
