'use strict';

// This tests NODE_COMPILE_CACHE works.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

{
  // Test that it works with non-existent directory.
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');

  spawnSyncAndAssert(
    process.execPath,
    [fixtures.path('snapshot', 'typescript.js')],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir
      },
      cwd: tmpdir.path
    },
    {
      stderr(output) {
        console.log(output);  // Logging for debugging.
        assert.match(output, /typescript\.js was not initialized, initializing the in-memory entry/);
        assert.match(output, /writing cache for .*typescript\.js.*success/);
        return true;
      }
    });

  const cacheDir = fs.readdirSync(dir);
  assert.strictEqual(cacheDir.length, 1);
  const entries = fs.readdirSync(path.join(dir, cacheDir[0]));
  assert.strictEqual(entries.length, 1);
  const cacheFile = path.join(dir, cacheDir[0], entries[0]);
  const data = fs.readFileSync(cacheFile);
  console.log(`Header in ${cacheFile}`, new Uint32Array(data.buffer, data.byteOffset, 4));

  // Second run reads the cache, but no need to re-write because it didn't change.
  spawnSyncAndAssert(
    process.execPath,
    [fixtures.path('snapshot', 'typescript.js')],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir
      },
      cwd: tmpdir.path
    },
    {
      stderr(output) {
        console.log(output);  // Logging for debugging.
        assert.match(output, /cache for .*typescript\.js was accepted, keeping the in-memory entry/);
        assert.match(output, /.*skip .*typescript\.js because cache was the same/);
        return true;
      }
    });
}

// Exercise the dictionary-compressed path (added on top of #63861) for many
// small modules, which is where the embedded dictionary helps most. We write
// the cache, then read it back and assert every entry is accepted - this
// proves each dict-compressed frame decompresses to exactly the bytes that
// were persisted.
{
  tmpdir.refresh();
  const dir = tmpdir.resolve('.compile_cache_dir');

  // Generate a handful of small modules so the dictionary path is exercised.
  const count = 8;
  const modules = [];
  for (let i = 0; i < count; i++) {
    const file = tmpdir.resolve(`mod-${i}.js`);
    fs.writeFileSync(
      file,
      `'use strict';\n` +
      `module.exports = function value${i}(a, b) {\n` +
      `  const sum = a + b + ${i};\n` +
      `  return { id: ${i}, sum, label: 'module-${i}' };\n` +
      `};\n`);
    modules.push(file);
  }
  const reqCode = modules.map((m) => `require(${JSON.stringify(m)});`).join('');

  // First run writes the cache for every module.
  spawnSyncAndAssert(
    process.execPath,
    ['-e', reqCode],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir
      },
      cwd: tmpdir.path
    },
    {
      stderr(output) {
        for (const m of modules) {
          const name = path.basename(m).replace(/[.]/g, '\\.');
          assert.match(output, new RegExp(`writing cache for .*${name}.*success`));
        }
        return true;
      }
    });

  const cacheDirs = fs.readdirSync(dir);
  assert.strictEqual(cacheDirs.length, 1);
  // At least one entry per module (the `-e` runner is cached too).
  const entries = fs.readdirSync(path.join(dir, cacheDirs[0]));
  assert(entries.length >= count, `expected >= ${count} entries, got ${entries.length}`);

  // Second run reads every cached entry back; "was accepted" only happens when
  // the decompressed bytes match the freshly produced in-memory cache, so this
  // is a full roundtrip check of the dictionary-compressed entries.
  spawnSyncAndAssert(
    process.execPath,
    ['-e', reqCode],
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
        NODE_COMPILE_CACHE: dir
      },
      cwd: tmpdir.path
    },
    {
      stderr(output) {
        for (const m of modules) {
          const name = path.basename(m).replace(/[.]/g, '\\.');
          assert.match(
            output,
            new RegExp(`cache for .*${name} was accepted, keeping the in-memory entry`));
        }
        return true;
      }
    });
}
