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

let requests = 0;
let responses = 0;

const headers = {};
const N = 2000;
for (let i = 0; i < N; ++i) {
  headers['key' + i] = i;
}

const maxAndExpected = [ // for server
  [50, 50],
  [1500, 1500],
  [0, N + 2] // Host and Connection
];
let max = maxAndExpected[requests][0];
let expected = maxAndExpected[requests][1];

const server = http.createServer(function(req, res) {
  assert.strictEqual(Object.keys(req.headers).length, expected);
  if (++requests < maxAndExpected.length) {
    max = maxAndExpected[requests][0];
    expected = maxAndExpected[requests][1];
    server.maxHeadersCount = max;
  }
  res.writeHead(200, headers);
  res.end();
});
server.maxHeadersCount = max;

server.listen(0, function() {
  const maxAndExpected = [ // for client
    [20, 20],
    [1200, 1200],
    [0, N + 3] // Connection, Date and Transfer-Encoding
  ];
  doRequest();

  function doRequest() {
    const max = maxAndExpected[responses][0];
    const expected = maxAndExpected[responses][1];
    const req = http.request({
      port: server.address().port,
      headers: headers
    }, function(res) {
      assert.strictEqual(Object.keys(res.headers).length, expected);
      res.on('end', function() {
        if (++responses < maxAndExpected.length) {
          doRequest();
        } else {
          server.close();
        }
      });
      res.resume();
    });
    req.maxHeadersCount = max;
    req.end();
  }
});

process.on('exit', function() {
  assert.strictEqual(requests, maxAndExpected.length);
  assert.strictEqual(responses, maxAndExpected.length);
});
