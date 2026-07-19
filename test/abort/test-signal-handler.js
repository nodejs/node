'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('No signals on Window');

const assert = require('assert');
const { spawnSync } = require('child_process');

// Test that a hard crash does not cause an endless loop.

if (process.argv[2] === 'child') {
  const { internalBinding } = require('internal/test/binding');
  const { causeSegfault } = internalBinding('process_methods');

  causeSegfault();
} else {
  const child = spawnSync(process.execPath,
                          ['--expose-internals', __filename, 'child'],
                          { stdio: 'inherit' });
  // FreeBSD uses SIGILL (v12.2) or SIGBUS (v12.4 and greater) for this kind of crash.
  // macOS uses SIGILL or SIGTRAP (arm64) for this kind of crash.
  const allowedSignals = ['SIGSEGV', 'SIGILL', 'SIGTRAP', 'SIGBUS'];
  assert(
    allowedSignals.includes(child.signal),
    `child.signal = ${child.signal}`,
  );
}
