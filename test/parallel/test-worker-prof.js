'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const assert = require('assert');
const { join } = require('path');
const { spawnSync } = require('child_process');
const { Worker } = require('worker_threads');

// Test that --prof also tracks Worker threads.
// Refs: https://github.com/nodejs/node/issues/24016

if (process.argv[2] === 'child') {
  const spin = `
  const start = Date.now();
  while (Date.now() - start < 1000);
  `;
  new Worker(spin, { eval: true });
  eval(spin);
  return;
}

tmpdir.refresh();
spawnSync(process.execPath, ['--prof', __filename, 'child'],
          { cwd: tmpdir.path });
const files = fs.readdirSync(tmpdir.path);
const logfiles = files.filter((name) => /\.log$/.test(name));
assert.strictEqual(logfiles.length, 2);  // Parent thread + child thread.

for (const logfile of logfiles) {
  const lines = fs.readFileSync(join(tmpdir.path, logfile), 'utf8').split('\n');
  const ticks = lines.filter((line) => /^tick,/.test(line)).length;

  // Test that at least 15 ticks have been recorded for both parent and child
  // threads. When not tracking Worker threads, only 1 or 2 ticks would
  // have been recorded.
  // When running locally on x64 Linux, this number is usually at least 700
  // for both threads, so 15 seems like a very safe threshold.
  assert(ticks >= 15, `${ticks} >= 15`);
}
