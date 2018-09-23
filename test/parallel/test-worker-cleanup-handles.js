// Flags: --experimental-worker
'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');
const { Server } = require('net');
const fs = require('fs');

if (isMainThread) {
  const w = new Worker(__filename);
  let fd = null;
  w.on('message', common.mustCall((fd_) => {
    assert.strictEqual(typeof fd_, 'number');
    fd = fd_;
  }));
  w.on('exit', common.mustCall((code) => {
    if (fd === -1) {
      // This happens when server sockets don’t have file descriptors,
      // i.e. on Windows.
      return;
    }
    common.expectsError(() => fs.fstatSync(fd),
                        { code: 'EBADF' });
  }));
} else {
  const server = new Server();
  server.listen(0);
  parentPort.postMessage(server._handle.fd);
  server.unref();
}
