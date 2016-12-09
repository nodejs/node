'use strict';
// Verify that the 'upgrade' header causes an 'upgrade' event to be emitted to
// the HTTP client. This test uses a raw TCP server to better control server
// behavior.

var common = require('../common');
var assert = require('assert');

var http = require('http');
var net = require('net');

// Create a TCP server
var srv = net.createServer(function(c) {
  c.on('data', function(d) {
    c.write('HTTP/1.1 101\r\n');
    c.write('hello: world\r\n');
    c.write('connection: upgrade\r\n');
    c.write('upgrade: websocket\r\n');
    c.write('\r\n');
    c.write('nurtzo');
  });

  c.on('end', function() {
    c.end();
  });
});

srv.listen(0, '127.0.0.1', common.mustCall(function() {

  var options = {
    port: this.address().port,
    host: '127.0.0.1',
    headers: {
      'connection': 'upgrade',
      'upgrade': 'websocket'
    }
  };
  var name = options.host + ':' + options.port;

  var req = http.request(options);
  req.end();

  req.on('upgrade', common.mustCall(function(res, socket, upgradeHead) {
    var recvData = upgradeHead;
    socket.on('data', function(d) {
      recvData += d;
    });

    socket.on('close', common.mustCall(function() {
      assert.equal(recvData, 'nurtzo');
    }));

    console.log(res.headers);
    var expectedHeaders = { 'hello': 'world',
                            'connection': 'upgrade',
                            'upgrade': 'websocket' };
    assert.deepStrictEqual(expectedHeaders, res.headers);

    // Make sure this request got removed from the pool.
    assert(!http.globalAgent.sockets.hasOwnProperty(name));

    req.on('close', common.mustCall(function() {
      socket.end();
      srv.close();
    }));
  }));
}));
