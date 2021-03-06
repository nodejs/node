'use strict';

const common = require('../common');
const assert = require('assert');
const { fork, spawn } = require('child_process');
const net = require('net');

const tmpdir = require('../common/tmpdir');

// Run in a child process because the PIPE file descriptor stays open until
// Node.js completes, blocking the tmpdir and preventing cleanup.

if (process.argv[2] !== 'child') {
  // Parent
  tmpdir.refresh();

  // Run test
  const child = fork(__filename, ['child'], { stdio: 'inherit' });
  child.on('exit', common.mustCall(function(code) {
    assert.strictEqual(code, 0);
  }));

  return;
}

// Child
const server = net.createServer((conn) => {
  spawn(process.execPath, ['-v'], {
    stdio: ['ignore', conn, 'ignore']
  }).on('close', common.mustCall(() => {
    conn.end();
  }));
}).listen(common.PIPE, () => {
  const client = net.connect(common.PIPE, common.mustCall());
  client.once('data', () => {
    client.end(() => {
      server.close();
    });
  });
});
