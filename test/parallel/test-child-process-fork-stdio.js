'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const net = require('net');

if (process.argv[2] === 'child') {
  process.stdout.write('this should be ignored');
  process.stderr.write('this should not be ignored');

  const pipe = new net.Socket({ fd: 4 });

  process.on('disconnect', () => {
    pipe.unref();
  });

  pipe.setEncoding('utf8');
  pipe.on('data', (data) => {
    process.send(data);
  });
} else {
  assert.throws(() => {
    cp.fork(__filename, {stdio: ['pipe', 'pipe', 'pipe', 'pipe']});
  }, /Forked processes must have an IPC channel/);

  let ipc = '';
  let stderr = '';
  const buf = Buffer.from('data to send via pipe');
  const child = cp.fork(__filename, ['child'], {
    stdio: [0, 'ignore', 'pipe', 'ipc', 'pipe']
  });

  assert.strictEqual(child.stdout, null);

  child.on('message', (msg) => {
    ipc += msg;

    if (ipc === buf.toString()) {
      child.disconnect();
    }
  });

  child.stderr.on('data', (chunk) => {
    stderr += chunk;
  });

  child.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stderr, 'this should not be ignored');
  }));

  child.stdio[4].write(buf);
}
