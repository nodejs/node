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

var serverGotConnect = false;
var clientGotConnect = false;

var server = http.createServer(function(req, res) {
  assert(false);
});
server.on('connect', function(req, socket, firstBodyChunk) {
  assert.equal(req.method, 'CONNECT');
  assert.equal(req.url, 'google.com:443');
  common.debug('Server got CONNECT request');
  serverGotConnect = true;

  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');

  var data = firstBodyChunk.toString();
  socket.on('data', function(buf) {
    data += buf.toString();
  });
  socket.on('end', function() {
    socket.end(data);
  });
});
server.listen(common.PORT, function() {
  var req = http.request({
    port: common.PORT,
    method: 'CONNECT',
    path: 'google.com:443'
  }, function(res) {
    assert(false);
  });

  var clientRequestClosed = false;
  req.on('close', function() {
    clientRequestClosed = true;
  });

  req.on('connect', function(res, socket, firstBodyChunk) {
    common.debug('Client got CONNECT request');
    clientGotConnect = true;

    // Make sure this request got removed from the pool.
    var name = 'localhost:' + common.PORT;
    assert(!http.globalAgent.sockets.hasOwnProperty(name));
    assert(!http.globalAgent.requests.hasOwnProperty(name));

    // Make sure this socket has detached.
    assert(!socket.ondata);
    assert(!socket.onend);
    assert.equal(socket.listeners('connect').length, 0);
    assert.equal(socket.listeners('data').length, 0);

    // the stream.Duplex onend listener
    // allow 0 here, so that i can run the same test on streams1 impl
    assert(socket.listeners('end').length <= 1);

    assert.equal(socket.listeners('free').length, 0);
    assert.equal(socket.listeners('close').length, 0);
    assert.equal(socket.listeners('error').length, 0);
    assert.equal(socket.listeners('agentRemove').length, 0);

    var data = firstBodyChunk.toString();
    socket.on('data', function(buf) {
      data += buf.toString();
    });
    socket.on('end', function() {
      assert.equal(data, 'HeadBody');
      assert(clientRequestClosed);
      server.close();
    });
    socket.write('Body');
    socket.end();
  });

  // It is legal for the client to send some data intended for the server
  // before the "200 Connection established" (or any other success or
  // error code) is received.
  req.write('Head');
  req.end();
});

process.on('exit', function() {
  assert.ok(serverGotConnect);
  assert.ok(clientGotConnect);
});
