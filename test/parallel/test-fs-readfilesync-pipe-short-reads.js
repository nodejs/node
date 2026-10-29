'use strict';
// fs.readFileSync(fd, 'utf8') collects short reads in a growing string and
// only moves to a heap buffer at the first full 8 KiB read. On a pipe, more
// than 64 KiB (the heap buffer's first size) can arrive in short reads before
// that happens.
// Refs: https://github.com/nodejs/node/issues/66341
const common = require('../common');

if (!common.isLinux)
  common.skip('Paces the writes with /proc/<pid>/io.');

const assert = require('assert');
const { spawn } = require('child_process');
const fs = require('fs');

const kShortWrite = 4000;
const kShortWrites = 20;
const kTail = 100_000;
const expected = 'x'.repeat(kShortWrite * kShortWrites) + 'y'.repeat(kTail);

if (process.argv[2] === 'child') {
  fs.writeSync(1, 'ready\n');
  assert.strictEqual(fs.readFileSync(0, 'utf8'), expected);
  return;
}

// Bytes the process has received from read() and similar calls.
function readBytes(pid) {
  const io = fs.readFileSync(`/proc/${pid}/io`, 'latin1');
  return Number(/^rchar: (\d+)$/m.exec(io)[1]);
}

const child = spawn(process.execPath, [__filename, 'child'], {
  stdio: ['pipe', 'pipe', 'inherit'],
});

child.stdout.once('data', common.mustCall(() => {
  // Each short write waits until the child, blocked in readFileSync(), has
  // read everything sent before it, so no two of them can be merged into a
  // full read.
  const start = readBytes(child.pid);
  let sent = 0;
  function writeNext() {
    if (readBytes(child.pid) - start < sent) {
      setTimeout(writeNext, 1);
    } else if (sent < kShortWrite * kShortWrites) {
      child.stdin.write('x'.repeat(kShortWrite));
      sent += kShortWrite;
      setTimeout(writeNext, 1);
    } else {
      child.stdin.end('y'.repeat(kTail));
    }
  }
  writeNext();
}));

child.on('exit', common.mustCall((code, signal) => {
  assert.deepStrictEqual({ code, signal }, { code: 0, signal: null });
}));
