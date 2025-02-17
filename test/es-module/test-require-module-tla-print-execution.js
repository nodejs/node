'use strict';

// Tests that require(esm) with top-level-await throws after execution
// if --experimental-print-required-tla is enabled.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  spawnSyncAndExit(process.execPath, [
    '--experimental-print-required-tla',
    fixtures.path('es-modules/tla/require-execution.js'),
  ], {
    signal: null,
    status: 1,
    stderr(output) {
      assert.match(output, /I am executed/);
      common.expectRequiredTLAError(output);
      assert.match(output, /Error: unexpected top-level await at.*execution\.mjs:3/);
      assert.match(output, /await Promise\.resolve\('hi'\)/);
      assert.match(output, /From .*require-execution\.js/);
      assert.match(output, /Requiring .*execution\.mjs/);
      return true;
    },
    stdout: ''
  });
}
