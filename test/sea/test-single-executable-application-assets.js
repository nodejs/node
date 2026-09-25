'use strict';

// This tests the assets support in single executable applications.

require('../common');
const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const {
  spawnSyncAndAssert,
} = require('../common/child_process');
const fixtures = require('../common/fixtures');

tmpdir.refresh();
const outputFile = buildSEA(fixtures.path('sea', 'assets'));

spawnSyncAndAssert(
  outputFile,
  {
    env: {
      ...process.env,
      NODE_DEBUG_NATIVE: 'SEA',
      __TEST_PERSON_JPG: fixtures.path('person.jpg'),
      __TEST_UTF8_TEXT_PATH: fixtures.path('utf8_test_text.txt'),
    },
  },
  {
    trim: true,
    stdout: fixtures.utf8TestText,
  },
);
