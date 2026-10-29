'use strict';

// This test verifies that a listening net.Server can be transferred to a worker
// thread via worker_threads postMessage()'s transferList. The parent thread
// binds and listens, then hands the server off; the worker accepts connections and
// responds, proving the listening socket (and its accept queue) moved loops.

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
  parentPort.on('message', common.mustCall(({ server }) => {
    assert.ok(server instanceof net.Server);
    server.on('connection', (socket) => {
      socket.end(`served-by:${threadId}`);
    });
  }));
  return;
}

const worker = new Worker(__filename, { workerData: { role: 'server' } });

const server = net.createServer();

server.listen(0, common.mustCall(() => {
  const { port } = server.address();

  // Move the listening server to the worker thread.
  worker.postMessage({ server }, [server]);
  assert.strictEqual(server._handle, null);

  const N = 3;
  let done = 0;
  for (let i = 0; i < N; i++) {
    const client = net.connect(port);
    client.setEncoding('utf8');
    let response = '';
    client.on('data', (chunk) => { response += chunk; });
    client.on('end', common.mustCall(() => {
      assert.match(response, /^served-by:\d+$/);
      assert.notStrictEqual(response, `served-by:${threadId}`);
      if (++done === N) {
        worker.terminate();
      }
    }));
  }
}));
