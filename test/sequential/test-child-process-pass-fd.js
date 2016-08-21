'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

if ((process.config.variables.arm_version === '6') ||
    (process.config.variables.arm_version === '7')) {
  common.skip('Too slow for armv6 and armv7 bots');
  return;
}

const N = 80;

if (process.argv[2] !== 'child') {
  for (let i = 0; i < N; ++i) {
    const worker = fork(__filename, ['child']);
    worker.once('message', common.mustCall((msg, handle) => {
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
    }));
  }
} else {
  let socket;
  let cbcalls = 0;
  function socketConnected() {
    if (++cbcalls === 2)
      process.send('handle', socket);
  }

  const server = net.createServer((c) => {
    process.once('message', function(msg) {
      assert.strictEqual(msg, 'got');
      c.end('hello');
    });
    socketConnected();
  });
  server.listen(0, common.localhostIPv4, () => {
    const port = server.address().port;
    socket = net.connect(port, common.localhostIPv4, socketConnected);
  });
}
