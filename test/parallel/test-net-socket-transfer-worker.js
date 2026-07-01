'use strict';

// This test verifies that an accepted net.Socket connection can be transferred
// to a worker thread via worker_threads postMessage()'s transferList, and that
// the worker can read from and write to it. The main thread accepts the
// connection and hands it off; the worker echoes data back.

const common = require('../common');

if (common.isWindows) {
  common.skip('transferring TCP handles between threads is not supported on ' +
              'Windows yet');
}

const assert = require('assert');
const net = require('net');
const {
  Worker,
  isMainThread,
  parentPort,
} = require('worker_threads');

if (!isMainThread) {
  // Worker side: receive the transferred connection and echo everything back.
  parentPort.on('message', common.mustCall(({ socket }) => {
    assert.ok(socket instanceof net.Socket);
    socket.setEncoding('utf8');
    socket.on('data', common.mustCall((chunk) => {
      socket.end(`echo:${chunk}`);
    }));
  }));
  return;
}

const worker = new Worker(__filename);

const server = net.createServer(common.mustCall((socket) => {
  // Hand the freshly accepted connection off to the worker. It must not be
  // read from in this thread before transferring.
  worker.postMessage({ socket }, [socket]);

  // The source socket is now owned by the worker: it is detached and destroyed
  // on this side, so further use fails cleanly instead of dropping data.
  assert.strictEqual(socket._handle, null);
  assert.strictEqual(socket.destroyed, true);
  assert.strictEqual(socket.writable, false);
  socket.write('lost', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
  }));

  server.close();
}));

server.listen(0, common.mustCall(() => {
  const client = net.connect(server.address().port, common.mustCall(() => {
    client.write('hello');
  }));
  client.setEncoding('utf8');
  let response = '';
  client.on('data', (chunk) => { response += chunk; });
  client.on('end', common.mustCall(() => {
    assert.strictEqual(response, 'echo:hello');
    worker.terminate();
  }));
}));
