'use strict';

// This tests NODE_COMPILE_CACHE works with the NODE_COMPILE_CACHE_PORTABLE
// environment variable.
// Also checks that module.getCompileCacheDir() returns the relative path.

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

const script = fixtures.path('compile-cache-wrapper.js');
const target = fixtures.path('snapshot', 'typescript.js');

// First run
{
  spawnSyncAndAssert(
    process.execPath,
    ['-r', script, target],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: cacheRel,
        NODE_COMPILE_CACHE_PORTABLE: '1',
      },
      cwd: workDir,
    },
    {
      stdout(output) {
        console.log(output); // For debugging
        assert.match(output, /Compile cache already enabled/);
        assert.match(
          output,
          new RegExp(`dir after enableCompileCache: ${cacheRel}`)
        );
        return true;
      },
      stderr(output) {
        assert.match(
          output,
          /typescript\.js was not initialized, initializing the in-memory entry/
        );
        assert.match(output, /writing cache for .*typescript\.js.*success/);
        return true;
      },
    }
  );
}

// Move the working directory and run again
{
  const movedWorkDir = `${workDir}_moved`;
  fs.renameSync(workDir, movedWorkDir);
  spawnSyncAndAssert(
    process.execPath,
    ['-r', script, target],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: cacheRel,
        NODE_COMPILE_CACHE_PORTABLE: '1'
      },
      cwd: movedWorkDir,
    },
    {
      stdout(output) {
        console.log(output); // For debugging
        assert.match(output, /Compile cache already enabled/);
        assert.match(
          output,
          new RegExp(`dir after enableCompileCache: ${cacheRel}`)
        );
        return true;
      },
      stderr(output) {
        assert.match(
          output,
          /cache for .*typescript\.js was accepted, keeping the in-memory entry/
        );
        assert.match(
          output,
          /.*skip .*typescript\.js because cache was the same/
        );
        return true;
      },
    }
  );
}
