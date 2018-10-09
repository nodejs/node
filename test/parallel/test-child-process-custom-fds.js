// Flags: --expose_internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const internalCp = require('internal/child_process');

if (!common.isMainThread)
  common.skip('stdio is not associated with file descriptors in Workers');

// This test uses the deprecated `customFds` option. We expect a deprecation
// warning, but only once (per node process).
const msg = 'child_process: options.customFds option is deprecated. ' +
            'Use options.stdio instead.';
common.expectWarning('DeprecationWarning', msg, 'DEP0006');

// Verify that customFds is used if stdio is not provided.
{
  const customFds = [-1, process.stdout.fd, process.stderr.fd];
  const oldSpawnSync = internalCp.spawnSync;
  internalCp.spawnSync = common.mustCall(function(opts) {
    assert.deepStrictEqual(opts.options.customFds, customFds);
    assert.deepStrictEqual(opts.options.stdio, [
      { type: 'pipe', readable: true, writable: false },
      { type: 'fd', fd: process.stdout.fd },
      { type: 'fd', fd: process.stderr.fd }
    ]);
  });
  spawnSync(...common.pwdCommand, { customFds });
  internalCp.spawnSync = oldSpawnSync;
}

// Verify that customFds is ignored when stdio is present.
{
  const customFds = [0, 1, 2];
  const oldSpawnSync = internalCp.spawnSync;
  internalCp.spawnSync = common.mustCall(function(opts) {
    assert.deepStrictEqual(opts.options.customFds, customFds);
    assert.deepStrictEqual(opts.options.stdio, [
      { type: 'pipe', readable: true, writable: false },
      { type: 'pipe', readable: false, writable: true },
      { type: 'pipe', readable: false, writable: true }
    ]);
  });
  spawnSync(...common.pwdCommand, { customFds, stdio: 'pipe' });
  internalCp.spawnSync = oldSpawnSync;
}
