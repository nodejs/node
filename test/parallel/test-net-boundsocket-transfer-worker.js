'use strict';

// This test verifies that an un-adopted net.BoundSocket can be transferred to
// a worker thread via worker_threads postMessage()'s transferList. The parent
// thread binds synchronously to reserve the port, then hands the bound socket
// off; the worker adopts it via server.listen() and accepts connections.

const common = require('../common');

const assert = require('assert');
const net = require('net');
const {
  Worker,
  parentPort,
  threadId,
  workerData,
} = require('worker_threads');

if (workerData?.role === 'server') {
  parentPort.on('message', common.mustCall(({ bound, port }) => {
    assert.ok(bound instanceof net.BoundSocket);
    // The bound address survives the transfer.
    assert.strictEqual(bound.address().port, port);

    const server = net.createServer((socket) => {
      socket.end(`served-by:${threadId}`);
    });
    server.listen(bound, common.mustCall(() => {
      assert.strictEqual(server.address().port, port);
      // Adoption consumed the bound socket.
      assert.throws(() => bound.address(), {
        code: 'ERR_SOCKET_HANDLE_ADOPTED',
      });
      parentPort.postMessage('listening');
    }));
  }));
  return;
}

const worker = new Worker(__filename, { workerData: { role: 'server' } });

const bound = new net.BoundSocket({ host: '127.0.0.1', port: 0 });
const { port } = bound.address();

// Move the bound socket to the worker thread.
worker.postMessage({ bound, port }, [bound]);

// The source is left in the adopted state; further use throws.
assert.throws(() => bound.address(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });
assert.throws(() => bound.fd(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });
assert.throws(() => bound.close(), { code: 'ERR_SOCKET_HANDLE_ADOPTED' });

worker.on('message', common.mustCall((msg) => {
  assert.strictEqual(msg, 'listening');
  const client = net.connect(port, '127.0.0.1');
  client.setEncoding('utf8');
  let response = '';
  client.on('data', (chunk) => { response += chunk; });
  client.on('end', common.mustCall(() => {
    assert.match(response, /^served-by:\d+$/);
    assert.notStrictEqual(response, `served-by:${threadId}`);
    worker.terminate();
  }));
}));
