'use strict';

// This tests NODE_COMPILE_CACHE works with the NODE_COMPILE_CACHE_RELATIVE_PATH
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
const cacheAbs = path.join(workDir, cacheRel);
fs.mkdirSync(workDir, { recursive: true });
const script = path.join(workDir, 'test.js');
fs.writeFileSync(script, '');

{
  fs.mkdirSync(cacheAbs, { recursive: true });
  spawnSyncAndAssert(
    process.execPath,
    [script],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: cacheRel,
        NODE_COMPILE_CACHE_RELATIVE_PATH: true,
      },
      cwd: workDir,
    },
    {
      stderr(output) {
        assert.match(
          output,
          /test\.js was not initialized, initializing the in-memory entry/
        );
        assert.match(output, /writing cache for .*test\.js.*success/);
        return true;
      },
    }
  );
}
{
  const movedWorkDir = `${workDir}_moved`;
  fs.renameSync(workDir, movedWorkDir);
  spawnSyncAndAssert(
    process.execPath,
    [path.join(movedWorkDir, 'test.js')],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: cacheRel,
        NODE_COMPILE_CACHE_RELATIVE_PATH: true,
      },
      cwd: movedWorkDir,
    },
    {
      stderr(output) {
        assert.match(
          output,
          /cache for .*test\.js was accepted, keeping the in-memory entry/
        );
        assert.match(output, /.*skip .*test\.js because cache was the same/);
        return true;
      },
    }
  );
}
