'use strict';

// Tests that require(esm) with top-level-await throws before execution starts
// and attaches the location of the top-level await to the error
// if --experimental-print-required-tla is enabled.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  const filename = fixtures.path('es-modules/tla/require-execution.js');
  spawnSyncAndExit(process.execPath, [
    '--experimental-print-required-tla',
    filename,
  ], {
    signal: null,
    status: 1,
    stderr(output) {
      output = output.replace(/\r/g, '');
      assert.doesNotMatch(output, /I am executed/);
      common.expectRequiredTLAError(output, [filename]);
      // The immediately required module is shown regardless of the flag.
      assert.match(output, /Required module: .*tla[/\\]execution\.mjs/);
      // The location of the top-level await is shown with a caret.
      assert.match(output, /tla\/execution\.mjs:3/);
      assert(output.includes("await Promise.resolve('hi');\n^"), output);
      return true;
    },
    stdout: '',
  });
}
