'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const cacheDir = tmpdir.resolve('compile_cache');
const env = {
  ...process.env,
  NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
  NODE_V8_COVERAGE: tmpdir.resolve('coverage'),
};

spawnSyncAndAssert(
  process.execPath,
  [fixtures.path('empty.js')],
  { env: { ...env, NODE_COMPILE_CACHE: cacheDir }, cwd: tmpdir.path },
  { stderr: /Disabled because V8 coverage is being collected/ });
assert(!fs.existsSync(cacheDir));

spawnSyncAndAssert(
  process.execPath,
  ['-r', fixtures.path('compile-cache-wrapper.js'), fixtures.path('empty.js')],
  {
    env: {
      ...env,
      NODE_COMPILE_CACHE: undefined,
      NODE_TEST_COMPILE_CACHE_DIR: cacheDir,
    },
    cwd: tmpdir.path,
  },
  {
    stdout(output) {
      assert.match(output, /Compile cache already disabled/);
      assert.match(output, /dir after enableCompileCache: undefined/);
    },
    stderr: /Disabled because V8 coverage is being collected/,
  });
assert(!fs.existsSync(cacheDir));
