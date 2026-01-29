// Flags: --no-warnings
'use strict';

// Test that running a main entry point with ESM syntax in a "type": "commonjs"
// package throws an error instead of silently failing with exit code 0.
// Regression test for https://github.com/nodejs/node/issues/61104

require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');
const fixtures = require('../common/fixtures.js');
const { execPath } = require('node:process');

const mainScript = fixtures.path('es-modules/package-type-commonjs-esm-syntax/esm-script.js');

spawnSyncAndAssert(execPath, [mainScript], {
  status: 1,
  signal: null,
  stderr: /import { version } from 'node:process'/,
});
