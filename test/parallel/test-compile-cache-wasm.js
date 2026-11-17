'use strict';

// This tests NODE_COMPILE_CACHE works for WebAssembly modules imported as ESM.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');

const script = fixtures.path('es-modules', 'test-wasm-js-string-builtins.mjs');

// V8 only serializes optimized code, so a complete cache requires eager
// optimizing compilation.
{
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');
  const flags = ['--no-liftoff', '--no-wasm-lazy-compilation', '--no-warnings'];

  spawnSyncAndAssert(
    process.execPath,
    [...flags, script],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir,
      },
      cwd: tmpdir.path,
    },
    {
      stderr(output) {
        assert.match(output, /no cache for Wasm .*js-string-builtins\.wasm/);
        assert.match(output, /V8 code cache for Wasm .*js-string-builtins\.wasm was not initialized, initializing the in-memory entry/);
        assert.match(output, /writing cache for Wasm .*js-string-builtins\.wasm.*success/);
        return true;
      },
    });

  spawnSyncAndAssert(
    process.execPath,
    [...flags, script],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir,
      },
      cwd: tmpdir.path,
    },
    {
      stderr(output) {
        assert.match(output, /reading cache from .* for Wasm .*js-string-builtins\.wasm.*success/);
        assert.match(output, /deserializing Wasm .*js-string-builtins\.wasm.*success/);
        assert.match(output, /skip persisting Wasm .*js-string-builtins\.wasm because cache was the same/);
        return true;
      },
    });
}

// With the default lazy baseline compilation there is no optimized code to
// cache at compile time, and no cache entry is written.
{
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');

  spawnSyncAndAssert(
    process.execPath,
    ['--no-warnings', script],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir,
      },
      cwd: tmpdir.path,
    },
    {
      stderr(output) {
        assert.match(output, /nothing to serialize for Wasm .*js-string-builtins\.wasm/);
        assert.match(output, /skip persisting Wasm .*js-string-builtins\.wasm because the cache was not initialized/);
        return true;
      },
    });
}
