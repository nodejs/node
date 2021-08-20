'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isPrimary) {
  const buf = Buffer.from('foobar');

  cluster.setupPrimary({
    stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'pipe']
  });

  const worker = cluster.fork();
  const channel = worker.process.stdio[4];
  let response = '';

  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));

  channel.setEncoding('utf8');
  channel.on('data', (data) => {
    response += data;

    if (response === buf.toString()) {
      worker.disconnect();
    }
  });
  channel.write(buf);
} else {
  const pipe = new net.Socket({ fd: 4 });

  pipe.unref();
  pipe.on('data', (data) => {
    assert.ok(data instanceof Buffer);
    pipe.write(data);
  });
}
