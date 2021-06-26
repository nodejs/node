'use strict';
const common = require('../common');

// On some OS X versions, when passing fd's between processes:
// When the handle associated to a specific file descriptor is closed by the
// sender process before it's received in the destination, the handle is indeed
// closed while it should remain opened. In order to fix this behavior, don't
// close the handle until the `NODE_HANDLE_ACK` is received by the sender.
// This test is basically `test-cluster-net-send` but creating lots of workers
// so the issue reproduces on OS X consistently.

if (process.config.variables.arm_version === '7') {
  common.skip('Too slow for armv7 bots');
}

const assert = require('assert');
const { fork } = require('child_process');
const net = require('net');

const N = 80;
let messageCallbackCount = 0;

function forkWorker() {
  const messageCallback = (msg, handle) => {
    messageCallbackCount++;
    assert.strictEqual(msg, 'handle');
    assert.ok(handle);
    worker.send('got');

    let recvData = '';
    handle.on('data', common.mustCall((data) => {
      recvData += data;
    }));

    handle.on('end', () => {
      assert.strictEqual(recvData, 'hello');
      worker.kill();
    });
  };

  const worker = fork(__filename, ['child']);
  worker.on('error', (err) => {
    if (/\bEAGAIN\b/.test(err.message)) {
      forkWorker();
      return;
    }
    throw err;
  });
  worker.once('message', messageCallback);
}

if (process.argv[2] !== 'child') {
  for (let i = 0; i < N; ++i) {
    forkWorker();
  }
  process.on('exit', () => { assert.strictEqual(messageCallbackCount, N); });
} else {
  let socket;
  let cbcalls = 0;
  function socketConnected() {
    if (++cbcalls === 2)
      process.send('handle', socket);
  }

  // As a side-effect, listening for the message event will ref the IPC channel,
  // so the child process will stay alive as long as it has a parent process/IPC
  // channel. Once this is done, we can unref our client and server sockets, and
  // the only thing keeping this worker alive will be IPC. This is important,
  // because it means a worker with no parent will have no referenced handles,
  // thus no work to do, and will exit immediately, preventing process leaks.
  process.on('message', common.mustCall());

  const server = net.createServer((c) => {
    process.once('message', (msg) => {
      assert.strictEqual(msg, 'got');
      c.end('hello');
    });
    socketConnected();
  }).unref();
  server.listen(0, common.localhostIPv4, () => {
    const { port } = server.address();
    socket = net.connect(port, common.localhostIPv4, socketConnected).unref();
  });
}
