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
  var data = '';
  c.on('data', function(d) {
    data += d.toString('utf8');

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

var gotUpgrade = false;

srv.listen(common.PORT, '127.0.0.1', function() {

  var req = http.get({ port: common.PORT });
  req.on('upgrade', function(res, socket, upgradeHead) {
    // XXX: This test isn't fantastic, as it assumes that the entire response
    //      from the server will arrive in a single data callback
    assert.equal(upgradeHead, 'nurtzo');

    console.log(res.headers);
    var expectedHeaders = {'hello': 'world',
                            'connection': 'upgrade',
                            'upgrade': 'websocket' };
    assert.deepEqual(expectedHeaders, res.headers);

    socket.end();
    srv.close();

    gotUpgrade = true;
  });
});

process.on('exit', function() {
  assert.ok(gotUpgrade);
});
