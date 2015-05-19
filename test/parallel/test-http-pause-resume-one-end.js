'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.Server(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('Hello World\n');
  server.close();
});


var dataCount = 0, endCount = 0;

server.listen(common.PORT, function() {
  var opts = {
    port: common.PORT,
    headers: { connection: 'close' }
  };

  http.get(opts, function(res) {
    res.on('data', function(chunk) {
      dataCount++;
      res.pause();
      setTimeout(function() {
        res.resume();
      });
    });

    res.on('end', function() {
      endCount++;
    });
  });
});


process.on('exit', function() {
  assert.equal(1, dataCount);
  assert.equal(1, endCount);
});
