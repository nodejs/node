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

const expected = 10000;
let responses = 0;
let requests = 0;
let connection;

const server = http.Server(function(req, res) {
  requests++;
  assert.strictEqual(req.connection, connection);
  res.writeHead(200);
  res.end('hello world\n');
});

server.once('connection', function(c) {
  connection = c;
});

server.listen(0, function connect() {
  const request = http.get({
    port: server.address().port,
    path: '/',
    headers: {
      'Connection': 'Keep-alive',
    },
  }, function(res) {
    res.on('end', function() {
      if (++responses < expected) {
        connect();
      } else {
        server.close();
      }
    });
    res.resume();
  }).on('error', function(e) {
    console.log(e.message);
    process.exit(1);
  });
  request.agent.maxSockets = 1;
});

process.on('exit', function() {
  assert.strictEqual(responses, expected);
  assert.strictEqual(requests, expected);
});
