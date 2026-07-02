'use strict';

// Tests that require(esm) with --experimental-print-required-tla points the
// caret at the right column for indented top-level await and for-await-of.

const common = require('../common');
const assert = require('assert');
const { spawnSyncAndExit } = require('../common/child_process');
const fixtures = require('../common/fixtures');

{
  spawnSyncAndExit(process.execPath, [
    '--experimental-print-required-tla',
    fixtures.path('es-modules/tla/require-indented.js'),
  ], {
    signal: null,
    status: 1,
    stderr(output) {
      output = output.replace(/\r/g, '');
      common.expectRequiredTLAError(output);
      // Indented await: the caret is aligned under the await.
      assert(output.includes('  await Promise.resolve(ready);\n  ^'), output);
      // for-await-of: the caret points at the for keyword.
      assert(output.includes('for await (const x of [Promise.resolve(1)]) {\n^'), output);
      return true;
    },
    stdout: '',
  });
}
