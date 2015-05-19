'use strict';
var common = require('../common');
var assert = require('assert');
var http = require('http');

var reqEndCount = 0;

var server = http.Server(function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');

  var buffer = '';

  req.setEncoding('utf8');

  req.on('data', function(s) {
    buffer += s;
  });

  req.on('end', function() {
    reqEndCount++;
    assert.equal(body, buffer);
  });
});

var responses = 0;
var N = 10;
var M = 10;

var body = '';
for (var i = 0; i < 1000; i++) {
  body += 'hello world';
}

var options = {
  port: common.PORT,
  path: '/',
  method: 'PUT'
};

server.listen(common.PORT, function() {
  for (var i = 0; i < N; i++) {
    setTimeout(function() {
      for (var j = 0; j < M; j++) {

        var req = http.request(options, function(res) {
          console.log(res.statusCode);
          if (++responses == N * M) server.close();
        }).on('error', function(e) {
          console.log(e.message);
          process.exit(1);
        });

        req.end(body);
      }
    }, i);
  }
});


process.on('exit', function() {
  assert.equal(N * M, responses);
  assert.equal(N * M, reqEndCount);
});
