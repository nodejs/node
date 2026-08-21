'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const net = require('net');
const { fork } = require('child_process');

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
const server = net.createServer((c) => {
  c.end();
}).listen(common.PIPE, common.mustCall(() => {
  let errored = false;
  tls.connect({ path: common.PIPE })
    .once('error', common.mustCall((e) => {
      assert.strictEqual(e.code, 'ECONNRESET');
      assert.strictEqual(e.path, common.PIPE);
      assert.strictEqual(e.port, undefined);
      assert.strictEqual(e.host, undefined);
      assert.strictEqual(e.localAddress, undefined);
      server.close();
      errored = true;
    }))
    .on('close', common.mustCall(() => {
      assert.strictEqual(errored, true);
    }));
}));
