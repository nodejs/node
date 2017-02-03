'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

if (process.argv[2] !== 'child') {
  console.error('[%d] master', process.pid);

  const worker = fork(__filename, ['child']);
  let result = '';

  worker.once('message', common.mustCall((msg, handle) => {
    assert.strictEqual(msg, 'handle');
    assert.ok(handle);
    worker.send('got');

    handle.on('data', (data) => {
      result += data.toString();
    });

    handle.on('end', () => {
      assert.strictEqual(result, 'hello');
      worker.kill();
    });
  }));
} else {
  console.error('[%d] worker', process.pid);

  let socket;
  let cbcalls = 0;
  function socketConnected() {
    if (++cbcalls === 2)
      process.send('handle', socket);
  }

  const server = net.createServer((c) => {
    process.once('message', common.mustCall((msg) => {
      assert.strictEqual(msg, 'got');
      c.end('hello');
    }));
    socketConnected();
  });

  server.listen(common.PORT, () => {
    socket = net.connect(server.address().port, server.address().address,
                         socketConnected);
  });

  process.on('disconnect', () => {
    server.close();
  });
}
