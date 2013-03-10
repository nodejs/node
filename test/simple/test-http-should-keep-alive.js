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
var net = require('net');

var SERVER_RESPONSES = [
  'HTTP/1.0 200 ok\r\nContent-Length: 0\r\n\r\n',
  'HTTP/1.0 200 ok\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n',
  'HTTP/1.0 200 ok\r\nContent-Length: 0\r\nConnection: close\r\n\r\n',
  'HTTP/1.1 200 ok\r\nContent-Length: 0\r\n\r\n',
  'HTTP/1.1 200 ok\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n',
  'HTTP/1.1 200 ok\r\nContent-Length: 0\r\nConnection: close\r\n\r\n'
];
var SHOULD_KEEP_ALIVE = [
  false, // HTTP/1.0, default
  true,  // HTTP/1.0, Connection: keep-alive
  false, // HTTP/1.0, Connection: close
  true,  // HTTP/1.1, default
  true,  // HTTP/1.1, Connection: keep-alive
  false  // HTTP/1.1, Connection: close
];
var requests = 0;
var responses = 0;

var server = net.createServer(function(socket) {
  socket.write(SERVER_RESPONSES[requests]);
  ++requests;
}).listen(common.PORT, function() {
  function makeRequest() {
    var req = http.get({port: common.PORT}, function(res) {
      assert.equal(req.shouldKeepAlive, SHOULD_KEEP_ALIVE[responses],
                   SERVER_RESPONSES[responses] + ' should ' +
                   (SHOULD_KEEP_ALIVE[responses] ? '' : 'not ') +
                   'Keep-Alive');
      ++responses;
      if (responses < SHOULD_KEEP_ALIVE.length) {
        makeRequest();
      } else {
        server.close();
      }
      res.resume();
    });
  }

  makeRequest();
});

process.on('exit', function() {
  assert.equal(requests, SERVER_RESPONSES.length);
  assert.equal(responses, SHOULD_KEEP_ALIVE.length);
});
