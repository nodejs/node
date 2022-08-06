'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const socket = new net.Socket();
assert.strictEqual(socket.writeQueueSize, -1);

const server = net.createServer()
  .listen(common.PIPE, common.mustCall(() => {
    // The pipe connection is a synchronous operation
    // `net.connect` will set `socket.connecting` to true
    const socket = net.connect(common.PIPE, common.mustCall(() => {
      socket.destroy();
      server.close();
    }));
    // Set connecting to false here to make `socket.write` can call into
    // libuv directly (see `_writeGeneric` in `net.js`).
    // because the socket is connecting currently in libuv, so libuv will
    // insert the write request into write queue, then we can get the
    // size of write queue by `socket.writeQueueSize`.
    socket.connecting = false;
    const data = 'hello';
    socket.write(data, 'utf-8', common.mustCall());
    assert.strictEqual(socket.writeQueueSize, common.isWindows ? 0 : data.length);
    // Restore it
    socket.connecting = true;
  }));
