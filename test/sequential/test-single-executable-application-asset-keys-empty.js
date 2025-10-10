'use strict';

// This test verifies that the `getAssetKeys()` function works correctly
// in a single executable application without any assets.

require('../common');

const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

const tmpdir = require('../common/tmpdir');

const { copyFileSync, writeFileSync, existsSync } = require('fs');
const {
  spawnSyncAndExitWithoutError,
  spawnSyncAndAssert,
} = require('../common/child_process');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const seaPrepBlob = tmpdir.resolve('sea-prep.blob');
const outputFile = tmpdir.resolve(process.platform === 'win32' ? 'sea.exe' : 'sea');

tmpdir.refresh();
copyFileSync(fixtures.path('sea', 'get-asset-keys.js'), tmpdir.resolve('sea.js'));

writeFileSync(tmpdir.resolve('sea-config.json'), `
{
  "main": "sea.js",
  "output": "sea-prep.blob"
}
`, 'utf8');

spawnSyncAndExitWithoutError(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
    cwd: tmpdir.path
  },
  {});

assert(existsSync(seaPrepBlob));

generateSEA(outputFile, process.execPath, seaPrepBlob);

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
    }
  },
  {
    stdout: /Asset keys: \[\]/,
  }
);
