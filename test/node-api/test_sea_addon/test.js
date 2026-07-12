'use strict';
// This tests that SEA can load addons packaged as assets by writing them to disk
// and loading them via process.dlopen().
const common = require('../../common');
const { generateSEA, skipIfSingleExecutableIsNotSupported } = require('../../common/sea');

skipIfSingleExecutableIsNotSupported();

const tmpdir = require('../../common/tmpdir');
const { copyFileSync, rmSync, cpSync } = require('fs');
const {
  spawnSyncAndAssert,
} = require('../../common/child_process');
const { join } = require('path');
const fixtures = require('../../common/fixtures');

const addonPath = join(__dirname, 'build', common.buildType, 'binding.node');
const copiedAddonPath = tmpdir.resolve('binding.node');

tmpdir.refresh();

// Copy fixture files to working directory.
const fixtureDir = fixtures.path('sea', 'addon');
cpSync(fixtureDir, tmpdir.path, { recursive: true });

// Copy the built addon to working directory.
copyFileSync(addonPath, copiedAddonPath);

// Generate the SEA using the working directory directly (skip copy).
const outputFile = generateSEA(tmpdir.path, tmpdir.path);

// Remove the copied addon after it's been packaged into the SEA blob.
rmSync(copiedAddonPath, { force: true });

spawnSyncAndAssert(
  outputFile,
  [],
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
    },
    cwd: tmpdir.path,
  },
  {
    stdout: /hello, world/,
  },
);
