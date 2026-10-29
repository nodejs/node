'use strict';

// Test that nonexistent asset file is rejected.

const common = require('../common');  // eslint-disable-line no-unused-vars
const {
  spawnSyncAndExit,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync } = require('fs');

tmpdir.refresh();

// Copy the fixture files to tmpdir
copyFileSync(
  fixtures.path('sea', 'assets-nonexistent-file', 'sea-config.json'),
  tmpdir.resolve('sea-config.json'),
);
copyFileSync(
  fixtures.path('sea', 'assets-nonexistent-file', 'sea.js'),
  tmpdir.resolve('sea.js'),
);

spawnSyncAndExit(
  process.execPath,
  ['--experimental-sea-config', 'sea-config.json'],
  {
    cwd: tmpdir.path,
  },
  {
    status: 1,
    signal: null,
    stderr: /Cannot read asset nonexistent\.txt: no such file or directory/,
  });
