'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const net = require('net');

if (process.argv[2] === 'child') {
  const server = net.createServer(common.mustCall());
  server.listen(0, common.mustCall(() => {
    process.send({ type: 'ready', data: { port: server.address().port } });
  }));
} else {
  const cp = spawn(process.execPath,
                   [__filename, 'child'],
                   {
                     stdio: ['ipc', 'inherit', 'inherit']
                   });

  cp.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, null);
    assert.strictEqual(signal, 'SIGKILL');
  }));

  cp.on('message', common.mustCall((msg) => {
    const { type, data } = msg;
    assert.strictEqual(type, 'ready');
    const port = data.port;

    const conn = net.createConnection({
      port,
      onread: {
        buffer: Buffer.alloc(65536),
        callback: () => {},
      }
    });

    conn.on('error', (err) => {
      // Error emitted on Windows.
      assert.strictEqual(err.code, 'ECONNRESET');
    });

    conn.on('connect', common.mustCall(() => {
      cp.kill('SIGKILL');
    }));
  }));
}
