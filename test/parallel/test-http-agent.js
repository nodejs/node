'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const server = http.Server(function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

let responses = 0;
const N = 4;
const M = 4;

server.listen(0, function() {
  const port = this.address().port;
  for (let i = 0; i < N; i++) {
    setTimeout(function() {
      for (let j = 0; j < M; j++) {
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
  assert.strictEqual(N * M, responses);
});
