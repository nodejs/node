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

var server = http.createServer(function(req, res) {
  console.log('get', req.url);
  res.setTimeout(100, function() {
    console.log('timeout on response', req.url);
  });
  req.on('end', function() {
    console.log('end', req.url);
  });
}).listen(3000, function() {
  var c = net.connect(3000, function() {
    c.write('GET /1 HTTP/1.1\r\n' +
      'Host: localhost\r\n\r\n');
    c.write('GET /2 HTTP/1.1\r\n' +
      'Host: localhost\r\n\r\n');
  });
});
server.setTimeout(200, function(socket) {
  console.log('timeout on server');
  socket.destroy();
  server.close();
});
