// This tests that --build-sea fails when the output file already contains a SEA.
// TODO(joyeecheung): support an option that allows overwriting the existing content.
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const { buildSEA, skipIfBuildSEAIsNotSupported } = require('../common/sea');
const fixtures = require('../common/fixtures');
skipIfBuildSEAIsNotSupported();

tmpdir.refresh();

const fixtureDir = fixtures.path('sea', 'already-exists');

// First, build a valid SEA.
buildSEA(fixtureDir);
buildSEA(fixtureDir, {
  configPath: 'sea-config-2.json',
  failure: /already exists/,
});
