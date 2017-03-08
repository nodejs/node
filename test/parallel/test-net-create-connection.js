'use strict';
require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');

const expectedConnections = 7;
let clientConnected = 0;
let serverConnected = 0;

const server = net.createServer(function(socket) {
  socket.end();
  if (++serverConnected === expectedConnections) {
    server.close();
  }
});

server.listen(0, 'localhost', function() {
  function cb() {
    ++clientConnected;
  }

  function fail(opts, errtype, msg) {
    assert.throws(() => net.createConnection(opts, cb),
                  (err) => err instanceof errtype && msg === err.code);
  }

  net.createConnection(this.address().port).on('connect', cb);
  net.createConnection(this.address().port, 'localhost').on('connect', cb);
  net.createConnection(this.address().port, cb);
  net.createConnection(this.address().port, 'localhost', cb);
  net.createConnection(this.address().port + '', 'localhost', cb);
  net.createConnection({port: this.address().port + ''}).on('connect', cb);
  net.createConnection({port: '0x' + this.address().port.toString(16)}, cb);

  fail({
    port: true
  }, TypeError, 'ERR_INVALID_ARG_TYPE');

  fail({
    port: false
  }, TypeError, 'ERR_INVALID_ARG_TYPE');

  fail({
    port: []
  }, TypeError, 'ERR_INVALID_ARG_TYPE');

  fail({
    port: {}
  }, TypeError, 'ERR_INVALID_ARG_TYPE');

  fail({
    port: null
  }, TypeError, 'ERR_INVALID_ARG_TYPE');

  fail({
    port: ''
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: ' '
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: '0x'
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: '-0x1'
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: NaN
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: Infinity
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: -1
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    port: 65536
  }, RangeError, 'ERR_INVALID_PORT');

  fail({
    hints: (dns.ADDRCONFIG | dns.V4MAPPED) + 42,
  }, Error, 'ERR_INVALID_FLAG');
});

// Try connecting to random ports, but do so once the server is closed
server.on('close', function() {
  function nop() {}

  net.createConnection({port: 0}).on('error', nop);
  net.createConnection({port: undefined}).on('error', nop);
});

process.on('exit', function() {
  assert.strictEqual(clientConnected, expectedConnections);
});
