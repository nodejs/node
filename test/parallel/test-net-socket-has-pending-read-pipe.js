'use strict';

const common = require('../common');
const assert = require('assert');
const { fork } = require('child_process');
const net = require('net');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'server') {
  const server = net.createServer(common.mustCall((socket) => {
    process.on('message', common.mustCall((message) => {
      assert.strictEqual(message, 'write');
      socket.write('pending', () => process.send('written'));
    }));
  }));

  server.listen(process.argv[3], () => process.send('listening'));
  return;
}

tmpdir.refresh();

const child = fork(__filename, ['server', common.PIPE], {
  stdio: ['ignore', 'ignore', 'inherit', 'ipc'],
});

child.on('message', common.mustCallAtLeast((message) => {
  if (message === 'listening') {
    const socket = net.connect(common.PIPE, () => {
      socket.pause();
      child.send('write');
    });

    child.once('message', common.mustCall((message) => {
      assert.strictEqual(message, 'written');
      assert.strictEqual(socket._handle.hasPendingRead(), true);
      socket.destroy();
      child.kill();
    }));
  }
}));
