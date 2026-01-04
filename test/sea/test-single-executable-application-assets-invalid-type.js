'use strict';

// Test that invalid "assets" field (not a map) is rejected.

require('../common');
const {
  spawnSyncAndExit,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const { copyFileSync } = require('fs');

tmpdir.refresh();

// Copy the fixture files to tmpdir
copyFileSync(
  fixtures.path('sea', 'assets-invalid-type', 'sea-config.json'),
  tmpdir.resolve('sea-config.json'),
);
copyFileSync(
  fixtures.path('sea', 'assets-invalid-type', 'sea.js'),
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
    stderr: /"assets" field of .*sea-config\.json is not a map of strings/,
  });
