'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

// Tests that a process which never changed the terminal mode does not write
// the terminal attributes recorded at startup back on exit. Doing so would
// clobber changes made in the meantime by another process sharing the
// terminal, e.g. the pager in `node -h | less`.

const child = child_process.spawn(process.execPath, [
  '-e', 'console.log("Y"); require("fs").readSync(3, Buffer.alloc(1));',
], { stdio: ['inherit', 'pipe', 'inherit', 'pipe'] });

child.stdout.on('data', common.mustCall(() => {
  // The child has recorded the terminal state by now. Change it behind its
  // back, like a pager does, then let the child exit.
  process.stdin.setRawMode(true);
  child.stdio[3].end('x');
}));

child.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
  const { stdout } = child_process.spawnSync('stty', ['-a'], {
    stdio: ['inherit', 'pipe', 'inherit'],
    encoding: 'utf8',
  });
  process.stdin.setRawMode(false);
  assert.match(stdout, /-icanon\b/);
}));
