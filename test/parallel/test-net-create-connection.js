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
    assert.throws(function() {
      net.createConnection(opts, cb);
    }, function(err) {
      return err instanceof errtype && msg === err.message;
    });
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
  }, TypeError, '"port" option should be a number or string: true');

  fail({
    port: false
  }, TypeError, '"port" option should be a number or string: false');

  fail({
    port: []
  }, TypeError, '"port" option should be a number or string: ');

  fail({
    port: {}
  }, TypeError, '"port" option should be a number or string: [object Object]');

  fail({
    port: null
  }, TypeError, '"port" option should be a number or string: null');

  fail({
    port: ''
  }, RangeError, '"port" option should be >= 0 and < 65536: ');

  fail({
    port: ' '
  }, RangeError, '"port" option should be >= 0 and < 65536:  ');

  fail({
    port: '0x'
  }, RangeError, '"port" option should be >= 0 and < 65536: 0x');

  fail({
    port: '-0x1'
  }, RangeError, '"port" option should be >= 0 and < 65536: -0x1');

  fail({
    port: NaN
  }, RangeError, '"port" option should be >= 0 and < 65536: NaN');

  fail({
    port: Infinity
  }, RangeError, '"port" option should be >= 0 and < 65536: Infinity');

  fail({
    port: -1
  }, RangeError, '"port" option should be >= 0 and < 65536: -1');

  fail({
    port: 65536
  }, RangeError, '"port" option should be >= 0 and < 65536: 65536');

  fail({
    hints: (dns.ADDRCONFIG | dns.V4MAPPED) + 42,
  }, TypeError, 'Invalid argument: hints must use valid flags');
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
