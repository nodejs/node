'use strict';

// Tests the output of require(esm) with top-level-await when --experimental-print-required-tla
// is NOT enabled.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  const filename = fixtures.path('es-modules/tla/require-execution.js');
  spawnSyncAndExit(process.execPath, [ filename ], {
    signal: null,
    status: 1,
    stderr(output) {
      assert.match(output, /To see where the top-level await comes from, use --experimental-print-required-tla/);
      assert.doesNotMatch(output, /I am executed/);
      common.expectRequiredTLAError(output, [filename]);
      return true;
    },
    stdout: '',
  });
}
