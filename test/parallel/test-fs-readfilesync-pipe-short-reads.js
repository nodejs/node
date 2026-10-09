'use strict';
const common = require('../common');

// readFileSync(fd, 'utf8') on a pipe must not overflow its heap buffer when
// more than 64 KiB arrive in short reads before the first full 8 KiB read.

if (common.isWindows)
  common.skip('short-read timing on Windows pipes is not reliable');

const assert = require('assert');
const fs = require('fs');
const { spawn } = require('child_process');

const kChunks = 24;
const kChunkSize = 4000;
const kTailSize = 400000;

if (process.argv[2] === 'child') {
  process.stderr.write('r');
  process.stdout.write(String(fs.readFileSync(0, 'utf8').length));
  return;
}

const child = spawn(process.execPath, [__filename, 'child']);

// Pace the short writes so each arrives as its own read, then send one large
// write to force the switch to the heap buffer.
child.stderr.once('data', common.mustCall(() => {
  let i = 0;
  const timer = setInterval(() => {
    child.stdin.write('x'.repeat(kChunkSize));
    if (++i === kChunks) {
      clearInterval(timer);
      child.stdin.end('y'.repeat(kTailSize));
    }
  }, common.platformTimeout(15));
}));

let stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', (d) => { stdout += d; });
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
  assert.strictEqual(stdout, String(kChunks * kChunkSize + kTailSize));
}));
