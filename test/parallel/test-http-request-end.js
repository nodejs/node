'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var expected = 'Post Body For Test';

var server = http.Server(function(req, res) {
  var result = '';

  req.setEncoding('utf8');
  req.on('data', function(chunk) {
    result += chunk;
  });

  req.on('end', function() {
    assert.strictEqual(expected, result);
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
