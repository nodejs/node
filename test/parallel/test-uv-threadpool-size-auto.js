'use strict';
require('../common');

const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const os = require('os');

const expectedSize = Math.min(Math.max(4, os.availableParallelism()), 1024);

// When UV_THREADPOOL_SIZE is not set, Node.js should auto-size it based on
// uv_available_parallelism(), with a minimum of 4 and a maximum of 1024.
{
  const env = { ...process.env };
  delete env.UV_THREADPOOL_SIZE;

  spawnSyncAndAssert(
    process.execPath,
    ['-e', 'console.log(process.env.UV_THREADPOOL_SIZE)'],
    { env },
    {
      stdout(output) {
        assert.strictEqual(output.trim(), String(expectedSize));
      },
    },
  );
}

// When UV_THREADPOOL_SIZE is explicitly set, Node.js should not override it.
{
  const env = { ...process.env, UV_THREADPOOL_SIZE: '8' };

  spawnSyncAndAssert(
    process.execPath,
    ['-e', 'console.log(process.env.UV_THREADPOOL_SIZE)'],
    { env },
    {
      stdout(output) {
        assert.strictEqual(output.trim(), '8');
      },
    },
  );
}
