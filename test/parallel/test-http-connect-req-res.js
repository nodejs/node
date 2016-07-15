'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.fail);
server.on('connect', common.mustCall(function(req, socket, firstBodyChunk) {
  assert.equal(req.method, 'CONNECT');
  assert.equal(req.url, 'example.com:443');
  console.error('Server got CONNECT request');

  // It is legal for the server to send some data intended for the client
  // along with the CONNECT response
  socket.write(
    'HTTP/1.1 200 Connection established\r\n' +
    'Date: Tue, 15 Nov 1994 08:12:31 GMT\r\n' +
    '\r\n' +
    'Head'
  );

  var data = firstBodyChunk.toString();
  socket.on('data', function(buf) {
    data += buf.toString();
  });
  socket.on('end', function() {
    socket.end(data);
  });
}));
server.listen(0, common.mustCall(function() {
  const req = http.request({
    port: this.address().port,
    method: 'CONNECT',
    path: 'example.com:443'
  }, common.fail);

  req.on('close', common.mustCall(function() { }));

  req.on('connect', common.mustCall(function(res, socket, firstBodyChunk) {
    console.error('Client got CONNECT request');

    // Make sure this request got removed from the pool.
    const name = 'localhost:' + server.address().port;
    assert(!http.globalAgent.sockets.hasOwnProperty(name));
    assert(!http.globalAgent.requests.hasOwnProperty(name));

    // Make sure this socket has detached.
    assert(!socket.ondata);
    assert(!socket.onend);
    assert.equal(socket.listeners('connect').length, 0);
    assert.equal(socket.listeners('data').length, 0);

    var data = firstBodyChunk.toString();

    // test that the firstBodyChunk was not parsed as HTTP
    assert.equal(data, 'Head');

    socket.on('data', function(buf) {
      data += buf.toString();
    });
    socket.on('end', function() {
      assert.equal(data, 'HeadRequestEnd');
      server.close();
    });
    socket.end('End');
  }));

  req.end('Request');
}));
