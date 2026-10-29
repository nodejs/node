'use strict';

const common = require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

// This tests the creation of a single executable application with an empty
// script.

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

tmpdir.refresh();

let outputFile;
try {
  outputFile = buildSEA(fixtures.path('sea', 'empty'), {
    verifyWorkflow: true,
  });
} catch (e) {
  if (/Cannot copy/.test(e.message)) {
    common.skip(e.message);
  } else if (common.isWindows) {
    if (/Cannot sign/.test(e.message) || /Cannot find signtool/.test(e.message)) {
      common.skip(e.message);
    }
  }

  throw e;
}

spawnSyncAndExitWithoutError(
  outputFile,
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  });
