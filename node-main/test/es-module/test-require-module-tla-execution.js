'use strict';

// Tests that require(esm) with top-level-await throws before execution starts
// if --experimental-print-required-tla is not enabled.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  spawnSyncAndExit(process.execPath, [
    fixtures.path('es-modules/tla/require-execution.js'),
  ], {
    signal: null,
    status: 1,
    stderr(output) {
      assert.doesNotMatch(output, /I am executed/);
      common.expectRequiredTLAError(output);
      assert.match(output, /From .*require-execution\.js/);
      assert.match(output, /Requiring .*execution\.mjs/);
      return true;
    },
    stdout: ''
  });
}
