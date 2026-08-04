'use strict';
// This tests --build-sea with a custom argv0 value.

require('../common');

const { skipIfBuildSEAIsNotSupported } = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { copyFileSync } = require('fs');

const { spawnSyncAndAssert } = require('../common/child_process');
tmpdir.refresh();

copyFileSync(
  fixtures.path('sea', 'basic', 'sea-config.json'),
  tmpdir.resolve('sea-config.json'),
);

copyFileSync(
  fixtures.path('sea', 'basic', 'sea.js'),
  tmpdir.resolve('sea.js'),
);

spawnSyncAndAssert(
  process.execPath,
  ['--build-sea', tmpdir.resolve('sea-config.json')], {
    cwd: tmpdir.path,
    argv0: 'argv0',
  }, {
    stdout: /Generated single executable/,
  });
