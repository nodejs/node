'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const { spawn } = require('node:child_process');
const assert = require('node:assert');

(async () => {
  const child = spawn(
    process.execPath,
    ['--inspect-wait=0', '-e', "console.log('test');"],
    {}
  );

  const url = await new Promise((resolve) => {
    child.stderr.on('data', (data) => {
      const msg = data.toString();
      const match = msg.match(/ws:\/\/127\.0\.0\.1:(\d+)\/([a-f0-9-]+)/);
      if (match) {
        child.stderr.removeAllListeners('data');
        return resolve(match[0]);
      }
    });
  });

  child.once('exit', common.mustCall((_, signal) => {
    assert.strictEqual(signal, 'SIGTERM');
  }));

  const socket = new WebSocket(url);

  socket.addEventListener('open', common.mustCall(() => {
    socket.send('This is not a valid protocol message');
  }));

  socket.addEventListener('message', common.mustCall((event) => {
    assert.ok(Object.keys(JSON.parse(event.data)).includes('error'));
    socket.close();
    child.kill();
  }));
})().then(common.mustCall());
