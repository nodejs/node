'use strict';

// This tests NODE_COMPILE_CACHE is disabled when policy is used.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

{
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const script = fixtures.path('policy', 'parent.js');
  const policy = fixtures.path(
    'policy',
    'dependencies',
    'dependencies-redirect-policy.json');
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-policy', policy, script],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir
      },
      cwd: tmpdir.path
    },
    {
      stderr: /skipping cache because policy is enabled/
    });
  assert(!fs.existsSync(dir));
}
