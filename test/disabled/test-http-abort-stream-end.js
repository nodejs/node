'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

var maxSize = 1024;
var size = 0;

var s = http.createServer(function(req, res) {
  this.close();

  res.writeHead(200, {'Content-Type': 'text/plain'});
  for (var i = 0; i < maxSize; i++) {
    res.write('x' + i);
  }
  res.end();
});

var aborted = false;
s.listen(common.PORT, function() {
  var req = http.get('http://localhost:' + common.PORT, function(res) {
    res.on('data', function(chunk) {
      size += chunk.length;
      assert(!aborted, 'got data after abort');
      if (size > maxSize) {
        aborted = true;
        req.abort();
        size = maxSize;
      }
    });
  });

  req.end();
});

process.on('exit', function() {
  assert(aborted);
  assert.equal(size, maxSize);
  console.log('ok');
});
