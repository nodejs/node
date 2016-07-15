'use strict';
var common = require('../common');
var http = require('http');
var fs = require('fs');
var path = require('path');

common.refreshTmpDir();

var file = path.join(common.tmpDir, 'http-pipe-fs-test.txt');

var server = http.createServer(common.mustCall(function(req, res) {
  var stream = fs.createWriteStream(file);
  req.pipe(stream);
  stream.on('close', function() {
    res.writeHead(200);
    res.end();
  });
}, 2)).listen(0, function() {
  http.globalAgent.maxSockets = 1;

  for (var i = 0; i < 2; ++i) {
    (function(i) {
      var req = http.request({
        port: server.address().port,
        method: 'POST',
        headers: {
          'Content-Length': 5
        }
      }, function(res) {
        res.on('end', function() {
          console.error('res' + i + ' end');
          if (i === 2) {
            server.close();
          }
        });
        res.resume();
      });
      req.on('socket', function(s) {
        console.error('req' + i + ' start');
      });
      req.end('12345');
    }(i + 1));
  }
});
