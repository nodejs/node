'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const tcpPort = common.PORT;
const expectedConnections = 7;
var clientConnected = 0;
var serverConnected = 0;

var server = net.createServer(function(socket) {
  socket.end();
  if (++serverConnected === expectedConnections) {
    server.close();
  }
});

const regex1 = /^'port' argument must be a string or number/;
const regex2 = /^Port should be > 0 and < 65536/;

server.listen(tcpPort, 'localhost', function() {
  function cb() {
    ++clientConnected;
  }

  function fail(opts, errtype, msg) {
    assert.throws(() => {
      net.createConnection(opts, cb);
    }, err => err instanceof errtype && msg.test(err.message));
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
  }, TypeError, regex1);

  fail({
    port: false
  }, TypeError, regex1);

  fail({
    port: []
  }, TypeError, regex1);

  fail({
    port: {}
  }, TypeError, regex1);

  fail({
    port: null
  }, TypeError, regex1);

  fail({
    port: ''
  }, RangeError, regex2);

  fail({
    port: ' '
  }, RangeError, regex2);

  fail({
    port: '0x'
  }, RangeError, regex2);

  fail({
    port: '-0x1'
  }, RangeError, regex2);

  fail({
    port: NaN
  }, RangeError, regex2);

  fail({
    port: Infinity
  }, RangeError, regex2);

  fail({
    port: -1
  }, RangeError, regex2);

  fail({
    port: 65536
  }, RangeError, regex2);
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
