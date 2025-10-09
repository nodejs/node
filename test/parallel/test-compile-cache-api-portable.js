'use strict';

// This tests module.enableCompileCache({ directory, portable: true }) works
// and supports portable paths across directory relocations.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const path = require('path');
const fixtures = require('../common/fixtures');

tmpdir.refresh();
const workDir = path.join(tmpdir.path, 'work');
const cacheRel = '.compile_cache_dir';
fs.mkdirSync(workDir, { recursive: true });

const wrapper = fixtures.path('compile-cache-wrapper-options.js');
const target = path.join(workDir, 'target.js');

fs.writeFileSync(target, '');
const NODE_TEST_COMPILE_CACHE_OPTIONS = JSON.stringify({ directory: cacheRel, portable: true });

// First run
{
  spawnSyncAndAssert(
    process.execPath,
    ['-r', wrapper, target],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_TEST_COMPILE_CACHE_OPTIONS,
      },
      cwd: workDir,
    },
    {
      stdout(output) {
        console.log(output);
        assert.match(output, /dir before enableCompileCache: undefined/);
        assert.match(
          output,
          /dir after enableCompileCache: .+\.compile_cache_dir/
        );
        return true;
      },
      stderr(output) {
        assert.match(
          output,
          /target\.js was not initialized, initializing the in-memory entry/
        );
        assert.match(output, /writing cache for .*target\.js.*success/);
        return true;
      },
    }
  );
}

// Second run — moved directory, but same relative cache path
{
  const movedWorkDir = `${workDir}_moved`;
  fs.renameSync(workDir, movedWorkDir);

  spawnSyncAndAssert(
    process.execPath,
    [
      '-r',
      wrapper,
      path.join(movedWorkDir, 'target.js'),
    ],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_TEST_COMPILE_CACHE_OPTIONS,
      },
      cwd: movedWorkDir,
    },
    {
      stdout(output) {
        console.log(output);
        assert.match(output, /dir before enableCompileCache: undefined/);
        assert.match(
          output,
          /dir after enableCompileCache: .+\.compile_cache_dir/
        );
        return true;
      },
      stderr(output) {
        assert.match(
          output,
          /cache for .*target\.js was accepted, keeping the in-memory entry/
        );
        assert.match(output, /.*skip .*target\.js because cache was the same/);
        return true;
      },
    }
  );
}
