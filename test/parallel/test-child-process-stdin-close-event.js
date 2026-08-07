'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

if (common.isWindows) {
  common.skip('Not applicable on Windows');
}

function spawnChild(script) {
  return spawn(process.execPath, ['-e', script], {
    stdio: ['pipe', 'ignore', 'ignore'],
  });
}

function runTest({ script, onSpawn }, done) {
  const child = spawnChild(script);

  const timeout = setTimeout(() => {
    assert.fail('stdin close event was not emitted');
  }, 2000);

  let closed = false;
  let exited = false;
  function maybeDone() {
    if (!closed || !exited) return;
    clearTimeout(timeout);
    done();
  }

  child.stdin.once('close', common.mustCall(() => {
    closed = true;
    child.kill();
    maybeDone();
  }));

  child.once('exit', common.mustCall(() => {
    exited = true;
    maybeDone();
  }));

  onSpawn?.(child);
}

runTest({
  script: 'setTimeout(() => require("fs").closeSync(0), 50); setTimeout(() => {}, 2000)',
}, common.mustCall(() => {
  runTest({
    script: 'setTimeout(() => require("fs").closeSync(0), 200); setTimeout(() => {}, 2000)',
    onSpawn: common.mustCall((child) => {
      const handle = child.stdin?._handle;
      assert.strictEqual(typeof handle?.watchPeerClose, 'function');
      handle.watchPeerClose(true, common.mustNotCall());
    }),
  }, common.mustCall(() => {
    runTest({
      script: 'setTimeout(() => {}, 2000)',
      onSpawn: common.mustCall((child) => {
        const handle = child.stdin?._handle;
        assert.strictEqual(typeof handle?.watchPeerClose, 'function');

        child.stdin.once('close', common.mustCall(() => {
          // Calling watchPeerClose again after close must not throw.
          handle.watchPeerClose(true, common.mustNotCall());
        }));
        child.stdin.destroy();
      }),
    }, common.mustCall());
  }));
}));
