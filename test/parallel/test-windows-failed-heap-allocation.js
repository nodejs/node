'use strict';
const common = require('../common');

// This test ensures that an out of memory error exits with code 134 on Windows

if (!common.isWindows) return common.skip('Windows-only');

const assert = require('assert');
const { exec } = require('child_process');

if (process.argv[2] === 'heapBomb') {
  // Heap bomb, imitates a memory leak quickly
  const fn = (nM) => [...Array(nM)].map((i) => fn(nM * 2));
  fn(2);
}

// Run child in tmpdir to avoid report files in repo
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// --max-old-space-size=3 is the min 'old space' in V8, explodes fast
const cmd = `"${process.execPath}" --max-old-space-size=30 "${__filename}"`;
exec(`${cmd} heapBomb`, { cwd: tmpdir.path }, common.mustCall((err, stdout, stderr) => {
  const msg = `Wrong exit code of ${err.code}! Expected 134 for abort`;
  // Note: common.nodeProcessAborted() is not asserted here because it
  // returns true on 134 as well as 0x80000003 (V8's base::OS::Abort)
  assert.strictEqual(err.code, 134, msg);
}));
