'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var expected = 'This is a unicode text: سلام';
var result = '';

var server = http.Server(function(req, res) {
  req.setEncoding('utf8');
  req.on('data', function(chunk) {
    result += chunk;
  }).on('end', function() {
    server.close();
    res.writeHead(200);
    res.end('hello world\n');
  });

});

server.listen(0, function() {
  http.request({
    port: this.address().port,
    path: '/',
    method: 'POST'
  }, function(res) {
    console.log(res.statusCode);
    res.resume();
  }).on('error', function(e) {
    console.log(e.message);
    process.exit(1);
  }).end(expected);
});

process.on('exit', function() {
  assert.equal(expected, result);
});
