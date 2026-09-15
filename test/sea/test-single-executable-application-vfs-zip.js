'use strict';

// This tests the SEA VFS archive integration - a prebuilt ZIP archive of the
// assets is embedded into the executable and mounted through the ZipProvider.

const common = require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const { createWriteStream } = require('fs');
const { pipeline } = require('stream/promises');
const { zipFiles } = require('zlib');

async function main() {
  tmpdir.refresh();

  // Build the assets archive with the ZIP support of node:zlib. This is what
  // "vfsArchive" users do themselves before running --build-sea.
  const fixture = (...args) => fixtures.path('sea', 'vfs-zip', ...args);
  await pipeline(
    zipFiles([
      [fixture('config.json'), 'config.json'],
      [fixture('greeting.txt'), 'data/greeting.txt'],
      [fixture('math.js'), 'modules/math.js'],
      [fixture('test-pkg-package.json'), 'node_modules/test-pkg/package.json'],
      [fixture('test-pkg-index.js'), 'node_modules/test-pkg/index.js'],
    ]),
    createWriteStream(tmpdir.resolve('assets.zip')),
  );

  const outputFile = buildSEA(fixtures.path('sea', 'vfs-zip'));

  spawnSyncAndAssert(
    outputFile,
    {
      env: {
        ...process.env,
        NODE_DEBUG_NATIVE: undefined,
      },
    },
    {
      stdout: /All SEA VFS zip tests passed!/,
      stderr(stderr) {
        if (/ExperimentalWarning: VirtualFileSystem/.test(stderr)) {
          throw new Error('SEA VFS should not emit the public VirtualFileSystem warning');
        }
      },
    },
  );
}

main().then(common.mustCall());
