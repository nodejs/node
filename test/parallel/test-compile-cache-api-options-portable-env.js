'use strict';

// This tests module.enableCompileCache() with an options object still works with environment
// variable NODE_COMPILE_CACHE overrides.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

const wrapper = fixtures.path('compile-cache-wrapper-options.js');
tmpdir.refresh();

// Create a build directory and copy the entry point file.
const buildDir = tmpdir.resolve('build');
fs.mkdirSync(buildDir);
const entryPoint = path.join(buildDir, 'empty.js');
fs.copyFileSync(fixtures.path('empty.js'), entryPoint);

// Check that the portable option can be overridden by NODE_COMPILE_CACHE_PORTABLE.
// We don't override NODE_COMPILE_CACHE because it will enable the cache before
// the wrapper is loaded.
spawnSyncAndAssert(
  process.execPath,
  ['-r', wrapper, entryPoint],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
      NODE_COMPILE_CACHE_PORTABLE: '1',
      NODE_TEST_COMPILE_CACHE_OPTIONS: JSON.stringify({ directory: 'build/.compile_cache' }),
    },
    cwd: tmpdir.path
  },
  {
    stdout(output) {
      console.log(output);  // Logging for debugging.
      assert.match(output, /dir before enableCompileCache: undefined/);
      assert.match(output, /Compile cache enabled/);
      assert.match(output, /dir after enableCompileCache: .*build[/\\]\.compile_cache/);
      return true;
    },
    stderr(output) {
      console.log(output);  // Logging for debugging.
      assert.match(output, /reading cache from .*build[/\\]\.compile_cache.* for CommonJS .*empty\.js/);
      assert.match(output, /empty\.js was not initialized, initializing the in-memory entry/);
      assert.match(output, /writing cache for .*empty\.js.*success/);
      return true;
    }
  });

assert(fs.existsSync(tmpdir.resolve('build/.compile_cache')));

const movedDir = buildDir + '_moved';
fs.renameSync(buildDir, movedDir);
const movedEntryPoint = path.join(movedDir, 'empty.js');

// When portable is undefined, it should use the env var NODE_COMPILE_CACHE_PORTABLE.
spawnSyncAndAssert(
  process.execPath,
  ['-r', wrapper, movedEntryPoint],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
      NODE_COMPILE_CACHE_PORTABLE: '1',
      NODE_TEST_COMPILE_CACHE_OPTIONS: JSON.stringify({ directory: 'build_moved/.compile_cache' }),
    },
    cwd: tmpdir.path
  },
  {
    stdout(output) {
      console.log(output);  // Logging for debugging.
      assert.match(output, /dir before enableCompileCache: undefined/);
      assert.match(output, /Compile cache enabled/);
      assert.match(output, /dir after enableCompileCache: .*build_moved[/\\]\.compile_cache/);
      return true;
    },
    stderr(output) {
      console.log(output);  // Logging for debugging.
      assert.match(output, /reading cache from .*build_moved[/\\]\.compile_cache.* for CommonJS .*empty\.js/);
      assert.match(output, /cache for .*empty\.js was accepted, keeping the in-memory entry/);
      assert.match(output, /.*skip .*empty\.js because cache was the same/);
      return true;
    }
  });
