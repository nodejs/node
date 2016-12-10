'use strict';
require('../common');
var assert = require('assert');
var http = require('http');

var server = http.Server(function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

var responses = 0;
var N = 4;
var M = 4;

server.listen(0, function() {
  const port = this.address().port;
  for (var i = 0; i < N; i++) {
    setTimeout(function() {
      for (var j = 0; j < M; j++) {
        http.get({ port: port, path: '/' }, function(res) {
          console.log('%d %d', responses, res.statusCode);
          if (++responses === N * M) {
            console.error('Received all responses, closing server');
            server.close();
          }
          res.resume();
        }).on('error', function(e) {
          console.log('Error!', e);
          process.exit(1);
        });
      }
    }, i);
  }
});


process.on('exit', function() {
  assert.equal(N * M, responses);
});
