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
  const portTypeError = {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  };

  syncFailToConnect(true, portTypeError);
  syncFailToConnect(false, portTypeError);
  syncFailToConnect([], portTypeError, true);
  syncFailToConnect({}, portTypeError, true);
  syncFailToConnect(null, portTypeError);
}

// Test out of range ports
{
  const portRangeError = {
    code: 'ERR_SOCKET_BAD_PORT',
    name: 'RangeError'
  };

  syncFailToConnect('', portRangeError);
  syncFailToConnect(' ', portRangeError);
  syncFailToConnect('0x', portRangeError, true);
  syncFailToConnect('-0x1', portRangeError, true);
  syncFailToConnect(NaN, portRangeError);
  syncFailToConnect(Infinity, portRangeError);
  syncFailToConnect(-1, portRangeError);
  syncFailToConnect(65536, portRangeError);
}

// Test invalid hints
{
  // connect({hint}, cb) and connect({hint})
  const hints = (dns.ADDRCONFIG | dns.V4MAPPED | dns.ALL) + 42;
  const hintOptBlocks = doConnect([{ port: 42, hints }],
                                  () => common.mustNotCall());
  for (const fn of hintOptBlocks) {
    assert.throws(fn, {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
      message: /The argument 'hints' is invalid\. Received \d+/
    });
  }
}

// Test valid combinations of connect(port) and connect(port, host)
{
  const expectedConnections = 72;
  let serverConnected = 0;

  const server = net.createServer(common.mustCall((socket) => {
    socket.end('ok');
    if (++serverConnected === expectedConnections) {
      server.close();
    }
  }, expectedConnections));

  server.listen(0, common.localhostIPv4, common.mustCall(() => {
    const port = server.address().port;

    // Total connections = 3 * 4(canConnect) * 6(doConnect) = 72
    canConnect(port);
    canConnect(String(port));
    canConnect(`0x${port.toString(16)}`);
  }));

  // Try connecting to random ports, but do so once the server is closed
  server.on('close', () => {
    asyncFailToConnect(0);
  });
}

function doConnect(args, getCb) {
  return [
    function createConnectionWithCb() {
      return net.createConnection.apply(net, args.concat(getCb()))
        .resume();
    },
    function createConnectionWithoutCb() {
      return net.createConnection.apply(net, args)
        .on('connect', getCb())
        .resume();
    },
    function connectWithCb() {
      return net.connect.apply(net, args.concat(getCb()))
        .resume();
    },
    function connectWithoutCb() {
      return net.connect.apply(net, args)
        .on('connect', getCb())
        .resume();
    },
    function socketConnectWithCb() {
      const socket = new net.Socket();
      return socket.connect.apply(socket, args.concat(getCb()))
        .resume();
    },
    function socketConnectWithoutCb() {
      const socket = new net.Socket();
      return socket.connect.apply(socket, args)
        .on('connect', getCb())
        .resume();
    },
  ];
}

function syncFailToConnect(port, assertErr, optOnly) {
  const family = 4;
  if (!optOnly) {
    // connect(port, cb) and connect(port)
    const portArgFunctions = doConnect([{ port, family }],
                                       () => common.mustNotCall());
    for (const fn of portArgFunctions) {
      assert.throws(fn, assertErr, `${fn.name}(${port})`);
    }

    // connect(port, host, cb) and connect(port, host)
    const portHostArgFunctions = doConnect([{ port,
                                              host: 'localhost',
                                              family }],
                                           () => common.mustNotCall());
    for (const fn of portHostArgFunctions) {
      assert.throws(fn, assertErr, `${fn.name}(${port}, 'localhost')`);
    }
  }
  // connect({port}, cb) and connect({port})
  const portOptFunctions = doConnect([{ port, family }],
                                     () => common.mustNotCall());
  for (const fn of portOptFunctions) {
    assert.throws(fn, assertErr, `${fn.name}({port: ${port}})`);
  }

  // connect({port, host}, cb) and connect({port, host})
  const portHostOptFunctions = doConnect([{ port: port,
                                            host: 'localhost',
                                            family: family }],
                                         () => common.mustNotCall());
  for (const fn of portHostOptFunctions) {
    assert.throws(fn,
                  assertErr,
                  `${fn.name}({port: ${port}, host: 'localhost'})`);
  }
}

function canConnect(port) {
  const noop = () => common.mustCall();
  const family = 4;

  // connect(port, cb) and connect(port)
  const portArgFunctions = doConnect([{ port, family }], noop);
  for (const fn of portArgFunctions) {
    fn();
  }

  // connect(port, host, cb) and connect(port, host)
  const portHostArgFunctions = doConnect([{ port, host: 'localhost', family }],
                                         noop);
  for (const fn of portHostArgFunctions) {
    fn();
  }

  // connect({port}, cb) and connect({port})
  const portOptFunctions = doConnect([{ port, family }], noop);
  for (const fn of portOptFunctions) {
    fn();
  }

  // connect({port, host}, cb) and connect({port, host})
  const portHostOptFns = doConnect([{ port, host: 'localhost', family }],
                                   noop);
  for (const fn of portHostOptFns) {
    fn();
  }
}

function asyncFailToConnect(port) {
  const onError = () => common.mustCall((err) => {
    const regexp = /^Error: connect E\w+.+$/;
    assert.match(String(err), regexp);
  });

  const dont = () => common.mustNotCall();
  const family = 4;
  // connect(port, cb) and connect(port)
  const portArgFunctions = doConnect([{ port, family }], dont);
  for (const fn of portArgFunctions) {
    fn().on('error', onError());
  }

  // connect({port}, cb) and connect({port})
  const portOptFunctions = doConnect([{ port, family }], dont);
  for (const fn of portOptFunctions) {
    fn().on('error', onError());
  }

  // connect({port, host}, cb) and connect({port, host})
  const portHostOptFns = doConnect([{ port, host: 'localhost', family }],
                                   dont);
  for (const fn of portHostOptFns) {
    fn().on('error', onError());
  }
}
