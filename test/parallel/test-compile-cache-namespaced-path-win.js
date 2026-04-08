'use strict';

// This tests NODE_COMPILE_CACHE works with a Windows namespaced path.

const common = require('../common');
if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
}

const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

{
  tmpdir.refresh();
  const cacheDir = tmpdir.resolve('.compile_cache_dir');
  const namespacedCacheDir = path.toNamespacedPath(cacheDir);

  spawnSyncAndAssert(
    process.execPath,
    [fixtures.path('empty.js')],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: namespacedCacheDir,
      },
      cwd: tmpdir.path,
    },
    {
      stderr(output) {
        assert.match(output, /writing cache for .*empty\.js.*success/);
        return true;
      },
    });

  const topEntries = fs.readdirSync(cacheDir);
  assert.strictEqual(topEntries.length, 1);
  const cacheEntries = fs.readdirSync(path.join(cacheDir, topEntries[0]));
  assert.strictEqual(cacheEntries.length, 1);

  spawnSyncAndAssert(
    process.execPath,
    [fixtures.path('empty.js')],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: namespacedCacheDir,
      },
      cwd: tmpdir.path,
    },
    {
      stderr(output) {
        assert.match(output, /cache for .*empty\.js was accepted/);
        return true;
      },
    });
}
