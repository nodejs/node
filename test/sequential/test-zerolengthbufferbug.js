'use strict';
// Serving up a zero-length buffer should work.

var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
  var buffer = new Buffer(0);
  // FIXME: WTF gjslint want this?
  res.writeHead(200, {'Content-Type': 'text/html',
                 'Content-Length': buffer.length});
  res.end(buffer);
});

var gotResponse = false;
var resBodySize = 0;

server.listen(common.PORT, function() {
  http.get({ port: common.PORT }, function(res) {
    gotResponse = true;

    res.on('data', function(d) {
      resBodySize += d.length;
    });

    res.on('end', function(d) {
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.ok(gotResponse);
  assert.equal(0, resBodySize);
});

