'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const dgram = require('dgram');

function noop() { }

if (cluster.isMaster) {
  const worker1 = cluster.fork();

  if (common.isWindows) {
    const checkErrType = (er) => {
      assert.strictEqual(er.code, 'ENOTSUP');
      worker1.kill();
    };

    worker1.on('error', common.mustCall(checkErrType, 1));
    return;
  }

  worker1.on('message', (msg) => {
    assert.strictEqual(msg, 'success');
    const worker2 = cluster.fork();

    worker2.on('message', (msg) => {
      assert.strictEqual(msg, 'socket2:EADDRINUSE');
      worker1.kill();
      worker2.kill();
    });
  });
} else {
  const socket1 = dgram.createSocket('udp4', noop);
  const socket2 = dgram.createSocket('udp4', noop);

  socket1.on('error', (err) => {
    // no errors expected
    process.send('socket1:' + err.code);
  });

  socket2.on('error', (err) => {
    // an error is expected on the second worker
    process.send('socket2:' + err.code);
  });

  socket1.bind({
    address: 'localhost',
    port: common.PORT,
    exclusive: false
  }, () => {
    socket2.bind({ port: common.PORT + 1, exclusive: true }, () => {
      // the first worker should succeed
      process.send('success');
    });
  });
}
