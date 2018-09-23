'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var http = require('http');

var body = 'hello world\n';

var sawFinish = false;
process.on('exit', function() {
  assert(sawFinish);
  console.log('ok');
});

var httpServer = http.createServer(function(req, res) {
  httpServer.close();

  res.on('finish', function() {
    sawFinish = true;
    assert(typeof(req.connection.bytesWritten) === 'number');
    assert(req.connection.bytesWritten > 0);
  });
  res.writeHead(200, { 'Content-Type': 'text/plain' });

  // Write 1.5mb to cause some requests to buffer
  // Also, mix up the encodings a bit.
  var chunk = new Array(1024 + 1).join('7');
  var bchunk = new Buffer(chunk);
  for (var i = 0; i < 1024; i++) {
    res.write(chunk);
    res.write(bchunk);
    res.write(chunk, 'hex');
  }
  // Get .bytesWritten while buffer is not empty
  assert(res.connection.bytesWritten > 0);

  res.end(body);
});

httpServer.listen(common.PORT, function() {
  http.get({ port: common.PORT });
});

