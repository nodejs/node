'use strict';
const common = require('../common');

// This test ensures that an out of memory error exits with code 134 on Windows

if (!common.isWindows) return common.skip('Windows-only');

const assert = require('assert');
const { exec } = require('child_process');

if (process.argv[2] === 'heapBomb') {
  // heap bomb, imitates a memory leak quickly
  const fn = (nM) => [...Array(nM)].map((i) => fn(nM * 2));
  fn(2);
}

// --max-old-space-size=3 is the min 'old space' in V8, explodes fast
const cmd = `"${process.execPath}" --max-old-space-size=3 "${__filename}"`;
exec(`${cmd} heapBomb`, common.mustCall((err) => {
  const msg = `Wrong exit code of ${err.code}! Expected 134 for abort`;
  // Note: common.nodeProcessAborted() is not asserted here because it
  // returns true on 134 as well as 0xC0000005 (V8's base::OS::Abort)
  assert.strictEqual(err.code, 134, msg);
}));
