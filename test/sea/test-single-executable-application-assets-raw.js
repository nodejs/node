'use strict';

require('../common');
const {
  generateSEA,
  skipIfSingleExecutableIsNotSupported,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

// This tests the snapshot support in single executable applications.
const tmpdir = require('../common/tmpdir');

const {
  spawnSyncAndExitWithoutError,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

tmpdir.refresh();
const outputFile = generateSEA(fixtures.path('sea', 'assets-raw'));

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
