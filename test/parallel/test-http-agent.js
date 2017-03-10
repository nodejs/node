// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
