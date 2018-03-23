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
const fork = require('child_process').fork;
const net = require('net');

// child
if (process.argv[2] === 'child') {

  // Check that the 'disconnect' event is deferred to the next event loop tick.
  const disconnect = process.disconnect;
  process.disconnect = function() {
    disconnect.apply(this, arguments);
    // If the event is emitted synchronously, we're too late by now.
    process.once('disconnect', common.mustCall(disconnectIsNotAsync));
    // The funky function name makes it show up legible in mustCall errors.
    function disconnectIsNotAsync() {}
  };

  const server = net.createServer();

  server.on('connection', function(socket) {

    socket.resume();

    process.on('disconnect', function() {
      socket.end((process.connected).toString());
    });

    // when the socket is closed, we will close the server
    // allowing the process to self terminate
    socket.on('end', function() {
      server.close();
    });

    socket.write('ready');
  });

  // when the server is ready tell parent
  server.on('listening', function() {
    process.send({ msg: 'ready', port: server.address().port });
  });

  server.listen(0);

} else {
  // testcase
  const child = fork(process.argv[1], ['child']);

  let exitEmitted = false;
  let disconnectEmitted = false;
  let gotMessage = false;

  // when calling .disconnect the event should emit
  // and the disconnected flag should be true.
  child.on('disconnect', common.mustCall(function() {
    assert.strictEqual(child.connected, false);

    disconnectEmitted = true;
  }));

  // the process should also self terminate without using signals
  child.on('exit', common.mustCall(function(code, signal) {
    assert.strictEqual(disconnectEmitted, true);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);

    exitEmitted = true;
  }));

  // when child is listening
  child.on('message', function(obj) {
    assert.ok(obj && obj.msg === 'ready');

    gotMessage = true;

    // connect to child using TCP to know if disconnect was emitted
    const socket = net.connect(obj.port);

    socket.setEncoding('utf-8');

    socket.on('data', function(data) {
      // ready to be disconnected
      if (data === 'ready') {
        child.disconnect();
        assert.throws(child.disconnect.bind(child),
                      { code: 'ERR_IPC_DISCONNECTED' });
        return;
      }

      assert.strictEqual(data, 'false');
    });
  });

  // The process emits 'close' after all streams have flushed and it has exited.
  child.on('close', common.mustCall(function() {
    assert.strictEqual(disconnectEmitted, true);
    assert.strictEqual(exitEmitted, true);
    assert.strictEqual(gotMessage, true);
  }));
}
