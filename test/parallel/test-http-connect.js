'use strict';
const common = require('../common');
var assert = require('assert');
var http = require('http');

var serverGotConnect = false;
var clientGotConnect = false;

var server = http.createServer(common.fail);
server.on('connect', function(req, socket, firstBodyChunk) {
  assert.equal(req.method, 'CONNECT');
  assert.equal(req.url, 'google.com:443');
  console.error('Server got CONNECT request');
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
server.listen(0, function() {
  var req = http.request({
    port: this.address().port,
    method: 'CONNECT',
    path: 'google.com:443'
  }, common.fail);

  var clientRequestClosed = false;
  req.on('close', function() {
    clientRequestClosed = true;
  });

  req.on('connect', function(res, socket, firstBodyChunk) {
    console.error('Client got CONNECT request');
    clientGotConnect = true;

    // Make sure this request got removed from the pool.
    var name = 'localhost:' + server.address().port;
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
