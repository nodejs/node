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
  const port = this.address().port;
  const headers = [
    {
      connection: 'upgrade',
      upgrade: 'websocket'
    },
    [
      ['Host', 'echo.websocket.org'],
      ['Connection', 'Upgrade'],
      ['Upgrade', 'websocket'],
      ['Origin', 'http://www.websocket.org']
    ]
  ];
  var left = headers.length;
  headers.forEach(function(h) {
    var req = http.get({
      port: port,
      headers: h
    });
    var sawUpgrade = false;
    req.on('upgrade', common.mustCall(function(res, socket, upgradeHead) {
      sawUpgrade = true;
      var recvData = upgradeHead;
      socket.on('data', function(d) {
        recvData += d;
      });

      socket.on('close', common.mustCall(function() {
        assert.strictEqual(recvData.toString(), 'nurtzo');
      }));

      console.log(res.headers);
      const expectedHeaders = {
        hello: 'world',
        connection: 'upgrade',
        upgrade: 'websocket'
      };
      assert.deepStrictEqual(expectedHeaders, res.headers);

      socket.end();
      if (--left === 0)
        srv.close();
    }));
    req.on('close', common.mustCall(function() {
      assert.strictEqual(sawUpgrade, true);
    }));
  });
}));
