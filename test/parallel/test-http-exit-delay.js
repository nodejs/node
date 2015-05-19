'use strict';
var assert = require('assert');
var common = require('../common');
var http = require('http');

var start;
var server = http.createServer(function(req, res) {
  req.resume();
  req.on('end', function() {
    res.end('Success');
  });

  server.close();
});

server.listen(common.PORT, 'localhost', function() {
  var interval_id = setInterval(function() {
    start = new Date();
    if (start.getMilliseconds() > 100)
      return;

    console.log(start.toISOString());
    var req = http.request({
      'host': 'localhost',
      'port': common.PORT,
      'agent': false,
      'method': 'PUT'
    });

    req.end('Test');
    clearInterval(interval_id);
  }, 10);
});

process.on('exit', function() {
  var end = new Date();
  console.log(end.toISOString());
  assert.equal(start.getSeconds(), end.getSeconds());
  assert(end.getMilliseconds() < 900);
  console.log('ok');
});
