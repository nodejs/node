'use strict';

// This tests that import() works in a CJS single executable application
// when useCodeCache is true. A V8 fix (https://chromium-review.googlesource.com
// /c/v8/v8/+/5401780) resolved the issue where code cache serialization
// wiped host-defined options needed by HostImportModuleDynamically.

require('../common');

const {
  buildSEA,
  skipIfBuildSEAIsNotSupported,
} = require('../common/sea');

skipIfBuildSEAIsNotSupported();

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const { spawnSyncAndAssert } = require('../common/child_process');

tmpdir.refresh();

const outputFile = buildSEA(fixtures.path('sea', 'use-code-cache-dynamic-import'));

spawnSyncAndAssert(
  outputFile,
  [],
  {
    env: {
      NODE_DEBUG_NATIVE: 'SEA',
      ...process.env,
    },
  },
  {
    stdout: 'dynamic import with code cache works\n',
  });
