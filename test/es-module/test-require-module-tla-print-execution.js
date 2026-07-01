'use strict';

// Tests that require(esm) with top-level-await throws before execution starts
// and attaches the location of the top-level await to the error
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
      output = output.replace(/\r/g, '');
      assert.doesNotMatch(output, /I am executed/);
      common.expectRequiredTLAError(output);
      // The location of the top-level await is shown with a caret.
      assert(output.includes(`${fixtures.path('es-modules/tla/execution.mjs')}:3`), output);
      assert(output.includes("await Promise.resolve('hi');\n^"), output);
      // The require() chain is shown as a require stack.
      assert.match(output, /Require stack:/);
      assert.match(output, /require-execution\.js/);
      return true;
    },
    stdout: '',
  });
}
