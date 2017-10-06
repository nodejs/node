// Flags: --expose_internals
'use strict';
const common = require('../common');
const assert = require('assert');
const internalCp = require('internal/child_process');
const oldSpawnSync = internalCp.spawnSync;

// Verify that customFds is used if stdio is not provided.
{
  const msg = 'child_process: options.customFds option is deprecated. ' +
              'Use options.stdio instead.';
  common.expectWarning('DeprecationWarning', msg);

  const customFds = [-1, process.stdout.fd, process.stderr.fd];
  internalCp.spawnSync = common.mustCall(function(opts) {
    assert.deepStrictEqual(opts.options.customFds, customFds);
    assert.deepStrictEqual(opts.options.stdio, [
      { type: 'pipe', readable: true, writable: false },
      { type: 'fd', fd: process.stdout.fd },
      { type: 'fd', fd: process.stderr.fd }
    ]);
  });
  common.spawnSyncPwd({ customFds });
  internalCp.spawnSync = oldSpawnSync;
}

// Verify that customFds is ignored when stdio is present.
{
  const customFds = [0, 1, 2];
  internalCp.spawnSync = common.mustCall(function(opts) {
    assert.deepStrictEqual(opts.options.customFds, customFds);
    assert.deepStrictEqual(opts.options.stdio, [
      { type: 'pipe', readable: true, writable: false },
      { type: 'pipe', readable: false, writable: true },
      { type: 'pipe', readable: false, writable: true }
    ]);
  });
  common.spawnSyncPwd({ customFds, stdio: 'pipe' });
  internalCp.spawnSync = oldSpawnSync;
}
