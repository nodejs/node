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
const net = require('net');
const TCP = process.binding('tcp_wrap').TCP;
const Pipe = process.binding('pipe_wrap').Pipe;

common.refreshTmpDir();

function testClients(getSocketOpt, getConnectOpt, getConnectCb) {
  const cloneOptions = (index) =>
    Object.assign({}, getSocketOpt(index), getConnectOpt(index));
  return [
    net.connect(cloneOptions(0), getConnectCb(0)),
    net.connect(cloneOptions(1))
      .on('connect', getConnectCb(1)),
    net.createConnection(cloneOptions(2), getConnectCb(2)),
    net.createConnection(cloneOptions(3))
      .on('connect', getConnectCb(3)),
    new net.Socket(getSocketOpt(4)).connect(getConnectOpt(4), getConnectCb(4)),
    new net.Socket(getSocketOpt(5)).connect(getConnectOpt(5))
      .on('connect', getConnectCb(5))
  ];
}

const CLIENT_VARIANTS = 6;  // Same length as array above
const forAllClients = (cb) => common.mustCall(cb, CLIENT_VARIANTS);

// Test allowHalfOpen
{
  let counter = 0;
  const server = net.createServer({
    allowHalfOpen: true
  })
  .on('connection', forAllClients(function(socket) {
    socket.resume();
    // 'end' on each socket must not be emitted twice
    socket.on('end', common.mustCall(function() {}, 1));
    socket.end();
  }))
  .listen(0, common.mustCall(function() {
    const getSocketOpt = () => ({ allowHalfOpen: true });
    const getConnectOpt = () => ({
      host: server.address().address,
      port: server.address().port,
    });
    const getConnectCb = () => common.mustCall(function() {
      const client = this;
      client.resume();
      client.on('end', common.mustCall(function() {
        setTimeout(function() {
          // when allowHalfOpen is true, client must still be writable
          // after the server closes the connections, but not readable
          assert(!client.readable);
          assert(client.writable);
          assert(client.write('foo'));
          client.end();
        }, 50);
      }));
      client.on('close', common.mustCall(function() {
        counter++;
        if (counter === CLIENT_VARIANTS) {
          setTimeout(() => {
            server.close();
          }, 50);
        }
      }));
    });

    testClients(getSocketOpt, getConnectOpt, getConnectCb);
  }));
}

// Test TCP fd is wrapped correctly
if (!common.isWindows) {  // Doesn't support this on windows
  let counter = 0;
  const server = net.createServer()
  .on('connection', forAllClients(function(socket) {
    socket.end('ok');
  }))
  .listen(0, common.mustCall(function() {
    const handleMap = new Map();
    const getSocketOpt = (index) => {
      const handle = new TCP();
      const address = server.address().address;
      let err = 0;
      if (net.isIPv6(address)) {
        err = handle.bind6(address, 0);
      } else {
        err = handle.bind(address, 0);
      }

      assert(err >= 0, '' + err);
      assert.notStrictEqual(handle.fd, -1);
      handleMap.set(index, handle);
      return { fd: handle.fd };
    };

    const getConnectOpt = () => ({
      host: server.address().address,
      port: server.address().port,
    });

    const getConnectCb = (index) => common.mustCall(function() {
      const client = this;
      // Test if it's wrapping an existing fd
      assert(handleMap.has(index));
      const oldHandle = handleMap.get(index);
      assert.strictEqual(oldHandle.fd, this._handle.fd);
      client.end();
      client.on('close', common.mustCall(function() {
        oldHandle.close();
        counter++;
        if (counter === CLIENT_VARIANTS) {
          server.close();
        }
      }));
    });

    testClients(getSocketOpt, getConnectOpt, getConnectCb);
  }));
}

// Test Pipe fd is wrapped correctly
if (!common.isWindows) {  // Doesn't support this on windows
  const prefix = `${common.PIPE}-net-connect-options-fd`;
  const serverPath = `${prefix}-server`;
  let counter = 0;
  let socketCounter = 0;
  const server = net.createServer()
  .on('connection', forAllClients(function(socket) {
    socket.end('ok');
  }))
  .listen({path: serverPath}, common.mustCall(function() {
    const handleMap = new Map();
    const getSocketOpt = (index) => {
      const handle = new Pipe();
      const err = handle.bind(`${prefix}-client-${socketCounter++}`);
      assert(err >= 0, '' + err);
      assert.notStrictEqual(handle.fd, -1);
      handleMap.set(index, handle);
      return { fd: handle.fd };
    };
    const getConnectOpt = () => ({
      path: serverPath
    });
    const getConnectCb = (index) => common.mustCall(function() {
      const client = this;
      // Test if it's wrapping an existing fd
      assert(handleMap.has(index));
      const oldHandle = handleMap.get(index);
      assert.strictEqual(oldHandle.fd, this._handle.fd);
      client.end();
      client.on('close', common.mustCall(function() {
        oldHandle.close();
        counter++;
        if (counter === CLIENT_VARIANTS) {
          server.close();
        }
      }));
    });

    testClients(getSocketOpt, getConnectOpt, getConnectCb);
  }));
}
