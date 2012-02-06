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

var common = require('../common');
var assert = require('assert');
var http = require('http');

var serverConnected = false;
var serverRequests = 0;
var clientResponses = 0;

var server = http.createServer(function(req, res) {
  common.debug('Server got GET request');
  ++serverRequests;
  res.writeHead(200);
  res.write('');
  setTimeout(function() {
    res.end(req.url);
  }, 50);
});
server.on('connect', function(req, socket, firstBodyChunk) {
  common.debug('Server got CONNECT request');
  serverConnected = true;
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');
  socket.on('end', function() {
    socket.end();
  });
});
server.listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
    method: 'CONNECT',
    path: 'google.com:80'
  });
  req.on('connect', function(res, socket, firstBodyChunk) {
    common.debug('Client got CONNECT response');
    socket.end();
    socket.on('end', function() {
      doRequest(0);
      doRequest(1);
    });
  });
  req.end();
});

function doRequest(i) {
  var req = http.get({
    port: common.PORT,
    path: '/request' + i
  }, function(res) {
    common.debug('Client got GET response');
    var data = '';
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      data += chunk;
    });
    res.on('end', function() {
      assert.equal(data, '/request' + i);
      ++clientResponses;
      if (clientResponses === 2) {
        server.close();
      }
    });
  });
}

process.on('exit', function() {
  assert(serverConnected);
  assert.equal(serverRequests, 2);
  assert.equal(clientResponses, 2);
});
