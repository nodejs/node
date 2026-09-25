'use strict';

// This tests NODE_COMPILE_CACHE works with the NODE_COMPILE_CACHE_PORTABLE
// environment variable.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const path = require('path');

tmpdir.refresh();
const workDir = path.join(tmpdir.path, 'work');
const cacheRel = '.compile_cache_dir';
fs.mkdirSync(workDir, { recursive: true });

const script = path.join(workDir, 'script.js');
fs.writeFileSync(script, '');

// First run
{
  spawnSyncAndAssert(
    process.execPath,
    [script],
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
      stderr(output) {
        console.log(output);
        assert.match(
          output,
          /script\.js was not initialized, initializing the in-memory entry/
        );
        assert.match(output, /writing cache for .*script\.js.*success/);
        return true;
      },
    }
  );

  // Move the working directory and run again
  const movedWorkDir = `${workDir}_moved`;
  fs.renameSync(workDir, movedWorkDir);
  spawnSyncAndAssert(
    process.execPath,
    [[path.join(movedWorkDir, 'script.js')]],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: cacheRel,
        NODE_COMPILE_CACHE_PORTABLE: '1',
      },
      cwd: movedWorkDir,
    },
    {
      stderr(output) {
        console.log(output);
        assert.match(
          output,
          /cache for .*script\.js was accepted, keeping the in-memory entry/
        );
        assert.match(output, /.*skip .*script\.js because cache was the same/);
        return true;
      },
    }
  );
}
