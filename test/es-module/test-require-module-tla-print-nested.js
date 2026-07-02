'use strict';

// Tests that require(esm) attaches the location of a top-level await found in
// an inner dependency of the graph if --experimental-print-required-tla is
// enabled. The entry point is a real CommonJS file that require()s the ESM.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  spawnSyncAndExit(process.execPath, [
    '--experimental-print-required-tla',
    fixtures.path('es-modules/tla/require-nested.js'),
  ], {
    signal: null,
    status: 1,
    stderr(output) {
      output = output.replace(/\r/g, '');
      common.expectRequiredTLAError(output);
      // The top-level await lives in a transitive dependency (a.mjs).
      assert(output.includes(`${fixtures.path('es-modules/tla/a.mjs')}:3`), output);
      assert(output.includes('await new Promise((resolve) => {\n^'), output);
      assert.match(output, /Require stack:/);
      assert.match(output, /require-nested\.js/);
      return true;
    },
    stdout: '',
  });
}
