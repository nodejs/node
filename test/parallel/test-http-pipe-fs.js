'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');
var fs = require('fs');
var path = require('path');

var file = path.join(common.tmpDir, 'http-pipe-fs-test.txt');
var requests = 0;

var server = http.createServer(function(req, res) {
  ++requests;
  var stream = fs.createWriteStream(file);
  req.pipe(stream);
  stream.on('close', function() {
    res.writeHead(200);
    res.end();
  });
}).listen(common.PORT, function() {
  http.globalAgent.maxSockets = 1;

  for (var i = 0; i < 2; ++i) {
    (function(i) {
      var req = http.request({
        port: common.PORT,
        method: 'POST',
        headers: {
          'Content-Length': 5
        }
      }, function(res) {
        res.on('end', function() {
          common.debug('res' + i + ' end');
          if (i === 2) {
            server.close();
          }
        });
        res.resume();
      });
      req.on('socket', function(s) {
        common.debug('req' + i + ' start');
      });
      req.end('12345');
    }(i + 1));
  }
});

process.on('exit', function() {
  assert.equal(requests, 2);
});
