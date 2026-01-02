'use strict';

require('../common');
const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the snapshot support in single executable applications.
const tmpdir = require('../common/tmpdir');

const {
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

tmpdir.refresh();
const outputFile = buildSEA(fixtures.path('sea', 'assets-raw'));

spawnSyncAndExitWithoutError(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
      __TEST_PERSON_JPG: fixtures.path('person.jpg'),
    },
  },
);
