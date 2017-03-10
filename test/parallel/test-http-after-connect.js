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
const common = require('../common');
const assert = require('assert');
const http = require('http');

let clientResponses = 0;

const server = http.createServer(common.mustCall(function(req, res) {
  console.error('Server got GET request');
  req.resume();
  res.writeHead(200);
  res.write('');
  setTimeout(function() {
    res.end(req.url);
  }, 50);
}, 2));
server.on('connect', common.mustCall(function(req, socket) {
  console.error('Server got CONNECT request');
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');
  socket.resume();
  socket.on('end', function() {
    socket.end();
  });
}));
server.listen(0, function() {
  const req = http.request({
    port: this.address().port,
    method: 'CONNECT',
    path: 'google.com:80'
  });
  req.on('connect', common.mustCall(function(res, socket) {
    console.error('Client got CONNECT response');
    socket.end();
    socket.on('end', function() {
      doRequest(0);
      doRequest(1);
    });
    socket.resume();
  }));
  req.end();
});

function doRequest(i) {
  http.get({
    port: server.address().port,
    path: '/request' + i
  }, common.mustCall(function(res) {
    console.error('Client got GET response');
    let data = '';
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      data += chunk;
    });
    res.on('end', function() {
      assert.strictEqual(data, '/request' + i);
      ++clientResponses;
      if (clientResponses === 2) {
        server.close();
      }
    });
  }));
}
