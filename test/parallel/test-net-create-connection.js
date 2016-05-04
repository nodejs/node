'use strict';
const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');

const tcpPort = common.PORT;
const expectedConnections = 7;
var clientConnected = 0;
var serverConnected = 0;

const server = net.createServer(function(socket) {
  socket.end();
  if (++serverConnected === expectedConnections) {
    server.close();
  }
});

server.listen(tcpPort, 'localhost', function() {
  function cb() {
    ++clientConnected;
  }

  function fail(opts, errtype, msg) {
    assert.throws(function() {
      net.createConnection(opts, cb);
    }, function(err) {
      return err instanceof errtype && msg === err.code;
    });
  }

  net.createConnection(tcpPort).on('connect', cb);
  net.createConnection(tcpPort, 'localhost').on('connect', cb);
  net.createConnection(tcpPort, cb);
  net.createConnection(tcpPort, 'localhost', cb);
  net.createConnection(tcpPort + '', 'localhost', cb);
  net.createConnection({port: tcpPort + ''}).on('connect', cb);
  net.createConnection({port: '0x' + tcpPort.toString(16)}, cb);

  fail({
    port: true
  }, TypeError, 'INVALIDOPT');

  fail({
    port: false
  }, TypeError, 'INVALIDOPT');

  fail({
    port: []
  }, TypeError, 'INVALIDOPT');

  fail({
    port: {}
  }, TypeError, 'INVALIDOPT');

  fail({
    port: null
  }, TypeError, 'INVALIDOPT');

  fail({
    port: ''
  }, RangeError, 'PORTRANGE');

  fail({
    port: ' '
  }, RangeError, 'PORTRANGE');

  fail({
    port: '0x'
  }, RangeError, 'PORTRANGE');

  fail({
    port: '-0x1'
  }, RangeError, 'PORTRANGE');

  fail({
    port: NaN
  }, RangeError, 'PORTRANGE');

  fail({
    port: Infinity
  }, RangeError, 'PORTRANGE');

  fail({
    port: -1
  }, RangeError, 'PORTRANGE');

  fail({
    port: 65536
  }, RangeError, 'PORTRANGE');

  fail({
    hints: (dns.ADDRCONFIG | dns.V4MAPPED) + 42,
  }, TypeError, 'INVALIDOPTVALUE');
});

// Try connecting to random ports, but do so once the server is closed
server.on('close', function() {
  function nop() {}

  net.createConnection({port: 0}).on('error', nop);
  net.createConnection({port: undefined}).on('error', nop);
});

process.on('exit', function() {
  assert.equal(clientConnected, expectedConnections);
});
