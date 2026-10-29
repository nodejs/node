'use strict';

// This tests NODE_COMPILE_CACHE works after moving directory and unusual characters in path are handled correctly.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const workDir = path.join(tmpdir.path, 'work');
const cacheRel = '.compile_cache_dir';
fs.mkdirSync(workDir, { recursive: true });

const script = path.join(workDir, 'message.mjs');
fs.writeFileSync(
  script,
  `
  export const message = 'A message';
  `
);

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
          /message\.mjs was not initialized, initializing the in-memory entry/
        );
        assert.match(output, /writing cache for .*message\.mjs.*success/);
        return true;
      },
    }
  );

  // Move the working directory and run again
  const movedWorkDir = `${workDir}_moved`;
  fs.renameSync(workDir, movedWorkDir);


  spawnSyncAndAssert(
    process.execPath,
    [[path.join(movedWorkDir, 'message.mjs')]],
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
          /cache for .*message\.mjs was accepted, keeping the in-memory entry/
        );
        assert.match(
          output,
          /.*skip .*message\.mjs because cache was the same/
        );
        return true;
      },
    }
  );
}
