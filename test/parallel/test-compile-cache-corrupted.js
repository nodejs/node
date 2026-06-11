'use strict';

// This tests that NODE_COMPILE_CACHE gracefully discards corrupted
// cache files and regenerates them.

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const tmpdir = require('../common/tmpdir');

// Offsets into the cache file headers (see src/compile_cache.h).
const kHeaderSize = 6 * 4;
const kCacheRawSizeOffset = 5 * 4;

function readU32(buf, offset) {
  return os.endianness() === 'LE' ?
    buf.readUInt32LE(offset) : buf.readUInt32BE(offset);
}

function writeU32(buf, value, offset) {
  if (os.endianness() === 'LE') {
    buf.writeUInt32LE(value, offset);
  } else {
    buf.writeUInt32BE(value, offset);
  }
}

tmpdir.refresh();
const dir = tmpdir.resolve('.compile_cache_dir');
const script = tmpdir.resolve('script.js');
fs.writeFileSync(script, 'const foo = 1;', 'utf-8');

const env = {
  ...process.env,
  NODE_DEBUG_NATIVE: 'COMPILE_CACHE',
  NODE_COMPILE_CACHE: dir,
};

function getCacheFile() {
  const subdirs = fs.readdirSync(dir);
  assert.strictEqual(subdirs.length, 1);
  const entries = fs.readdirSync(path.join(dir, subdirs[0]));
  assert.strictEqual(entries.length, 1);
  return path.join(dir, subdirs[0], entries[0]);
}

// Runs the script and expects the corrupted cache to be discarded
// with the given debug message and then regenerated.
function expectRecovery(mismatchRE) {
  spawnSyncAndAssert(
    process.execPath,
    [script],
    { env, cwd: tmpdir.path },
    {
      stderr(output) {
        console.log(output);  // Logging for debugging.
        assert.match(output, mismatchRE);
        assert.match(output, /writing cache for .*script\.js.*success/);
        return true;
      }
    });
}

// Warm the cache.
spawnSyncAndAssert(
  process.execPath,
  [script],
  { env, cwd: tmpdir.path },
  {
    stderr(output) {
      console.log(output);  // Logging for debugging.
      assert.match(output, /writing cache for .*script\.js.*success/);
      return true;
    }
  });
const cacheFile = getCacheFile();
assert(fs.readFileSync(cacheFile).length > kHeaderSize);

{
  // Corrupt the magic number.
  const data = fs.readFileSync(cacheFile);
  for (let i = 0; i < 4; i++) data[i] ^= 0xff;
  fs.writeFileSync(cacheFile, data);
  expectRecovery(
    /reading cache from .* for CommonJS .*script\.js.*magic number mismatch/);
}

{
  // Truncate the cache content.
  const data = fs.readFileSync(cacheFile);
  fs.writeFileSync(cacheFile, data.subarray(0, data.length - 3));
  expectRecovery(
    /reading cache from .* for CommonJS .*script\.js.*cache size mismatch/);
}

{
  // Flip a byte in the middle of the cache content.
  const data = fs.readFileSync(cacheFile);
  data[kHeaderSize + Math.floor((data.length - kHeaderSize) / 2)] ^= 0xff;
  fs.writeFileSync(cacheFile, data);
  expectRecovery(
    /reading cache from .* for CommonJS .*script\.js.*cache hash mismatch/);
}

{
  // Corrupt the uncompressed size field in the headers.
  const data = fs.readFileSync(cacheFile);
  writeU32(data, readU32(data, kCacheRawSizeOffset) + 1, kCacheRawSizeOffset);
  fs.writeFileSync(cacheFile, data);
  expectRecovery(
    /reading cache from .* for CommonJS .*script\.js.*uncompressed size mismatch/);
}

// After the last recovery the cache should be consumed just fine.
spawnSyncAndAssert(
  process.execPath,
  [script],
  { env, cwd: tmpdir.path },
  {
    stderr(output) {
      console.log(output);  // Logging for debugging.
      assert.match(output, /cache for .*script\.js was accepted/);
      return true;
    }
  });
