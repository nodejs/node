'use strict';
// This tests --build-sea with a custom argv0 value.

require('../common');

const { skipIfBuildSEAIsNotSupported } = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const { resolve } = require('path');
const fixtures = require('../common/fixtures');
const { copyFileSync } = require('fs');

const { spawnSyncAndAssert } = require('../common/child_process');

const fixturesDir = fixtures.path('sea', 'basic');
const tempDir = tmpdir.path;

copyFileSync(
  resolve(fixturesDir, 'sea-config.json'),
  resolve(tempDir, 'sea-config.json'),
);

copyFileSync(
  resolve(fixturesDir, 'sea.js'),
  resolve(tempDir, 'sea.js'),
);

spawnSyncAndAssert(
  process.execPath,
  ['--build-sea', resolve(tempDir, 'sea-config.json')], {
    cwd: tempDir,
    argv0: 'argv0',
  }, {
    stdout: /Generated single executable/,
  });
