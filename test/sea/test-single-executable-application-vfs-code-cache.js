'use strict';

// This tests that "useCodeCache" combined with "useVfs" embeds a compile
// cache entry for every module in the virtual file system into the
// preparation blob, and that the resulting executable runs.

const common = require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { createWriteStream } = require('fs');
const { pipeline } = require('stream/promises');
const { zipFiles } = require('zlib');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');

// Every entry is stored as an asset under this reserved prefix, see
// src/node_sea.cc.
const kCompileCachePrefix = 'node:sea:compile-cache/';

function countCompileCacheEntries(blobPath) {
  const blob = fs.readFileSync(blobPath).toString('latin1');
  return blob.split(kCompileCachePrefix).length - 1;
}

// Generates the preparation blob of a fixture with "useCodeCache" and returns
// the number of compile cache entries it contains.
async function generateBlob(fixtureDir, prepare) {
  tmpdir.refresh();
  fs.cpSync(fixtureDir, tmpdir.path, { recursive: true });
  await prepare?.();
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', 'sea-config-code-cache.json'],
    {
      cwd: tmpdir.path,
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: 'SEA',
      },
    },
    {
      stderr: /Generated \d+ compile cache entries for \d+ modules/,
    });
  return countCompileCacheEntries(tmpdir.resolve('sea-prep.blob'));
}

async function main() {
  // CommonJS: sea.js, modules/math.js, modules/calculator.js and the two
  // packages under node_modules; config.json and greeting.txt are data.
  assert.strictEqual(await generateBlob(fixtures.path('sea', 'vfs')), 5);

  // ESM: main.mjs, modules/math.mjs, modules/calculator.cjs and the package.
  assert.strictEqual(await generateBlob(fixtures.path('sea', 'vfs-esm')), 4);

  // From a ZIP archive: sea.js, modules/math.js and the package.
  const zipFixture = (...args) => fixtures.path('sea', 'vfs-zip', ...args);
  assert.strictEqual(await generateBlob(zipFixture(), () => pipeline(
    zipFiles([
      [zipFixture('config.json'), 'config.json'],
      [zipFixture('greeting.txt'), 'data/greeting.txt'],
      [zipFixture('math.js'), 'modules/math.js'],
      [zipFixture('test-pkg-package.json'), 'node_modules/test-pkg/package.json'],
      [zipFixture('test-pkg-index.js'), 'node_modules/test-pkg/index.js'],
    ]),
    createWriteStream(tmpdir.resolve('assets.zip')),
  )), 3);

  // The build is not affected by the compile cache configuration of the
  // build process itself: the cache is generated although NODE_COMPILE_CACHE
  // points at an unusable location and the compile cache is disabled.
  tmpdir.refresh();
  fs.cpSync(fixtures.path('sea', 'vfs'), tmpdir.path, { recursive: true });
  spawnSyncAndAssert(
    process.execPath,
    ['--experimental-sea-config', 'sea-config-code-cache.json'],
    {
      cwd: tmpdir.path,
      env: {
        ...process.env,
        NODE_COMPILE_CACHE: path.join(tmpdir.path, 'config.json', 'nope'),
        NODE_DISABLE_COMPILE_CACHE: '1',
        NODE_DEBUG_NATIVE: undefined,
      },
    },
    {
      stderr: /Wrote single executable preparation blob/,
    });
  assert.strictEqual(
    countCompileCacheEntries(tmpdir.resolve('sea-prep.blob')), 5);

  // The executable runs with the cache embedded, and the reserved assets
  // are not visible as files of the virtual file system.
  tmpdir.refresh();
  const outputFile = buildSEA(fixtures.path('sea', 'vfs'), {
    configPath: 'sea-config-code-cache.json',
  });
  spawnSyncAndAssert(
    outputFile,
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: undefined,
      },
    },
    {
      stdout(stdout) {
        assert.match(stdout, /All SEA VFS tests passed!/);
        assert.doesNotMatch(stdout, /node:sea:/);
      },
      stderr(stderr) {
        assert.doesNotMatch(stderr, /Code cache data rejected/);
        assert.doesNotMatch(stderr, /ExperimentalWarning: VirtualFileSystem/);
      },
    },
  );
}

main().then(common.mustCall());
