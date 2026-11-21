'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { spawnSync } = require('child_process');

// randomFillSync() should be reported by --trace-sync-io when it runs after
// the first event loop turn.

if (process.argv[2] === 'child') {
  setImmediate(() => {
    require('crypto').randomFillSync(Buffer.alloc(16));
  });
  return;
}

const { stderr, status } = spawnSync(process.execPath,
                                     ['--trace-sync-io', __filename, 'child'],
                                     { encoding: 'utf8' });
assert.strictEqual(status, 0);
assert.match(stderr, /WARNING: Detected use of sync API[\s\S]*randomFillSync/);
