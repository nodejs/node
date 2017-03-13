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
const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const net = require('net');

// Test wrong type of ports
{
  function portTypeError(opt) {
    const prefix = '"port" option should be a number or string: ';
    const cleaned = opt.replace(/[\\^$.*+?()[\]{}|=!<>:-]/g, '\\$&');
    return new RegExp(`^TypeError: ${prefix}${cleaned}$`);
  }

  syncFailToConnect(true, portTypeError('true'));
  syncFailToConnect(false, portTypeError('false'));
  syncFailToConnect([], portTypeError(''), true);
  syncFailToConnect({}, portTypeError('[object Object]'), true);
  syncFailToConnect(null, portTypeError('null'));
}

// Test out of range ports
{
  function portRangeError(opt) {
    const prefix = '"port" option should be >= 0 and < 65536: ';
    const cleaned = opt.replace(/[\\^$.*+?()[\]{}|=!<>:-]/g, '\\$&');
    return new RegExp(`^RangeError: ${prefix}${cleaned}$`);
  }

  syncFailToConnect('', portRangeError(''));
  syncFailToConnect(' ', portRangeError(' '));
  syncFailToConnect('0x', portRangeError('0x'), true);
  syncFailToConnect('-0x1', portRangeError('-0x1'), true);
  syncFailToConnect(NaN, portRangeError('NaN'));
  syncFailToConnect(Infinity, portRangeError('Infinity'));
  syncFailToConnect(-1, portRangeError('-1'));
  syncFailToConnect(65536, portRangeError('65536'));
}

// Test invalid hints
{
  const regexp = /^TypeError: Invalid argument: hints must use valid flags$/;
  // connect({hint}, cb) and connect({hint})
  const hints = (dns.ADDRCONFIG | dns.V4MAPPED) + 42;
  const hintOptBlocks = doConnect([{hints: hints}],
                                  () => common.mustNotCall());
  for (const block of hintOptBlocks) {
    assert.throws(block, regexp,
                  `${block.name}({hints: ${hints})`);
  }
}

// Test valid combinations of connect(port) and connect(port, host)
{
  const expectedConnections = 72;
  let serverConnected = 0;

  const server = net.createServer(common.mustCall(function(socket) {
    socket.end('ok');
    if (++serverConnected === expectedConnections) {
      server.close();
    }
  }, expectedConnections));

  server.listen(0, 'localhost', common.mustCall(function() {
    const port = this.address().port;

    // Total connections = 3 * 4(canConnect) * 6(doConnect) = 72
    canConnect(port);
    canConnect(port + '');
    canConnect('0x' + port.toString(16));
  }));

  // Try connecting to random ports, but do so once the server is closed
  server.on('close', function() {
    asyncFailToConnect(0);
    asyncFailToConnect(/* undefined */);
  });
}

function doConnect(args, getCb) {
  return [
    function createConnectionWithCb() {
      return net.createConnection.apply(net, args.concat(getCb()));
    },
    function createConnectionWithoutCb() {
      return net.createConnection.apply(net, args)
        .on('connect', getCb());
    },
    function connectWithCb() {
      return net.connect.apply(net, args.concat(getCb()));
    },
    function connectWithoutCb() {
      return net.connect.apply(net, args)
        .on('connect', getCb());
    },
    function socketConnectWithCb() {
      const socket = new net.Socket();
      return socket.connect.apply(socket, args.concat(getCb()));
    },
    function socketConnectWithoutCb() {
      const socket = new net.Socket();
      return socket.connect.apply(socket, args)
        .on('connect', getCb());
    }
  ];
}

function syncFailToConnect(port, regexp, optOnly) {
  if (!optOnly) {
    // connect(port, cb) and connect(port)
    const portArgBlocks = doConnect([port], () => common.mustNotCall());
    for (const block of portArgBlocks) {
      assert.throws(block, regexp, `${block.name}(${port})`);
    }

    // connect(port, host, cb) and connect(port, host)
    const portHostArgBlocks = doConnect([port, 'localhost'],
                                        () => common.mustNotCall());
    for (const block of portHostArgBlocks) {
      assert.throws(block, regexp, `${block.name}(${port}, 'localhost')`);
    }
  }
  // connect({port}, cb) and connect({port})
  const portOptBlocks = doConnect([{port}],
                                  () => common.mustNotCall());
  for (const block of portOptBlocks) {
    assert.throws(block, regexp, `${block.name}({port: ${port}})`);
  }

  // connect({port, host}, cb) and connect({port, host})
  const portHostOptBlocks = doConnect([{port: port, host: 'localhost'}],
                                      () => common.mustNotCall());
  for (const block of portHostOptBlocks) {
    assert.throws(block, regexp,
                  `${block.name}({port: ${port}, host: 'localhost'})`);
  }
}

function canConnect(port) {
  const noop = () => common.mustCall(function() {});
  // connect(port, cb) and connect(port)
  const portArgBlocks = doConnect([port], noop);
  for (const block of portArgBlocks) {
    assert.doesNotThrow(block, `${block.name}(${port})`);
  }

  // connect(port, host, cb) and connect(port, host)
  const portHostArgBlocks = doConnect([port, 'localhost'], noop);
  for (const block of portHostArgBlocks) {
    assert.doesNotThrow(block, `${block.name}(${port})`);
  }

  // connect({port}, cb) and connect({port})
  const portOptBlocks = doConnect([{port}], noop);
  for (const block of portOptBlocks) {
    assert.doesNotThrow(block, `${block.name}({port: ${port}})`);
  }

  // connect({port, host}, cb) and connect({port, host})
  const portHostOptBlocks = doConnect([{port: port, host: 'localhost'}],
                                      noop);
  for (const block of portHostOptBlocks) {
    assert.doesNotThrow(block,
                        `${block.name}({port: ${port}, host: 'localhost'})`);
  }
}

function asyncFailToConnect(port) {
  const onError = () => common.mustCall(function(err) {
    const regexp = /^Error: connect (E\w+)(.+)$/;
    assert(regexp.test(err + ''), err + '');
  });

  const dont = () => common.mustNotCall();
  // connect(port, cb) and connect(port)
  const portArgBlocks = doConnect([port], dont);
  for (const block of portArgBlocks) {
    assert.doesNotThrow(function() {
      block().on('error', onError());
    }, `${block.name}(${port})`);
  }

  // connect({port}, cb) and connect({port})
  const portOptBlocks = doConnect([{port}], dont);
  for (const block of portOptBlocks) {
    assert.doesNotThrow(function() {
      block().on('error', onError());
    }, `${block.name}({port: ${port}})`);
  }

  // connect({port, host}, cb) and connect({port, host})
  const portHostOptBlocks = doConnect([{port: port, host: 'localhost'}],
                                      dont);
  for (const block of portHostOptBlocks) {
    assert.doesNotThrow(function() {
      block().on('error', onError());
    }, `${block.name}({port: ${port}, host: 'localhost'})`);
  }
}
