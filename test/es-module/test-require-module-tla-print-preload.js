'use strict';

// Tests that preloading an ESM with top-level await via -r throws before
// execution starts and attaches the location of the top-level await
// if --experimental-print-required-tla is enabled.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  spawnSyncAndExit(process.execPath, [
    '--experimental-print-required-tla',
    '-r', fixtures.path('es-modules/tla/execution.mjs'),
    '-e', 'console.log("main ran")',
  ], {
    signal: null,
    status: 1,
    stderr(output) {
      output = output.replace(/\r/g, '');
      // The main script should not run because preloading fails first.
      assert.doesNotMatch(output, /main ran/);
      assert.doesNotMatch(output, /I am executed/);
      common.expectRequiredTLAError(output);
      assert(output.includes(`${fixtures.path('es-modules/tla/execution.mjs')}:3`), output);
      assert(output.includes("await Promise.resolve('hi');\n^"), output);
      return true;
    },
    stdout: '',
  });
}
